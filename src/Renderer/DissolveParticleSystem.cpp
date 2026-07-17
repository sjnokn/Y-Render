#include "Renderer/DissolveParticleSystem.h"

#include "Renderer/OpenGL.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

using namespace DirectX;

namespace YRender
{
namespace
{
constexpr size_t kMaxParticles = 1200;

XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
}

XMFLOAT3 Multiply(const XMFLOAT3& value, float scale)
{
    return XMFLOAT3(value.x * scale, value.y * scale, value.z * scale);
}

float Saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float Fract(float value)
{
    return value - std::floor(value);
}

float Hash(float x, float y)
{
    return Fract(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f);
}

float ValueNoise(float x, float y)
{
    const float ix = std::floor(x);
    const float iy = std::floor(y);
    float fx = Fract(x);
    float fy = Fract(y);
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    const float a = Hash(ix, iy);
    const float b = Hash(ix + 1.0f, iy);
    const float c = Hash(ix, iy + 1.0f);
    const float d = Hash(ix + 1.0f, iy + 1.0f);
    return (a + (b - a) * fx) +
        ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

float FractalNoise(float x, float y)
{
    float result = 0.0f;
    float amplitude = 0.5f;
    for (int octave = 0; octave < 4; ++octave)
    {
        result += ValueNoise(x, y) * amplitude;
        x = x * 2.0f + 17.0f;
        y = y * 2.0f + 29.0f;
        amplitude *= 0.5f;
    }
    return Saturate(result);
}

float IncinerationField(const XMFLOAT3& position, const Mesh& mesh, const Material& material)
{
    const XMFLOAT3 boundsSize{
        std::max(mesh.boundsMax.x - mesh.boundsMin.x, 0.0001f),
        std::max(mesh.boundsMax.y - mesh.boundsMin.y, 0.0001f),
        std::max(mesh.boundsMax.z - mesh.boundsMin.z, 0.0001f),
    };
    XMFLOAT3 direction = material.dissolveDirection;
    const float directionLength = std::sqrt(
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z);
    direction = directionLength > 0.001f
        ? Multiply(direction, 1.0f / directionLength)
        : XMFLOAT3(0.0f, 1.0f, 0.0f);
    const float directionSpan = std::max(
        std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z),
        0.001f);
    const XMFLOAT3 local01{
        (position.x - mesh.boundsMin.x) / boundsSize.x,
        (position.y - mesh.boundsMin.y) / boundsSize.y,
        (position.z - mesh.boundsMin.z) / boundsSize.z,
    };
    const float directional =
        ((local01.x - 0.5f) * direction.x +
         (local01.y - 0.5f) * direction.y +
         (local01.z - 0.5f) * direction.z) /
            directionSpan +
        0.5f;
    const float scale = std::max(material.dissolveNoiseScale, 0.01f);
    const float noiseXY = FractalNoise(position.x * scale, position.y * scale);
    const float noiseXZ = FractalNoise(position.x * scale, position.z * scale);
    const float noiseZY = FractalNoise(position.z * scale, position.y * scale);
    const float coarseNoise = (noiseXY + noiseXZ + noiseZY) / 3.0f;
    const float fineNoise = FractalNoise(
        position.x * scale * 2.45f,
        position.y * scale * 2.45f);
    const float burnNoise = coarseNoise * 0.72f + fineNoise * 0.28f;
    return Saturate(
        directional + (burnNoise - 0.5f) * material.dissolveNoiseInfluence);
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
    XMStoreFloat4x4(&m_sourceWorld, XMMatrixIdentity());
    m_particles.clear();
    m_gpuVertices.clear();
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

void DissolveParticleSystem::Spawn(const SceneObject& source, int mode)
{
    if (!source.mesh || source.mesh->vertices.empty() || m_particles.size() >= kMaxParticles)
    {
        return;
    }

    const Mesh& mesh = *source.mesh;
    const Material& material = source.material;
    const XMFLOAT3 boundsSize{
        std::max(mesh.boundsMax.x - mesh.boundsMin.x, 0.0001f),
        std::max(mesh.boundsMax.y - mesh.boundsMin.y, 0.0001f),
        std::max(mesh.boundsMax.z - mesh.boundsMin.z, 0.0001f),
    };
    XMFLOAT3 direction = material.dissolveDirection;
    const float directionLength = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (directionLength < 0.001f)
    {
        direction = XMFLOAT3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        direction = Multiply(direction, 1.0f / directionLength);
    }
    const float directionSpan = std::max(
        std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z),
        0.001f);
    const float amount = CurrentAmount(material);
    const float targetAmount = mode == 3
        ? std::clamp(amount + (Random01() - 0.5f) * 0.045f, 0.0f, 1.0f)
        : amount;

    size_t bestIndex = static_cast<size_t>(Random01() * static_cast<float>(mesh.vertices.size()));
    bestIndex = std::min(bestIndex, mesh.vertices.size() - 1);
    float bestDistance = 10.0f;
    const int candidateCount = mode == 3 ? 104 : 28;
    for (int attempt = 0; attempt < candidateCount; ++attempt)
    {
        size_t index = static_cast<size_t>(Random01() * static_cast<float>(mesh.vertices.size()));
        index = std::min(index, mesh.vertices.size() - 1);
        const XMFLOAT3& position = mesh.vertices[index].position;
        const XMFLOAT3 local01{
            (position.x - mesh.boundsMin.x) / boundsSize.x,
            (position.y - mesh.boundsMin.y) / boundsSize.y,
            (position.z - mesh.boundsMin.z) / boundsSize.z,
        };
        const float field = mode == 3
            ? IncinerationField(position, mesh, material)
            : ((local01.x - 0.5f) * direction.x +
               (local01.y - 0.5f) * direction.y +
               (local01.z - 0.5f) * direction.z) /
                    directionSpan +
                0.5f;
        const float distance = std::abs(field - targetAmount);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    const Vertex& sourceVertex = mesh.vertices[bestIndex];
    XMFLOAT3 position;
    XMFLOAT3 normal;
    if (mode == 3)
    {
        position = sourceVertex.position;
        normal = sourceVertex.normal;
    }
    else
    {
        const XMMATRIX world = source.transform.Matrix();
        XMStoreFloat3(
            &position,
            XMVector3TransformCoord(XMLoadFloat3(&sourceVertex.position), world));
        XMStoreFloat3(
            &normal,
            XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&sourceVertex.normal), world)));
    }

    Particle particle;
    particle.seed = Random01();
    particle.age = 0.0f;
    particle.lifetime = material.dissolveParticleLifetime * (0.72f + Random01() * 0.56f);
    particle.size = material.dissolveParticleSize * (0.65f + Random01() * 0.75f);
    particle.position = Add(position, Multiply(normal, 0.006f + Random01() * 0.016f));

    const float lateralSpread = mode == 3 ? 0.105f : 0.34f;
    const XMFLOAT3 randomDrift{
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
    particle.velocity = Add(
        Add(material.dissolveParticleWind, randomDrift),
        Multiply(normal, normalImpulse));

    if (mode == 3)
    {
        const float kind = particle.seed;
        if (kind < 0.15f)
        {
            // Tiny, short-lived sparks provide highlights without turning the
            // entire ash stream orange.
            particle.color = XMFLOAT4(
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
            particle.color = XMFLOAT4(
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
            particle.color = XMFLOAT4(gray * 1.03f, gray, gray * 0.96f, 0.26f + Random01() * 0.24f);
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
            ? XMFLOAT4(1.0f, 0.34f + hot * 0.35f, 0.035f, 0.95f)
            : XMFLOAT4(0.16f + hot * 0.22f, 0.055f + hot * 0.08f, 0.025f, 0.88f);
    }
    else
    {
        const float gray = 0.18f + Random01() * 0.24f;
        particle.color = XMFLOAT4(gray * 1.08f, gray, gray * 0.94f, 0.42f + Random01() * 0.20f);
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
    XMStoreFloat4x4(&m_sourceWorld, source->transform.Matrix());

    const float amount = CurrentAmount(source->material);
    if (mode == 3)
    {
        if (!m_amountInitialized)
        {
            m_previousAmount = 0.0f;
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
        m_previousAmount = amount;
    }
    if (source->material.dissolveAutoProgress && source->material.dissolvePaused)
    {
        return;
    }
    if (mode == 3 && !m_increasing)
    {
        return;
    }

    const float dt = std::min(deltaSeconds, 0.05f);
    for (Particle& particle : m_particles)
    {
        particle.age += dt;
        particle.position = Add(particle.position, Multiply(particle.velocity, dt));
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
            particle.velocity = Multiply(particle.velocity, std::exp(-0.28f * dt));
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

    m_spawnAccumulator += dt * std::max(source->material.dissolveParticleRate, 0.0f);
    int spawnCount = std::min(static_cast<int>(m_spawnAccumulator), 18);
    m_spawnAccumulator -= static_cast<float>(spawnCount);
    while (spawnCount-- > 0)
    {
        Spawn(*source, mode);
    }
}

int DissolveParticleSystem::Render(
    const ShaderProgram& shader,
    const XMMATRIX& viewProjection,
    float viewportHeight,
    float time)
{
    if (m_particles.empty() || shader.program == 0 || m_vao == 0)
    {
        return 0;
    }

    m_gpuVertices.clear();
    m_gpuVertices.reserve(m_particles.size());
    const XMMATRIX sourceWorld = XMLoadFloat4x4(&m_sourceWorld);
    for (const Particle& particle : m_particles)
    {
        const float normalizedAge = std::clamp(particle.age / std::max(particle.lifetime, 0.001f), 0.0f, 1.0f);
        Vertex vertex;
        if (m_activeMode == 3)
        {
            XMStoreFloat3(
                &vertex.position,
                XMVector3TransformCoord(XMLoadFloat3(&particle.position), sourceWorld));
        }
        else
        {
            vertex.position = particle.position;
        }
        vertex.normal = XMFLOAT3(particle.seed, normalizedAge, static_cast<float>(m_activeMode));
        vertex.uv = XMFLOAT2(particle.size, normalizedAge);
        vertex.color = particle.color;
        vertex.color.w *= 1.0f - normalizedAge;
        m_gpuVertices.push_back(vertex);
    }

    glUseProgram(shader.program);
    XMFLOAT4X4 matrix;
    XMStoreFloat4x4(&matrix, XMMatrixTranspose(viewProjection));
    glUniformMatrix4fv(glGetUniformLocation(shader.program, "uViewProj"), 1, GL_FALSE, &matrix.m[0][0]);
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
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_gpuVertices.size()));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_POINT_SPRITE);
    glBindVertexArray(0);
    glUseProgram(0);
    return static_cast<int>(m_gpuVertices.size());
}
} // namespace YRender
