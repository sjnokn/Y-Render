#include "Renderer/RendererApp.h"

#include "Assets/MeshFactory.h"
#include "Core/Platform.h"
#include "Renderer/DebugOverlay.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

using namespace DirectX;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace YRender
{
namespace
{
constexpr wchar_t kWindowClassName[] = L"YRenderWindowClass";
}

int RendererApp::Run(HINSTANCE instance, int showCommand)
{
    try
    {
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
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
    case WM_DESTROY:
        m_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (m_renderDevice.Device() && wParam != SIZE_MINIMIZED)
        {
            m_width = std::max<UINT>(1, LOWORD(lParam));
            m_height = std::max<UINT>(1, HIWORD(lParam));
            m_renderDevice.Resize(m_width, m_height);
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam < 256)
        {
            const bool wasDown = m_keys[wParam];
            m_keys[wParam] = true;
            if (!wasDown)
            {
                OnKeyPressed(static_cast<UINT>(wParam));
            }
        }
        return 0;
    case WM_KEYUP:
        if (wParam < 256)
        {
            m_keys[wParam] = false;
        }
        return 0;
    default:
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
    }
    else if (key == VK_TAB)
    {
        m_demo = (m_demo + 1) % 2;
        BuildScene();
    }
    else if (key >= '1' && key <= '4')
    {
        m_postMode = static_cast<int>(key - '1');
    }
}

void RendererApp::CreateAppWindow(HINSTANCE instance, int showCommand)
{
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
    m_renderDevice.Initialize(m_hwnd, m_width, m_height);
    m_resources.Initialize(m_renderDevice.Device(), m_renderDevice.Context());

    m_cameraBuffer = m_resources.CreateConstantBuffer(sizeof(CameraConstants));
    m_materialBuffer = m_resources.CreateConstantBuffer(sizeof(MaterialConstants));
    m_postBuffer = m_resources.CreateConstantBuffer(sizeof(PostConstants));
}

void RendererApp::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_renderDevice.Device(), m_renderDevice.Context());
}

void RendererApp::ShutdownImGui()
{
    if (!ImGui::GetCurrentContext())
    {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void RendererApp::LoadAssets()
{
    m_standardShader = m_resources.CreateStandardShader();
    m_postShader = m_resources.CreatePostShader();

    m_cubeMesh = CreateCubeMesh();
    m_planeMesh = CreatePlaneMesh();
    if (!LoadObj(FindAsset(L"assets\\models\\sample.obj"), m_objMesh))
    {
        m_objMesh = CreateCubeMesh();
    }

    m_resources.UploadMesh(m_cubeMesh);
    m_resources.UploadMesh(m_planeMesh);
    m_resources.UploadMesh(m_objMesh);

    m_checkerTexture = m_resources.CreateCheckerTexture(128, 128);
    m_postVertexBuffer = m_resources.CreatePostQuad();
}

void RendererApp::BuildScene()
{
    m_scene.BuildDemo(m_demo, m_cubeMesh, m_planeMesh, m_objMesh);
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
    m_fpsAccumulator += dt;
    ++m_fpsFrames;
    if (m_fpsAccumulator >= 0.5f)
    {
        m_currentFps = static_cast<float>(m_fpsFrames) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrames = 0;
        UpdateWindowTitle();
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard)
    {
        const float moveSpeed = (m_keys[VK_SHIFT] ? 8.0f : 3.5f) * dt;
        const float lookSpeed = 1.6f * dt;
        if (m_keys[VK_LEFT])
        {
            m_cameraYaw -= lookSpeed;
        }
        if (m_keys[VK_RIGHT])
        {
            m_cameraYaw += lookSpeed;
        }
        if (m_keys[VK_UP])
        {
            m_cameraPitch += lookSpeed;
        }
        if (m_keys[VK_DOWN])
        {
            m_cameraPitch -= lookSpeed;
        }
        m_cameraPitch = std::clamp(m_cameraPitch, -1.45f, 1.45f);

        XMVECTOR forward = XMVector3Normalize(XMVectorSet(std::sin(m_cameraYaw), 0.0f, std::cos(m_cameraYaw), 0.0f));
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward));
        XMVECTOR position = XMLoadFloat3(&m_cameraPosition);

        if (m_keys['W'])
        {
            position += forward * moveSpeed;
        }
        if (m_keys['S'])
        {
            position -= forward * moveSpeed;
        }
        if (m_keys['D'])
        {
            position += right * moveSpeed;
        }
        if (m_keys['A'])
        {
            position -= right * moveSpeed;
        }
        if (m_keys['E'])
        {
            position += XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * moveSpeed;
        }
        if (m_keys['Q'])
        {
            position -= XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * moveSpeed;
        }

        XMStoreFloat3(&m_cameraPosition, position);
    }

    m_scene.Animate(m_time, m_demo);
}

void RendererApp::UpdateWindowTitle()
{
    DebugState state;
    state.fps = m_currentFps;
    state.demoIndex = m_demo;
    state.postMode = m_postMode;
    state.wireframe = m_wireframe;
    SetWindowTextW(m_hwnd, BuildDebugTitle(state).c_str());
}

XMMATRIX RendererApp::CameraViewProjection() const
{
    const XMVECTOR position = XMLoadFloat3(&m_cameraPosition);
    const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
        std::cos(m_cameraPitch) * std::sin(m_cameraYaw),
        std::sin(m_cameraPitch),
        std::cos(m_cameraPitch) * std::cos(m_cameraYaw),
        0.0f));
    const XMMATRIX view = XMMatrixLookToLH(position, forward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const float aspect = static_cast<float>(m_renderDevice.Width()) / static_cast<float>(std::max<UINT>(1, m_renderDevice.Height()));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.05f, 200.0f);
    return view * projection;
}

