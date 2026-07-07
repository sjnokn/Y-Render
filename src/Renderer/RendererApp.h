#pragma once

#include "Assets/ResourceManager.h"
#include "Core/Input.h"
#include "Core/ProjectConfig.h"
#include "Renderer/FrameStats.h"
#include "Renderer/RenderDevice.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

namespace YRender
{
class RendererApp
{
public:
    int Run(HINSTANCE instance, int showCommand);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnKeyPressed(UINT key);
    void CreateAppWindow(HINSTANCE instance, int showCommand);
    void InitializeRenderer();
    void InitializeImGui();
    void ShutdownImGui();
    void LoadAssets();
    void ReloadShaders();
    void CaptureScreenshot();
    void BuildScene();
    void MainLoop();
    void Update(float dt);
    void UpdateWindowTitle();
    DirectX::XMMATRIX CameraViewProjection() const;
    void Render();
    void RenderSceneObjects();
    void RenderPostProcess();
    void RenderDebugUi();

    HWND m_hwnd = nullptr;
    UINT m_width = 1280;
    UINT m_height = 720;
    bool m_running = true;
    bool m_wireframe = false;
    bool m_showDebugUi = true;
    int m_postMode = 0;
    int m_demo = 0;

    float m_time = 0.0f;
    float m_fpsAccumulator = 0.0f;
    int m_fpsFrames = 0;
    float m_currentFps = 0.0f;

    RenderDevice m_renderDevice;
    ResourceManager m_resources;
    Scene m_scene;
    Camera m_camera;
    InputState m_input;
    ProjectConfig m_config;
    FrameStats m_frameStats;

    ComPtr<ID3D11Buffer> m_cameraBuffer;
    ComPtr<ID3D11Buffer> m_materialBuffer;
    ComPtr<ID3D11Buffer> m_postBuffer;
    ComPtr<ID3D11Buffer> m_postVertexBuffer;

    ShaderProgram m_standardShader;
    ShaderProgram m_postShader;
    Texture m_checkerTexture;

    Mesh m_cubeMesh;
    Mesh m_planeMesh;
    Mesh m_objMesh;
};
} // namespace YRender
