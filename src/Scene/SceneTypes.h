#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

namespace YRender
{
struct Vertex
{
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 normal{};
    DirectX::XMFLOAT2 uv{};
    DirectX::XMFLOAT4 color{1.0f, 1.0f, 1.0f, 1.0f};
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

struct EmbeddedImage
{
    std::string name;
    std::string mimeType;
    std::vector<uint8_t> bytes;
    unsigned int texture = 0;
    unsigned int width = 0;
    unsigned int height = 0;
};

struct ImportedMaterial
{
    std::string name;
    std::string alphaMode = "OPAQUE";
    DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    int baseColorImage = -1;
    bool alphaBlend = false;
    bool alphaMask = false;
    bool unlit = false;
    float alphaCutoff = 0.5f;
};

struct SubMesh
{
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
    int materialIndex = -1;
};

struct Material
{
    DirectX::XMFLOAT4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT4 ambientColor{0.12f, 0.14f, 0.18f, 1.0f};
    DirectX::XMFLOAT4 rimColor{0.55f, 0.85f, 1.0f, 1.0f};
    DirectX::XMFLOAT4 shadowColor{0.18f, 0.20f, 0.32f, 1.0f};
    float diffuseIntensity = 1.0f;
    float specularIntensity = 0.35f;
    float specularPower = 48.0f;
    float toonSteps = 3.0f;
    float rimPower = 3.0f;
    float rimIntensity = 0.45f;
    int lightingModel = 0;
    int surfaceEffect = 0;
    float dissolveAmount = 0.0f;
    float dissolveNoiseScale = 3.0f;
    float dissolveEdgeWidth = 0.08f;
    float dissolveSpeed = 0.20f;
    float dissolveEdgeIntensity = 2.5f;
    float dissolveProgressSpeed = 0.16f;
    bool dissolveAutoProgress = false;
    DirectX::XMFLOAT4 dissolveEdgeColor{1.0f, 0.24f, 0.03f, 1.0f};
    DirectX::XMFLOAT2 uvTiling{1.0f, 1.0f};
    DirectX::XMFLOAT2 uvOffset{0.0f, 0.0f};
    bool useAlbedoTexture = true;
    bool preserveImportedUnlit = false;
    Texture* albedoTexture = nullptr;
};

inline void ApplyDissolvePreset(Material& material)
{
    material.surfaceEffect = 1;
    material.dissolveAmount = 0.20f;
    material.dissolveNoiseScale = 3.2f;
    material.dissolveEdgeWidth = 0.055f;
    material.dissolveSpeed = 0.16f;
    material.dissolveEdgeIntensity = 1.35f;
    material.dissolveProgressSpeed = 0.16f;
    material.dissolveAutoProgress = true;
    material.dissolveEdgeColor = DirectX::XMFLOAT4(0.18f, 0.76f, 1.0f, 1.0f);
}

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
    std::vector<SubMesh> submeshes;
    std::vector<ImportedMaterial> importedMaterials;
    std::vector<EmbeddedImage> embeddedImages;
    std::string importNotes;
    std::string sourcePath;
    std::string displayName = "Mesh";
    int gltfNodeCount = 0;
    int gltfMeshNodeCount = 0;
    int gltfSkinCount = 0;
    DirectX::XMFLOAT3 boundsMin{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 boundsMax{0.0f, 0.0f, 0.0f};
    bool hasNormals = false;
    bool hasTexcoords = false;
    bool normalizedForPreview = false;
    unsigned int vao = 0;
    unsigned int vertexBuffer = 0;
    unsigned int indexBuffer = 0;
};

struct Texture
{
    unsigned int id = 0;
    unsigned int width = 0;
    unsigned int height = 0;
};

struct ShaderProgram
{
    unsigned int program = 0;
};

struct RenderTarget
{
    unsigned int framebuffer = 0;
    unsigned int colorTexture = 0;
    unsigned int depthTexture = 0;
};

struct SceneObject
{
    std::string name;
    Mesh* mesh = nullptr;
    Transform transform;
    Material material;
};
} // namespace YRender
