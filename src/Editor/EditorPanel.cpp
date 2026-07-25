#include "Editor/EditorPanel.h"

#include "imgui.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>

namespace YRender
{
namespace
{
constexpr float kToolbarHeight = 44.0f;
constexpr float kSplitterSize = 6.0f;
constexpr float kMinSideWidth = 180.0f;
constexpr float kMinBottomHeight = 140.0f;
constexpr float kMinPreviewWidth = 360.0f;
constexpr float kMinPreviewHeight = 240.0f;
constexpr float kPi = 3.14159265358979323846f;

float ToDegrees(float radians)
{
    return radians * 180.0f / kPi;
}

float ToRadians(float degrees)
{
    return degrees * kPi / 180.0f;
}

void DragRotationDegrees(const char* label, glm::vec3& rotation)
{
    float degrees[3] = {ToDegrees(rotation.x), ToDegrees(rotation.y), ToDegrees(rotation.z)};
    if (ImGui::DragFloat3(label, degrees, 0.5f, -360.0f, 360.0f, "%.1f"))
    {
        rotation.x = ToRadians(degrees[0]);
        rotation.y = ToRadians(degrees[1]);
        rotation.z = ToRadians(degrees[2]);
    }
}

const char* ObjectLabel(const SceneObject& object, int index)
{
    if (!object.name.empty())
    {
        return object.name.c_str();
    }

    static std::string fallback;
    fallback = "Object " + std::to_string(index);
    return fallback.c_str();
}

ImTextureRef TextureRef(unsigned int texture)
{
    return ImTextureRef(static_cast<ImTextureID>(texture));
}

bool IsShaderStatusError(const std::string& status)
{
    return status.find("failed") != std::string::npos ||
        status.find("error") != std::string::npos ||
        status.find("Unable") != std::string::npos;
}

std::string ToolbarShaderStatus(const std::string& status)
{
    if (IsShaderStatusError(status))
    {
        return "Shader compile failed, see Diagnostics";
    }
    if (status == "Shaders reloaded successfully")
    {
        return "Shaders recompiled successfully";
    }
    return status;
}

void PreviewTexture(const char* label, unsigned int texture, const ImVec2& size)
{
    ImGui::TextUnformatted(label);
    if (texture != 0)
    {
        ImGui::Image(TextureRef(texture), size, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
    else
    {
        ImGui::Button("Unavailable", size);
    }
}

float ClampFloat(float value, float minValue, float maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

void NormalizeDirection(glm::vec3& direction)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (length <= 0.0001f)
    {
        direction = glm::vec3(-0.35f, -0.85f, 0.35f);
        return;
    }

    direction.x /= length;
    direction.y /= length;
    direction.z /= length;
}

void ToolbarLabel(const char* label)
{
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
}

void RenderSplitter(const char* id, const ImVec2& pos, const ImVec2& size, float* target, float direction, float minValue, float maxValue, bool vertical)
{
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(id, size);
    if (ImGui::IsItemActive())
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        *target = ClampFloat(*target + (vertical ? delta.x : delta.y) * direction, minValue, maxValue);
    }

    const ImU32 color = ImGui::GetColorU32(ImGui::IsItemHovered() || ImGui::IsItemActive() ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), color);
}

void RenderLayoutSplitters(const ImGuiViewport* viewport, ShaderLabLayout& layout)
{
    const ImVec2 workPos = viewport->WorkPos;
    const ImVec2 workSize = viewport->WorkSize;
    const float contentY = workPos.y + kToolbarHeight;
    const float contentHeight = std::max(1.0f, workSize.y - kToolbarHeight);
    const float maxSideWidth = std::max(kMinSideWidth, (workSize.x - kMinPreviewWidth) * 0.5f);
    const float maxBottomHeight = std::max(kMinBottomHeight, contentHeight - kMinPreviewHeight);

    layout.leftWidth = ClampFloat(layout.leftWidth, kMinSideWidth, maxSideWidth);
    layout.rightWidth = ClampFloat(layout.rightWidth, kMinSideWidth, maxSideWidth);
    layout.bottomHeight = ClampFloat(layout.bottomHeight, kMinBottomHeight, maxBottomHeight);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(workPos);
    ImGui::SetNextWindowSize(workSize);
    ImGui::Begin("Shader Lab Splitters", nullptr, flags);

    RenderSplitter(
        "Left Splitter",
        ImVec2(workPos.x + layout.leftWidth, contentY),
        ImVec2(kSplitterSize, contentHeight - layout.bottomHeight),
        &layout.leftWidth,
        1.0f,
        kMinSideWidth,
        maxSideWidth,
        true);

    RenderSplitter(
        "Right Splitter",
        ImVec2(workPos.x + workSize.x - layout.rightWidth - kSplitterSize, contentY),
        ImVec2(kSplitterSize, contentHeight - layout.bottomHeight),
        &layout.rightWidth,
        -1.0f,
        kMinSideWidth,
        maxSideWidth,
        true);

    RenderSplitter(
        "Bottom Splitter",
        ImVec2(workPos.x, workPos.y + workSize.y - layout.bottomHeight - kSplitterSize),
        ImVec2(workSize.x, kSplitterSize),
        &layout.bottomHeight,
        -1.0f,
        kMinBottomHeight,
        maxBottomHeight,
        false);

    ImGui::End();
}
} // namespace

void EditorPanel::ClampLayout(float width, float height)
{
    const float contentHeight = std::max(1.0f, height - kToolbarHeight);
    const float maxSideWidth = std::max(kMinSideWidth, (width - kMinPreviewWidth) * 0.5f);
    const float maxBottomHeight = std::max(kMinBottomHeight, contentHeight - kMinPreviewHeight);
    m_layout.leftWidth = ClampFloat(m_layout.leftWidth, kMinSideWidth, maxSideWidth);
    m_layout.rightWidth = ClampFloat(m_layout.rightWidth, kMinSideWidth, maxSideWidth);
    m_layout.bottomHeight = ClampFloat(m_layout.bottomHeight, kMinBottomHeight, maxBottomHeight);
}

ShaderLabViewport EditorPanel::PreviewViewport(float width, float height) const
{
    ShaderLabViewport rect;
    rect.x = m_layout.leftWidth + kSplitterSize;
    rect.y = kToolbarHeight;
    rect.width = std::max(1.0f, width - m_layout.leftWidth - m_layout.rightWidth - kSplitterSize * 2.0f);
    rect.height = std::max(1.0f, height - kToolbarHeight - m_layout.bottomHeight - kSplitterSize);
    return rect;
}

void EditorPanel::Render(EditorPanelContext& context)
{
    ClampLayout(static_cast<float>(context.width), static_cast<float>(context.height));

    const int objectCount = static_cast<int>(context.scene.Objects().size());
    if (objectCount == 0)
    {
        context.selectedObject = -1;
    }
    else if (context.selectedObject < 0 || context.selectedObject >= objectCount)
    {
        context.selectedObject = 0;
    }

    RenderToolbar(context);
    RenderObjects(context);
    RenderInspector(context);
    RenderDebugViews(context);
    RenderLayoutSplitters(ImGui::GetMainViewport(), m_layout);
}

void EditorPanel::RenderToolbar(EditorPanelContext& context)
{
    m_shaderStatusToastSeconds = std::max(0.0f, m_shaderStatusToastSeconds - context.frameStats.deltaSeconds);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, kToolbarHeight));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Shader Lab Toolbar", nullptr, flags);
    if (ImGui::Button("Recompile Shaders"))
    {
        context.reloadShaders();
        m_shaderStatusToastSeconds = IsShaderStatusError(context.shaderStatus) ? 0.0f : 2.0f;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", "Click after editing shader files; no renderer restart needed.");
    }
    const bool shaderHasError = IsShaderStatusError(context.shaderStatus);
    if (shaderHasError || m_shaderStatusToastSeconds > 0.0f)
    {
        ImGui::SameLine();
        const std::string shaderStatus = ToolbarShaderStatus(context.shaderStatus);
        const ImVec4 shaderStatusColor = shaderHasError ?
            ImVec4(0.95f, 0.32f, 0.25f, 1.0f) :
            ImVec4(0.55f, 0.86f, 0.45f, 1.0f);
        ImGui::TextColored(shaderStatusColor, "%s", shaderStatus.c_str());
        if (shaderHasError && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", context.shaderStatus.c_str());
        }
    }
    ToolbarLabel("Demo");
    ImGui::SetNextItemWidth(150.0f);
    int demo = context.demo;
    const char* demos[] = {"Basic Lighting", "Toon Ramp + Rim", "Dissolve + Noise", "Depth Fog + Outline"};
    if (ImGui::Combo("##ScenePreset", &demo, demos, IM_ARRAYSIZE(demos)))
    {
        context.demo = demo;
        context.rebuildScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Preset"))
    {
        context.rebuildScene();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "%s",
            "Basic Lighting: ambient, diffuse, and specular material lighting.\n"
            "Toon Ramp + Rim: stepped lighting with a bright character outline.\n"
            "Dissolve + Noise: breaks the surface apart over time.\n"
            "Depth Fog + Outline: fades distant pixels and separates silhouettes.");
    }

    ImGui::End();
}

