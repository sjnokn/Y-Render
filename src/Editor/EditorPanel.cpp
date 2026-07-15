#include "Editor/EditorPanel.h"

#include "imgui.h"

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

void DragRotationDegrees(const char* label, DirectX::XMFLOAT3& rotation)
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
        status.find("Unable") != std::string::npos ||
        status.find("失败") != std::string::npos ||
        status.find("错误") != std::string::npos;
}

std::string LocalizedShaderStatus(const std::string& status, UiLanguage language)
{
    if (status == "Shaders loaded")
    {
        return language == UiLanguage::Chinese ? "着色器已加载" : "Shaders loaded";
    }
    if (status == "Shaders reloaded successfully")
    {
        return language == UiLanguage::Chinese ? "着色器重新编译成功" : "Shaders recompiled successfully";
    }
    return status;
}

std::string ToolbarShaderStatus(const std::string& status, UiLanguage language)
{
    if (IsShaderStatusError(status))
    {
        return language == UiLanguage::Chinese ? "着色器编译失败，查看诊断" : "Shader compile failed, see Diagnostics";
    }
    return LocalizedShaderStatus(status, language);
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

void NormalizeDirection(DirectX::XMFLOAT3& direction)
{
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (length <= 0.0001f)
    {
        direction = DirectX::XMFLOAT3(-0.35f, -0.85f, 0.35f);
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

const char* EditorPanel::T(const char* chinese, const char* english) const
{
    return m_language == UiLanguage::Chinese ? chinese : english;
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

    ImGui::Begin(T("着色器实验工具栏", "Shader Lab Toolbar"), nullptr, flags);
    if (ImGui::Button(T("重新编译着色器", "Recompile Shaders")))
    {
        context.reloadShaders();
        m_shaderStatusToastSeconds = IsShaderStatusError(context.shaderStatus) ? 0.0f : 2.0f;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", T("修改 shader 文件后点击这里，无需重启渲染器。", "Click after editing shader files; no renderer restart needed."));
    }
    const bool shaderHasError = IsShaderStatusError(context.shaderStatus);
    if (shaderHasError || m_shaderStatusToastSeconds > 0.0f)
    {
        ImGui::SameLine();
        const std::string shaderStatus = ToolbarShaderStatus(context.shaderStatus, m_language);
        const ImVec4 shaderStatusColor = shaderHasError ?
            ImVec4(0.95f, 0.32f, 0.25f, 1.0f) :
            ImVec4(0.55f, 0.86f, 0.45f, 1.0f);
        ImGui::TextColored(shaderStatusColor, "%s", shaderStatus.c_str());
        if (shaderHasError && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", context.shaderStatus.c_str());
        }
    }
    ToolbarLabel(T("当前演示", "Demo"));
    ImGui::SetNextItemWidth(150.0f);
    int demo = context.demo;
    const char* demosZh[] = {"基础光照", "卡通分层 + 边缘光", "溶解 + 噪声"};
    const char* demosEn[] = {"Basic Lighting", "Toon Ramp + Rim", "Dissolve + Noise"};
    const char** demos = m_language == UiLanguage::Chinese ? demosZh : demosEn;
    if (ImGui::Combo("##ScenePreset", &demo, demos, IM_ARRAYSIZE(demosZh)))
    {
        context.demo = demo;
        context.rebuildScene();
    }
    ImGui::SameLine();
    if (ImGui::Button(T("重置预设", "Reset Preset")))
    {
        context.rebuildScene();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "%s",
            T(
                "基础光照：普通材质的环境光、漫反射和高光。\n卡通分层 + 边缘光：把明暗分层，并增加角色轮廓亮边。",
                "Basic Lighting: ambient, diffuse, and specular material lighting.\nToon Ramp + Rim: stepped lighting with a bright character outline."));
    }

    ToolbarLabel(T("语言", "Language"));
    ImGui::SetNextItemWidth(90.0f);
    int language = m_language == UiLanguage::Chinese ? 0 : 1;
    const char* languages[] = {"中文", "English"};
    if (ImGui::Combo("##Language", &language, languages, IM_ARRAYSIZE(languages)))
    {
        m_language = language == 0 ? UiLanguage::Chinese : UiLanguage::English;
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

    ImGui::Begin(T("对象", "Objects"), nullptr, flags);
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

    ImGui::Begin(T("检查器", "Inspector"), nullptr, flags);
    auto& objects = context.scene.Objects();
    if (context.selectedObject < 0 || context.selectedObject >= static_cast<int>(objects.size()))
    {
        ImGui::TextUnformatted(T("未选择对象", "No object selected"));
        ImGui::End();
        return;
    }

    SceneObject& object = objects[context.selectedObject];
    if (ImGui::BeginTabBar("InspectorTabs"))
    {
        if (ImGui::BeginTabItem(T("对象", "Object")))
        {
            char nameBuffer[128]{};
            std::copy_n(object.name.c_str(), std::min<size_t>(object.name.size(), sizeof(nameBuffer) - 1), nameBuffer);
            if (ImGui::InputText(T("名称", "Name"), nameBuffer, sizeof(nameBuffer)))
            {
                object.name = nameBuffer;
            }

            ImGui::SeparatorText(T("变换", "Transform"));
            ImGui::DragFloat3(T("位置", "Position"), &object.transform.position.x, 0.05f, -100.0f, 100.0f, "%.2f");
            DragRotationDegrees(T("旋转", "Rotation"), object.transform.rotation);
            ImGui::DragFloat3(T("缩放", "Scale"), &object.transform.scale.x, 0.02f, 0.01f, 100.0f, "%.2f");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(T("材质", "Material")))
        {
            ImGui::ColorEdit4(T("基础颜色", "Base Color"), &object.material.baseColor.x);
            if (ImGui::Button(T("重置基础材质", "Reset Base Material")))
            {
                object.material.baseColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                object.material.useAlbedoTexture = true;
                object.material.preserveImportedUnlit = false;
                object.material.uvTiling = DirectX::XMFLOAT2(1.0f, 1.0f);
                object.material.uvOffset = DirectX::XMFLOAT2(0.0f, 0.0f);
            }

            ImGui::SeparatorText(T("贴图槽", "Texture Slots"));
            ImGui::Checkbox(T("基础色贴图", "Albedo Texture"), &object.material.useAlbedoTexture);
            ImGui::Checkbox(T("保留导入的无光照材质", "Preserve Imported Unlit"), &object.material.preserveImportedUnlit);

            ImGui::SeparatorText(T("贴图坐标", "Texture Coordinate"));
            ImGui::DragFloat2(T("平铺", "Tiling"), &object.material.uvTiling.x, 0.01f, 0.01f, 32.0f, "%.2f");
            ImGui::DragFloat2(T("偏移", "Offset"), &object.material.uvOffset.x, 0.01f, -10.0f, 10.0f, "%.2f");
            if (ImGui::Button(T("重置贴图坐标", "Reset Texture Coordinate")))
            {
                object.material.uvTiling = DirectX::XMFLOAT2(1.0f, 1.0f);
                object.material.uvOffset = DirectX::XMFLOAT2(0.0f, 0.0f);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(T("光照", "Lighting")))
        {
            ImGui::SeparatorText(T("光照模型", "Lighting Model"));
            const char* lightingModesZh[] = {"基础光照", "卡通渐变 + 边缘光"};
            const char* lightingModesEn[] = {"Lit", "Toon Ramp + Rim"};
            const char** lightingModes = m_language == UiLanguage::Chinese ? lightingModesZh : lightingModesEn;
            ImGui::Combo(T("模型", "Model"), &object.material.lightingModel, lightingModes, IM_ARRAYSIZE(lightingModesZh));
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", T(
                    "这里选择光照计算方式。基础光照使用连续明暗；卡通渐变 + 边缘光使用分层明暗和轮廓亮边。",
                    "Choose how lighting is calculated. Lit uses continuous shading; Toon Ramp + Rim uses stepped shading and a rim light."));
            }

            if (object.material.lightingModel == 1)
            {
                ImGui::SeparatorText(T("卡通 / 边缘光", "Toon / Rim"));
                ImGui::SliderFloat(T("卡通阶数", "Toon Steps"), &object.material.toonSteps, 1.0f, 8.0f, "%.0f");
                ImGui::ColorEdit4(T("阴影颜色", "Shadow Color"), &object.material.shadowColor.x);
                ImGui::ColorEdit4(T("边缘光颜色", "Rim Color"), &object.material.rimColor.x);
                ImGui::SliderFloat(T("边缘光范围", "Rim Power"), &object.material.rimPower, 0.25f, 8.0f, "%.2f");
                ImGui::SliderFloat(T("边缘光强度", "Rim Intensity"), &object.material.rimIntensity, 0.0f, 2.0f, "%.2f");
            }

            ImGui::SeparatorText(T("材质受光", "Material Lighting"));
            ImGui::ColorEdit4(T("环境光", "Ambient Color"), &object.material.ambientColor.x);
            ImGui::SliderFloat(T("漫反射强度", "Diffuse Intensity"), &object.material.diffuseIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat(T("高光强度", "Specular Intensity"), &object.material.specularIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat(T("高光范围", "Specular Power"), &object.material.specularPower, 1.0f, 256.0f, "%.0f");
            if (ImGui::Button(T("重置材质光照", "Reset Material Lighting")))
            {
                object.material.ambientColor = DirectX::XMFLOAT4(0.12f, 0.14f, 0.18f, 1.0f);
                object.material.diffuseIntensity = 1.0f;
                object.material.specularIntensity = 0.35f;
                object.material.specularPower = 48.0f;
                object.material.toonSteps = 4.0f;
                object.material.shadowColor = DirectX::XMFLOAT4(0.18f, 0.20f, 0.32f, 1.0f);
                object.material.rimColor = DirectX::XMFLOAT4(0.55f, 0.85f, 1.0f, 1.0f);
                object.material.rimPower = 3.0f;
                object.material.rimIntensity = 0.45f;
            }

            ImGui::SeparatorText(T("方向光", "Directional Lights"));
            auto& lights = context.scene.DirectionalLights();
            ImGui::Text("%s: %d", T("数量", "Count"), static_cast<int>(lights.size()));
            ImGui::TextWrapped(
                "%s",
                T(
                    "方向光可以理解为太阳光：没有固定位置，场景中的物体都会从同一个方向受光。第一盏通常是主光，第二盏通常是补光。",
                    "Directional lights behave like the sun: they have no position, only a shared light direction. The first light is usually the key light and the second is the fill light."));
            if (lights.size() < 4 && ImGui::Button(T("添加方向光", "Add Directional Light")))
            {
                lights.push_back({DirectX::XMFLOAT3(-0.35f, -0.85f, 0.35f), 0.5f, DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f), 0.0f});
            }
            if (lights.size() > 1)
            {
                ImGui::SameLine();
                if (ImGui::Button(T("删除最后一个", "Remove Last")))
                {
                    lights.pop_back();
                }
            }

            for (int i = 0; i < static_cast<int>(lights.size()); ++i)
            {
                DirectionalLight& light = lights[static_cast<size_t>(i)];
                ImGui::PushID(i);
                const char* role = i == 0 ? T("（主光）", " (Key Light)") : (i == 1 ? T("（补光）", " (Fill Light)") : "");
                const std::string header = std::string(T("方向光", "Directional Light")) + " " + std::to_string(i + 1) + role;
                if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragFloat3(T("方向", "Direction"), &light.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip(
                            "%s",
                            T(
                                "这是光线传播方向，不是灯的位置。修改后点击“归一化方向”可保持方向向量长度为 1。",
                                "This is the light travel direction, not a position. Click Normalize Direction to keep the vector length at 1."));
                    }
                    if (ImGui::Button(T("归一化方向", "Normalize Direction")))
                    {
                        NormalizeDirection(light.direction);
                    }
                    ImGui::ColorEdit3(T("颜色", "Color"), &light.color.x);
                    ImGui::SliderFloat(T("强度", "Intensity"), &light.intensity, 0.0f, 3.0f, "%.2f");
                }
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        const ImGuiTabItemFlags selectEffectsTab =
            (object.material.surfaceEffect == 1 && ImGui::IsWindowAppearing()) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem(T("效果", "Effects"), nullptr, selectEffectsTab))
        {
            ImGui::SeparatorText(T("表面 / 材质效果", "Surface / Material Effects"));
            const char* surfaceEffectsZh[] = {"无额外效果", "溶解 + 噪声"};
            const char* surfaceEffectsEn[] = {"None", "Dissolve + Noise"};
            const char** surfaceEffects = m_language == UiLanguage::Chinese ? surfaceEffectsZh : surfaceEffectsEn;
            if (ImGui::Combo(T("效果", "Effect"), &object.material.surfaceEffect, surfaceEffects, IM_ARRAYSIZE(surfaceEffectsZh)))
            {
                if (object.material.surfaceEffect == 1)
                {
                    ApplyDissolvePreset(object.material);
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", T(
                    "这里选择叠加在光照之上的表面效果。溶解不会改变光照模型，只会裁切像素并给边缘增加发光。",
                    "These effects are layered on top of lighting. Dissolve does not change the lighting model; it clips pixels and adds edge emission."));
            }

            if (object.material.surfaceEffect == 1)
            {
                ImGui::SeparatorText(T("溶解 / 噪声", "Dissolve / Noise"));
                ImGui::SliderFloat(T("溶解进度", "Dissolve Amount"), &object.material.dissolveAmount, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat(T("噪声尺度", "Noise Scale"), &object.material.dissolveNoiseScale, 0.1f, 12.0f, "%.2f");
                ImGui::SliderFloat(T("边缘宽度", "Edge Width"), &object.material.dissolveEdgeWidth, 0.001f, 0.5f, "%.3f");
                ImGui::ColorEdit4(T("边缘颜色", "Edge Color"), &object.material.dissolveEdgeColor.x);
                ImGui::SliderFloat(T("边缘亮度", "Edge Intensity"), &object.material.dissolveEdgeIntensity, 0.0f, 8.0f, "%.2f");
                ImGui::SliderFloat(T("噪声动画速度", "Noise Speed"), &object.material.dissolveSpeed, -1.0f, 1.0f, "%.2f");
                ImGui::Checkbox(T("自动循环溶解", "Auto Progress"), &object.material.dissolveAutoProgress);
                ImGui::SliderFloat(T("溶解循环速度", "Progress Speed"), &object.material.dissolveProgressSpeed, 0.02f, 1.0f, "%.2f");
                if (ImGui::Button(T("重置溶解参数", "Reset Dissolve Parameters")))
                {
                    ApplyDissolvePreset(object.material);
                }
                ImGui::TextWrapped("%s", T(
                    "这是一个表面溶解预设：噪声决定消失位置，边缘由暗色烧蚀层、彩色过渡和细亮芯组成。柔和光晕会让亮边自然扩散。",
                    "This surface dissolve preset uses noise for the breakup and a three-layer edge: dark crust, colored transition, and a thin hot core. A soft glow gently spreads the bright edge."));
            }
            else
            {
                ImGui::TextWrapped("%s", T("当前没有额外表面效果。光照模型仍然在“光照”页调整。", "No extra surface effect. Adjust the lighting model in the Lighting tab."));
            }

            ImGui::SeparatorText(T("柔和光晕（Bloom）", "Soft Glow (Bloom)"));
            ImGui::Checkbox(T("启用柔和光晕（Bloom）", "Enable Soft Glow (Bloom)"), &context.bloomEnabled);
            ImGui::SliderFloat(T("光晕出现起点", "Glow Start"), &context.bloomThreshold, 0.1f, 1.0f, "%.2f");
            ImGui::SliderFloat(T("光晕强度", "Glow Strength"), &context.bloomStrength, 0.0f, 2.0f, "%.2f");
            ImGui::TextWrapped("%s", T(
                "Bloom 会把画面中较亮的部分扩散开来，让溶解边缘看起来更自然，不会改变模型的溶解形状。",
                "Bloom spreads the brighter parts of the image so the dissolve edge feels more natural. It does not change the model's dissolve shape."));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(T("视图调试", "View Debug")))
        {
            ImGui::SeparatorText(T("主视口", "Main Viewport"));
            ImGui::Checkbox(T("线框显示", "Wireframe Overlay"), &context.wireframe);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", T("也可以按 F1 切换。用于检查模型拓扑、导入破面和网格密度。", "F1 also toggles this. Use it to inspect topology, import artifacts, and mesh density."));
            }
            ImGui::Text("%s: %s", T("显示模式", "Display Mode"), context.wireframe ? T("线框", "Wireframe") : T("实体", "Solid"));
            ImGui::Checkbox(T("启用当前效果", "Enable Current Effect"), &context.effectsEnabled);
            ImGui::Checkbox(T("自动转台", "Auto Turntable"), &context.autoTurntable);
            if (ImGui::Button(T("重置相机", "Reset Camera")))
            {
                context.resetCamera();
            }
            ImGui::SameLine();
            if (ImGui::Button(T("截图", "Screenshot")))
            {
                context.captureScreenshot();
            }
            ImGui::TextWrapped("%s: %s", T("截图状态", "Screenshot"), context.captureStatus.c_str());
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
        ImGui::TextUnformatted(T("未选择对象", "No object selected"));
        return;
    }

    const SceneObject& object = objects[context.selectedObject];
    ImGui::Columns(3, "DiagnosticsColumns", false);
    ImGui::SeparatorText(T("状态", "Status"));
    ImGui::TextWrapped("%s: %s", T("着色器", "Shader"), context.shaderStatus.c_str());
    ImGui::TextWrapped("%s: %s", T("模型", "Model"), context.modelStatus.c_str());
    ImGui::TextWrapped("%s: %s", T("截图", "Screenshot"), context.captureStatus.c_str());
    ImGui::Text("%s: %.1f", "FPS", context.fps);
    ImGui::Text("%s: %u x %u", T("分辨率", "Resolution"), context.width, context.height);

    ImGui::NextColumn();
    ImGui::SeparatorText(T("网格", "Mesh"));
    if (object.mesh)
    {
        ImGui::Text("%s: %d", T("顶点", "Vertices"), static_cast<int>(object.mesh->vertices.size()));
        ImGui::Text("%s: %d", T("三角形", "Triangles"), static_cast<int>(object.mesh->indices.size() / 3));
        ImGui::Text("UV: %s", object.mesh->hasTexcoords ? T("是", "Yes") : T("否", "No"));
        ImGui::Text("%s: %s", T("法线", "Normals"), object.mesh->hasNormals ? T("是", "Yes") : T("否", "No"));
        ImGui::Text("%s: %d", T("子网格", "Submeshes"), static_cast<int>(object.mesh->submeshes.size()));
        ImGui::Text("%s: %d", T("材质", "Materials"), static_cast<int>(object.mesh->importedMaterials.size()));
        ImGui::Text("%s: %d", T("内嵌贴图", "Embedded Textures"), static_cast<int>(object.mesh->embeddedImages.size()));
        if (!object.mesh->importNotes.empty())
        {
            ImGui::TextWrapped("%s: %s", T("备注", "Notes"), object.mesh->importNotes.c_str());
        }
    }
    else
    {
        ImGui::TextUnformatted(T("没有网格", "No mesh"));
    }

    ImGui::NextColumn();
    ImGui::SeparatorText(T("相机", "Camera"));
    const DirectX::XMFLOAT3& cameraPosition = context.camera.Position();
    ImGui::Text("%s: %.2f %.2f %.2f", T("位置", "Position"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
    ImGui::Text("%s: %.2f %.2f", T("偏航/俯仰", "Yaw/Pitch"), context.camera.Yaw(), context.camera.Pitch());
    ImGui::Text("%s: %d", T("方向光", "Directional Lights"), static_cast<int>(context.scene.DirectionalLights().size()));
    ImGui::Text("%s: %d", T("对象", "Objects"), static_cast<int>(objects.size()));
    ImGui::Columns(1);

    if (!object.mesh)
    {
        return;
    }

    ImGui::SeparatorText(T("子网格", "Submeshes"));
    if (ImGui::BeginTable("BottomSubmeshDiagnostics", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn(T("索引数", "Indices"));
        ImGui::TableSetupColumn(T("三角形", "Triangles"));
        ImGui::TableSetupColumn(T("材质", "Material"));
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

    ImGui::Begin(T("着色器实验视图", "Shader Lab Views"), nullptr, flags);
    if (ImGui::BeginTabBar("ShaderLabTabs"))
    {
        if (ImGui::BeginTabItem(T("调试视图", "Debug Views")))
        {
            ImGui::BeginChild("DebugViewClip", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const float previewWidth = std::max(80.0f, (available.x - 24.0f) / 3.0f);
            const float previewHeight = std::max(48.0f, available.y - ImGui::GetTextLineHeightWithSpacing());
            const ImVec2 previewSize(previewWidth, previewHeight);
            PreviewTexture(T("场景颜色", "Scene Color"), context.sceneColor, previewSize);
            ImGui::SameLine();
            PreviewTexture(T("深度", "Depth"), context.sceneDepth, previewSize);
            ImGui::SameLine();
            PreviewTexture(T("棋盘格 / 基础色", "Checker / Albedo"), context.checkerTexture, previewSize);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T("诊断", "Diagnostics")))
        {
            RenderDiagnosticsContent(context);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T("贴图", "Textures")))
        {
            ImGui::BulletText("%s", T("程序生成的棋盘格贴图", "Generated checker texture"));
            auto& objects = context.scene.Objects();
            if (context.selectedObject >= 0 && context.selectedObject < static_cast<int>(objects.size()))
            {
                const SceneObject& object = objects[context.selectedObject];
                if (object.mesh && !object.mesh->embeddedImages.empty())
                {
                    if (!object.mesh->importedMaterials.empty())
                    {
                        ImGui::SeparatorText(T("导入贴图槽", "Imported Texture Slots"));
                        ImGui::Text("%s: %d  %s: %d",
                            T("材质", "Materials"),
                            static_cast<int>(object.mesh->importedMaterials.size()),
                            T("内嵌贴图", "Embedded Textures"),
                            static_cast<int>(object.mesh->embeddedImages.size()));
                        if (ImGui::BeginTable("BottomMaterialTextureSlots", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 120.0f)))
                        {
                            ImGui::TableSetupColumn(T("材质", "Material"));
                            ImGui::TableSetupColumn(T("贴图", "Texture"));
                            ImGui::TableSetupColumn(T("尺寸", "Size"));
                            ImGui::TableSetupColumn(T("模式", "Mode"));
                            ImGui::TableSetupColumn(T("预览", "Preview"));
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
                                const char* materialMode = material.unlit ? T("无光照", "Unlit") : T("受光照", "Lit");
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

                    ImGui::SeparatorText(T("当前对象贴图", "Selected Object Textures"));
                    ImGui::Text("%s: %d", T("内嵌贴图", "Embedded Textures"), static_cast<int>(object.mesh->embeddedImages.size()));
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
                            ImGui::Button(T("缺失", "Missing"), ImVec2(tileSize, tileSize));
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
