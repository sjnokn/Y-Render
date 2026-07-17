#pragma once

#include "Scene/SceneTypes.h"

#include <DirectXMath.h>

#include <random>
#include <vector>

namespace YRender
{
class DissolveParticleSystem
{
public:
    ~DissolveParticleSystem();

    void Initialize();
    void Reset();
    void Update(float deltaSeconds, const SceneObject* source, bool effectsEnabled);
    int Render(
        const ShaderProgram& shader,
        const DirectX::XMMATRIX& viewProjection,
        float viewportHeight,
        float time);

private:
    struct Particle
    {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 velocity{};
        DirectX::XMFLOAT4 color{};
        float age = 0.0f;
        float lifetime = 1.0f;
        float size = 0.05f;
        float seed = 0.0f;
    };

    float Random01();
    void Spawn(const SceneObject& source, int mode);
    float CurrentAmount(const Material& material) const;

    std::vector<Particle> m_particles;
    std::vector<Vertex> m_gpuVertices;
    std::mt19937 m_random{0x59444C56u};
    unsigned int m_vao = 0;
    unsigned int m_vertexBuffer = 0;
    DirectX::XMFLOAT4X4 m_sourceWorld{};
    float m_spawnAccumulator = 0.0f;
    float m_previousAmount = 0.0f;
    bool m_amountInitialized = false;
    bool m_increasing = true;
    int m_activeMode = -1;
};
} // namespace YRender
