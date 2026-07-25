#include <glad/glad.h>

#include "Renderer/DissolveParticleSystem.h"

#include "Renderer/IncinerationField.h"
#include "Renderer/ShaderUniform.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace YRender
{
namespace
{
constexpr size_t kMaxParticles = 1200;
constexpr size_t kMaxIncinerationSamples = 140000;

float Length(const glm::vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

// glm::normalize on a zero-length vector returns NaN; preserve the old
// hand-written Normalize fallback semantics.
glm::vec3 Normalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length = Length(value);
    return length > 0.00001f ? value * (1.0f / length) : fallback;
}
} // namespace

DissolveParticleSystem::~DissolveParticleSystem()
{
    if (m_vertexBuffer != 0)
    {
        glDeleteBuffers(1, &m_vertexBuffer);
    }
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void DissolveParticleSystem::Initialize()
{
    if (m_vao != 0)
    {
        return;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vertexBuffer);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kMaxParticles * sizeof(Vertex)), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, color)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void DissolveParticleSystem::Reset()
{
    m_sourceWorld = glm::mat4(1.0f);
    m_particles.clear();
    m_surfaceSamples.clear();
    m_gpuVertices.clear();
    m_sourceMesh = nullptr;
    m_sourceNoiseScale = -1.0f;
    m_spawnAccumulator = 0.0f;
    m_previousAmount = 0.0f;
    m_amountInitialized = false;
    m_increasing = true;
    m_activeMode = -1;
}

float DissolveParticleSystem::Random01()
{
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(m_random);
}

float DissolveParticleSystem::CurrentAmount(const Material& material) const
{
    if (!material.dissolveAutoProgress)
    {
        return std::clamp(material.dissolveAmount, 0.0f, 1.0f);
    }

    const float cycle = std::fmod(
        material.dissolvePlaybackTime * std::max(material.dissolveProgressSpeed, 0.01f),
        1.0f);
    return cycle < 0.5f ? cycle * 2.0f : 2.0f - cycle * 2.0f;
}

void DissolveParticleSystem::RebuildIncinerationSource(const Mesh& mesh, float noiseScale)
{
    m_sourceMesh = &mesh;
    m_sourceNoiseScale = noiseScale;
    m_surfaceSamples.clear();

    const size_t triangleCount = mesh.indices.size() / 3;
    if (triangleCount == 0)
    {
        return;
    }

    const float diagonal = Length(mesh.boundsMax - mesh.boundsMin);
    const float targetSpacing = std::max(diagonal * 0.022f, 0.010f);
    m_surfaceSamples.reserve(std::min(triangleCount * 2, kMaxIncinerationSamples));

    const auto interpolatePosition = [](const std::array<glm::vec3, 3>& values, float b0, float b1, float b2)
    {
        return glm::vec3(
            values[0].x * b0 + values[1].x * b1 + values[2].x * b2,
            values[0].y * b0 + values[1].y * b1 + values[2].y * b2,
            values[0].z * b0 + values[1].z * b1 + values[2].z * b2);
    };

    const auto appendSample = [&](const std::array<glm::vec3, 3>& positions,
                                  const std::array<glm::vec3, 3>& normals,
                                  float b0,
                                  float b1,
                                  float b2,
                                  size_t sourceIndex)
    {
        if (m_surfaceSamples.size() >= kMaxIncinerationSamples)
        {
            return;
        }
        SurfaceSample sample;
        sample.position = interpolatePosition(positions, b0, b1, b2);
        sample.normal = Normalize(
            interpolatePosition(normals, b0, b1, b2),
            glm::vec3(0.0f, 1.0f, 0.0f));
        sample.burnNoise = IncinerationField::BurnNoise(sample.position, noiseScale);
        sample.seed = IncinerationField::Hash(
            sample.position.x * 37.0f + sample.position.z * 17.0f +
                static_cast<float>(sourceIndex % 8191) * 0.013f,
            sample.position.y * 53.0f + static_cast<float>(sourceIndex % 4093) * 0.019f);
        m_surfaceSamples.push_back(sample);
    };

    for (size_t triangleIndex = 0;
         triangleIndex < triangleCount && m_surfaceSamples.size() < kMaxIncinerationSamples;
         ++triangleIndex)
    {
        const uint32_t indices[3]{
            mesh.indices[triangleIndex * 3],
            mesh.indices[triangleIndex * 3 + 1],
            mesh.indices[triangleIndex * 3 + 2],
        };
        if (indices[0] >= mesh.vertices.size() ||
            indices[1] >= mesh.vertices.size() ||
            indices[2] >= mesh.vertices.size())
        {
            continue;
        }

        std::array<glm::vec3, 3> positions{};
        std::array<glm::vec3, 3> normals{};
        for (size_t corner = 0; corner < 3; ++corner)
        {
            positions[corner] = mesh.vertices[indices[corner]].position;
            normals[corner] = mesh.vertices[indices[corner]].normal;
        }
        const float maximumEdge = std::max({
            Length(positions[1] - positions[0]),
            Length(positions[2] - positions[1]),
            Length(positions[0] - positions[2]),
        });
        const int segments = std::clamp(
            static_cast<int>(std::ceil(maximumEdge / targetSpacing)),
            1,
            8);
        const float inverseSegments = 1.0f / static_cast<float>(segments);

        for (int row = 0;
             row < segments && m_surfaceSamples.size() < kMaxIncinerationSamples;
             ++row)
        {
            for (int column = 0;
                 column < segments - row && m_surfaceSamples.size() < kMaxIncinerationSamples;
                 ++column)
            {
                // Centroids of both sub-triangles distribute emitters across
                // the actual mesh surface instead of restricting them to the
                // original model vertices.
                const float lowerB1 = (static_cast<float>(row) + 1.0f / 3.0f) * inverseSegments;
                const float lowerB2 = (static_cast<float>(column) + 1.0f / 3.0f) * inverseSegments;
                appendSample(
                    positions,
                    normals,
                    1.0f - lowerB1 - lowerB2,
                    lowerB1,
                    lowerB2,
                    triangleIndex * 131 + static_cast<size_t>(row * 11 + column));

                if (row + column < segments - 1)
                {
                    const float upperB1 = (static_cast<float>(row) + 2.0f / 3.0f) * inverseSegments;
                    const float upperB2 = (static_cast<float>(column) + 2.0f / 3.0f) * inverseSegments;
                    appendSample(
                        positions,
                        normals,
                        1.0f - upperB1 - upperB2,
                        upperB1,
                        upperB2,
                        triangleIndex * 131 + static_cast<size_t>(row * 11 + column + 67));
                }
            }
        }
    }
}

void DissolveParticleSystem::Spawn(
    const SceneObject& source,
    int mode,
    const SurfaceSample* incinerationSample)
{
    if (!source.mesh || source.mesh->vertices.empty() || m_particles.size() >= kMaxParticles)
    {
        return;
    }

    const Mesh& mesh = *source.mesh;
    const Material& material = source.material;
    const glm::vec3 boundsSize{
        std::max(mesh.boundsMax.x - mesh.boundsMin.x, 0.0001f),
        std::max(mesh.boundsMax.y - mesh.boundsMin.y, 0.0001f),
        std::max(mesh.boundsMax.z - mesh.boundsMin.z, 0.0001f),
    };
    glm::vec3 direction = material.dissolveDirection;
    const float directionLength = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (directionLength < 0.001f)
    {
        direction = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        direction = direction * (1.0f / directionLength);
    }
    const float directionSpan = std::max(
        std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z),
        0.001f);
    const float amount = CurrentAmount(material);
    size_t bestIndex = 0;
    if (!incinerationSample)
    {
        bestIndex = static_cast<size_t>(Random01() * static_cast<float>(mesh.vertices.size()));
        bestIndex = std::min(bestIndex, mesh.vertices.size() - 1);
        float bestDistance = 10.0f;
        for (int attempt = 0; attempt < 28; ++attempt)
        {
            size_t index = static_cast<size_t>(Random01() * static_cast<float>(mesh.vertices.size()));
            index = std::min(index, mesh.vertices.size() - 1);
            const glm::vec3& position = mesh.vertices[index].position;
            const glm::vec3 local01{
                (position.x - mesh.boundsMin.x) / boundsSize.x,
                (position.y - mesh.boundsMin.y) / boundsSize.y,
                (position.z - mesh.boundsMin.z) / boundsSize.z,
            };
            const float field =
                ((local01.x - 0.5f) * direction.x +
                 (local01.y - 0.5f) * direction.y +
                 (local01.z - 0.5f) * direction.z) /
                    directionSpan +
                0.5f;
            const float distance = std::abs(field - amount);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = index;
            }
        }
    }

    glm::vec3 position;
    glm::vec3 normal;
    if (incinerationSample)
    {
        position = incinerationSample->position;
        normal = incinerationSample->normal;
    }
    else
    {
        const Vertex& sourceVertex = mesh.vertices[bestIndex];
        const glm::mat4 world = source.transform.Matrix();
        position = glm::vec3(world * glm::vec4(sourceVertex.position, 1.0f));
        // intentionally preserves pre-existing bug: transforms the normal by
        // `world` directly instead of the inverse-transpose. Kept bug-for-bug
        // during the GLM migration; fix deferred.
        normal = Normalize(
            glm::vec3(glm::mat3(world) * sourceVertex.normal),
            glm::vec3(0.0f, 1.0f, 0.0f));
    }

    Particle particle;
    particle.seed = incinerationSample
        ? IncinerationField::Fract(incinerationSample->seed * 1.37f + Random01() * 0.31f)
        : Random01();
    particle.age = 0.0f;
    particle.lifetime = material.dissolveParticleLifetime * (0.72f + Random01() * 0.56f);
    particle.size = material.dissolveParticleSize * (0.65f + Random01() * 0.75f);
    particle.position = position + normal * (0.006f + Random01() * 0.016f);

    const float lateralSpread = mode == 3 ? 0.105f : 0.34f;
    const glm::vec3 randomDrift{
        (Random01() - 0.5f) * lateralSpread,
        (Random01() - 0.5f) * (mode == 3 ? 0.10f : 0.18f),
        (Random01() - 0.5f) * lateralSpread,
    };
    float normalImpulse = 0.04f + Random01() * 0.10f;
    if (mode == 3)
    {
        normalImpulse = 0.035f + Random01() * 0.085f;
    }
    else if (mode == 6)
    {
        normalImpulse = 0.16f + Random01() * 0.32f;
    }
    particle.velocity = material.dissolveParticleWind + randomDrift + normal * normalImpulse;

    if (mode == 3)
    {
        particle.attachedTime = 0.025f + Random01() * 0.055f;
        const float kind = particle.seed;
        if (kind < 0.15f)
        {
            // Tiny, short-lived sparks provide highlights without turning the
            // entire ash stream orange.
            particle.color = glm::vec4(
                1.0f,
                0.35f + Random01() * 0.42f,
                0.012f,
                0.94f);
            particle.size *= 0.22f + Random01() * 0.22f;
            particle.lifetime *= 0.14f + Random01() * 0.13f;
            particle.velocity.y += 0.30f + Random01() * 0.24f;
        }
        else if (kind < 0.72f)
        {
            // Visible fire tongues rise out of the boundary. Their soft
            // procedural shape is built in the fragment shader.
            particle.color = glm::vec4(
                1.0f,
                0.11f + Random01() * 0.28f,
                0.004f,
                0.72f + Random01() * 0.20f);
            particle.size *= 0.82f + Random01() * 0.68f;
            particle.lifetime *= 0.12f + Random01() * 0.10f;
            particle.velocity.x *= 0.20f;
            particle.velocity.z *= 0.20f;
            particle.velocity.y += 0.12f + Random01() * 0.20f;
        }
        else
        {
            // Fine suspended ash complements the solid charcoal crumbs. A
            // soft noisy radial mask keeps these from reading as gray cards.
            const float gray = 0.19f + Random01() * 0.22f;
            particle.color = glm::vec4(gray * 1.03f, gray, gray * 0.96f, 0.26f + Random01() * 0.24f);
            particle.size *= 0.18f + Random01() * 0.30f;
            particle.lifetime *= 0.52f + Random01() * 0.48f;
            particle.velocity.x *= 0.55f;
            particle.velocity.z *= 0.55f;
            particle.velocity.y += 0.10f + Random01() * 0.18f;
        }
    }
    else if (mode == 6)
    {
        const float hot = Random01();
        particle.color = hot > 0.72f
            ? glm::vec4(1.0f, 0.34f + hot * 0.35f, 0.035f, 0.95f)
            : glm::vec4(0.16f + hot * 0.22f, 0.055f + hot * 0.08f, 0.025f, 0.88f);
    }
    else
    {
        const float gray = 0.18f + Random01() * 0.24f;
        particle.color = glm::vec4(gray * 1.08f, gray, gray * 0.94f, 0.42f + Random01() * 0.20f);
    }
    m_particles.push_back(particle);
}

