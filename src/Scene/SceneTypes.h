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
    DirectX::XMFLOAT4 lightDirection{-0.35f, -0.85f, 0.35f, 0.0f};
    DirectX::XMFLOAT4 lightColor{1.0f, 0.95f, 0.85f, 1.0f};
    DirectX::XMFLOAT4 ambientColor{0.12f, 0.14f, 0.18f, 1.0f};
    float specularPower = 48.0f;
    float useTexture = 1.0f;
    DirectX::XMFLOAT2 padding{};
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

struct Material
{
    DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
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
} // namespace YRender
