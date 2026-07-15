#include "Renderer/RendererApp.h"

#include "Assets/MeshFactory.h"
#include "Core/Log.h"
#include "Core/Platform.h"
#include "Core/ProjectConfig.h"
#include "Renderer/DebugOverlay.h"
#include "Renderer/OpenGL.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <cstdio>
#include <vector>
#include <windowsx.h>

using namespace DirectX;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace YRender
{
namespace
{
constexpr wchar_t kWindowClassName[] = L"YRenderWindowClass";
constexpr int kDemoCount = 4;

const char* ShowcaseEffectName(int demo)
{
    if (demo == 0)
    {
        return "基础光照";
    }
    if (demo == 1)
    {
        return "卡通分层 + 边缘光";
    }
    if (demo == 2)
    {
        return "溶解 + 噪声";
    }
    return "深度雾 + 深度描边";
}

std::wstring ResolveAssetPath(const std::wstring& path)
{
    if (FileExists(path))
    {
        return path;
    }
    return FindAsset(path);
}

std::string ModelStatusLine(const Mesh& mesh, bool externalCharacterLoaded)
{
    std::ostringstream stream;
    stream << (externalCharacterLoaded ? "External character loaded: " : "Using sample placeholder: ");
    stream << (mesh.sourcePath.empty() ? mesh.displayName : mesh.sourcePath);
    stream << " | vertices " << mesh.vertices.size();
    stream << " | triangles " << mesh.indices.size() / 3;
    stream << " | submeshes " << mesh.submeshes.size();
    stream << " | materials " << mesh.importedMaterials.size();
    stream << " | images " << mesh.embeddedImages.size();
    const size_t uploadedImages = static_cast<size_t>(std::count_if(
        mesh.embeddedImages.begin(),
        mesh.embeddedImages.end(),
        [](const EmbeddedImage& image) { return image.texture != 0; }));
    if (!mesh.embeddedImages.empty())
    {
        stream << " (" << uploadedImages << " uploaded)";
    }
    stream << " | normals " << (mesh.hasNormals ? "yes" : "generated");
    stream << " | uv " << (mesh.hasTexcoords ? "yes" : "no");
    if (mesh.gltfNodeCount > 0)
    {
        stream << " | nodes " << mesh.gltfNodeCount;
    }
    if (mesh.gltfSkinCount > 0)
    {
        stream << " | skins " << mesh.gltfSkinCount << " (static preview)";
    }
    return stream.str();
}
}

int RendererApp::Run(HINSTANCE instance, int showCommand)
{
    try
    {
        ImGui_ImplWin32_EnableDpiAwareness();
        CreateAppWindow(instance, showCommand);
        InitializeRenderer();
        InitializeImGui();
        LoadAssets();
        BuildScene();
        MainLoop();
        ShutdownImGui();
        return 0;
    }
    catch (const std::exception& error)
    {
        ShutdownImGui();
        const std::wstring message = Utf8ToWide(error.what());
        MessageBoxW(m_hwnd, message.c_str(), L"Y-Render Error", MB_ICONERROR | MB_OK);
        return -1;
    }
}

LRESULT CALLBACK RendererApp::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RendererApp* app = nullptr;
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<RendererApp*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else
    {
        app = reinterpret_cast<RendererApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app)
    {
        return app->HandleMessage(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT RendererApp::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const bool imguiHandled = ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_DESTROY:
        m_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (m_renderDevice.IsInitialized() && wParam != SIZE_MINIMIZED)
        {
            m_width = std::max<UINT>(1, LOWORD(lParam));
            m_height = std::max<UINT>(1, HIWORD(lParam));
            m_renderDevice.Resize(m_width, m_height);
        }
        return 0;
    case WM_DPICHANGED:
        if (lParam)
        {
            const RECT* suggestedRect = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(
                hwnd,
                nullptr,
                suggestedRect->left,
                suggestedRect->top,
                suggestedRect->right - suggestedRect->left,
                suggestedRect->bottom - suggestedRect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_KILLFOCUS:
        m_input.Clear();
        m_viewportInputCaptured = false;
        m_mouseButtons[0] = false;
        m_mouseButtons[1] = false;
        m_mouseButtons[2] = false;
        m_hasLastMousePoint = false;
        ReleaseCapture();
        return 0;
    case WM_KEYDOWN:
        if (wParam < 256)
        {
            const bool wasDown = m_input.IsDown(static_cast<UINT>(wParam));
            m_input.SetKey(static_cast<UINT>(wParam), true);
            if (!wasDown)
            {
                OnKeyPressed(static_cast<UINT>(wParam));
            }
        }
        return 0;
    case WM_KEYUP:
        if (wParam < 256)
        {
            m_input.SetKey(static_cast<UINT>(wParam), false);
        }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        OnMouseButton(message, lParam);
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(lParam);
        return 0;
    case WM_MOUSEWHEEL:
        OnMouseWheel(wParam, lParam);
        return 0;
    default:
        if (imguiHandled)
        {
            return true;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void RendererApp::OnKeyPressed(UINT key)
{
    if (key == VK_ESCAPE)
    {
        DestroyWindow(m_hwnd);
    }
    else if (key == VK_F1)
    {
        m_wireframe = !m_wireframe;
    }
    else if (key == VK_F2)
    {
        m_showDebugUi = !m_showDebugUi;
        ResetCameraForDemo();
    }
    else if (key == VK_F5)
    {
        ReloadShaders();
    }
    else if (key == VK_F9)
    {
        CaptureScreenshot();
    }
    else if (key == VK_TAB)
    {
        m_demo = (m_demo + 1) % kDemoCount;
        BuildScene();
    }
    else if (key == VK_SPACE)
    {
        m_effectsEnabled = !m_effectsEnabled;
    }
    else if (key == 'T')
    {
        m_autoTurntable = !m_autoTurntable;
    }
    else if (key == 'R' || key == VK_HOME)
    {
        ResetCameraForDemo();
    }
    else if (key >= '1' && key < '1' + kDemoCount)
    {
        m_demo = static_cast<int>(key - '1');
        BuildScene();
    }
}

void RendererApp::OnMouseButton(UINT message, LPARAM lParam)
{
    const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const bool inPreview = IsPointInPreview(point);
    switch (message)
    {
    case WM_LBUTTONDOWN:
        m_mouseButtons[0] = true;
        break;
    case WM_RBUTTONDOWN:
        m_mouseButtons[1] = true;
        break;
    case WM_MBUTTONDOWN:
        m_mouseButtons[2] = true;
        break;
    case WM_LBUTTONUP:
        m_mouseButtons[0] = false;
        break;
    case WM_RBUTTONUP:
        m_mouseButtons[1] = false;
        break;
    case WM_MBUTTONUP:
        m_mouseButtons[2] = false;
        break;
    default:
        break;
    }

    if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
    {
        m_viewportInputCaptured = inPreview;
        m_hasLastMousePoint = true;
        m_lastMousePoint = point;
        if (m_viewportInputCaptured)
        {
            SetCapture(m_hwnd);
        }
    }

    if (!m_mouseButtons[0] && !m_mouseButtons[1] && !m_mouseButtons[2])
    {
        m_viewportInputCaptured = false;
        m_hasLastMousePoint = false;
        if (GetCapture() == m_hwnd)
        {
            ReleaseCapture();
        }
    }
}

void RendererApp::OnMouseMove(LPARAM lParam)
{
    const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (m_viewportInputCaptured && m_hasLastMousePoint && (m_mouseButtons[0] || m_mouseButtons[1]))
    {
        m_pendingLookDeltaX += static_cast<float>(point.x - m_lastMousePoint.x);
        m_pendingLookDeltaY += static_cast<float>(point.y - m_lastMousePoint.y);
    }
    m_lastMousePoint = point;
    m_hasLastMousePoint = true;
}

void RendererApp::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(m_hwnd, &point);
    if (IsPointInPreview(point))
    {
        m_pendingWheelDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
    }
}

bool RendererApp::IsPointInPreview(POINT clientPoint) const
{
    const ShaderLabViewport viewport = CurrentPreviewViewport();
    return static_cast<float>(clientPoint.x) >= viewport.x &&
        static_cast<float>(clientPoint.y) >= viewport.y &&
        static_cast<float>(clientPoint.x) < viewport.x + viewport.width &&
        static_cast<float>(clientPoint.y) < viewport.y + viewport.height;
}

ShaderLabViewport RendererApp::CurrentPreviewViewport() const
{
    const float width = static_cast<float>(std::max<UINT>(1, m_renderDevice.Width()));
    const float height = static_cast<float>(std::max<UINT>(1, m_renderDevice.Height()));
    if (!m_showDebugUi)
    {
        return {0.0f, 0.0f, width, height};
    }
    return m_editorPanel.PreviewViewport(width, height);
}

void RendererApp::CreateAppWindow(HINSTANCE instance, int showCommand)
{
    CreateDirectoryW(L"logs", nullptr);
    InitializeLog(L"logs\\y-render.log");
    LogInfo("Starting Y-Render");

    m_config = LoadProjectConfig(FindAsset(L"config\\y-render.ini"));
    m_width = m_config.windowWidth;
    m_height = m_config.windowHeight;
    m_demo = m_config.defaultDemo;
    m_showDebugUi = m_config.showDebugUi;
    for (const std::wstring& root : m_config.assetRoots)
    {
        AddAssetRoot(root);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    RECT rect{0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"Y-Render",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        this);

    if (!m_hwnd)
    {
        throw std::runtime_error("CreateWindowEx failed");
    }

    ShowWindow(m_hwnd, showCommand);
}

void RendererApp::InitializeRenderer()
{
    RECT clientRect{};
    if (GetClientRect(m_hwnd, &clientRect))
    {
        m_width = std::max<UINT>(1, static_cast<UINT>(clientRect.right - clientRect.left));
        m_height = std::max<UINT>(1, static_cast<UINT>(clientRect.bottom - clientRect.top));
    }
    m_renderDevice.Initialize(m_hwnd, m_width, m_height);
    m_resources.Initialize();
}

void RendererApp::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 3;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;
    fontConfig.RasterizerMultiply = 1.12f;
    if (FileExists(L"C:\\Windows\\Fonts\\Deng.ttf"))
    {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Deng.ttf", 19.0f, &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
    }
    else if (FileExists(L"C:\\Windows\\Fonts\\simhei.ttf"))
    {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\simhei.ttf", 19.0f, &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
    }
    else if (FileExists(L"C:\\Windows\\Fonts\\msyh.ttc"))
    {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 19.0f, &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
    }
    else
    {
        io.Fonts->AddFontDefault();
    }
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 2.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8.0f, 6.0f);
    style.FramePadding = ImVec2(7.0f, 4.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.88f, 0.82f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.46f, 0.40f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.057f, 0.055f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.057f, 0.055f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.070f, 0.070f, 0.065f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.24f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.20f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.25f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.080f, 0.078f, 0.070f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.13f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.095f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.045f, 0.045f, 0.043f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.32f, 0.29f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.44f, 0.36f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.58f, 0.45f, 0.20f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.86f, 0.62f, 0.22f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.66f, 0.49f, 0.21f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.88f, 0.62f, 0.20f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.17f, 0.14f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.27f, 0.16f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.48f, 0.36f, 0.14f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.26f, 0.22f, 0.14f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.39f, 0.31f, 0.16f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.52f, 0.38f, 0.14f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.26f, 0.24f, 0.19f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.63f, 0.45f, 0.17f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.82f, 0.55f, 0.16f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.36f, 0.27f, 0.12f, 0.80f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.62f, 0.42f, 0.15f, 0.90f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.82f, 0.55f, 0.16f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.15f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.36f, 0.28f, 0.14f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.26f, 0.21f, 0.12f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.77f, 0.52f, 0.16f, 1.00f);

    ImGui_ImplWin32_InitForOpenGL(m_hwnd);
    ImGui_ImplOpenGL3_Init();
}

void RendererApp::ShutdownImGui()
{
    if (!ImGui::GetCurrentContext())
    {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void RendererApp::LoadAssets()
{
    m_standardShader = m_resources.CreateStandardShader();
    m_postShader = m_resources.CreatePostShader();

    m_planeMesh = CreatePlaneMesh();

    const std::vector<std::wstring> characterCandidates{
        m_config.characterModel,
        L"assets\\models\\character.obj",
        L"assets\\models\\character.gltf",
        L"assets\\models\\character.glb",
        L"assets\\models\\anime_character.obj",
        L"assets\\models\\anime_character.gltf",
        L"assets\\models\\anime_character.glb",
        L"assets\\models\\sample.obj",
    };

    m_externalCharacterLoaded = false;
    for (size_t i = 0; i < characterCandidates.size(); ++i)
    {
        const std::wstring candidate = ResolveAssetPath(characterCandidates[i]);
        if (!FileExists(candidate))
        {
            continue;
        }

        Mesh loadedMesh;
        if (LoadStaticMesh(candidate, loadedMesh))
        {
            m_characterMesh = std::move(loadedMesh);
            NormalizeMeshForPreview(m_characterMesh);
            m_externalCharacterLoaded = i + 1 < characterCandidates.size();
            break;
        }
    }

    if (m_characterMesh.vertices.empty() || m_characterMesh.indices.empty())
    {
        m_characterMesh = CreateDogMesh();
        NormalizeMeshForPreview(m_characterMesh);
        m_externalCharacterLoaded = false;
    }

    m_resources.UploadEmbeddedTextures(m_characterMesh);
    m_resources.UploadMesh(m_characterMesh);
    m_resources.UploadMesh(m_planeMesh);
    m_modelStatus = ModelStatusLine(m_characterMesh, m_externalCharacterLoaded);
    LogInfo(m_modelStatus);

    m_checkerTexture = m_resources.CreateCheckerTexture(128, 128);
    m_postQuadMesh.vertices = {
        Vertex{{-1.0f, -1.0f, 0.0f}, {}, {0.0f, 0.0f}},
        Vertex{{-1.0f, 1.0f, 0.0f}, {}, {0.0f, 1.0f}},
        Vertex{{1.0f, 1.0f, 0.0f}, {}, {1.0f, 1.0f}},
        Vertex{{1.0f, -1.0f, 0.0f}, {}, {1.0f, 0.0f}},
    };
    m_postQuadMesh.indices = {0, 1, 2, 0, 2, 3};
    m_resources.UploadMesh(m_postQuadMesh);
}

void RendererApp::ReloadShaders()
{
    ShaderProgram newStandardShader;
    ShaderProgram newPostShader;
    try
    {
        newStandardShader = m_resources.CreateStandardShader();
        newPostShader = m_resources.CreatePostShader();
        if (m_standardShader.program != 0)
        {
            glDeleteProgram(m_standardShader.program);
        }
        if (m_postShader.program != 0)
        {
            glDeleteProgram(m_postShader.program);
        }
        m_standardShader = newStandardShader;
        m_postShader = newPostShader;
        m_shaderStatus = "Shaders reloaded successfully";
        LogInfo("Shaders reloaded");
    }
    catch (const std::exception& error)
    {
        if (newStandardShader.program != 0)
        {
            glDeleteProgram(newStandardShader.program);
        }
        if (newPostShader.program != 0)
        {
            glDeleteProgram(newPostShader.program);
        }
        m_shaderStatus = error.what();
        LogError(std::string("Shader reload failed: ") + error.what());
    }
}

void RendererApp::CaptureScreenshot()
{
    m_captureScreenshotPending = true;
}

void RendererApp::CaptureScreenshotNow()
{
    CreateDirectoryW(L"screenshots", nullptr);

    std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_s(&localTime, &now);

    wchar_t filename[128]{};
    wcsftime(filename, std::size(filename), L"screenshots\\y-render-%Y%m%d-%H%M%S.png", &localTime);
    if (m_renderDevice.CaptureBackBufferPng(filename))
    {
        m_captureStatus = "Screenshot saved: " + WideToUtf8(filename);
        LogInfo("Screenshot captured");
    }
    else
    {
        m_captureStatus = "Screenshot capture failed";
        LogError("Screenshot capture failed");
    }
}

void RendererApp::BuildScene()
{
    m_scene.BuildDemo(m_demo, m_characterMesh, m_planeMesh, m_externalCharacterLoaded);
    if (m_scene.Objects().empty())
    {
        m_selectedObject = -1;
    }
    else if (m_scene.Objects().size() > 1)
    {
        m_selectedObject = 1;
    }
    else
    {
        m_selectedObject = 0;
    }
    m_effectsEnabled = true;
    ResetCameraForDemo();
}

void RendererApp::ResetCameraForDemo()
{
    const auto& objects = m_scene.Objects();
    if (objects.empty())
    {
        m_camera.SetOrbit(XMFLOAT3(0.0f, 0.25f, 0.0f), 4.0f, -0.20f, 0.04f);
        return;
    }

    const SceneObject& subject = objects.size() > 1 ? objects[1] : objects[0];
    if (!subject.mesh)
    {
        return;
    }

    const XMFLOAT3& minValue = subject.mesh->boundsMin;
    const XMFLOAT3& maxValue = subject.mesh->boundsMax;
    const XMFLOAT3 center{
        subject.transform.position.x + (minValue.x + maxValue.x) * 0.5f * subject.transform.scale.x,
        subject.transform.position.y + (minValue.y + maxValue.y) * 0.5f * subject.transform.scale.y,
        subject.transform.position.z + (minValue.z + maxValue.z) * 0.5f * subject.transform.scale.z,
    };
    const float width = std::abs((maxValue.x - minValue.x) * subject.transform.scale.x);
    const float height = std::abs((maxValue.y - minValue.y) * subject.transform.scale.y);
    const float depth = std::abs((maxValue.z - minValue.z) * subject.transform.scale.z);
    const float longest = std::max({width, height, depth, 1.0f});
    m_camera.SetOrbit(center, longest * 1.72f, -0.20f, 0.04f);
}

void RendererApp::MainLoop()
{
    LARGE_INTEGER frequency{};
    LARGE_INTEGER previous{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);

    MSG msg{};
    while (m_running)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                m_running = false;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        LARGE_INTEGER current{};
        QueryPerformanceCounter(&current);
        const float dt = static_cast<float>(static_cast<double>(current.QuadPart - previous.QuadPart) / static_cast<double>(frequency.QuadPart));
        previous = current;

        Update(dt);
        Render();
    }
}

void RendererApp::Update(float dt)
{
    m_time += dt;
    m_frameStats.Reset(dt);
    m_fpsAccumulator += dt;
    ++m_fpsFrames;
    if (m_fpsAccumulator >= 0.5f)
    {
        m_currentFps = static_cast<float>(m_fpsFrames) / m_fpsAccumulator;
        m_frameStats.fps = m_currentFps;
        m_fpsAccumulator = 0.0f;
        m_fpsFrames = 0;
        UpdateWindowTitle();
    }

    const ImGuiIO& io = ImGui::GetIO();
    POINT cursorPoint{};
    bool viewportHovered = false;
    if (GetCursorPos(&cursorPoint))
    {
        ScreenToClient(m_hwnd, &cursorPoint);
        viewportHovered = IsPointInPreview(cursorPoint);
    }

    const bool acceptCameraInput = (m_viewportInputCaptured || viewportHovered) && !io.WantTextInput;
    m_camera.Update(dt, m_input, acceptCameraInput, m_pendingLookDeltaX, m_pendingLookDeltaY, m_pendingWheelDelta);
    if (m_autoTurntable && !m_showDebugUi)
    {
        m_scene.Animate(dt, m_demo);
    }
    m_pendingLookDeltaX = 0.0f;
    m_pendingLookDeltaY = 0.0f;
    m_pendingWheelDelta = 0.0f;
}

void RendererApp::UpdateWindowTitle()
{
    DebugState state;
    state.demoIndex = m_demo;
    if (m_scene.Objects().size() > 1)
    {
        const SceneObject& character = m_scene.Objects()[1];
        state.demoIndex = character.material.surfaceEffect == 1 ? 2 :
            (character.material.surfaceEffect == 2 ? 3 : (character.material.lightingModel > 0 ? 1 : 0));
    }
    state.wireframe = m_wireframe;
    SetWindowTextW(m_hwnd, BuildDebugTitle(state).c_str());
}

XMMATRIX RendererApp::CameraViewProjection(float aspect) const
{
    return m_camera.ViewProjection(aspect);
}

void RendererApp::Render()
{
    const bool captureCleanFrame = m_captureScreenshotPending;
    m_hideShowcaseOverlay = captureCleanFrame;
    const FLOAT studioClear[] = {0.72f, 0.88f, 0.88f, 1.0f};
    const FLOAT toonClear[] = {0.70f, 0.84f, 0.90f, 1.0f};
    const FLOAT dissolveClear[] = {0.70f, 0.88f, 0.80f, 1.0f};
    const FLOAT depthClear[] = {0.72f, 0.86f, 0.92f, 1.0f};
    const FLOAT* sceneClear = m_demo == 0 ? studioClear : (m_demo == 1 ? toonClear : (m_demo == 2 ? dissolveClear : depthClear));
    m_renderDevice.BeginEvent(L"Scene Pass");
    ++m_frameStats.passes;
    m_renderDevice.BeginScene(sceneClear);
    const ShaderLabViewport previewViewport = CurrentPreviewViewport();
    m_renderDevice.SetViewport(previewViewport.x, previewViewport.y, previewViewport.width, previewViewport.height);
    m_renderDevice.SetWireframe(m_wireframe);

    RenderSceneObjects();
    m_renderDevice.EndEvent();

    if (ShouldBloom())
    {
        RenderBloomPass();
    }

    if (m_showDebugUi)
    {
        RenderDepthPreviewPass();
    }

    m_renderDevice.BeginEvent(L"Presentation Pass");
    ++m_frameStats.passes;
    RenderPresentationPass();
    m_renderDevice.EndEvent();

    m_renderDevice.BeginEvent(L"Debug UI");
    RenderDebugUi();
    m_renderDevice.EndEvent();

    if (captureCleanFrame)
    {
        CaptureScreenshotNow();
        m_captureScreenshotPending = false;
        m_hideShowcaseOverlay = false;
    }

    m_renderDevice.Present();
}

void RendererApp::RenderSceneObjects()
{
    glUseProgram(m_standardShader.program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(m_standardShader.program, "uBaseTexture"), 0);

    const ShaderLabViewport previewViewport = CurrentPreviewViewport();
    const float aspect = previewViewport.width / std::max(1.0f, previewViewport.height);
    const XMMATRIX viewProj = CameraViewProjection(aspect);

    for (const SceneObject& object : m_scene.Objects())
    {
        if (!object.mesh)
        {
            continue;
        }

        const XMMATRIX world = object.transform.Matrix();
        const XMMATRIX normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
        SetMat4(m_standardShader.program, "uWorld", world);
        SetMat4(m_standardShader.program, "uNormalMatrix", normalMatrix);
        SetMat4(m_standardShader.program, "uViewProj", viewProj);
        const XMFLOAT3& cameraPosition = m_camera.Position();
        glUniform3f(glGetUniformLocation(m_standardShader.program, "uCameraPosition"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uTime"), m_time);
        glUniform4f(glGetUniformLocation(m_standardShader.program, "uBaseColor"), object.material.baseColor.x, object.material.baseColor.y, object.material.baseColor.z, object.material.baseColor.w);
        glUniform4f(glGetUniformLocation(m_standardShader.program, "uAmbientColor"), object.material.ambientColor.x, object.material.ambientColor.y, object.material.ambientColor.z, object.material.ambientColor.w);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDiffuseIntensity"), object.material.diffuseIntensity);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uSpecularPower"), object.material.specularPower);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uSpecularIntensity"), object.material.specularIntensity);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uUseTexture"), object.material.useAlbedoTexture ? 1.0f : 0.0f);
        glUniform2f(glGetUniformLocation(m_standardShader.program, "uUVTiling"), object.material.uvTiling.x, object.material.uvTiling.y);
        glUniform2f(glGetUniformLocation(m_standardShader.program, "uUVOffset"), object.material.uvOffset.x, object.material.uvOffset.y);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uUseAlphaTest"), 0.0f);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uAlphaCutoff"), 0.5f);
        const bool isShowcaseCharacter = object.mesh == &m_characterMesh;
        const bool forceUnlitComparison = isShowcaseCharacter && !m_effectsEnabled && m_demo == 0;
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uUseUnlit"), forceUnlitComparison ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uShowcaseStage"), object.mesh == &m_planeMesh ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uLightingModel"), m_effectsEnabled ? static_cast<float>(object.material.lightingModel) : 0.0f);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uSurfaceEffect"), m_effectsEnabled ? static_cast<float>(object.material.surfaceEffect) : 0.0f);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uToonSteps"), object.material.toonSteps);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uRimPower"), object.material.rimPower);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uRimIntensity"), object.material.rimIntensity);
        glUniform4f(glGetUniformLocation(m_standardShader.program, "uRimColor"), object.material.rimColor.x, object.material.rimColor.y, object.material.rimColor.z, object.material.rimColor.w);
        glUniform4f(glGetUniformLocation(m_standardShader.program, "uShadowColor"), object.material.shadowColor.x, object.material.shadowColor.y, object.material.shadowColor.z, object.material.shadowColor.w);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDissolveAmount"), object.material.dissolveAmount);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDissolveNoiseScale"), object.material.dissolveNoiseScale);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDissolveEdgeWidth"), object.material.dissolveEdgeWidth);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDissolveSpeed"), object.material.dissolveSpeed);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDissolveEdgeIntensity"), object.material.dissolveEdgeIntensity);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDissolveProgressSpeed"), object.material.dissolveProgressSpeed);
        glUniform1f(glGetUniformLocation(m_standardShader.program, "uDissolveAutoProgress"), object.material.dissolveAutoProgress ? 1.0f : 0.0f);
        glUniform4f(glGetUniformLocation(m_standardShader.program, "uDissolveEdgeColor"), object.material.dissolveEdgeColor.x, object.material.dissolveEdgeColor.y, object.material.dissolveEdgeColor.z, object.material.dissolveEdgeColor.w);
        const auto& lights = m_scene.DirectionalLights();
        const int lightCount = static_cast<int>(std::min<size_t>(lights.size(), 4));
        glUniform1i(glGetUniformLocation(m_standardShader.program, "uLightCount"), lightCount);
        for (int i = 0; i < lightCount; ++i)
        {
            char name[64]{};
            sprintf_s(name, "uLightDirections[%d]", i);
            glUniform3f(glGetUniformLocation(m_standardShader.program, name), lights[i].direction.x, lights[i].direction.y, lights[i].direction.z);
            sprintf_s(name, "uLightColors[%d]", i);
            glUniform4f(glGetUniformLocation(m_standardShader.program, name), lights[i].color.x, lights[i].color.y, lights[i].color.z, lights[i].intensity);
        }

        glBindVertexArray(object.mesh->vao);
        if (!object.mesh->submeshes.empty())
        {
            const auto drawImportedSubmesh = [&](const SubMesh& submesh)
            {
                const ImportedMaterial* importedMaterial = nullptr;
                if (submesh.materialIndex >= 0 && submesh.materialIndex < static_cast<int>(object.mesh->importedMaterials.size()))
                {
                    importedMaterial = &object.mesh->importedMaterials[static_cast<size_t>(submesh.materialIndex)];
                }

                unsigned int texture = 0;
                if (importedMaterial && importedMaterial->baseColorImage >= 0 && importedMaterial->baseColorImage < static_cast<int>(object.mesh->embeddedImages.size()))
                {
                    texture = object.mesh->embeddedImages[static_cast<size_t>(importedMaterial->baseColorImage)].texture;
                }

                const bool useTexture = object.material.useAlbedoTexture && texture != 0;
                glUniform1f(glGetUniformLocation(m_standardShader.program, "uUseTexture"), useTexture ? 1.0f : 0.0f);
                glUniform1f(glGetUniformLocation(m_standardShader.program, "uUseAlphaTest"), importedMaterial && importedMaterial->alphaMask ? 1.0f : 0.0f);
                glUniform1f(glGetUniformLocation(m_standardShader.program, "uAlphaCutoff"), importedMaterial ? importedMaterial->alphaCutoff : 0.5f);
                const bool useImportedUnlit = forceUnlitComparison ||
                    (object.material.preserveImportedUnlit && importedMaterial && importedMaterial->unlit);
                glUniform1f(glGetUniformLocation(m_standardShader.program, "uUseUnlit"), useImportedUnlit ? 1.0f : 0.0f);
                glBindTexture(GL_TEXTURE_2D, useTexture ? texture : m_checkerTexture.id);
                glDrawElements(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(submesh.indexCount),
                    GL_UNSIGNED_INT,
                    reinterpret_cast<const void*>(static_cast<size_t>(submesh.indexStart) * sizeof(uint32_t)));
                ++m_frameStats.drawCalls;
                m_frameStats.triangles += static_cast<int>(submesh.indexCount / 3);
            };

            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            for (const SubMesh& submesh : object.mesh->submeshes)
            {
                const bool isBlend = submesh.materialIndex >= 0 &&
                    submesh.materialIndex < static_cast<int>(object.mesh->importedMaterials.size()) &&
                    object.mesh->importedMaterials[static_cast<size_t>(submesh.materialIndex)].alphaBlend;
                if (!isBlend)
                {
                    drawImportedSubmesh(submesh);
                }
            }

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            for (const SubMesh& submesh : object.mesh->submeshes)
            {
                const bool isBlend = submesh.materialIndex >= 0 &&
                    submesh.materialIndex < static_cast<int>(object.mesh->importedMaterials.size()) &&
                    object.mesh->importedMaterials[static_cast<size_t>(submesh.materialIndex)].alphaBlend;
                if (isBlend)
                {
                    drawImportedSubmesh(submesh);
                }
            }
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        else
        {
            const unsigned int texture = object.material.albedoTexture ? object.material.albedoTexture->id : m_checkerTexture.id;
            glUniform1f(glGetUniformLocation(m_standardShader.program, "uUseUnlit"), forceUnlitComparison ? 1.0f : 0.0f);
            glBindTexture(GL_TEXTURE_2D, texture);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(object.mesh->indices.size()), GL_UNSIGNED_INT, nullptr);
            ++m_frameStats.drawCalls;
            m_frameStats.triangles += static_cast<int>(object.mesh->indices.size() / 3);
        }
    }
    glBindVertexArray(0);
    glUseProgram(0);
}