void DissolveParticleSystem::Update(float deltaSeconds, const SceneObject* source, bool effectsEnabled)
{
    const int mode = source ? source->material.dissolveMode : -1;
    const bool active =
        effectsEnabled && source && source->material.surfaceEffect == 1 &&
        (mode == 3 || mode == 6 || mode == 7);
    if (!active)
    {
        Reset();
        return;
    }
    if (m_activeMode != mode)
    {
        Reset();
        m_activeMode = mode;
    }
    m_sourceWorld = source->transform.Matrix();

    if (mode == 3 &&
        (m_sourceMesh != source->mesh ||
         std::abs(m_sourceNoiseScale - source->material.dissolveNoiseScale) > 0.025f))
    {
        RebuildIncinerationSource(*source->mesh, source->material.dissolveNoiseScale);
    }

    const float amount = CurrentAmount(source->material);
    float previousAmount = m_previousAmount;
    if (mode == 3)
    {
        if (!m_amountInitialized)
        {
            m_previousAmount = 0.0f;
            previousAmount = 0.0f;
            m_amountInitialized = true;
            m_increasing = true;
        }
        const float amountDelta = amount - m_previousAmount;
        if (amountDelta < -0.0005f)
        {
            m_increasing = false;
            m_particles.clear();
            m_spawnAccumulator = 0.0f;
        }
        else if (amountDelta > 0.0005f && !m_increasing)
        {
            m_increasing = true;
            m_particles.clear();
            m_spawnAccumulator = 0.0f;
        }
    }
    if (source->material.dissolvePaused)
    {
        return;
    }
    if (mode == 3 && !m_increasing)
    {
        m_previousAmount = amount;
        return;
    }

    const float dt = std::min(deltaSeconds, 0.05f);
    for (Particle& particle : m_particles)
    {
        particle.age += dt;
        if (mode == 3 && particle.age <= particle.attachedTime)
        {
            continue;
        }
        particle.position += particle.velocity * dt;
        if (mode == 3)
        {
            const float flutter = std::sin(
                particle.seed * 27.0f +
                particle.age * (5.2f + particle.seed * 4.5f));
            if (particle.seed < 0.15f)
            {
                particle.velocity.y += 0.16f * dt;
                particle.velocity.x += flutter * 0.055f * dt;
            }
            else if (particle.seed < 0.72f)
            {
                particle.velocity.y += 0.32f * dt;
                particle.velocity.x += flutter * 0.24f * dt;
                particle.velocity.z += std::cos(
                    particle.seed * 31.0f + particle.age * 7.2f) * 0.14f * dt;
            }
            else
            {
                particle.velocity.y += 0.035f * dt;
                particle.velocity.x += flutter * 0.13f * dt;
                particle.velocity.z += std::cos(
                    particle.seed * 18.0f + particle.age * 4.1f) * 0.10f * dt;
            }
            particle.velocity = particle.velocity * std::exp(-0.28f * dt);
        }
        else if (mode == 7)
        {
            particle.velocity.y += 0.075f * dt;
            particle.velocity.x += std::sin(particle.seed * 19.0f + particle.age * 2.2f) * 0.018f * dt;
            particle.size += source->material.dissolveParticleSize * 0.26f * dt;
        }
        else
        {
            particle.velocity.y -= 0.15f * dt;
            particle.velocity.x += std::sin(particle.seed * 23.0f + particle.age * 5.0f) * 0.03f * dt;
        }
    }
    m_particles.erase(
        std::remove_if(
            m_particles.begin(),
            m_particles.end(),
            [](const Particle& particle) { return particle.age >= particle.lifetime; }),
        m_particles.end());

    m_spawnAccumulator = std::min(
        m_spawnAccumulator + dt * std::max(source->material.dissolveParticleRate, 0.0f),
        48.0f);
    int spawnCount = std::min(static_cast<int>(m_spawnAccumulator), 36);

    if (mode == 3)
    {
        std::vector<size_t> crossedSamples;
        if (amount > previousAmount + 0.000001f)
        {
            crossedSamples.reserve(256);
            for (size_t sampleIndex = 0; sampleIndex < m_surfaceSamples.size(); ++sampleIndex)
            {
                const SurfaceSample& sample = m_surfaceSamples[sampleIndex];
                const float ignition = IncinerationField::EvaluateFromNoise(
                    sample.position,
                    sample.burnNoise,
                    *source->mesh,
                    source->material);
                if (ignition > previousAmount && ignition <= amount)
                {
                    crossedSamples.push_back(sampleIndex);
                }
            }
        }

        if (!crossedSamples.empty() && spawnCount > 0)
        {
            std::shuffle(crossedSamples.begin(), crossedSamples.end(), m_random);
            spawnCount = std::min(spawnCount, static_cast<int>(crossedSamples.size()));
            m_spawnAccumulator -= static_cast<float>(spawnCount);
            for (int spawnIndex = 0; spawnIndex < spawnCount; ++spawnIndex)
            {
                Spawn(*source, mode, &m_surfaceSamples[crossedSamples[static_cast<size_t>(spawnIndex)]]);
            }
        }
    }
    else
    {
        m_spawnAccumulator -= static_cast<float>(spawnCount);
        while (spawnCount-- > 0)
        {
            Spawn(*source, mode);
        }
    }

    if (mode == 3)
    {
        m_previousAmount = amount;
    }
}

