#include "Renderer/BurnFragmentSystem.h"

#include "Renderer/OpenGL.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

using namespace DirectX;

namespace YRender
{
namespace
{
constexpr size_t kMaxSourceTriangles = 240000;
constexpr size_t kMaxActiveFragments = 2200;

float Saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float SmoothStep(float edge0, float edge1, float value)
{
    const float t = Saturate((value - edge0) / std::max(edge1 - edge0, 0.0001f));
    return t * t * (3.0f - 2.0f * t);
}

float Fract(float value)
{
    return value - std::floor(value);
}

float Hash(float x, float y)
{
    return Fract(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f);
}

float Hash3(const XMFLOAT3& value)
{
    return Fract(std::sin(value.x * 127.1f + value.y * 311.7f + value.z * 74.7f) * 43758.5453f);
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
    const float lower = a + (b - a) * fx;
    const float upper = c + (d - c) * fx;
    return lower + (upper - lower) * fy;
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

XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
}

XMFLOAT3 Subtract(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

XMFLOAT3 Multiply(const XMFLOAT3& value, float scale)
{
    return XMFLOAT3(value.x * scale, value.y * scale, value.z * scale);
}

float Length(const XMFLOAT3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

XMFLOAT3 Normalize(const XMFLOAT3& value, const XMFLOAT3& fallback)
{
    const float length = Length(value);
    return length > 0.00001f ? Multiply(value, 1.0f / length) : fallback;
}

XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b)
{
    return XMFLOAT3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

XMFLOAT3 RotateAroundAxis(const XMFLOAT3& value, const XMFLOAT3& axis, float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float dot = value.x * axis.x + value.y * axis.y + value.z * axis.z;
    const XMFLOAT3 cross = Cross(axis, value);
    return Add(
        Add(Multiply(value, cosine), Multiply(cross, sine)),
        Multiply(axis, dot * (1.0f - cosine)));
}
} // namespace

BurnFragmentSystem::~BurnFragmentSystem()
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

void BurnFragmentSystem::Initialize()
{
    if (m_vao != 0)
    {
        return;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vertexBuffer);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(kMaxSourceTriangles * 3 * sizeof(FragmentVertex)),
        nullptr,
        GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FragmentVertex), reinterpret_cast<const void*>(offsetof(FragmentVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FragmentVertex), reinterpret_cast<const void*>(offsetof(FragmentVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(FragmentVertex), reinterpret_cast<const void*>(offsetof(FragmentVertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(FragmentVertex), reinterpret_cast<const void*>(offsetof(FragmentVertex, color)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(FragmentVertex), reinterpret_cast<const void*>(offsetof(FragmentVertex, barycentric)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void BurnFragmentSystem::Reset()
{
    XMStoreFloat4x4(&m_sourceWorld, XMMatrixIdentity());
    XMStoreFloat4x4(&m_sourceNormalMatrix, XMMatrixIdentity());
    m_sourceMesh = nullptr;
    m_sourceNoiseScale = -1.0f;
    m_sourceTriangles.clear();
    m_activeFragments.clear();
    m_gpuVertices.clear();
    m_previousAmount = 0.0f;
    m_amountInitialized = false;
    m_increasing = true;
}

float BurnFragmentSystem::CurrentAmount(const Material& material) const
{
    if (!material.dissolveAutoProgress)
    {
        return Saturate(material.dissolveAmount);
    }

    const float cycle = std::fmod(
        material.dissolvePlaybackTime * std::max(material.dissolveProgressSpeed, 0.01f),
        1.0f);
    return cycle < 0.5f ? cycle * 2.0f : 2.0f - cycle * 2.0f;
}

void BurnFragmentSystem::RebuildSource(const Mesh& mesh, float noiseScale)
{
    m_sourceMesh = &mesh;
    m_sourceNoiseScale = noiseScale;
    m_sourceTriangles.clear();
    m_activeFragments.clear();
    m_gpuVertices.clear();
    m_previousAmount = 0.0f;
    m_amountInitialized = false;
    m_increasing = true;

    const size_t triangleCount = mesh.indices.size() / 3;
    if (triangleCount == 0)
    {
        return;
    }

    const XMFLOAT3 boundsDiagonal = Subtract(mesh.boundsMax, mesh.boundsMin);
    const float targetEdgeLength = std::max(Length(boundsDiagonal) * 0.018f, 0.012f);
    const float resolvedNoiseScale = std::max(noiseScale, 0.01f);
    m_sourceTriangles.reserve(std::min(triangleCount * 2, kMaxSourceTriangles));

    const auto interpolate = [](const std::array<XMFLOAT3, 3>& values, float b0, float b1, float b2)
    {
        return XMFLOAT3(
            values[0].x * b0 + values[1].x * b1 + values[2].x * b2,
            values[0].y * b0 + values[1].y * b1 + values[2].y * b2,
            values[0].z * b0 + values[1].z * b1 + values[2].z * b2);
    };

    const auto appendTriangle = [&](const std::array<XMFLOAT3, 3>& positions,
                                    const std::array<XMFLOAT3, 3>& normals,
                                    size_t sourceIndex)
    {
        if (m_sourceTriangles.size() >= kMaxSourceTriangles)
        {
            return;
        }

        SourceTriangle triangle;
        triangle.positions = positions;
        triangle.normals = normals;
        for (size_t corner = 0; corner < 3; ++corner)
        {
            triangle.center = Add(triangle.center, Multiply(triangle.positions[corner], 1.0f / 3.0f));
            triangle.normal = Add(triangle.normal, triangle.normals[corner]);
        }

        const XMFLOAT3 edgeA = Subtract(triangle.positions[1], triangle.positions[0]);
        const XMFLOAT3 edgeB = Subtract(triangle.positions[2], triangle.positions[0]);
        const XMFLOAT3 faceNormal = Cross(edgeA, edgeB);
        if (Length(faceNormal) < 0.000001f)
        {
            return;
        }
        triangle.normal = Normalize(triangle.normal, Normalize(faceNormal, XMFLOAT3(0.0f, 1.0f, 0.0f)));
        triangle.seed = Hash3(Add(triangle.center, XMFLOAT3(
            static_cast<float>(sourceIndex) * 0.013f,
            static_cast<float>(sourceIndex) * 0.007f,
            static_cast<float>(sourceIndex) * 0.019f)));
        triangle.rotationAxis = Normalize(
            XMFLOAT3(
                Hash(triangle.seed * 31.0f, 4.0f) * 2.0f - 1.0f,
                Hash(triangle.seed * 47.0f, 8.0f) * 2.0f - 1.0f,
                Hash(triangle.seed * 59.0f, 2.0f) * 2.0f - 1.0f),
            XMFLOAT3(0.0f, 1.0f, 0.0f));

        const float noiseXY = FractalNoise(
            triangle.center.x * resolvedNoiseScale,
            triangle.center.y * resolvedNoiseScale);
        const float noiseXZ = FractalNoise(
            triangle.center.x * resolvedNoiseScale,
            triangle.center.z * resolvedNoiseScale);
        const float noiseZY = FractalNoise(
            triangle.center.z * resolvedNoiseScale,
            triangle.center.y * resolvedNoiseScale);
        const float coarseNoise = (noiseXY + noiseXZ + noiseZY) / 3.0f;
        const float fineNoise = FractalNoise(
            triangle.center.x * resolvedNoiseScale * 2.45f,
            triangle.center.y * resolvedNoiseScale * 2.45f);
        triangle.burnNoise = coarseNoise * 0.72f + fineNoise * 0.28f;
        m_sourceTriangles.push_back(triangle);
    };

    for (size_t triangleIndex = 0;
         triangleIndex < triangleCount && m_sourceTriangles.size() < kMaxSourceTriangles;
         ++triangleIndex)
    {
        const uint32_t i0 = mesh.indices[triangleIndex * 3];
        const uint32_t i1 = mesh.indices[triangleIndex * 3 + 1];
        const uint32_t i2 = mesh.indices[triangleIndex * 3 + 2];
        if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size())
        {
            continue;
        }

        std::array<XMFLOAT3, 3> sourcePositions{};
        std::array<XMFLOAT3, 3> sourceNormals{};
        const uint32_t indices[3]{i0, i1, i2};
        for (size_t corner = 0; corner < 3; ++corner)
        {
            sourcePositions[corner] = mesh.vertices[indices[corner]].position;
            sourceNormals[corner] = mesh.vertices[indices[corner]].normal;
        }

        const float maximumEdge = std::max({
            Length(Subtract(sourcePositions[1], sourcePositions[0])),
            Length(Subtract(sourcePositions[2], sourcePositions[1])),
            Length(Subtract(sourcePositions[0], sourcePositions[2])),
        });
        const int segments = std::clamp(
            static_cast<int>(std::ceil(maximumEdge / targetEdgeLength)),
            1,
            10);
        const float inverseSegments = 1.0f / static_cast<float>(segments);
        const auto sampleBarycentric = [&](int row, int column)
        {
            float b1 = static_cast<float>(row) * inverseSegments;
            float b2 = static_cast<float>(column) * inverseSegments;
            if (row > 0 && column > 0 && row + column < segments)
            {
                const float key = static_cast<float>(triangleIndex % 8191);
                const float jitter = inverseSegments * 0.28f;
                b1 += (Hash(key + static_cast<float>(row) * 17.0f, static_cast<float>(column) * 13.0f) - 0.5f) * jitter;
                b2 += (Hash(key + static_cast<float>(column) * 23.0f, static_cast<float>(row) * 19.0f) - 0.5f) * jitter;
                b1 = std::max(b1, 0.001f);
                b2 = std::max(b2, 0.001f);
                const float sum = b1 + b2;
                if (sum > 0.999f)
                {
                    b1 *= 0.999f / sum;
                    b2 *= 0.999f / sum;
                }
            }
            return XMFLOAT2(b1, b2);
        };
        const auto samplePosition = [&](int row, int column)
        {
            const XMFLOAT2 barycentric = sampleBarycentric(row, column);
            return interpolate(
                sourcePositions,
                1.0f - barycentric.x - barycentric.y,
                barycentric.x,
                barycentric.y);
        };
        const auto sampleNormal = [&](int row, int column)
        {
            const XMFLOAT2 barycentric = sampleBarycentric(row, column);
            return Normalize(
                interpolate(
                    sourceNormals,
                    1.0f - barycentric.x - barycentric.y,
                    barycentric.x,
                    barycentric.y),
                XMFLOAT3(0.0f, 1.0f, 0.0f));
        };

        for (int row = 0; row < segments && m_sourceTriangles.size() < kMaxSourceTriangles; ++row)
        {
            for (int column = 0;
                 column < segments - row && m_sourceTriangles.size() < kMaxSourceTriangles;
                 ++column)
            {
                const std::array<XMFLOAT3, 3> lowerPositions{
                    samplePosition(row, column),
                    samplePosition(row + 1, column),
                    samplePosition(row, column + 1),
                };
                const std::array<XMFLOAT3, 3> lowerNormals{
                    sampleNormal(row, column),
                    sampleNormal(row + 1, column),
                    sampleNormal(row, column + 1),
                };
                appendTriangle(
                    lowerPositions,
                    lowerNormals,
                    triangleIndex * 131 + static_cast<size_t>(row * 11 + column));

                if (row + column < segments - 1)
                {
                    const std::array<XMFLOAT3, 3> upperPositions{
                        samplePosition(row + 1, column),
                        samplePosition(row + 1, column + 1),
                        samplePosition(row, column + 1),
                    };
                    const std::array<XMFLOAT3, 3> upperNormals{
                        sampleNormal(row + 1, column),
                        sampleNormal(row + 1, column + 1),
                        sampleNormal(row, column + 1),
                    };
                    appendTriangle(
                        upperPositions,
                        upperNormals,
                        triangleIndex * 131 + static_cast<size_t>(row * 11 + column + 67));
                }
            }
        }
    }
}

void BurnFragmentSystem::SpawnFragment(
    const SourceTriangle& source,
    const Material& material)
{
    if (m_activeFragments.size() >= kMaxActiveFragments)
    {
        return;
    }

    AshFragment fragment;
    fragment.seed = source.seed;
    std::array<XMFLOAT3, 3> localPositions{};
    std::array<XMFLOAT3, 3> localNormals{};
    for (size_t corner = 0; corner < 3; ++corner)
    {
        localPositions[corner] = source.positions[corner];
        localNormals[corner] = source.normals[corner];
        fragment.position = Add(fragment.position, Multiply(localPositions[corner], 1.0f / 3.0f));
    }

    fragment.faceNormal = Normalize(
        Cross(
            Subtract(localPositions[1], localPositions[0]),
            Subtract(localPositions[2], localPositions[0])),
        XMFLOAT3(0.0f, 1.0f, 0.0f));
    if (fragment.faceNormal.x * localNormals[0].x +
        fragment.faceNormal.y * localNormals[0].y +
        fragment.faceNormal.z * localNormals[0].z < 0.0f)
    {
        fragment.faceNormal = Multiply(fragment.faceNormal, -1.0f);
    }

    fragment.basis[0] = Normalize(
        Subtract(localPositions[1], localPositions[0]),
        XMFLOAT3(1.0f, 0.0f, 0.0f));
    fragment.basis[2] = fragment.faceNormal;
    fragment.basis[1] = Normalize(
        Cross(fragment.basis[2], fragment.basis[0]),
        XMFLOAT3(0.0f, 1.0f, 0.0f));

    const float maximumEdge = std::max({
        Length(Subtract(localPositions[1], localPositions[0])),
        Length(Subtract(localPositions[2], localPositions[1])),
        Length(Subtract(localPositions[0], localPositions[2])),
    });
    const float baseRadius = std::clamp(
        maximumEdge * (0.085f + Hash(source.seed * 67.0f, 5.0f) * 0.105f),
        0.0032f,
        0.0115f);
    fragment.shapeKind = Hash(source.seed * 193.0f, 23.0f) < 0.86f ? 0 : 1;
    if (fragment.shapeKind == 0)
    {
        // Compact irregular ash clumps. All three axes have meaningful size,
        // so the silhouette remains volumetric from every camera angle.
        fragment.radii = XMFLOAT3(
            baseRadius * (0.72f + Hash(source.seed * 71.0f, 2.0f) * 0.55f),
            baseRadius * (0.68f + Hash(source.seed * 79.0f, 4.0f) * 0.70f),
            baseRadius * (0.58f + Hash(source.seed * 83.0f, 6.0f) * 0.58f));
    }
    else
    {
        // Keep only a few mildly elongated pieces. Their depth is still large
        // enough to retain a solid silhouette when viewed from the side.
        fragment.radii = XMFLOAT3(
            baseRadius * (0.76f + Hash(source.seed * 71.0f, 2.0f) * 0.38f),
            baseRadius * (1.02f + Hash(source.seed * 79.0f, 4.0f) * 0.54f),
            baseRadius * (0.68f + Hash(source.seed * 83.0f, 6.0f) * 0.40f));
    }
    fragment.attachedTime = 0.055f + Hash(source.seed * 43.0f, 7.0f) * 0.095f;
    fragment.lifetime = material.dissolveParticleLifetime *
        (0.78f + Hash(source.seed * 59.0f, 13.0f) * 0.50f);
    fragment.flutter = 0.72f + Hash(source.seed * 109.0f, 29.0f) * 1.15f;
    fragment.buoyancy = source.seed > 0.82f
        ? -0.025f - Hash(source.seed * 127.0f, 31.0f) * 0.035f
        : 0.075f + Hash(source.seed * 127.0f, 31.0f) * 0.14f;

    const XMFLOAT3 randomDirection = Normalize(
        XMFLOAT3(
            Hash(source.seed * 73.0f, 3.0f) * 2.0f - 1.0f,
            Hash(source.seed * 89.0f, 11.0f) * 2.0f - 1.0f,
            Hash(source.seed * 97.0f, 17.0f) * 2.0f - 1.0f),
        XMFLOAT3(1.0f, 0.0f, 0.0f));
    const float randomSpeed = 0.030f + Hash(source.seed * 139.0f, 43.0f) * 0.085f;
    fragment.velocity = Add(
        Add(
            Multiply(fragment.faceNormal, 0.018f + source.seed * 0.085f),
            Multiply(randomDirection, randomSpeed)),
        Add(
            Multiply(material.dissolveParticleWind, 0.22f + source.seed * 0.22f),
            XMFLOAT3(
                0.0f,
                0.040f + Hash(source.seed * 151.0f, 37.0f) * 0.105f,
                0.0f)));
    fragment.angularVelocity = Multiply(
        Normalize(
            Add(source.rotationAxis, Multiply(randomDirection, 0.72f)),
            XMFLOAT3(0.0f, 1.0f, 0.0f)),
        2.2f + Hash(source.seed * 101.0f, 19.0f) * 8.4f);

    // Give every fragment a different orientation before its first rendered
    // frame. Waiting for angular velocity to accumulate made adjacent pieces
    // expose the same face and read as one aligned sheet.
    const XMFLOAT3 initialRotationAxis = Normalize(
        XMFLOAT3(
            Hash(source.seed * 163.0f, 47.0f) * 2.0f - 1.0f,
            Hash(source.seed * 173.0f, 53.0f) * 2.0f - 1.0f,
            Hash(source.seed * 181.0f, 59.0f) * 2.0f - 1.0f),
        XMFLOAT3(0.0f, 1.0f, 0.0f));
    const float initialAngle = Hash(source.seed * 211.0f, 61.0f) * XM_2PI;
    XMStoreFloat4(
        &fragment.orientation,
        XMQuaternionRotationAxis(XMLoadFloat3(&initialRotationAxis), initialAngle));

    // Start on the source surface and only add sub-fragment-scale variation.
    // The subsequent velocity field creates volume; a large birth offset
    // would make the ash look like an unrelated emitter beside the model.
    const float tangentSpread = 0.002f + Hash(source.seed * 223.0f, 67.0f) * 0.010f;
    const float tangentOffset =
        (Hash(source.seed * 227.0f, 71.0f) * 2.0f - 1.0f) * tangentSpread;
    const float bitangentOffset =
        (Hash(source.seed * 233.0f, 73.0f) * 2.0f - 1.0f) * tangentSpread;
    const float normalOffset =
        (0.25f + Hash(source.seed * 239.0f, 79.0f) * 0.75f) *
        (0.003f + Hash(source.seed * 241.0f, 83.0f) * 0.009f);
    fragment.position = Add(fragment.position, Multiply(fragment.basis[0], tangentOffset));
    fragment.position = Add(fragment.position, Multiply(fragment.basis[1], bitangentOffset));
    fragment.position = Add(fragment.position, Multiply(fragment.faceNormal, normalOffset));
    m_activeFragments.push_back(fragment);
}

void BurnFragmentSystem::Simulate(float deltaSeconds, const Material& material, float time)
{
    const float dt = std::clamp(deltaSeconds, 0.0f, 0.05f);
    if (dt <= 0.0f)
    {
        return;
    }

    for (AshFragment& fragment : m_activeFragments)
    {
        fragment.age += dt;
        if (fragment.age <= fragment.attachedTime)
        {
            continue;
        }

        const XMFLOAT3& p = fragment.position;
        const float phase = fragment.seed * 31.0f;
        XMFLOAT3 curl{
            std::sin(p.z * 3.1f + time * 1.17f + phase) -
                std::cos(p.y * 2.7f - time * 0.83f + phase * 0.71f),
            std::sin(p.x * 2.6f - time * 0.69f + phase * 1.13f) -
                std::cos(p.z * 3.4f + time * 0.91f + phase * 0.43f),
            std::sin(p.y * 3.0f + time * 0.77f + phase * 0.83f) -
                std::cos(p.x * 2.9f - time * 1.03f + phase * 1.27f),
        };
        curl = Normalize(curl, XMFLOAT3(0.0f, 1.0f, 0.0f));
        const float normalizedAge = Saturate(fragment.age / std::max(fragment.lifetime, 0.001f));
        const float turbulence = (0.15f + fragment.seed * 0.22f) * fragment.flutter *
            (0.55f + SmoothStep(0.08f, 0.42f, normalizedAge) * 0.45f);
        fragment.velocity = Add(fragment.velocity, Multiply(curl, turbulence * dt));
        fragment.velocity = Add(
            fragment.velocity,
            Multiply(material.dissolveParticleWind, (0.10f + normalizedAge * 0.12f) * dt));
        fragment.velocity.y += fragment.buoyancy * dt;
        const float spiral =
            std::sin(time * (2.1f + fragment.seed * 1.7f) + fragment.seed * 47.0f) *
            (0.020f + normalizedAge * 0.040f) * fragment.flutter;
        fragment.velocity.x += spiral * dt;
        fragment.velocity.z += std::cos(
            time * (1.8f + fragment.seed * 1.4f) + fragment.seed * 39.0f) *
            (0.018f + normalizedAge * 0.035f) * fragment.flutter * dt;
        fragment.velocity.x +=
            std::sin(time * 0.73f + fragment.seed * 91.0f) *
            (0.025f + fragment.seed * 0.045f) * dt;
        fragment.velocity.z +=
            std::cos(time * 0.61f + fragment.seed * 83.0f) *
            (0.028f + (1.0f - fragment.seed) * 0.050f) * dt;
        fragment.velocity = Multiply(fragment.velocity, std::exp(-(0.76f + fragment.seed * 0.28f) * dt));
        fragment.position = Add(fragment.position, Multiply(fragment.velocity, dt));

        fragment.angularVelocity = Add(
            fragment.angularVelocity,
            Multiply(curl, (0.38f + normalizedAge * 0.46f) * dt));
        fragment.angularVelocity = Multiply(fragment.angularVelocity, std::exp(-0.12f * dt));
        const float angularSpeed = Length(fragment.angularVelocity);
        if (angularSpeed > 0.0001f)
        {
            const XMVECTOR axis = XMVector3Normalize(XMLoadFloat3(&fragment.angularVelocity));
            const XMVECTOR deltaRotation = XMQuaternionRotationAxis(axis, angularSpeed * dt);
            XMVECTOR orientation = XMLoadFloat4(&fragment.orientation);
            orientation = XMQuaternionNormalize(XMQuaternionMultiply(orientation, deltaRotation));
            XMStoreFloat4(&fragment.orientation, orientation);
        }
    }

    m_activeFragments.erase(
        std::remove_if(
            m_activeFragments.begin(),
            m_activeFragments.end(),
            [](const AshFragment& fragment) { return fragment.age >= fragment.lifetime; }),
        m_activeFragments.end());
}

void BurnFragmentSystem::BuildGpuVertices()
{
    m_gpuVertices.clear();
    m_gpuVertices.reserve(m_activeFragments.size() * 60);
    const XMMATRIX sourceWorld = XMLoadFloat4x4(&m_sourceWorld);
    const XMMATRIX sourceNormalMatrix = XMLoadFloat4x4(&m_sourceNormalMatrix);

    for (const AshFragment& fragment : m_activeFragments)
    {
        const float normalizedAge = Saturate(fragment.age / std::max(fragment.lifetime, 0.001f));
        const float scale =
            1.0f - SmoothStep(0.54f, 1.0f, normalizedAge) * (0.34f + fragment.seed * 0.18f);
        const float cooling = SmoothStep(0.015f, 0.12f, normalizedAge);
        const float fade = 1.0f - SmoothStep(0.74f, 1.0f, normalizedAge);
        const float ash = 0.13f + fragment.seed * 0.18f;
        const XMFLOAT3 hotColor{
            0.11f + fragment.seed * 0.045f,
            0.012f + fragment.seed * 0.010f,
            0.002f,
        };
        const XMFLOAT3 ashColor{ash * 1.06f, ash, ash * 0.93f};
        const XMFLOAT4 color{
            hotColor.x + (ashColor.x - hotColor.x) * cooling,
            hotColor.y + (ashColor.y - hotColor.y) * cooling,
            hotColor.z + (ashColor.z - hotColor.z) * cooling,
            fade,
        };

        const XMVECTOR orientation = XMLoadFloat4(&fragment.orientation);
        std::array<XMFLOAT3, 3> axes{};
        for (size_t axis = 0; axis < 3; ++axis)
        {
            XMStoreFloat3(
                &axes[axis],
                XMVector3Normalize(XMVector3Rotate(XMLoadFloat3(&fragment.basis[axis]), orientation)));
        }

        const auto appendTriangle = [&](const XMFLOAT3& p0,
                                        const XMFLOAT3& p1,
                                        const XMFLOAT3& p2,
                                        bool glowingFace)
        {
            const XMFLOAT3 localTriangleNormal = Normalize(
                Cross(Subtract(p1, p0), Subtract(p2, p0)),
                axes[2]);
            const XMFLOAT3 localPositions[3]{p0, p1, p2};
            for (size_t corner = 0; corner < 3; ++corner)
            {
                FragmentVertex vertex;
                XMStoreFloat3(
                    &vertex.position,
                    XMVector3TransformCoord(XMLoadFloat3(&localPositions[corner]), sourceWorld));
                XMStoreFloat3(
                    &vertex.normal,
                    XMVector3Normalize(XMVector3TransformNormal(
                        XMLoadFloat3(&localTriangleNormal),
                        sourceNormalMatrix)));
                vertex.uv = XMFLOAT2(normalizedAge, fragment.seed);
                vertex.color = color;
                if (glowingFace)
                {
                    vertex.barycentric = corner == 0
                        ? XMFLOAT3(1.0f, 0.0f, 0.0f)
                        : (corner == 1
                            ? XMFLOAT3(0.0f, 1.0f, 0.0f)
                            : XMFLOAT3(0.0f, 0.0f, 1.0f));
                }
                else
                {
                    vertex.barycentric = XMFLOAT3(-1.0f, -1.0f, -1.0f);
                }
                m_gpuVertices.push_back(vertex);
            }
        };

        const auto makePoint = [&](float x, float y, float z)
        {
            XMFLOAT3 point = fragment.position;
            point = Add(point, Multiply(axes[0], x * fragment.radii.x * scale));
            point = Add(point, Multiply(axes[1], y * fragment.radii.y * scale));
            point = Add(point, Multiply(axes[2], z * fragment.radii.z * scale));
            return point;
        };

        // A closed irregular icosahedral crumb has no privileged front plane.
        // At ash scale it reads as a rounded charcoal fragment rather than
        // the regular diamond silhouette of an octahedron.
        const float asymmetry = (fragment.seed - 0.5f) * 0.34f;
        constexpr float t = 1.61803398875f;
        constexpr float unitScale = 0.52573111212f;
        const std::array<XMFLOAT3, 12> rawPoints{
            XMFLOAT3(-1.0f, t, 0.0f), XMFLOAT3(1.0f, t, 0.0f),
            XMFLOAT3(-1.0f, -t, 0.0f), XMFLOAT3(1.0f, -t, 0.0f),
            XMFLOAT3(0.0f, -1.0f, t), XMFLOAT3(0.0f, 1.0f, t),
            XMFLOAT3(0.0f, -1.0f, -t), XMFLOAT3(0.0f, 1.0f, -t),
            XMFLOAT3(t, 0.0f, -1.0f), XMFLOAT3(t, 0.0f, 1.0f),
            XMFLOAT3(-t, 0.0f, -1.0f), XMFLOAT3(-t, 0.0f, 1.0f),
        };
        std::array<XMFLOAT3, 12> points{};
        for (size_t pointIndex = 0; pointIndex < rawPoints.size(); ++pointIndex)
        {
            const float jitter = 0.84f + Hash(
                fragment.seed * 157.0f + static_cast<float>(pointIndex) * 13.0f,
                41.0f) * 0.27f;
            points[pointIndex] = makePoint(
                (rawPoints[pointIndex].x * unitScale + asymmetry * rawPoints[pointIndex].y * 0.08f) * jitter,
                rawPoints[pointIndex].y * unitScale * jitter,
                (rawPoints[pointIndex].z * unitScale - asymmetry * rawPoints[pointIndex].x * 0.06f) * jitter);
        }
        const size_t faces[20][3]{
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
        };
        const size_t hotFace = static_cast<size_t>(fragment.seed * 20.0f) % 20;
        for (size_t face = 0; face < 20; ++face)
        {
            appendTriangle(
                points[faces[face][0]],
                points[faces[face][1]],
                points[faces[face][2]],
                face == hotFace);
        }
    }
}

void BurnFragmentSystem::Update(float deltaSeconds, const SceneObject* source, bool effectsEnabled)
{
    const bool active =
        effectsEnabled && source && source->mesh &&
        source->material.surfaceEffect == 1 && source->material.dissolveMode == 3;
    if (!active)
    {
        if (m_amountInitialized)
        {
            for (SourceTriangle& triangle : m_sourceTriangles)
            {
                triangle.emitted = false;
            }
        }
        m_activeFragments.clear();
        m_gpuVertices.clear();
        m_amountInitialized = false;
        return;
    }

    const Mesh& mesh = *source->mesh;
    const Material& material = source->material;
    const XMMATRIX world = source->transform.Matrix();
    const XMMATRIX normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
    XMStoreFloat4x4(&m_sourceWorld, world);
    XMStoreFloat4x4(&m_sourceNormalMatrix, normalMatrix);
    if (m_sourceMesh != &mesh ||
        std::abs(m_sourceNoiseScale - material.dissolveNoiseScale) > 0.025f)
    {
        RebuildSource(mesh, material.dissolveNoiseScale);
        m_activeFragments.reserve(kMaxActiveFragments);
    }

    const float amount = CurrentAmount(material);
    if (!m_amountInitialized)
    {
        m_previousAmount = 0.0f;
        m_amountInitialized = true;
        m_increasing = true;
    }

    const float amountDelta = amount - m_previousAmount;
    if (amountDelta < -0.0005f)
    {
        if (m_increasing)
        {
            m_activeFragments.clear();
        }
        m_increasing = false;
    }
    else if (amountDelta > 0.0005f && !m_increasing)
    {
        m_increasing = true;
        m_activeFragments.clear();
        for (SourceTriangle& triangle : m_sourceTriangles)
        {
            triangle.emitted = false;
        }
    }

    if (!material.dissolvePaused && m_increasing && amount > 0.0001f)
    {
        const XMFLOAT3 boundsSize{
            std::max(mesh.boundsMax.x - mesh.boundsMin.x, 0.0001f),
            std::max(mesh.boundsMax.y - mesh.boundsMin.y, 0.0001f),
            std::max(mesh.boundsMax.z - mesh.boundsMin.z, 0.0001f),
        };
        const XMFLOAT3 direction = Normalize(material.dissolveDirection, XMFLOAT3(0.0f, 1.0f, 0.0f));
        const float directionSpan = std::max(
            std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z),
            0.001f);
        for (SourceTriangle& triangle : m_sourceTriangles)
        {
            if (triangle.emitted)
            {
                continue;
            }
            const XMFLOAT3 local01{
                (triangle.center.x - mesh.boundsMin.x) / boundsSize.x,
                (triangle.center.y - mesh.boundsMin.y) / boundsSize.y,
                (triangle.center.z - mesh.boundsMin.z) / boundsSize.z,
            };
            const float directional =
                ((local01.x - 0.5f) * direction.x +
                 (local01.y - 0.5f) * direction.y +
                 (local01.z - 0.5f) * direction.z) /
                    directionSpan +
                0.5f;
            float ignition = Saturate(
                directional + (triangle.burnNoise - 0.5f) * material.dissolveNoiseInfluence);
            ignition = std::max(ignition, triangle.seed * 0.035f);
            if (ignition > amount)
            {
                continue;
            }

            triangle.emitted = true;
            const float densitySelector = Hash(
                triangle.seed * 173.0f + ignition * 37.0f,
                triangle.seed * 251.0f + 11.0f);
            if (densitySelector < 0.070f)
            {
                SpawnFragment(triangle, material);
            }
        }
    }

    if (!material.dissolvePaused && m_increasing)
    {
        Simulate(deltaSeconds, material, material.dissolvePlaybackTime);
    }
    BuildGpuVertices();
    m_previousAmount = amount;
}

int BurnFragmentSystem::Render(
    const ShaderProgram& shader,
    const XMMATRIX& viewProjection,
    const XMFLOAT3& cameraPosition,
    float time)
{
    if (m_gpuVertices.empty() || shader.program == 0 || m_vao == 0)
    {
        return 0;
    }

    glUseProgram(shader.program);
    XMFLOAT4X4 matrix;
    XMStoreFloat4x4(&matrix, XMMatrixTranspose(viewProjection));
    glUniformMatrix4fv(glGetUniformLocation(shader.program, "uViewProj"), 1, GL_FALSE, &matrix.m[0][0]);
    glUniform3f(
        glGetUniformLocation(shader.program, "uCameraPosition"),
        cameraPosition.x,
        cameraPosition.y,
        cameraPosition.z);
    glUniform1f(glGetUniformLocation(shader.program, "uTime"), time);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_gpuVertices.size() * sizeof(FragmentVertex)),
        m_gpuVertices.data(),
        GL_STREAM_DRAW);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_gpuVertices.size()));
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glUseProgram(0);
    return static_cast<int>(m_gpuVertices.size() / 3);
}
} // namespace YRender
