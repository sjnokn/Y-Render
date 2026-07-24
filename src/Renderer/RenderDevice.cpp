#include <glad/glad.h>

#include "Renderer/RenderDevice.h"

#include "Core/Platform.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
#include <stb_image_write.h>

namespace YRender
{
void RenderDevice::Initialize(GLFWwindow* window, int width, int height)
{
    m_window = window;
    m_width = std::max<UINT>(1, static_cast<UINT>(width));
    m_height = std::max<UINT>(1, static_cast<UINT>(height));

    // The GL context is created and made current by the caller (GLFW). GLAD must
    // load after MakeCurrent or every function pointer comes back null.
    const int loaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
    assert(loaded != 0 && "gladLoadGLLoader failed; is the context current?");
    if (loaded == 0)
    {
        throw std::runtime_error("GLAD failed to load OpenGL functions");
    }
    // Minimal sanity: a core 3.3 loader must resolve the framebuffer entrypoints
    // the renderer relies on immediately. Fail fast instead of crashing mid-frame.
    assert(glad_glGenFramebuffers != nullptr);
    if (glad_glGenFramebuffers == nullptr)
    {
        throw std::runtime_error("GLAD resolved no framebuffer entrypoints");
    }

    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    // glEnable(GL_TEXTURE_2D) intentionally removed: it is GL_INVALID_ENUM under
    // the OpenGL 3.3 core profile GLFW now requests.
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
    glfwSwapBuffers(m_window);
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

    // Encode the Y-flipped RGBA buffer to PNG. stbi_write_png expects the first row
    // to be the top of the image, which is exactly what `flipped` holds.
    const std::string utf8Path = WideToUtf8(path);
    const int stride = static_cast<int>(m_width) * 4;
    const int ok = stbi_write_png(
        utf8Path.c_str(),
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        4,
        flipped.data(),
        stride);
    return ok != 0;
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
