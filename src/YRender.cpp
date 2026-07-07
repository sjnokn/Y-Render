#include <windows.h>
#include <wincodec.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
constexpr UINT kInitialWidth = 1280;
constexpr UINT kInitialHeight = 720;
constexpr wchar_t kWindowClassName[] = L"YRenderWindowClass";

struct Vertex
{
    XMFLOAT3 position{};
    XMFLOAT3 normal{};
    XMFLOAT2 uv{};
};

struct PostVertex
{
    XMFLOAT3 position{};
    XMFLOAT2 uv{};
};

struct CameraConstants
{
    XMFLOAT4X4 world{};
    XMFLOAT4X4 viewProj{};
    XMFLOAT3 cameraPosition{};
    float time = 0.0f;
};

struct MaterialConstants
{
    XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    XMFLOAT4 lightDirection{-0.35f, -0.85f, 0.35f, 0.0f};
    XMFLOAT4 lightColor{1.0f, 0.95f, 0.85f, 1.0f};
    XMFLOAT4 ambientColor{0.12f, 0.14f, 0.18f, 1.0f};
    float specularPower = 48.0f;
    float useTexture = 1.0f;
    XMFLOAT2 padding{};
};

struct PostConstants
{
    float mode = 0.0f;
    float time = 0.0f;
    XMFLOAT2 invResolution{};
};

struct Transform
{
    XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    XMFLOAT3 rotation{0.0f, 0.0f, 0.0f};
    XMFLOAT3 scale{1.0f, 1.0f, 1.0f};

    XMMATRIX Matrix() const
    {
        return XMMatrixScaling(scale.x, scale.y, scale.z) *
            XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) *
            XMMatrixTranslation(position.x, position.y, position.z);
    }
};

struct Material
{
    XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    float specularPower = 48.0f;
    bool useTexture = true;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
};

struct Texture
{
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
};

struct ShaderProgram
{
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> inputLayout;
};

struct RenderTarget
{
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
};

struct SceneObject
{
    Mesh* mesh = nullptr;
    Transform transform;
    Material material;
};

struct ObjKey
{
    int position = 0;
    int texcoord = 0;
    int normal = 0;

    bool operator==(const ObjKey& other) const
    {
        return position == other.position && texcoord == other.texcoord && normal == other.normal;
    }
};

struct ObjKeyHash
{
    size_t operator()(const ObjKey& key) const
    {
        return static_cast<size_t>((key.position * 73856093) ^ (key.texcoord * 19349663) ^ (key.normal * 83492791));
    }
};

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

void ThrowIfFailed(HRESULT hr, const char* message)
{
    if (FAILED(hr))
    {
        std::ostringstream stream;
        stream << message << " HRESULT=0x" << std::hex << static_cast<uint32_t>(hr);
        throw std::runtime_error(stream.str());
    }
}

std::wstring GetExecutableDirectory()
{
    std::array<wchar_t, MAX_PATH> path{};
    GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    std::wstring fullPath = path.data();
    const size_t slash = fullPath.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : fullPath.substr(0, slash);
}

bool FileExists(const std::wstring& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring FindAsset(const std::wstring& relativePath)
{
    std::vector<std::wstring> roots;
    roots.push_back(L".");
    roots.push_back(GetExecutableDirectory());
    roots.push_back(GetExecutableDirectory() + L"\\..");
    roots.push_back(GetExecutableDirectory() + L"\\..\\..");

    for (const std::wstring& root : roots)
    {
        const std::wstring candidate = root + L"\\" + relativePath;
        if (FileExists(candidate))
        {
            return candidate;
        }
    }

    return relativePath;
}

ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entry, const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, flags, 0, &shader, &errors);
    if (FAILED(hr))
    {
        std::string errorText = errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()) : "unknown shader compile error";
        OutputDebugStringA(errorText.c_str());
        throw std::runtime_error("Shader compilation failed for " + WideToUtf8(path) + ":\n" + errorText);
    }

    return shader;
}

template <typename T>
ComPtr<ID3D11Buffer> CreateConstantBuffer(ID3D11Device* device)
{
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>((sizeof(T) + 15) & ~15);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(device->CreateBuffer(&desc, nullptr, &buffer), "Create constant buffer");
    return buffer;
}

