#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace YRender
{
using Microsoft::WRL::ComPtr;

struct Vertex
{
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 normal{};
    DirectX::XMFLOAT2 uv{};
};

struct PostVertex
{
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT2 uv{};
};

struct CameraConstants
{
    DirectX::XMFLOAT4X4 world{};
    DirectX::XMFLOAT4X4 viewProj{};
    DirectX::XMFLOAT3 cameraPosition{};
    float time = 0.0f;
};

struct MaterialConstants
{
    DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT4 ambientColor{0.12f, 0.14f, 0.18f, 1.0f};
    DirectX::XMFLOAT4 lightDirections[4]{
        {-0.35f, -0.85f, 0.35f, 0.0f},
        {0.65f, -0.45f, -0.35f, 0.0f},
        {0.0f, -1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 0.0f, 0.0f},
    };
    DirectX::XMFLOAT4 lightColors[4]{
        {1.0f, 0.95f, 0.85f, 1.0f},
        {0.25f, 0.35f, 0.55f, 0.45f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    };
    float specularPower = 48.0f;
    float useTexture = 1.0f;
    float useNormalTexture = 0.0f;
    float useSpecularTexture = 0.0f;
    int lightCount = 2;
    DirectX::XMFLOAT3 padding{};
};

struct PostConstants
{
    float mode = 0.0f;
    float time = 0.0f;
    DirectX::XMFLOAT2 invResolution{};
};

struct Transform
{
    DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 rotation{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};

    DirectX::XMMATRIX Matrix() const
    {
        return DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
            DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) *
            DirectX::XMMatrixTranslation(position.x, position.y, position.z);
    }
};

struct Texture;

struct Material
{
    DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    float specularPower = 48.0f;
    bool useAlbedoTexture = true;
    bool useNormalTexture = false;
    bool useSpecularTexture = false;
    Texture* albedoTexture = nullptr;
    Texture* normalTexture = nullptr;
    Texture* specularTexture = nullptr;
};

struct DirectionalLight
{
    DirectX::XMFLOAT3 direction{-0.35f, -0.85f, 0.35f};
    float intensity = 1.0f;
    DirectX::XMFLOAT3 color{1.0f, 0.95f, 0.85f};
    float padding = 0.0f;
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
} // namespace YRender
