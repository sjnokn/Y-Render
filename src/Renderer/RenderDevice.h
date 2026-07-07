#pragma once

#include "Scene/SceneTypes.h"

#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>

namespace YRender
{
class RenderDevice
{
public:
    void Initialize(HWND hwnd, UINT width, UINT height);
    void Resize(UINT width, UINT height);
    void BeginScene(const float clearColor[4]);
    void BeginBackBuffer(const float clearColor[4]);
    void SetSceneViewport();
    void SetWireframe(bool enabled);
    void Present();

    ID3D11Device* Device() const { return m_device.Get(); }
    ID3D11DeviceContext* Context() const { return m_context.Get(); }
    IDXGISwapChain* SwapChain() const { return m_swapChain.Get(); }
    ID3D11SamplerState* LinearSampler() const { return m_linearSampler.Get(); }
    ID3D11ShaderResourceView* SceneColorSrv() const { return m_sceneTarget.srv.Get(); }

    UINT Width() const { return m_width; }
    UINT Height() const { return m_height; }

private:
    void CreateBackBufferAndDepth();
    void CreateSceneTarget();
    void CreateCommonStates();

    HWND m_hwnd = nullptr;
    UINT m_width = 1;
    UINT m_height = 1;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_backBufferRtv;
    ComPtr<ID3D11Texture2D> m_depthTexture;
    ComPtr<ID3D11DepthStencilView> m_depthDsv;
    ComPtr<ID3D11SamplerState> m_linearSampler;
    ComPtr<ID3D11RasterizerState> m_solidRasterizer;
    ComPtr<ID3D11RasterizerState> m_wireRasterizer;
    RenderTarget m_sceneTarget;
};
} // namespace YRender