void EditorPanel::RenderObjects(EditorPanelContext& context)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + kToolbarHeight));
    ImGui::SetNextWindowSize(ImVec2(m_layout.leftWidth, viewport->WorkSize.y - kToolbarHeight - m_layout.bottomHeight - kSplitterSize));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Objects", nullptr, flags);
    const auto& objects = context.scene.Objects();
    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        const bool selected = context.selectedObject == i;
        if (ImGui::Selectable(ObjectLabel(objects[i], i), selected))
        {
            context.selectedObject = i;
        }
    }

    ImGui::End();
}

void EditorPanel::RenderInspector(EditorPanelContext& context)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(
        viewport->WorkPos.x + viewport->WorkSize.x - m_layout.rightWidth,
        viewport->WorkPos.y + kToolbarHeight));
    ImGui::SetNextWindowSize(ImVec2(m_layout.rightWidth, viewport->WorkSize.y - kToolbarHeight - m_layout.bottomHeight - kSplitterSize));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Inspector", nullptr, flags);
    auto& objects = context.scene.Objects();
    if (context.selectedObject < 0 || context.selectedObject >= static_cast<int>(objects.size()))
    {
        ImGui::TextUnformatted("No object selected");
        ImGui::End();
        return;
    }

    SceneObject& object = objects[context.selectedObject];
    if (ImGui::BeginTabBar("InspectorTabs"))
    {
        if (ImGui::BeginTabItem("Object"))
        {
            char nameBuffer[128]{};
            std::copy_n(object.name.c_str(), std::min<size_t>(object.name.size(), sizeof(nameBuffer) - 1), nameBuffer);
            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                object.name = nameBuffer;
            }

            ImGui::SeparatorText("Transform");
            ImGui::DragFloat3("Position", &object.transform.position.x, 0.05f, -100.0f, 100.0f, "%.2f");
            DragRotationDegrees("Rotation", object.transform.rotation);
            ImGui::DragFloat3("Scale", &object.transform.scale.x, 0.02f, 0.01f, 100.0f, "%.2f");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Material"))
        {
            ImGui::ColorEdit4("Base Color", &object.material.baseColor.x);
            if (ImGui::Button("Reset Base Material"))
            {
                object.material.baseColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                object.material.useAlbedoTexture = true;
                object.material.preserveImportedUnlit = false;
                object.material.uvTiling = glm::vec2(1.0f, 1.0f);
                object.material.uvOffset = glm::vec2(0.0f, 0.0f);
            }

            ImGui::SeparatorText("Texture Slots");
            ImGui::Checkbox("Albedo Texture", &object.material.useAlbedoTexture);
            ImGui::Checkbox("Preserve Imported Unlit", &object.material.preserveImportedUnlit);

            ImGui::SeparatorText("Texture Coordinate");
            ImGui::DragFloat2("Tiling", &object.material.uvTiling.x, 0.01f, 0.01f, 32.0f, "%.2f");
            ImGui::DragFloat2("Offset", &object.material.uvOffset.x, 0.01f, -10.0f, 10.0f, "%.2f");
            if (ImGui::Button("Reset Texture Coordinate"))
            {
                object.material.uvTiling = glm::vec2(1.0f, 1.0f);
                object.material.uvOffset = glm::vec2(0.0f, 0.0f);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Lighting"))
        {
            ImGui::SeparatorText("Lighting Model");
            const char* lightingModes[] = {"Lit", "Toon Ramp + Rim"};
            ImGui::Combo("Model", &object.material.lightingModel, lightingModes, IM_ARRAYSIZE(lightingModes));
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "%s",
                    "Choose how lighting is calculated. Lit uses continuous shading; "
                    "Toon Ramp + Rim uses stepped shading and a rim light.");
            }

            if (object.material.lightingModel == 1)
            {
                ImGui::SeparatorText("Toon / Rim");
                ImGui::SliderFloat("Toon Steps", &object.material.toonSteps, 1.0f, 8.0f, "%.0f");
                ImGui::ColorEdit4("Shadow Color", &object.material.shadowColor.x);
                ImGui::ColorEdit4("Rim Color", &object.material.rimColor.x);
                ImGui::SliderFloat("Rim Power", &object.material.rimPower, 0.25f, 8.0f, "%.2f");
                ImGui::SliderFloat("Rim Intensity", &object.material.rimIntensity, 0.0f, 2.0f, "%.2f");
            }

            ImGui::SeparatorText("Material Lighting");
            ImGui::ColorEdit4("Ambient Color", &object.material.ambientColor.x);
            ImGui::SliderFloat("Diffuse Intensity", &object.material.diffuseIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Specular Intensity", &object.material.specularIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Specular Power", &object.material.specularPower, 1.0f, 256.0f, "%.0f");
            if (ImGui::Button("Reset Material Lighting"))
            {
                object.material.ambientColor = glm::vec4(0.12f, 0.14f, 0.18f, 1.0f);
                object.material.diffuseIntensity = 1.0f;
                object.material.specularIntensity = 0.35f;
                object.material.specularPower = 48.0f;
                object.material.toonSteps = 4.0f;
                object.material.shadowColor = glm::vec4(0.18f, 0.20f, 0.32f, 1.0f);
                object.material.rimColor = glm::vec4(0.55f, 0.85f, 1.0f, 1.0f);
                object.material.rimPower = 3.0f;
                object.material.rimIntensity = 0.45f;
            }

            ImGui::SeparatorText("Directional Lights");
            auto& lights = context.scene.DirectionalLights();
            ImGui::Text("%s: %d", "Count", static_cast<int>(lights.size()));
            ImGui::TextWrapped(
                "%s",
                "Directional lights behave like the sun: they have no position, only a shared light direction. "
                "The first light is usually the key light and the second is the fill light.");
            if (lights.size() < 4 && ImGui::Button("Add Directional Light"))
            {
                lights.push_back({glm::vec3(-0.35f, -0.85f, 0.35f), 0.5f, glm::vec3(1.0f, 1.0f, 1.0f), 0.0f});
            }
            if (lights.size() > 1)
            {
                ImGui::SameLine();
                if (ImGui::Button("Remove Last"))
                {
                    lights.pop_back();
                }
            }

            for (int i = 0; i < static_cast<int>(lights.size()); ++i)
            {
                DirectionalLight& light = lights[static_cast<size_t>(i)];
                ImGui::PushID(i);
                const char* role = i == 0 ? " (Key Light)" : (i == 1 ? " (Fill Light)" : "");
                const std::string header = std::string("Directional Light") + " " + std::to_string(i + 1) + role;
                if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat3("Direction", &light.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip(
                            "%s",
                            "This is the light travel direction, not a position. "
                            "Click Normalize Direction to keep the vector length at 1.");
                    }
                    if (ImGui::Button("Normalize Direction"))
                    {
                        NormalizeDirection(light.direction);
                    }
                    ImGui::ColorEdit3("Color", &light.color.x);
                    ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 3.0f, "%.2f");
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        const ImGuiTabItemFlags selectEffectsTab =
            (object.material.surfaceEffect > 0 && ImGui::IsWindowAppearing()) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("Effects", nullptr, selectEffectsTab))
        {
            ImGui::SeparatorText("Surface / Material Effects");
            const char* surfaceEffects[] = {"None", "Dissolve Effects", "Depth Fog + Outline"};
            if (ImGui::Combo("Effect", &object.material.surfaceEffect, surfaceEffects, IM_ARRAYSIZE(surfaceEffects)))
            {
                if (object.material.surfaceEffect == 1)
                {
                    ApplyDissolvePreset(object.material);
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "%s",
                    "These effects are layered on top of lighting. Dissolve does not change the lighting model; "
                    "it clips pixels and adds edge emission.");
            }

            if (object.material.surfaceEffect == 1)
            {
                const char* dissolveModes[] = {
                    "Basic Noise (Default)",
                    "Height / Directional",
                    "Local Sphere",
                    "Incineration / Ash",
                    "FlowMap Energy",
                    "Pixelated",
                    "Edge Particles / Ash",
                    "Smoke",
                };
                ImGui::SeparatorText("Dissolve Effect");
                if (ImGui::Combo(
                        "Type",
                        &object.material.dissolveMode,
                        dissolveModes,
                        IM_ARRAYSIZE(dissolveModes)))
                {
                    ApplyDissolvePreset(object.material, object.material.dissolveMode);
                }
                ImGui::SliderFloat("Dissolve Amount", &object.material.dissolveAmount, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Noise Scale", &object.material.dissolveNoiseScale, 0.1f, 12.0f, "%.2f");
                ImGui::SliderFloat("Edge Width", &object.material.dissolveEdgeWidth, 0.001f, 0.5f, "%.3f");
                ImGui::ColorEdit4("Edge Color", &object.material.dissolveEdgeColor.x);
                ImGui::ColorEdit4("Secondary Edge Color", &object.material.dissolveSecondaryColor.x);
                ImGui::SliderFloat("Edge Intensity", &object.material.dissolveEdgeIntensity, 0.0f, 8.0f, "%.2f");
                ImGui::SliderFloat("Noise Speed", &object.material.dissolveSpeed, -1.0f, 1.0f, "%.2f");

                if (object.material.dissolveMode == 1 ||
                    object.material.dissolveMode == 3 ||
                    object.material.dissolveMode == 5 ||
                    object.material.dissolveMode == 6 ||
                    object.material.dissolveMode == 7)
                {
                    ImGui::DragFloat3("Dissolve Direction", &object.material.dissolveDirection.x, 0.01f, -1.0f, 1.0f, "%.2f");
                    if (ImGui::Button("Normalize Dissolve Direction"))
                    {
                        NormalizeDirection(object.material.dissolveDirection);
                    }
                    ImGui::SliderFloat("Boundary Noise", &object.material.dissolveNoiseInfluence, 0.0f, 0.5f, "%.2f");
                }
                else if (object.material.dissolveMode == 2)
                {
                    ImGui::SliderFloat3("Sphere Center (Local)", &object.material.dissolveCenter.x, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Sphere Noise", &object.material.dissolveNoiseInfluence, 0.0f, 0.5f, "%.2f");
                }
                else if (object.material.dissolveMode == 4)
                {
                    ImGui::SliderFloat("Flow Distortion", &object.material.dissolveFlowStrength, 0.0f, 1.5f, "%.2f");
                }
                if (object.material.dissolveMode == 5)
                {
                    ImGui::SliderFloat("World Pixel Block Size", &object.material.dissolvePixelSize, 0.012f, 0.16f, "%.3f");
                }

                if (object.material.dissolveMode == 3 ||
                    object.material.dissolveMode == 6 ||
                    object.material.dissolveMode == 7)
                {
                    ImGui::SeparatorText("Dynamic Particle Layer");
                    ImGui::SliderFloat("Particle Rate", &object.material.dissolveParticleRate, 0.0f, 220.0f, "%.0f");
                    ImGui::SliderFloat("Particle Size", &object.material.dissolveParticleSize, 0.01f, 0.35f, "%.3f");
                    ImGui::SliderFloat("Particle Lifetime", &object.material.dissolveParticleLifetime, 0.2f, 5.0f, "%.2f");
                    ImGui::DragFloat3("Particle Wind", &object.material.dissolveParticleWind.x, 0.01f, -1.5f, 1.5f, "%.2f");
                }

                if (ImGui::Checkbox("Auto Progress", &object.material.dissolveAutoProgress) &&
                    !object.material.dissolveAutoProgress)
                {
                    object.material.dissolvePaused = false;
                }
                if (object.material.dissolveAutoProgress)
                {
                    ImGui::SameLine();
                    if (ImGui::Button(object.material.dissolvePaused ? "Resume" : "Pause"))
                    {
                        object.material.dissolvePaused = !object.material.dissolvePaused;
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip(
                            "%s",
                            "Freeze the current dissolve frame. Resume continues from the same position.");
                    }
                    const float cycle = std::fmod(
                        object.material.dissolvePlaybackTime * std::max(object.material.dissolveProgressSpeed, 0.01f),
                        1.0f);
                    const float currentProgress = cycle < 0.5f ? cycle * 2.0f : 2.0f - cycle * 2.0f;
                    ImGui::Text(
                        "%s: %.2f%s",
                        "Current Auto Progress",
                        currentProgress,
                        object.material.dissolvePaused ? " (Paused)" : "");
                }
                ImGui::SliderFloat("Progress Speed", &object.material.dissolveProgressSpeed, 0.02f, 1.0f, "%.2f");
                if (ImGui::Button("Reset Current Dissolve"))
                {
                    ApplyDissolvePreset(object.material, object.material.dissolveMode);
                }
                ImGui::TextWrapped(
                    "%s",
                    "Incineration uses one noisy burn field for model clipping, flames, and ash. "
                    "Surface samples emit only when the moving dissolve threshold crosses them, so fire, sparks, fine ash, "
                    "and sparse charcoal crumbs are born directly on the visible burn edge. They briefly remain attached, "
                    "then detach with independent local-space motion.");
            }
            else if (object.material.surfaceEffect == 2)
            {
                ImGui::SeparatorText("Depth Fog");
                ImGui::Checkbox("Enable Depth Fog", &context.depthFogEnabled);
                ImGui::SliderFloat("Fog Start Distance", &context.depthFogStart, 0.1f, 20.0f, "%.2f");
                ImGui::SliderFloat("Fog End Distance", &context.depthFogEnd, 0.2f, 40.0f, "%.2f");
                ImGui::SliderFloat("Fog Density", &context.depthFogDensity, 0.0f, 2.0f, "%.2f");
                ImGui::ColorEdit4("Fog Color", &context.depthFogColor.x);
                ImGui::SeparatorText("Depth Outline");
                ImGui::Checkbox("Enable Depth Outline", &context.depthOutlineEnabled);
                ImGui::SliderFloat("Outline Width", &context.depthOutlineWidth, 0.25f, 4.0f, "%.2f");
                ImGui::SliderFloat("Outline Strength", &context.depthOutlineStrength, 0.0f, 1.0f, "%.2f");
                ImGui::ColorEdit4("Outline Color", &context.depthOutlineColor.x);
                if (ImGui::Button("Reset Depth Effect"))
                {
                    context.depthFogEnabled = true;
                    context.depthFogStart = 3.5f;
                    context.depthFogEnd = 14.0f;
                    context.depthFogDensity = 0.38f;
                    context.depthFogColor = glm::vec4(0.62f, 0.78f, 0.84f, 1.0f);
                    context.depthOutlineEnabled = true;
                    context.depthOutlineWidth = 1.25f;
                    context.depthOutlineStrength = 0.78f;
                    context.depthOutlineColor = glm::vec4(0.08f, 0.18f, 0.24f, 1.0f);
                }
                ImGui::TextWrapped(
                    "%s",
                    "Depth fog gently blends distant pixels toward the background. "
                    "Depth outline compares nearby pixel distances and draws a silhouette edge. "
                    "Both are image effects and do not change the model.");
            }
            else
            {
                ImGui::TextWrapped(
                    "%s",
                    "No extra surface effect. Adjust the lighting model in the Lighting tab.");
            }

            ImGui::SeparatorText("Soft Glow (Bloom)");
            ImGui::Checkbox("Enable Soft Glow (Bloom)", &context.bloomEnabled);
            ImGui::SliderFloat("Glow Start", &context.bloomThreshold, 0.1f, 1.0f, "%.2f");
            ImGui::SliderFloat("Glow Strength", &context.bloomStrength, 0.0f, 2.0f, "%.2f");
            ImGui::TextWrapped(
                "%s",
                "Bloom spreads the brighter parts of the image so the dissolve edge feels more natural. "
                "It does not change the model's dissolve shape.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("View Debug"))
        {
            ImGui::SeparatorText("Main Viewport");
            ImGui::Checkbox("Wireframe Overlay", &context.wireframe);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "%s",
                    "F1 also toggles this. Use it to inspect topology, import artifacts, and mesh density.");
            }
            ImGui::Text("%s: %s", "Display Mode", context.wireframe ? "Wireframe" : "Solid");
            ImGui::Checkbox("Enable Current Effect", &context.effectsEnabled);
            ImGui::Checkbox("Auto Turntable", &context.autoTurntable);
            if (ImGui::Button("Reset Camera"))
            {
                context.resetCamera();
            }
            ImGui::SameLine();
            if (ImGui::Button("Screenshot"))
            {
                context.captureScreenshot();
            }
            ImGui::TextWrapped("%s: %s", "Screenshot", context.captureStatus.c_str());
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}

void EditorPanel::RenderDiagnosticsContent(EditorPanelContext& context)
{
    auto& objects = context.scene.Objects();
    if (context.selectedObject < 0 || context.selectedObject >= static_cast<int>(objects.size()))
    {
        ImGui::TextUnformatted("No object selected");
        return;
    }

    const SceneObject& object = objects[context.selectedObject];
    ImGui::Columns(3, "DiagnosticsColumns", false);
    ImGui::SeparatorText("Status");
    ImGui::TextWrapped("%s: %s", "Shader", context.shaderStatus.c_str());
    ImGui::TextWrapped("%s: %s", "Model", context.modelStatus.c_str());
    ImGui::TextWrapped("%s: %s", "Screenshot", context.captureStatus.c_str());
    ImGui::Text("%s: %.1f", "FPS", context.fps);
    ImGui::Text("%s: %u x %u", "Resolution", context.width, context.height);

    ImGui::NextColumn();
    ImGui::SeparatorText("Mesh");
    if (object.mesh)
    {
        ImGui::Text("%s: %d", "Vertices", static_cast<int>(object.mesh->vertices.size()));
        ImGui::Text("%s: %d", "Triangles", static_cast<int>(object.mesh->indices.size() / 3));
        ImGui::Text("UV: %s", object.mesh->hasTexcoords ? "Yes" : "No");
        ImGui::Text("%s: %s", "Normals", object.mesh->hasNormals ? "Yes" : "No");
        ImGui::Text("%s: %d", "Submeshes", static_cast<int>(object.mesh->submeshes.size()));
        ImGui::Text("%s: %d", "Materials", static_cast<int>(object.mesh->importedMaterials.size()));
        ImGui::Text("%s: %d", "Embedded Textures", static_cast<int>(object.mesh->embeddedImages.size()));
        if (!object.mesh->importNotes.empty())
        {
            ImGui::TextWrapped("%s: %s", "Notes", object.mesh->importNotes.c_str());
        }
    }
    else
    {
        ImGui::TextUnformatted("No mesh");
    }

    ImGui::NextColumn();
    ImGui::SeparatorText("Camera");
    const glm::vec3& cameraPosition = context.camera.Position();
    ImGui::Text("%s: %.2f %.2f %.2f", "Position", cameraPosition.x, cameraPosition.y, cameraPosition.z);
    ImGui::Text("%s: %.2f %.2f", "Yaw/Pitch", context.camera.Yaw(), context.camera.Pitch());
    ImGui::Text("%s: %d", "Directional Lights", static_cast<int>(context.scene.DirectionalLights().size()));
    ImGui::Text("%s: %d", "Objects", static_cast<int>(objects.size()));
    ImGui::Columns(1);

    if (!object.mesh)
    {
        return;
    }

    ImGui::SeparatorText("Submeshes");
    if (ImGui::BeginTable("BottomSubmeshDiagnostics", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("Indices");
        ImGui::TableSetupColumn("Triangles");
        ImGui::TableSetupColumn("Material");
        ImGui::TableHeadersRow();
        for (int i = 0; i < static_cast<int>(object.mesh->submeshes.size()); ++i)
        {
            const SubMesh& submesh = object.mesh->submeshes[static_cast<size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", submesh.indexCount);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", submesh.indexCount / 3);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", submesh.materialIndex);
        }
        ImGui::EndTable();
    }
}

void EditorPanel::RenderDebugViews(EditorPanelContext& context)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - m_layout.bottomHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, m_layout.bottomHeight));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Shader Lab Views", nullptr, flags);
    if (ImGui::BeginTabBar("ShaderLabTabs"))
    {
        if (ImGui::BeginTabItem("Debug Views"))
        {
            ImGui::BeginChild("DebugViewClip", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const float previewWidth = std::max(80.0f, (available.x - 24.0f) / 3.0f);
            const float previewHeight = std::max(48.0f, available.y - ImGui::GetTextLineHeightWithSpacing());
            const ImVec2 previewSize(previewWidth, previewHeight);
            PreviewTexture("Scene Color", context.sceneColor, previewSize);
            ImGui::SameLine();
            PreviewTexture("Depth", context.depthPreview, previewSize);
            ImGui::SameLine();
            PreviewTexture("Checker / Albedo", context.checkerTexture, previewSize);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Diagnostics"))
        {
            RenderDiagnosticsContent(context);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Textures"))
        {
            ImGui::BulletText("%s", "Generated checker texture");
            auto& objects = context.scene.Objects();
            if (context.selectedObject >= 0 && context.selectedObject < static_cast<int>(objects.size()))
            {
                const SceneObject& object = objects[context.selectedObject];
                if (object.mesh && !object.mesh->embeddedImages.empty())
                {
                    if (!object.mesh->importedMaterials.empty())
                    {
                        ImGui::SeparatorText("Imported Texture Slots");
                        ImGui::Text("%s: %d  %s: %d",
                            "Materials",
                            static_cast<int>(object.mesh->importedMaterials.size()),
                            "Embedded Textures",
                            static_cast<int>(object.mesh->embeddedImages.size()));
                        if (ImGui::BeginTable("BottomMaterialTextureSlots", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 120.0f)))
                        {
                            ImGui::TableSetupColumn("Material");
                            ImGui::TableSetupColumn("Texture");
                            ImGui::TableSetupColumn("Size");
                            ImGui::TableSetupColumn("Mode");
                            ImGui::TableSetupColumn("Preview");
                            ImGui::TableHeadersRow();
                            for (int i = 0; i < static_cast<int>(object.mesh->importedMaterials.size()); ++i)
                            {
                                const ImportedMaterial& material = object.mesh->importedMaterials[static_cast<size_t>(i)];
                                const EmbeddedImage* image = nullptr;
                                if (material.baseColorImage >= 0 && material.baseColorImage < static_cast<int>(object.mesh->embeddedImages.size()))
                                {
                                    image = &object.mesh->embeddedImages[static_cast<size_t>(material.baseColorImage)];
                                }

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextWrapped("%s", material.name.empty() ? "(unnamed)" : material.name.c_str());
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%d", material.baseColorImage);
                                ImGui::TableSetColumnIndex(2);
                                if (image)
                                {
                                    ImGui::Text("%u x %u", image->width, image->height);
                                }
                                else
                                {
                                    ImGui::TextUnformatted("-");
                                }
                                ImGui::TableSetColumnIndex(3);
                                const char* materialMode = material.unlit ? "Unlit" : "Lit";
                                const char* alphaMode = material.alphaBlend ? " + BLEND" : (material.alphaMask ? " + MASK" : "");
                                ImGui::Text("%s%s", materialMode, alphaMode);
                                ImGui::TableSetColumnIndex(4);
                                if (image && image->texture != 0)
                                {
                                    ImGui::Image(TextureRef(image->texture), ImVec2(36.0f, 36.0f), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                                }
                                else
                                {
                                    ImGui::TextUnformatted("-");
                                }
                            }
                            ImGui::EndTable();
                        }
                    }

                    ImGui::SeparatorText("Selected Object Textures");
                    ImGui::Text("%s: %d", "Embedded Textures", static_cast<int>(object.mesh->embeddedImages.size()));
                    const float tileSize = 72.0f;
                    const float spacing = ImGui::GetStyle().ItemSpacing.x;
                    const float availableWidth = std::max(tileSize, ImGui::GetContentRegionAvail().x);
                    const int columns = std::max(1, static_cast<int>(availableWidth / (tileSize + spacing)));
                    for (int i = 0; i < static_cast<int>(object.mesh->embeddedImages.size()); ++i)
                    {
                        const EmbeddedImage& image = object.mesh->embeddedImages[static_cast<size_t>(i)];
                        ImGui::BeginGroup();
                        if (image.texture != 0)
                        {
                            ImGui::Image(TextureRef(image.texture), ImVec2(tileSize, tileSize), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                        }
                        else
                        {
                            ImGui::Button("Missing", ImVec2(tileSize, tileSize));
                        }
                        ImGui::Text("%d", i);
                        ImGui::EndGroup();
                        if ((i + 1) % columns != 0)
                        {
                            ImGui::SameLine();
                        }
                    }
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
} // namespace YRender
