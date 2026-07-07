#include "Assets/ResourceManager.h"

#include "Core/Platform.h"

#include <d3dcompiler.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace YRender
{
void ResourceManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    m_device = device;
    m_context = context;
}

ShaderProgram ResourceManager::CreateStandardShader()
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

ShaderProgram ResourceManager::CreatePostShader()
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

void ResourceManager::UploadMesh(Mesh& mesh)
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

Texture ResourceManager::CreateCheckerTexture(UINT width, UINT height)
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

Texture ResourceManager::CreateTextureFromRgba(UINT width, UINT height, const void* pixels, UINT pitch)
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

Texture ResourceManager::LoadTextureWic(const std::wstring& path)
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

ComPtr<ID3D11Buffer> ResourceManager::CreatePostQuad()
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

    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(m_device->CreateBuffer(&desc, &data, &buffer), "Create fullscreen quad vertex buffer");
    return buffer;
}

ComPtr<ID3D11Buffer> ResourceManager::CreateConstantBuffer(UINT byteWidth)
{
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = (byteWidth + 15) & ~15u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(m_device->CreateBuffer(&desc, nullptr, &buffer), "Create constant buffer");
    return buffer;
}

ComPtr<ID3DBlob> ResourceManager::CompileShader(const std::wstring& path, const char* entry, const char* target)
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
} // namespace YRender
