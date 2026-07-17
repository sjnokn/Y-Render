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
    int dissolveMode = 0;
    float dissolveAmount = 0.0f;
    float dissolveNoiseScale = 3.0f;
    float dissolveEdgeWidth = 0.08f;
    float dissolveSpeed = 0.20f;
    float dissolveEdgeIntensity = 2.5f;
    float dissolveProgressSpeed = 0.16f;
    bool dissolveAutoProgress = false;
    bool dissolvePaused = false;
    float dissolvePlaybackTime = 0.0f;
    DirectX::XMFLOAT4 dissolveEdgeColor{1.0f, 0.24f, 0.03f, 1.0f};
    DirectX::XMFLOAT4 dissolveSecondaryColor{1.0f, 0.85f, 0.35f, 1.0f};
    DirectX::XMFLOAT3 dissolveDirection{0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT3 dissolveCenter{0.5f, 0.5f, 0.5f};
    float dissolveNoiseInfluence = 0.15f;
    float dissolveFlowStrength = 0.38f;
    float dissolvePixelSize = 0.09f;
    float dissolveParticleRate = 70.0f;
    float dissolveParticleSize = 0.055f;
    float dissolveParticleLifetime = 1.6f;
    DirectX::XMFLOAT3 dissolveParticleWind{0.18f, 0.42f, 0.04f};
    DirectX::XMFLOAT2 uvTiling{1.0f, 1.0f};
    DirectX::XMFLOAT2 uvOffset{0.0f, 0.0f};
    bool useAlbedoTexture = true;
    bool preserveImportedUnlit = false;
    Texture* albedoTexture = nullptr;
};

inline void ApplyDissolvePreset(Material& material, int mode = 0)
{
    material.surfaceEffect = 1;
    material.dissolveMode = mode;
    material.dissolveAmount = 0.35f;
    material.dissolveNoiseScale = 2.8f;
    material.dissolveEdgeWidth = 0.075f;
    material.dissolveSpeed = 0.0f;
    material.dissolveEdgeIntensity = 0.9f;
    material.dissolveProgressSpeed = 0.12f;
    material.dissolveAutoProgress = true;
    material.dissolvePaused = false;
    material.dissolvePlaybackTime = 0.0f;
    material.dissolveEdgeColor = DirectX::XMFLOAT4(1.0f, 0.42f, 0.08f, 1.0f);
    material.dissolveSecondaryColor = DirectX::XMFLOAT4(1.0f, 0.86f, 0.34f, 1.0f);
    material.dissolveDirection = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
    material.dissolveCenter = DirectX::XMFLOAT3(0.5f, 0.5f, 0.5f);
    material.dissolveNoiseInfluence = 0.15f;
    material.dissolveFlowStrength = 0.38f;
    material.dissolvePixelSize = 0.09f;
    material.dissolveParticleRate = 70.0f;
    material.dissolveParticleSize = 0.055f;
    material.dissolveParticleLifetime = 1.6f;
    material.dissolveParticleWind = DirectX::XMFLOAT3(0.18f, 0.42f, 0.04f);

    switch (mode)
    {
    case 1: // Height / directional
        material.dissolveNoiseScale = 4.2f;
        material.dissolveNoiseInfluence = 0.12f;
        material.dissolveEdgeWidth = 0.055f;
        material.dissolveEdgeColor = DirectX::XMFLOAT4(0.18f, 0.72f, 1.0f, 1.0f);
        material.dissolveSecondaryColor = DirectX::XMFLOAT4(0.78f, 0.95f, 1.0f, 1.0f);
        break;
    case 2: // Local sphere
        material.dissolveNoiseScale = 5.0f;
        material.dissolveNoiseInfluence = 0.10f;
        material.dissolveEdgeWidth = 0.065f;
        material.dissolveEdgeColor = DirectX::XMFLOAT4(0.50f, 0.22f, 1.0f, 1.0f);
        material.dissolveSecondaryColor = DirectX::XMFLOAT4(0.95f, 0.72f, 1.0f, 1.0f);
        break;
    case 3: // Incineration / ash
        material.dissolveNoiseScale = 3.5f;
        material.dissolveNoiseInfluence = 0.42f;
        material.dissolveEdgeWidth = 0.050f;
        material.dissolveEdgeIntensity = 1.95f;
        material.dissolveSpeed = 0.11f;
        material.dissolveProgressSpeed = 0.085f;
        material.dissolveEdgeColor = DirectX::XMFLOAT4(1.0f, 0.075f, 0.006f, 1.0f);
        material.dissolveSecondaryColor = DirectX::XMFLOAT4(1.0f, 0.86f, 0.24f, 1.0f);
        material.dissolveParticleRate = 138.0f;
        material.dissolveParticleSize = 0.070f;
        material.dissolveParticleLifetime = 1.55f;
        material.dissolveParticleWind = DirectX::XMFLOAT3(0.0f, 0.13f, 0.0f);
        break;
    case 4: // FlowMap energy
        material.dissolveNoiseScale = 4.5f;
        material.dissolveEdgeWidth = 0.09f;
        material.dissolveEdgeIntensity = 2.8f;
        material.dissolveSpeed = 0.28f;
        material.dissolveFlowStrength = 0.52f;
        material.dissolveEdgeColor = DirectX::XMFLOAT4(0.02f, 0.48f, 1.0f, 1.0f);
        material.dissolveSecondaryColor = DirectX::XMFLOAT4(0.30f, 1.0f, 0.92f, 1.0f);
        break;
    case 5: // Pixelated
        material.dissolveNoiseScale = 1.15f;
        material.dissolveNoiseInfluence = 0.24f;
        material.dissolveEdgeWidth = 0.12f;
        material.dissolveEdgeIntensity = 2.6f;
        material.dissolvePixelSize = 0.045f;
        material.dissolveEdgeColor = DirectX::XMFLOAT4(0.02f, 0.82f, 1.0f, 1.0f);
        material.dissolveSecondaryColor = DirectX::XMFLOAT4(0.96f, 1.0f, 1.0f, 1.0f);
        break;
    case 6: // Edge particles / ash
        material.dissolveNoiseScale = 4.0f;
        material.dissolveNoiseInfluence = 0.18f;
        material.dissolveEdgeWidth = 0.09f;
        material.dissolveEdgeColor = DirectX::XMFLOAT4(0.95f, 0.18f, 0.02f, 1.0f);
        material.dissolveSecondaryColor = DirectX::XMFLOAT4(1.0f, 0.65f, 0.08f, 1.0f);
        material.dissolveParticleRate = 90.0f;
        material.dissolveParticleSize = 0.045f;
        material.dissolveParticleLifetime = 1.45f;
        material.dissolveParticleWind = DirectX::XMFLOAT3(0.28f, 0.36f, 0.04f);
        break;
    case 7: // Smoke
        material.dissolveNoiseScale = 3.4f;
        material.dissolveNoiseInfluence = 0.22f;
        material.dissolveEdgeWidth = 0.12f;
        material.dissolveEdgeIntensity = 0.55f;
        material.dissolveEdgeColor = DirectX::XMFLOAT4(0.16f, 0.12f, 0.10f, 1.0f);
        material.dissolveSecondaryColor = DirectX::XMFLOAT4(0.72f, 0.32f, 0.12f, 1.0f);
        material.dissolveParticleRate = 38.0f;
        material.dissolveParticleSize = 0.16f;
        material.dissolveParticleLifetime = 2.6f;
        material.dissolveParticleWind = DirectX::XMFLOAT3(0.12f, 0.52f, 0.03f);
        break;
    default:
        break;
    }
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
