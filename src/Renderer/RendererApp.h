#pragma once

#include "Scene/SceneTypes.h"

#include <windows.h>

#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>
#include <vector>

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
    void InitializeD3D();
    void CreateBackBufferAndDepth();
    void ResizeTargets();
    void CreateSceneTarget();
    void CreateCommonStates();
    void LoadAssets();
    ShaderProgram CreateStandardShader();
    ShaderProgram CreatePostShader();
    void UploadMesh(Mesh& mesh);
    Texture CreateCheckerTexture(UINT width, UINT height);
    Texture CreateTextureFromRgba(UINT width, UINT height, const void* pixels, UINT pitch);
    Texture LoadTextureWic(const std::wstring& path);
    void CreatePostQuad();
    void BuildScene();
    void MainLoop();
    void Update(float dt);
    void UpdateWindowTitle();
    DirectX::XMMATRIX CameraViewProjection() const;
    void Render();
    void RenderSceneObjects();
    void RenderPostProcess();

    HWND m_hwnd = nullptr;
    UINT m_width = 1280;
    UINT m_height = 720;
    bool m_running = true;
    bool m_keys[256]{};
    bool m_wireframe = false;
    int m_postMode = 0;
    int m_demo = 0;

    DirectX::XMFLOAT3 m_cameraPosition{0.0f, 2.0f, -6.0f};
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = -0.15f;
    float m_time = 0.0f;
    float m_fpsAccumulator = 0.0f;
    int m_fpsFrames = 0;
    float m_currentFps = 0.0f;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_backBufferRtv;
    ComPtr<ID3D11Texture2D> m_depthTexture;
    ComPtr<ID3D11DepthStencilView> m_depthDsv;
    ComPtr<ID3D11SamplerState> m_linearSampler;
    ComPtr<ID3D11RasterizerState> m_solidRasterizer;
    ComPtr<ID3D11RasterizerState> m_wireRasterizer;
    ComPtr<ID3D11Buffer> m_cameraBuffer;
    ComPtr<ID3D11Buffer> m_materialBuffer;
    ComPtr<ID3D11Buffer> m_postBuffer;
    ComPtr<ID3D11Buffer> m_postVertexBuffer;

    ShaderProgram m_standardShader;
    ShaderProgram m_postShader;
    RenderTarget m_sceneTarget;
    Texture m_checkerTexture;

    Mesh m_cubeMesh;
    Mesh m_planeMesh;
    Mesh m_objMesh;
    std::vector<SceneObject> m_sceneObjects;
};
} // namespace YRender
