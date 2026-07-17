#pragma once

#include "Scene/SceneTypes.h"

#include <DirectXMath.h>

#include <array>
#include <vector>

namespace YRender
{
class BurnFragmentSystem
{
public:
    ~BurnFragmentSystem();

    void Initialize();
    void Reset();
    void Update(float deltaSeconds, const SceneObject* source, bool effectsEnabled);
    int Render(
        const ShaderProgram& shader,
        const DirectX::XMMATRIX& viewProjection,
        const DirectX::XMFLOAT3& cameraPosition,
        float time);

private:
    struct SourceTriangle
    {
        std::array<DirectX::XMFLOAT3, 3> positions{};
        std::array<DirectX::XMFLOAT3, 3> normals{};
        DirectX::XMFLOAT3 center{};
        DirectX::XMFLOAT3 normal{};
        DirectX::XMFLOAT3 rotationAxis{};
        float seed = 0.0f;
        float burnNoise = 0.5f;
        bool emitted = false;
    };

    struct AshFragment
    {
        std::array<DirectX::XMFLOAT3, 3> basis{};
        DirectX::XMFLOAT3 faceNormal{};
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 velocity{};
        DirectX::XMFLOAT3 angularVelocity{};
        DirectX::XMFLOAT3 radii{0.01f, 0.01f, 0.01f};
        DirectX::XMFLOAT4 orientation{0.0f, 0.0f, 0.0f, 1.0f};
        float age = 0.0f;
        float lifetime = 2.0f;
        float attachedTime = 0.08f;
        float buoyancy = 0.2f;
        float flutter = 1.0f;
        float seed = 0.0f;
        int shapeKind = 0;
    };

    struct FragmentVertex
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 normal{};
        DirectX::XMFLOAT2 uv{};
        DirectX::XMFLOAT4 color{};
        DirectX::XMFLOAT3 barycentric{};
    };

    void RebuildSource(const Mesh& mesh, float noiseScale);
    void SpawnFragment(
        const SourceTriangle& source,
        const Material& material);
    void Simulate(float deltaSeconds, const Material& material, float time);
    void BuildGpuVertices();
    float CurrentAmount(const Material& material) const;

    const Mesh* m_sourceMesh = nullptr;
    std::vector<SourceTriangle> m_sourceTriangles;
    std::vector<AshFragment> m_activeFragments;
    std::vector<FragmentVertex> m_gpuVertices;
    unsigned int m_vao = 0;
    unsigned int m_vertexBuffer = 0;
    DirectX::XMFLOAT4X4 m_sourceWorld{};
    DirectX::XMFLOAT4X4 m_sourceNormalMatrix{};
    float m_sourceNoiseScale = -1.0f;
    float m_previousAmount = 0.0f;
    bool m_amountInitialized = false;
    bool m_increasing = true;
};
} // namespace YRender
