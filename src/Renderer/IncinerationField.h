#pragma once

#include "Scene/SceneTypes.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace YRender::IncinerationField
{
inline float Saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

inline float Fract(float value)
{
    return value - std::floor(value);
}

inline float Hash(float x, float y)
{
    return Fract(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f);
}

inline float ValueNoise(float x, float y)
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

inline float FractalNoise(float x, float y)
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

inline float BurnNoise(const glm::vec3& position, float noiseScale)
{
    const float scale = std::max(noiseScale, 0.01f);
    const float noiseXY = FractalNoise(position.x * scale, position.y * scale);
    const float noiseXZ = FractalNoise(position.x * scale, position.z * scale);
    const float noiseZY = FractalNoise(position.z * scale, position.y * scale);
    const float coarseNoise = (noiseXY + noiseXZ + noiseZY) / 3.0f;
    const float fineNoise = FractalNoise(
        position.x * scale * 2.45f,
        position.y * scale * 2.45f);
    return coarseNoise * 0.72f + fineNoise * 0.28f;
}

inline glm::vec3 NormalizedDirection(const glm::vec3& value)
{
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 0.001f)
    {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }
    return glm::vec3(value.x / length, value.y / length, value.z / length);
}

inline float EvaluateFromNoise(
    const glm::vec3& position,
    float burnNoise,
    const Mesh& mesh,
    const Material& material)
{
    const glm::vec3 boundsSize{
        std::max(mesh.boundsMax.x - mesh.boundsMin.x, 0.0001f),
        std::max(mesh.boundsMax.y - mesh.boundsMin.y, 0.0001f),
        std::max(mesh.boundsMax.z - mesh.boundsMin.z, 0.0001f),
    };
    const glm::vec3 local01{
        Saturate((position.x - mesh.boundsMin.x) / boundsSize.x),
        Saturate((position.y - mesh.boundsMin.y) / boundsSize.y),
        Saturate((position.z - mesh.boundsMin.z) / boundsSize.z),
    };
    const glm::vec3 direction = NormalizedDirection(material.dissolveDirection);
    const float directionSpan = std::max(
        std::abs(direction.x) + std::abs(direction.y) + std::abs(direction.z),
        0.001f);
    const float directional =
        ((local01.x - 0.5f) * direction.x +
         (local01.y - 0.5f) * direction.y +
         (local01.z - 0.5f) * direction.z) /
            directionSpan +
        0.5f;
    return Saturate(
        directional + (burnNoise - 0.5f) * material.dissolveNoiseInfluence);
}

inline float Evaluate(
    const glm::vec3& position,
    const Mesh& mesh,
    const Material& material)
{
    return EvaluateFromNoise(
        position,
        BurnNoise(position, material.dissolveNoiseScale),
        mesh,
        material);
}
} // namespace YRender::IncinerationField
