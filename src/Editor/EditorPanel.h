#pragma once

#include "Renderer/FrameStats.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

#include <functional>
#include <string>

namespace YRender
{
enum class UiLanguage
{
    Chinese,
    English,
};

struct ShaderLabLayout
{
    float leftWidth = 260.0f;
    float rightWidth = 380.0f;
    float bottomHeight = 260.0f;
};

struct ShaderLabViewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};

struct EditorPanelContext
{
    Scene& scene;
    const Camera& camera;
    const FrameStats& frameStats;
    int& selectedObject;
    bool& wireframe;
    int& demo;
    bool& effectsEnabled;
    bool& bloomEnabled;
    float& bloomThreshold;
    float& bloomStrength;
    bool& depthFogEnabled;
    float& depthFogStart;
    float& depthFogEnd;
    float& depthFogDensity;
    DirectX::XMFLOAT4& depthFogColor;
    bool& depthOutlineEnabled;
    float& depthOutlineWidth;
    float& depthOutlineStrength;
    DirectX::XMFLOAT4& depthOutlineColor;
    bool& autoTurntable;
    float fps = 0.0f;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int sceneColor = 0;
    unsigned int sceneDepth = 0;
    unsigned int depthPreview = 0;
    unsigned int checkerTexture = 0;
    const std::string& shaderStatus;
    const std::string& modelStatus;
    const std::string& captureStatus;
    std::function<void()> rebuildScene;
    std::function<void()> reloadShaders;
    std::function<void()> resetCamera;
    std::function<void()> captureScreenshot;
};

class EditorPanel
{
public:
    void Render(EditorPanelContext& context);
    ShaderLabViewport PreviewViewport(float width, float height) const;

private:
    void ClampLayout(float width, float height);
    const char* T(const char* chinese, const char* english) const;
    void RenderToolbar(EditorPanelContext& context);
    void RenderObjects(EditorPanelContext& context);
    void RenderInspector(EditorPanelContext& context);
    void RenderDebugViews(EditorPanelContext& context);
    void RenderDiagnosticsContent(EditorPanelContext& context);

    ShaderLabLayout m_layout;
    UiLanguage m_language = UiLanguage::Chinese;
    float m_shaderStatusToastSeconds = 0.0f;
};
} // namespace YRender
