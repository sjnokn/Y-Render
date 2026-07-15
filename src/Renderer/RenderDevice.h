#pragma once

#include "Scene/SceneTypes.h"

#include <windows.h>
#include <string>

namespace YRender
{
class RenderDevice
{
public:
    void Initialize(HWND hwnd, UINT width, UINT height);
    void Resize(UINT width, UINT height);
    void BeginScene(const float clearColor[4]);
    void BeginBloomTarget();
    void BeginBlurTargetA();
    void BeginBlurTargetB();
    void BeginDepthPreviewTarget();
    void BeginBackBuffer(const float clearColor[4]);
    void SetSceneViewport();
    void SetViewport(float x, float y, float width, float height);
    void SetWireframe(bool enabled);
    void Present();
    void BeginEvent(const wchar_t* name);
    void EndEvent();
    bool CaptureBackBufferPng(const std::wstring& path);

    bool IsInitialized() const { return m_glrc != nullptr; }
    unsigned int SceneColorTexture() const { return m_sceneTarget.colorTexture; }
    unsigned int DepthTexture() const { return m_sceneTarget.depthTexture; }
    unsigned int BloomTexture() const { return m_bloomTarget.colorTexture; }
    unsigned int BlurTextureA() const { return m_blurTargetA.colorTexture; }
    unsigned int BlurTextureB() const { return m_blurTargetB.colorTexture; }
    unsigned int DepthPreviewTexture() const { return m_depthPreviewTarget.colorTexture; }

    UINT Width() const { return m_width; }
    UINT Height() const { return m_height; }

private:
    void CreateSceneTarget();
    void DestroySceneTarget();
    void CreateColorTarget(RenderTarget& target);
    void DestroyColorTarget(RenderTarget& target);
    void BeginColorTarget(const RenderTarget& target);

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_glrc = nullptr;
    UINT m_width = 1;
    UINT m_height = 1;
    RenderTarget m_sceneTarget;
    RenderTarget m_bloomTarget;
    RenderTarget m_blurTargetA;
    RenderTarget m_blurTargetB;
    RenderTarget m_depthPreviewTarget;
};
} // namespace YRender
