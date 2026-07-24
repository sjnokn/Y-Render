#pragma once

#include "Assets/ResourceManager.h"
#include "Core/Input.h"
#include "Core/ProjectConfig.h"
#include "Editor/EditorPanel.h"
#include "Renderer/BurnFragmentSystem.h"
#include "Renderer/FrameStats.h"
#include "Renderer/DissolveParticleSystem.h"
#include "Renderer/RenderDevice.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

#include <windows.h>

#include <string>

namespace YRender
{
class RendererApp
{
public:
    int Run();

private:
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double x, double y);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowCloseCallback(GLFWwindow* window);
    static void WindowFocusCallback(GLFWwindow* window, int focused);

    void OnKeyPressed(int key);
    bool IsPointInPreview(float x, float y) const;
    ShaderLabViewport CurrentPreviewViewport() const;
    void CreateAppWindow();
    void RegisterCallbacks();
    void InitializeRenderer();
    void InitializeImGui();
    void ShutdownImGui();
    void LoadAssets();
    void ReloadShaders();
    void CaptureScreenshot();
    void CaptureScreenshotNow();
    void BuildScene();
    void ResetCameraForDemo();
    void MainLoop();
    void Update(float dt);
    void UpdateWindowTitle();
    DirectX::XMMATRIX CameraViewProjection(float aspect) const;
    void Render();
    void RenderSceneObjects();
    void RenderBloomPass();
    void RenderDepthPreviewPass();
    void RenderPresentationPass();
    void RenderDebugUi();
    void RenderShowcaseOverlay();
    void RenderPostQuad(float mode, unsigned int inputTexture, unsigned int bloomTexture, float bloomStrength);
    bool ShouldBloom() const;
    bool ShouldDepthEffects() const;
    void SetMat4(unsigned int program, const char* name, const DirectX::XMMATRIX& matrix) const;

    GLFWwindow* m_window = nullptr;
    UINT m_width = 1280;
    UINT m_height = 720;
    bool m_running = true;
    bool m_wireframe = false;
    bool m_showDebugUi = false;
    bool m_effectsEnabled = true;
    bool m_bloomEnabled = true;
    float m_bloomThreshold = 0.78f;
    float m_bloomStrength = 0.42f;
    bool m_depthFogEnabled = true;
    float m_depthFogStart = 3.5f;
    float m_depthFogEnd = 14.0f;
    float m_depthFogDensity = 0.38f;
    DirectX::XMFLOAT4 m_depthFogColor{0.62f, 0.78f, 0.84f, 1.0f};
    bool m_depthOutlineEnabled = true;
    float m_depthOutlineWidth = 1.25f;
    float m_depthOutlineStrength = 0.78f;
    DirectX::XMFLOAT4 m_depthOutlineColor{0.08f, 0.18f, 0.24f, 1.0f};
    bool m_autoTurntable = true;
    bool m_captureScreenshotPending = false;
    bool m_hideShowcaseOverlay = false;
    int m_demo = 0;
    int m_selectedObject = -1;
    std::string m_shaderStatus = "Shaders loaded";

    float m_time = 0.0f;
    float m_fpsAccumulator = 0.0f;
    int m_fpsFrames = 0;
    float m_currentFps = 0.0f;
    bool m_viewportInputCaptured = false;
    bool m_mouseButtons[3]{};
    bool m_hasLastMousePoint = false;
    double m_lastCursorX = 0.0;
    double m_lastCursorY = 0.0;
    double m_cursorX = 0.0;
    double m_cursorY = 0.0;
    float m_pendingLookDeltaX = 0.0f;
    float m_pendingLookDeltaY = 0.0f;
    float m_pendingWheelDelta = 0.0f;

    RenderDevice m_renderDevice;
    ResourceManager m_resources;
    Scene m_scene;
    Camera m_camera;
    InputState m_input;
    ProjectConfig m_config;
    FrameStats m_frameStats;
    EditorPanel m_editorPanel;

    ShaderProgram m_standardShader;
    ShaderProgram m_postShader;
    ShaderProgram m_dissolveParticleShader;
    ShaderProgram m_burnFragmentShader;
    BurnFragmentSystem m_burnFragments;
    DissolveParticleSystem m_dissolveParticles;
    Texture m_checkerTexture;
    Mesh m_postQuadMesh;

    Mesh m_characterMesh;
    Mesh m_planeMesh;
    bool m_externalCharacterLoaded = false;
    std::string m_modelStatus = "No model loaded";
    std::string m_captureStatus = "No screenshot captured";
};
} // namespace YRender
