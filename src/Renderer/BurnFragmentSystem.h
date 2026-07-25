#pragma once

#include "Scene/SceneTypes.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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
        const glm::mat4& viewProjection,
        const glm::vec3& cameraPosition,
        float time);

private:
    struct SourceTriangle
    {
        std::array<glm::vec3, 3> positions{};
        std::array<glm::vec3, 3> normals{};
        glm::vec3 center{};
        glm::vec3 normal{};
        glm::vec3 rotationAxis{};
        float seed = 0.0f;
        float burnNoise = 0.5f;
        bool emitted = false;
    };

    struct AshFragment
    {
        std::array<glm::vec3, 3> basis{};
        glm::vec3 faceNormal{};
        glm::vec3 position{};
        glm::vec3 velocity{};
        glm::vec3 angularVelocity{};
        glm::vec3 radii{0.01f, 0.01f, 0.01f};
        glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
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
        glm::vec3 position{};
        glm::vec3 normal{};
        glm::vec2 uv{};
        glm::vec4 color{};
        glm::vec3 barycentric{};
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
    glm::mat4 m_sourceWorld{1.0f};
    glm::mat4 m_sourceNormalMatrix{1.0f};
    float m_sourceNoiseScale = -1.0f;
    float m_previousAmount = 0.0f;
    bool m_amountInitialized = false;
    bool m_increasing = true;
};
} // namespace YRender
