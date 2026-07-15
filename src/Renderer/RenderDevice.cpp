#include "Renderer/RenderDevice.h"

#include "Core/Platform.h"
#include "Renderer/OpenGL.h"

#include <algorithm>
#include <cstdint>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

namespace YRender
{
void RenderDevice::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_hwnd = hwnd;
    m_width = std::max<UINT>(1, width);
    m_height = std::max<UINT>(1, height);
    m_hdc = GetDC(m_hwnd);
    if (!m_hdc)
    {
        throw std::runtime_error("GetDC failed");
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(m_hdc, pixelFormat, &pfd))
    {
        throw std::runtime_error("SetPixelFormat failed");
    }

    m_glrc = wglCreateContext(m_hdc);
    if (!m_glrc || !wglMakeCurrent(m_hdc, m_glrc))
    {
        throw std::runtime_error("Create OpenGL context failed");
    }
    if (!LoadOpenGLFunctions())
    {
        throw std::runtime_error("OpenGL function loading failed");
    }
    if (wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT(1);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    CreateSceneTarget();
}

void RenderDevice::Resize(UINT width, UINT height)
{
    m_width = std::max<UINT>(1, width);
    m_height = std::max<UINT>(1, height);
    if (IsInitialized())
    {
        CreateSceneTarget();
    }
}

void RenderDevice::BeginScene(const float clearColor[4])
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneTarget.framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glEnable(GL_DEPTH_TEST);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RenderDevice::BeginBloomTarget()
{
    BeginColorTarget(m_bloomTarget);
}

void RenderDevice::BeginBlurTargetA()
{
    BeginColorTarget(m_blurTargetA);
}

void RenderDevice::BeginBlurTargetB()
{
    BeginColorTarget(m_blurTargetB);
}

void RenderDevice::BeginDepthPreviewTarget()
{
    BeginColorTarget(m_depthPreviewTarget);
}

void RenderDevice::BeginBackBuffer(const float clearColor[4])
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderDevice::SetSceneViewport()
{
    SetViewport(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
}

void RenderDevice::SetViewport(float x, float y, float width, float height)
{
    glViewport(
        static_cast<GLint>(std::max(0.0f, x)),
        static_cast<GLint>(std::max(0.0f, static_cast<float>(m_height) - y - height)),
        static_cast<GLsizei>(std::max(1.0f, width)),
        static_cast<GLsizei>(std::max(1.0f, height)));
}

void RenderDevice::SetWireframe(bool enabled)
{
    glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
}

void RenderDevice::Present()
{
    SwapBuffers(m_hdc);
}

void RenderDevice::BeginEvent(const wchar_t*)
{
}

void RenderDevice::EndEvent()
{
}

bool RenderDevice::CaptureBackBufferPng(const std::wstring& path)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(m_width) * m_height * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadPixels(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    std::vector<uint8_t> flipped(pixels.size());
    const size_t rowSize = static_cast<size_t>(m_width) * 4;
    for (UINT y = 0; y < m_height; ++y)
    {
        std::copy(
            pixels.data() + static_cast<size_t>(y) * rowSize,
            pixels.data() + static_cast<size_t>(y + 1) * rowSize,
            flipped.data() + static_cast<size_t>(m_height - 1 - y) * rowSize);
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
    {
        return false;
    }

    Microsoft::WRL::ComPtr<IWICStream> stream;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    HRESULT hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr))
    {
        hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    }
    if (SUCCEEDED(hr))
    {
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    }
    if (SUCCEEDED(hr))
    {
        hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr))
    {
        hr = encoder->CreateNewFrame(&frame, nullptr);
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->Initialize(nullptr);
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->SetSize(m_width, m_height);
    }
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    if (SUCCEEDED(hr))
    {
        hr = frame->SetPixelFormat(&format);
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->WritePixels(m_height, static_cast<UINT>(rowSize), static_cast<UINT>(flipped.size()), flipped.data());
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->Commit();
    }
    if (SUCCEEDED(hr))
    {
        hr = encoder->Commit();
    }
    return SUCCEEDED(hr);
}

void RenderDevice::CreateSceneTarget()
{
    DestroySceneTarget();

    glGenFramebuffers(1, &m_sceneTarget.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneTarget.framebuffer);

    glGenTextures(1, &m_sceneTarget.colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_sceneTarget.colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneTarget.colorTexture, 0);

    glGenTextures(1, &m_sceneTarget.depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_sceneTarget.depthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height), 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_sceneTarget.depthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("OpenGL scene framebuffer is incomplete");
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    CreateColorTarget(m_bloomTarget);
    CreateColorTarget(m_blurTargetA);
    CreateColorTarget(m_blurTargetB);
    CreateColorTarget(m_depthPreviewTarget);
}

void RenderDevice::DestroySceneTarget()
{
    if (m_sceneTarget.colorTexture != 0)
    {
        glDeleteTextures(1, &m_sceneTarget.colorTexture);
    }
    if (m_sceneTarget.depthTexture != 0)
    {
        glDeleteTextures(1, &m_sceneTarget.depthTexture);
    }
    if (m_sceneTarget.framebuffer != 0)
    {
        glDeleteFramebuffers(1, &m_sceneTarget.framebuffer);
    }
    m_sceneTarget = {};
    DestroyColorTarget(m_bloomTarget);
    DestroyColorTarget(m_blurTargetA);
    DestroyColorTarget(m_blurTargetB);
    DestroyColorTarget(m_depthPreviewTarget);
}

void RenderDevice::CreateColorTarget(RenderTarget& target)
{
    DestroyColorTarget(target);
    glGenFramebuffers(1, &target.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);

    glGenTextures(1, &target.colorTexture);
    glBindTexture(GL_TEXTURE_2D, target.colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.colorTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("OpenGL post-process framebuffer is incomplete");
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderDevice::DestroyColorTarget(RenderTarget& target)
{
    if (target.colorTexture != 0)
    {
        glDeleteTextures(1, &target.colorTexture);
    }
    if (target.framebuffer != 0)
    {
        glDeleteFramebuffers(1, &target.framebuffer);
    }
    target = {};
}

void RenderDevice::BeginColorTarget(const RenderTarget& target)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
} // namespace YRender