bool RendererApp::ShouldBloom() const
{
    if (!m_effectsEnabled || !m_bloomEnabled)
    {
        return false;
    }
    for (const SceneObject& object : m_scene.Objects())
    {
        if (object.material.surfaceEffect == 1)
        {
            return true;
        }
    }
    return false;
}

bool RendererApp::ShouldDepthEffects() const
{
    if (!m_effectsEnabled || (!m_depthFogEnabled && !m_depthOutlineEnabled))
    {
        return false;
    }
    for (const SceneObject& object : m_scene.Objects())
    {
        if (object.material.surfaceEffect == 2)
        {
            return true;
        }
    }
    return false;
}

void RendererApp::RenderPostQuad(float mode, unsigned int inputTexture, unsigned int bloomTexture, float bloomStrength)
{
    glUseProgram(m_postShader.program);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uMode"), mode);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uTime"), m_time);
    glUniform2f(glGetUniformLocation(m_postShader.program, "uInvResolution"), 1.0f / static_cast<float>(m_renderDevice.Width()), 1.0f / static_cast<float>(m_renderDevice.Height()));
    glUniform1f(glGetUniformLocation(m_postShader.program, "uBloomThreshold"), m_bloomThreshold);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uBloomStrength"), bloomStrength);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthEffectsEnabled"), ShouldDepthEffects() ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthFogEnabled"), m_depthFogEnabled ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthFogStart"), m_depthFogStart);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthFogEnd"), m_depthFogEnd);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthFogDensity"), m_depthFogDensity);
    glUniform4f(glGetUniformLocation(m_postShader.program, "uDepthFogColor"), m_depthFogColor.x, m_depthFogColor.y, m_depthFogColor.z, m_depthFogColor.w);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthOutlineEnabled"), m_depthOutlineEnabled ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthOutlineWidth"), m_depthOutlineWidth);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uDepthOutlineStrength"), m_depthOutlineStrength);
    glUniform4f(glGetUniformLocation(m_postShader.program, "uDepthOutlineColor"), m_depthOutlineColor.x, m_depthOutlineColor.y, m_depthOutlineColor.z, m_depthOutlineColor.w);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uNearPlane"), 0.05f);
    glUniform1f(glGetUniformLocation(m_postShader.program, "uFarPlane"), 200.0f);
    glUniform1i(glGetUniformLocation(m_postShader.program, "uSceneColor"), 0);
    glUniform1i(glGetUniformLocation(m_postShader.program, "uSceneDepth"), 1);
    glUniform1i(glGetUniformLocation(m_postShader.program, "uBloomTexture"), 2);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, m_renderDevice.DepthTexture());
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, bloomTexture);
    glBindVertexArray(m_postQuadMesh.vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_postQuadMesh.indices.size()), GL_UNSIGNED_INT, nullptr);
    ++m_frameStats.drawCalls;
    m_frameStats.triangles += static_cast<int>(m_postQuadMesh.indices.size() / 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

void RendererApp::RenderBloomPass()
{
    m_renderDevice.BeginEvent(L"Bloom Bright Pass");
    ++m_frameStats.passes;
    m_renderDevice.BeginBloomTarget();
    m_renderDevice.SetSceneViewport();
    m_renderDevice.SetWireframe(false);
    RenderPostQuad(4.0f, m_renderDevice.SceneColorTexture(), 0, 0.0f);
    m_renderDevice.EndEvent();

    m_renderDevice.BeginEvent(L"Bloom Horizontal Blur");
    ++m_frameStats.passes;
    m_renderDevice.BeginBlurTargetA();
    m_renderDevice.SetSceneViewport();
    RenderPostQuad(5.0f, m_renderDevice.BloomTexture(), 0, 0.0f);
    m_renderDevice.EndEvent();

    m_renderDevice.BeginEvent(L"Bloom Vertical Blur");
    ++m_frameStats.passes;
    m_renderDevice.BeginBlurTargetB();
    m_renderDevice.SetSceneViewport();
    RenderPostQuad(6.0f, m_renderDevice.BlurTextureA(), 0, 0.0f);
    m_renderDevice.EndEvent();
}

void RendererApp::RenderDepthPreviewPass()
{
    m_renderDevice.BeginEvent(L"Depth Preview");
    ++m_frameStats.passes;
    m_renderDevice.BeginDepthPreviewTarget();
    m_renderDevice.SetSceneViewport();
    m_renderDevice.SetWireframe(false);
    RenderPostQuad(8.0f, m_renderDevice.SceneColorTexture(), 0, 0.0f);
    m_renderDevice.EndEvent();
}

void RendererApp::RenderPresentationPass()
{
    const FLOAT clear[] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_renderDevice.BeginBackBuffer(clear);
    m_renderDevice.SetSceneViewport();
    m_renderDevice.SetWireframe(false);

    const bool bloom = ShouldBloom();
    RenderPostQuad(bloom ? 7.0f : 0.0f, m_renderDevice.SceneColorTexture(), bloom ? m_renderDevice.BlurTextureB() : 0, bloom ? m_bloomStrength : 0.0f);
}

void RendererApp::RenderDebugUi()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (m_showDebugUi)
    {
        EditorPanelContext context{
            m_scene,
            m_camera,
            m_frameStats,
            m_selectedObject,
            m_wireframe,
            m_demo,
            m_effectsEnabled,
            m_bloomEnabled,
            m_bloomThreshold,
            m_bloomStrength,
            m_depthFogEnabled,
            m_depthFogStart,
            m_depthFogEnd,
            m_depthFogDensity,
            m_depthFogColor,
            m_depthOutlineEnabled,
            m_depthOutlineWidth,
            m_depthOutlineStrength,
            m_depthOutlineColor,
            m_autoTurntable,
            m_currentFps,
            m_renderDevice.Width(),
            m_renderDevice.Height(),
            m_renderDevice.SceneColorTexture(),
            m_renderDevice.DepthTexture(),
            m_renderDevice.DepthPreviewTexture(),
            m_checkerTexture.id,
            m_shaderStatus,
            m_modelStatus,
            m_captureStatus,
            [this]() { BuildScene(); },
            [this]() { ReloadShaders(); },
            [this]() { ResetCameraForDemo(); },
            [this]() { CaptureScreenshot(); },
        };
        m_editorPanel.Render(context);
    }
    else
    {
        RenderShowcaseOverlay();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RendererApp::RenderShowcaseOverlay()
{
    if (m_hideShowcaseOverlay)
    {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 18.0f, viewport->WorkPos.y + 18.0f));
    ImGui::SetNextWindowBgAlpha(0.68f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs;

    ImGui::Begin("Y-Render Showcase", nullptr, flags);
    ImGui::TextDisabled("Y-RENDER  /  展示模式");
    int currentEffect = m_demo;
    if (m_scene.Objects().size() > 1)
    {
        const SceneObject& character = m_scene.Objects()[1];
        currentEffect = character.material.surfaceEffect == 1 ? 2 :
            (character.material.surfaceEffect == 2 ? 3 : (character.material.lightingModel > 0 ? 1 : 0));
    }
    ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.30f, 1.0f), "当前演示 %d/%d：%s", currentEffect + 1, kDemoCount, ShowcaseEffectName(currentEffect));
    ImGui::Text("演示状态：%s", m_effectsEnabled ? "开启" : "旁路");
    ImGui::Text("自动转台：%s", m_autoTurntable ? "开启" : "关闭");
    ImGui::Separator();
    ImGui::TextDisabled("拖动旋转  |  滚轮缩放  |  R 重置视角");
    ImGui::TextDisabled("Tab/1/2/3/4 切换效果  |  Space 对比  |  T 转台  |  F2 实验面板");
    ImGui::End();
}

void RendererApp::SetMat4(unsigned int program, const char* name, const XMMATRIX& matrix) const
{
    XMFLOAT4X4 value{};
    XMStoreFloat4x4(&value, matrix);
    glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, &value.m[0][0]);
}
} // namespace YRender