ObjKey ParseObjKey(const std::string& text)
{
    ObjKey key{};
    std::stringstream stream(text);
    std::string item;

    if (std::getline(stream, item, '/'))
    {
        key.position = item.empty() ? 0 : std::stoi(item);
    }
    if (std::getline(stream, item, '/'))
    {
        key.texcoord = item.empty() ? 0 : std::stoi(item);
    }
    if (std::getline(stream, item, '/'))
    {
        key.normal = item.empty() ? 0 : std::stoi(item);
    }

    return key;
}

template <typename T>
const T& ObjAt(const std::vector<T>& values, int objIndex, const T& fallback)
{
    if (objIndex == 0)
    {
        return fallback;
    }

    const int resolved = objIndex > 0 ? objIndex - 1 : static_cast<int>(values.size()) + objIndex;
    if (resolved < 0 || resolved >= static_cast<int>(values.size()))
    {
        return fallback;
    }

    return values[static_cast<size_t>(resolved)];
}

bool LoadObj(const std::wstring& path, Mesh& outMesh)
{
    std::ifstream file(path);
    if (!file)
    {
        return false;
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> texcoords;
    std::unordered_map<ObjKey, uint32_t, ObjKeyHash> vertexMap;

    const XMFLOAT3 fallbackPosition{0.0f, 0.0f, 0.0f};
    const XMFLOAT3 fallbackNormal{0.0f, 1.0f, 0.0f};
    const XMFLOAT2 fallbackUv{0.0f, 0.0f};

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "v")
        {
            XMFLOAT3 value{};
            stream >> value.x >> value.y >> value.z;
            positions.push_back(value);
        }
        else if (type == "vn")
        {
            XMFLOAT3 value{};
            stream >> value.x >> value.y >> value.z;
            normals.push_back(value);
        }
        else if (type == "vt")
        {
            XMFLOAT2 value{};
            stream >> value.x >> value.y;
            value.y = 1.0f - value.y;
            texcoords.push_back(value);
        }
        else if (type == "f")
        {
            std::vector<uint32_t> faceIndices;
            std::string token;
            while (stream >> token)
            {
                ObjKey key = ParseObjKey(token);
                auto found = vertexMap.find(key);
                if (found == vertexMap.end())
                {
                    Vertex vertex{};
                    vertex.position = ObjAt(positions, key.position, fallbackPosition);
                    vertex.normal = ObjAt(normals, key.normal, fallbackNormal);
                    vertex.uv = ObjAt(texcoords, key.texcoord, fallbackUv);
                    const uint32_t newIndex = static_cast<uint32_t>(outMesh.vertices.size());
                    outMesh.vertices.push_back(vertex);
                    vertexMap.emplace(key, newIndex);
                    faceIndices.push_back(newIndex);
                }
                else
                {
                    faceIndices.push_back(found->second);
                }
            }

            for (size_t i = 1; i + 1 < faceIndices.size(); ++i)
            {
                outMesh.indices.push_back(faceIndices[0]);
                outMesh.indices.push_back(faceIndices[i]);
                outMesh.indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    return !outMesh.vertices.empty() && !outMesh.indices.empty();
}

Mesh CreateCubeMesh()
{
    Mesh mesh;
    const std::array<Vertex, 24> vertices = {
        Vertex{{-1, -1, -1}, {0, 0, -1}, {0, 1}}, Vertex{{-1, 1, -1}, {0, 0, -1}, {0, 0}}, Vertex{{1, 1, -1}, {0, 0, -1}, {1, 0}}, Vertex{{1, -1, -1}, {0, 0, -1}, {1, 1}},
        Vertex{{1, -1, 1}, {0, 0, 1}, {0, 1}}, Vertex{{1, 1, 1}, {0, 0, 1}, {0, 0}}, Vertex{{-1, 1, 1}, {0, 0, 1}, {1, 0}}, Vertex{{-1, -1, 1}, {0, 0, 1}, {1, 1}},
        Vertex{{-1, -1, 1}, {-1, 0, 0}, {0, 1}}, Vertex{{-1, 1, 1}, {-1, 0, 0}, {0, 0}}, Vertex{{-1, 1, -1}, {-1, 0, 0}, {1, 0}}, Vertex{{-1, -1, -1}, {-1, 0, 0}, {1, 1}},
        Vertex{{1, -1, -1}, {1, 0, 0}, {0, 1}}, Vertex{{1, 1, -1}, {1, 0, 0}, {0, 0}}, Vertex{{1, 1, 1}, {1, 0, 0}, {1, 0}}, Vertex{{1, -1, 1}, {1, 0, 0}, {1, 1}},
        Vertex{{-1, 1, -1}, {0, 1, 0}, {0, 1}}, Vertex{{-1, 1, 1}, {0, 1, 0}, {0, 0}}, Vertex{{1, 1, 1}, {0, 1, 0}, {1, 0}}, Vertex{{1, 1, -1}, {0, 1, 0}, {1, 1}},
        Vertex{{-1, -1, 1}, {0, -1, 0}, {0, 1}}, Vertex{{-1, -1, -1}, {0, -1, 0}, {0, 0}}, Vertex{{1, -1, -1}, {0, -1, 0}, {1, 0}}, Vertex{{1, -1, 1}, {0, -1, 0}, {1, 1}},
    };

    const std::array<uint32_t, 36> indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23,
    };

    mesh.vertices.assign(vertices.begin(), vertices.end());
    mesh.indices.assign(indices.begin(), indices.end());
    return mesh;
}

Mesh CreatePlaneMesh()
{
    Mesh mesh;
    mesh.vertices = {
        {{-4.0f, 0.0f, -4.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 4.0f}},
        {{-4.0f, 0.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{4.0f, 0.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, {4.0f, 0.0f}},
        {{4.0f, 0.0f, -4.0f}, {0.0f, 1.0f, 0.0f}, {4.0f, 4.0f}},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

class App
{
public:
    int Run(HINSTANCE instance, int showCommand)
    {
        try
        {
            CreateAppWindow(instance, showCommand);
            InitializeD3D();
            LoadAssets();
            BuildScene();
            MainLoop();
            return 0;
        }
        catch (const std::exception& error)
        {
            const std::wstring message = Utf8ToWide(error.what());
            MessageBoxW(m_hwnd, message.c_str(), L"Y-Render Error", MB_ICONERROR | MB_OK);
            return -1;
        }
    }

private:
    HWND m_hwnd = nullptr;
    UINT m_width = kInitialWidth;
    UINT m_height = kInitialHeight;
    bool m_running = true;
    bool m_keys[256]{};
    bool m_wireframe = false;
    int m_postMode = 0;
    int m_demo = 0;

    XMFLOAT3 m_cameraPosition{0.0f, 2.0f, -6.0f};
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

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        App* app = nullptr;
        if (message == WM_NCCREATE)
        {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            app = static_cast<App*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        else
        {
            app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (app)
        {
            return app->HandleMessage(hwnd, message, wParam, lParam);
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_DESTROY:
            m_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (m_device && wParam != SIZE_MINIMIZED)
            {
                m_width = std::max<UINT>(1, LOWORD(lParam));
                m_height = std::max<UINT>(1, HIWORD(lParam));
                ResizeTargets();
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

    void OnKeyPressed(UINT key)
    {
        if (key == VK_ESCAPE)
        {
            DestroyWindow(m_hwnd);
        }
        else if (key == VK_F1)
        {
            m_wireframe = !m_wireframe;
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

    void CreateAppWindow(HINSTANCE instance, int showCommand)
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

    void InitializeD3D()
    {
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

        m_cameraBuffer = CreateConstantBuffer<CameraConstants>(m_device.Get());
        m_materialBuffer = CreateConstantBuffer<MaterialConstants>(m_device.Get());
        m_postBuffer = CreateConstantBuffer<PostConstants>(m_device.Get());
    }

    void CreateBackBufferAndDepth()
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

    void ResizeTargets()
    {
        if (!m_swapChain)
        {
            return;
        }

        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_backBufferRtv.Reset();
        m_depthDsv.Reset();
        m_depthTexture.Reset();
        m_sceneTarget = {};

        ThrowIfFailed(m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0), "Resize swap chain buffers");
        CreateBackBufferAndDepth();
    }

    void CreateSceneTarget()
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

    void CreateCommonStates()
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

    void LoadAssets()
    {
        m_standardShader = CreateStandardShader();
        m_postShader = CreatePostShader();

        m_cubeMesh = CreateCubeMesh();
        m_planeMesh = CreatePlaneMesh();
        if (!LoadObj(FindAsset(L"assets\\models\\sample.obj"), m_objMesh))
        {
            m_objMesh = CreateCubeMesh();
        }

        UploadMesh(m_cubeMesh);
        UploadMesh(m_planeMesh);
        UploadMesh(m_objMesh);

        m_checkerTexture = CreateCheckerTexture(128, 128);
        CreatePostQuad();
    }

    ShaderProgram CreateStandardShader()
    {
        ShaderProgram program;
        const std::wstring shaderPath = FindAsset(L"assets\\shaders\\Standard.hlsl");
        ComPtr<ID3DBlob> vs = CompileShader(shaderPath, "VSMain", "vs_5_0");
        ComPtr<ID3DBlob> ps = CompileShader(shaderPath, "PSMain", "ps_5_0");

        ThrowIfFailed(m_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &program.vertexShader), "Create standard VS");
        ThrowIfFailed(m_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &program.pixelShader), "Create standard PS");

        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        ThrowIfFailed(m_device->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), vs->GetBufferPointer(), vs->GetBufferSize(), &program.inputLayout), "Create standard input layout");

        return program;
    }

    ShaderProgram CreatePostShader()
    {
        ShaderProgram program;
        const std::wstring shaderPath = FindAsset(L"assets\\shaders\\PostProcess.hlsl");
        ComPtr<ID3DBlob> vs = CompileShader(shaderPath, "VSMain", "vs_5_0");
        ComPtr<ID3DBlob> ps = CompileShader(shaderPath, "PSMain", "ps_5_0");

        ThrowIfFailed(m_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &program.vertexShader), "Create post VS");
        ThrowIfFailed(m_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &program.pixelShader), "Create post PS");

        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(PostVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(PostVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        ThrowIfFailed(m_device->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), vs->GetBufferPointer(), vs->GetBufferSize(), &program.inputLayout), "Create post input layout");

        return program;
    }

    void UploadMesh(Mesh& mesh)
    {
        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData{};
        vbData.pSysMem = mesh.vertices.data();
        ThrowIfFailed(m_device->CreateBuffer(&vbDesc, &vbData, &mesh.vertexBuffer), "Create mesh vertex buffer");

        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData{};
        ibData.pSysMem = mesh.indices.data();
        ThrowIfFailed(m_device->CreateBuffer(&ibDesc, &ibData, &mesh.indexBuffer), "Create mesh index buffer");
    }

    Texture CreateCheckerTexture(UINT width, UINT height)
    {
        std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
        for (UINT y = 0; y < height; ++y)
        {
            for (UINT x = 0; x < width; ++x)
            {
                const bool bright = ((x / 16) + (y / 16)) % 2 == 0;
                const uint8_t r = bright ? 235 : 68;
                const uint8_t g = bright ? 235 : 86;
                const uint8_t b = bright ? 225 : 112;
                pixels[static_cast<size_t>(y) * width + x] = 0xff000000u | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | r;
            }
        }

        return CreateTextureFromRgba(width, height, pixels.data(), width * sizeof(uint32_t));
    }

    Texture CreateTextureFromRgba(UINT width, UINT height, const void* pixels, UINT pitch)
    {
        Texture result;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = pixels;
        data.SysMemPitch = pitch;

        ThrowIfFailed(m_device->CreateTexture2D(&desc, &data, &result.texture), "Create texture");
        ThrowIfFailed(m_device->CreateShaderResourceView(result.texture.Get(), nullptr, &result.srv), "Create texture SRV");
        return result;
    }

    Texture LoadTextureWic(const std::wstring& path)
    {
        ComPtr<IWICImagingFactory> factory;
        ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)), "Create WIC factory");

        ComPtr<IWICBitmapDecoder> decoder;
        ThrowIfFailed(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder), "Create WIC decoder");

        ComPtr<IWICBitmapFrameDecode> frame;
        ThrowIfFailed(decoder->GetFrame(0, &frame), "Get WIC frame");

        ComPtr<IWICFormatConverter> converter;
        ThrowIfFailed(factory->CreateFormatConverter(&converter), "Create WIC converter");
        ThrowIfFailed(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom), "Initialize WIC converter");

        UINT width = 0;
        UINT height = 0;
        ThrowIfFailed(converter->GetSize(&width, &height), "Get WIC texture size");

        std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
        ThrowIfFailed(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()), "Copy WIC pixels");

        return CreateTextureFromRgba(width, height, pixels.data(), width * 4);
    }

    void CreatePostQuad()
    {
        const std::array<PostVertex, 6> vertices = {
            PostVertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            PostVertex{{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            PostVertex{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            PostVertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            PostVertex{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            PostVertex{{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        };

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(PostVertex));
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = vertices.data();
        ThrowIfFailed(m_device->CreateBuffer(&desc, &data, &m_postVertexBuffer), "Create fullscreen quad vertex buffer");
    }

    void BuildScene()
    {
        m_sceneObjects.clear();

        if (m_demo == 0)
        {
            SceneObject floor;
            floor.mesh = &m_planeMesh;
            floor.material.baseColor = XMFLOAT4(0.72f, 0.72f, 0.68f, 1.0f);
            floor.material.specularPower = 18.0f;
            floor.transform.position = XMFLOAT3(0.0f, -1.0f, 0.0f);
            m_sceneObjects.push_back(floor);

            SceneObject cube;
            cube.mesh = &m_cubeMesh;
            cube.material.baseColor = XMFLOAT4(0.85f, 0.65f, 0.32f, 1.0f);
            cube.transform.position = XMFLOAT3(0.0f, 0.2f, 0.0f);
            cube.transform.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
            m_sceneObjects.push_back(cube);
        }
        else
        {
            SceneObject obj;
            obj.mesh = &m_objMesh;
            obj.material.baseColor = XMFLOAT4(0.45f, 0.74f, 0.95f, 1.0f);
            obj.material.specularPower = 72.0f;
            obj.transform.position = XMFLOAT3(0.0f, -0.7f, 0.0f);
            obj.transform.scale = XMFLOAT3(1.4f, 1.4f, 1.4f);
            m_sceneObjects.push_back(obj);
        }
    }

    void MainLoop()
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

    void Update(float dt)
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

        if (m_demo == 0 && m_sceneObjects.size() > 1)
        {
            m_sceneObjects[1].transform.rotation.y = m_time * 0.75f;
        }
        if (m_demo == 1 && !m_sceneObjects.empty())
        {
            m_sceneObjects[0].transform.rotation.y = m_time * 0.55f;
        }
    }

    void UpdateWindowTitle()
    {
        static const wchar_t* postNames[] = {L"None", L"Grayscale", L"Invert", L"Vignette"};
        std::wstringstream title;
        title << L"Y-Render | FPS " << static_cast<int>(m_currentFps)
              << L" | Demo " << m_demo
              << L" | Post " << postNames[std::clamp(m_postMode, 0, 3)]
              << L" | " << (m_wireframe ? L"Wireframe" : L"Solid");
        SetWindowTextW(m_hwnd, title.str().c_str());
    }

    XMMATRIX CameraViewProjection() const
    {
        const XMVECTOR position = XMLoadFloat3(&m_cameraPosition);
        const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
            std::cos(m_cameraPitch) * std::sin(m_cameraYaw),
            std::sin(m_cameraPitch),
            std::cos(m_cameraPitch) * std::cos(m_cameraYaw),
            0.0f));
        const XMMATRIX view = XMMatrixLookToLH(position, forward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        const float aspect = static_cast<float>(m_width) / static_cast<float>(std::max<UINT>(1, m_height));
        const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.05f, 200.0f);
        return view * projection;
    }

    void Render()
    {
        ID3D11ShaderResourceView* nullSrvs[] = {nullptr};
        m_context->PSSetShaderResources(0, 1, nullSrvs);

        const FLOAT sceneClear[] = {0.08f, 0.10f, 0.13f, 1.0f};
        m_context->OMSetRenderTargets(1, m_sceneTarget.rtv.GetAddressOf(), m_depthDsv.Get());
        m_context->ClearRenderTargetView(m_sceneTarget.rtv.Get(), sceneClear);
        m_context->ClearDepthStencilView(m_depthDsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(m_width);
        viewport.Height = static_cast<float>(m_height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &viewport);
        m_context->RSSetState(m_wireframe ? m_wireRasterizer.Get() : m_solidRasterizer.Get());

        RenderSceneObjects();
        RenderPostProcess();

        m_swapChain->Present(1, 0);
    }

    void RenderSceneObjects()
    {
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->IASetInputLayout(m_standardShader.inputLayout.Get());
        m_context->VSSetShader(m_standardShader.vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_standardShader.pixelShader.Get(), nullptr, 0);
        m_context->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());
        m_context->VSSetConstantBuffers(0, 1, m_cameraBuffer.GetAddressOf());
        m_context->PSSetConstantBuffers(0, 1, m_cameraBuffer.GetAddressOf());
        m_context->PSSetConstantBuffers(1, 1, m_materialBuffer.GetAddressOf());

        const XMMATRIX viewProj = CameraViewProjection();

        for (const SceneObject& object : m_sceneObjects)
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
            m_context->UpdateSubresource(m_cameraBuffer.Get(), 0, nullptr, &cameraConstants, 0, 0);

            MaterialConstants materialConstants;
            materialConstants.baseColor = object.material.baseColor;
            materialConstants.specularPower = object.material.specularPower;
            materialConstants.useTexture = object.material.useTexture ? 1.0f : 0.0f;
            m_context->UpdateSubresource(m_materialBuffer.Get(), 0, nullptr, &materialConstants, 0, 0);

            ID3D11ShaderResourceView* srv = m_checkerTexture.srv.Get();
            m_context->PSSetShaderResources(0, 1, &srv);

            const UINT stride = sizeof(Vertex);
            const UINT offset = 0;
            ID3D11Buffer* vertexBuffer = object.mesh->vertexBuffer.Get();
            m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            m_context->IASetIndexBuffer(object.mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            m_context->DrawIndexed(static_cast<UINT>(object.mesh->indices.size()), 0, 0);
        }
    }

    void RenderPostProcess()
    {
        ID3D11ShaderResourceView* nullSrvs[] = {nullptr};
        m_context->PSSetShaderResources(0, 1, nullSrvs);

        const FLOAT clear[] = {0.0f, 0.0f, 0.0f, 1.0f};
        m_context->OMSetRenderTargets(1, m_backBufferRtv.GetAddressOf(), nullptr);
        m_context->ClearRenderTargetView(m_backBufferRtv.Get(), clear);
        m_context->RSSetState(m_solidRasterizer.Get());

        PostConstants constants;
        constants.mode = static_cast<float>(m_postMode);
        constants.time = m_time;
        constants.invResolution = XMFLOAT2(1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height));
        m_context->UpdateSubresource(m_postBuffer.Get(), 0, nullptr, &constants, 0, 0);

        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->IASetInputLayout(m_postShader.inputLayout.Get());
        const UINT stride = sizeof(PostVertex);
        const UINT offset = 0;
        ID3D11Buffer* buffer = m_postVertexBuffer.Get();
        m_context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

        m_context->VSSetShader(m_postShader.vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_postShader.pixelShader.Get(), nullptr, 0);
        m_context->PSSetConstantBuffers(0, 1, m_postBuffer.GetAddressOf());
        m_context->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());
        ID3D11ShaderResourceView* sceneSrv = m_sceneTarget.srv.Get();
        m_context->PSSetShaderResources(0, 1, &sceneSrv);
        m_context->Draw(6, 0);

        m_context->PSSetShaderResources(0, 1, nullSrvs);
    }
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx");
    App app;
    const int result = app.Run(instance, showCommand);
    CoUninitialize();
    return result;
}