int DissolveParticleSystem::Render(
    const ShaderProgram& shader,
    const glm::mat4& viewProjection,
    float viewportHeight,
    float time)
{
    if (m_particles.empty() || shader.program == 0 || m_vao == 0)
    {
        return 0;
    }

    m_gpuVertices.clear();
    m_gpuVertices.reserve(m_particles.size());
    const glm::mat4 sourceWorld = m_sourceWorld;
    for (const Particle& particle : m_particles)
    {
        const float normalizedAge = std::clamp(particle.age / std::max(particle.lifetime, 0.001f), 0.0f, 1.0f);
        Vertex vertex;
        if (m_activeMode == 3)
        {
            vertex.position = glm::vec3(sourceWorld * glm::vec4(particle.position, 1.0f));
        }
        else
        {
            vertex.position = particle.position;
        }
        vertex.normal = glm::vec3(particle.seed, normalizedAge, static_cast<float>(m_activeMode));
        vertex.uv = glm::vec2(particle.size, normalizedAge);
        vertex.color = particle.color;
        vertex.color.w *= 1.0f - normalizedAge;
        m_gpuVertices.push_back(vertex);
    }

    glUseProgram(shader.program);
    // Phase 4: unified upload convention (GL_FALSE + value_ptr, no transpose).
    // The old path double-transposed (XMMatrixTranspose + GL_FALSE) and shipped
    // wrong matrices; DissolveParticle.vert has the same column-vector M*v
    // semantics as Standard.vert, so this now matches the Standard upload.
    UploadMat4(shader.program, "uViewProj", viewProjection);
    glUniform1f(glGetUniformLocation(shader.program, "uViewportHeight"), viewportHeight);
    glUniform1f(glGetUniformLocation(shader.program, "uTime"), time);
    glUniform1f(glGetUniformLocation(shader.program, "uParticleMode"), static_cast<float>(m_activeMode));

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_gpuVertices.size() * sizeof(Vertex)),
        m_gpuVertices.data(),
        GL_STREAM_DRAW);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_gpuVertices.size()));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
    return static_cast<int>(m_gpuVertices.size());
}
} // namespace YRender
