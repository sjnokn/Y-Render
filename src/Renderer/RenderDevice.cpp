#include "Renderer/RenderDevice.h"

#include "Core/Platform.h"

#include <algorithm>
#include <iterator>

namespace YRender
{
void RenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_hwnd = hwnd;
    m_width = std::max<UINT>(1, width);
    m_height = std::max<UINT>(1, height);

    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapDesc.BufferCount = 1;
    swapDesc.BufferDesc.Width = m_width;
    swapDesc.BufferDesc.Height = m_height;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = m_hwnd;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.Windowed = TRUE;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL requestedLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        requestedLevels,
        static_cast<UINT>(std::size(requestedLevels)),
        D3D11_SDK_VERSION,
        &swapDesc,
        &m_swapChain,
        &m_device,
        &featureLevel,
        &m_context);

    if (hr == E_INVALIDARG)
    {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            &requestedLevels[1],
            1,
            D3D11_SDK_VERSION,
            &swapDesc,
            &m_swapChain,
            &m_device,
            &featureLevel,
            &m_context);
    }

    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain");
    CreateBackBufferAndDepth();
    CreateCommonStates();
}

void RenderDevice::Resize(UINT width, UINT height)
{
    if (!m_swapChain)
    {
        return;
    }

    m_width = std::max<UINT>(1, width);
    m_height = std::max<UINT>(1, height);

    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_backBufferRtv.Reset();
    m_depthDsv.Reset();
    m_depthTexture.Reset();
    m_sceneTarget = {};

    ThrowIfFailed(m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0), "Resize swap chain buffers");
    CreateBackBufferAndDepth();
}

void RenderDevice::BeginScene(const float clearColor[4])
{
    ID3D11ShaderResourceView* nullSrvs[] = {nullptr};
    m_context->PSSetShaderResources(0, 1, nullSrvs);
    m_context->OMSetRenderTargets(1, m_sceneTarget.rtv.GetAddressOf(), m_depthDsv.Get());
    m_context->ClearRenderTargetView(m_sceneTarget.rtv.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthDsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void RenderDevice::BeginBackBuffer(const float clearColor[4])
{
    m_context->OMSetRenderTargets(1, m_backBufferRtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_backBufferRtv.Get(), clearColor);
}

void RenderDevice::SetSceneViewport()
{
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);
}

void RenderDevice::SetWireframe(bool enabled)
{
    m_context->RSSetState(enabled ? m_wireRasterizer.Get() : m_solidRasterizer.Get());
}

void RenderDevice::Present()
{
    m_swapChain->Present(1, 0);
}

void RenderDevice::CreateBackBufferAndDepth()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)), "Get swap chain back buffer");
    ThrowIfFailed(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_backBufferRtv), "Create back buffer RTV");

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ThrowIfFailed(m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthTexture), "Create depth texture");
    ThrowIfFailed(m_device->CreateDepthStencilView(m_depthTexture.Get(), nullptr, &m_depthDsv), "Create depth DSV");
    CreateSceneTarget();
}

void RenderDevice::CreateSceneTarget()
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ThrowIfFailed(m_device->CreateTexture2D(&desc, nullptr, &m_sceneTarget.texture), "Create scene render target texture");
    ThrowIfFailed(m_device->CreateRenderTargetView(m_sceneTarget.texture.Get(), nullptr, &m_sceneTarget.rtv), "Create scene RTV");
    ThrowIfFailed(m_device->CreateShaderResourceView(m_sceneTarget.texture.Get(), nullptr, &m_sceneTarget.srv), "Create scene SRV");
}

void RenderDevice::CreateCommonStates()
{
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ThrowIfFailed(m_device->CreateSamplerState(&samplerDesc, &m_linearSampler), "Create linear sampler");

    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.DepthClipEnable = TRUE;
    ThrowIfFailed(m_device->CreateRasterizerState(&rasterDesc, &m_solidRasterizer), "Create solid rasterizer");

    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    ThrowIfFailed(m_device->CreateRasterizerState(&rasterDesc, &m_wireRasterizer), "Create wire rasterizer");
}
} // namespace YRender