void RendererApp::Render()
{
    const FLOAT sceneClear[] = {0.08f, 0.10f, 0.13f, 1.0f};
    m_renderDevice.BeginScene(sceneClear);
    m_renderDevice.SetSceneViewport();
    m_renderDevice.SetWireframe(m_wireframe);

    RenderSceneObjects();
    RenderPostProcess();
    RenderDebugUi();

    m_renderDevice.Present();
}

void RendererApp::RenderSceneObjects()
{
    ID3D11DeviceContext* context = m_renderDevice.Context();

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(m_standardShader.inputLayout.Get());
    context->VSSetShader(m_standardShader.vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_standardShader.pixelShader.Get(), nullptr, 0);
    ID3D11SamplerState* sampler = m_renderDevice.LinearSampler();
    context->PSSetSamplers(0, 1, &sampler);
    context->VSSetConstantBuffers(0, 1, m_cameraBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_cameraBuffer.GetAddressOf());
    context->PSSetConstantBuffers(1, 1, m_materialBuffer.GetAddressOf());

    const XMMATRIX viewProj = CameraViewProjection();

    for (const SceneObject& object : m_scene.Objects())
    {
        if (!object.mesh)
        {
            continue;
        }

        CameraConstants cameraConstants;
        XMStoreFloat4x4(&cameraConstants.world, object.transform.Matrix());
        XMStoreFloat4x4(&cameraConstants.viewProj, viewProj);
        cameraConstants.cameraPosition = m_cameraPosition;
        cameraConstants.time = m_time;
        context->UpdateSubresource(m_cameraBuffer.Get(), 0, nullptr, &cameraConstants, 0, 0);

        MaterialConstants materialConstants;
        materialConstants.baseColor = object.material.baseColor;
        materialConstants.specularPower = object.material.specularPower;
        materialConstants.useTexture = object.material.useTexture ? 1.0f : 0.0f;
        context->UpdateSubresource(m_materialBuffer.Get(), 0, nullptr, &materialConstants, 0, 0);

        ID3D11ShaderResourceView* srv = m_checkerTexture.srv.Get();
        context->PSSetShaderResources(0, 1, &srv);

        const UINT stride = sizeof(Vertex);
        const UINT offset = 0;
        ID3D11Buffer* vertexBuffer = object.mesh->vertexBuffer.Get();
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        context->IASetIndexBuffer(object.mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(static_cast<UINT>(object.mesh->indices.size()), 0, 0);
    }
}

void RendererApp::RenderPostProcess()
{
    ID3D11DeviceContext* context = m_renderDevice.Context();
    ID3D11ShaderResourceView* nullSrvs[] = {nullptr};
    context->PSSetShaderResources(0, 1, nullSrvs);

    const FLOAT clear[] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_renderDevice.BeginBackBuffer(clear);
    m_renderDevice.SetWireframe(false);

    PostConstants constants;
    constants.mode = static_cast<float>(m_postMode);
    constants.time = m_time;
    constants.invResolution = XMFLOAT2(1.0f / static_cast<float>(m_renderDevice.Width()), 1.0f / static_cast<float>(m_renderDevice.Height()));
    context->UpdateSubresource(m_postBuffer.Get(), 0, nullptr, &constants, 0, 0);

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(m_postShader.inputLayout.Get());
    const UINT stride = sizeof(PostVertex);
    const UINT offset = 0;
    ID3D11Buffer* buffer = m_postVertexBuffer.Get();
    context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

    context->VSSetShader(m_postShader.vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_postShader.pixelShader.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, m_postBuffer.GetAddressOf());
    ID3D11SamplerState* sampler = m_renderDevice.LinearSampler();
    context->PSSetSamplers(0, 1, &sampler);
    ID3D11ShaderResourceView* sceneSrv = m_renderDevice.SceneColorSrv();
    context->PSSetShaderResources(0, 1, &sceneSrv);
    context->Draw(6, 0);

    context->PSSetShaderResources(0, 1, nullSrvs);
}

void RendererApp::RenderDebugUi()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (m_showDebugUi)
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 300.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Y-Render Debug", &m_showDebugUi);

        ImGui::Text("Frame");
        ImGui::Text("FPS: %.1f", m_currentFps);
        ImGui::Text("Resolution: %u x %u", m_renderDevice.Width(), m_renderDevice.Height());
        ImGui::Separator();

        int demo = m_demo;
        const char* demos[] = {"Primitive Scene", "OBJ Scene"};
        if (ImGui::Combo("Demo", &demo, demos, IM_ARRAYSIZE(demos)))
        {
            m_demo = demo;
            BuildScene();
        }

        const char* postModes[] = {"None", "Grayscale", "Invert", "Vignette"};
        ImGui::Combo("Post Process", &m_postMode, postModes, IM_ARRAYSIZE(postModes));
        ImGui::Checkbox("Wireframe", &m_wireframe);

        ImGui::Separator();
        ImGui::Text("Camera");
        ImGui::Text("Position: %.2f %.2f %.2f", m_cameraPosition.x, m_cameraPosition.y, m_cameraPosition.z);
        ImGui::Text("Yaw/Pitch: %.2f %.2f", m_cameraYaw, m_cameraPitch);
        ImGui::Separator();
        ImGui::Text("Controls");
        ImGui::Text("F1 wireframe, F2 debug UI, Tab demo");

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
} // namespace YRender
