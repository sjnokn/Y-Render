#pragma once

#include "Core/Input.h"

#include <DirectXMath.h>

namespace YRender
{
class Camera
{
public:
    void Update(float dt, const InputState& input, bool acceptInput, float lookDeltaX = 0.0f, float lookDeltaY = 0.0f, float wheelDelta = 0.0f);
    DirectX::XMMATRIX ViewProjection(float aspect) const;
    void SetOrbit(const DirectX::XMFLOAT3& target, float distance, float yaw, float pitch);

    const DirectX::XMFLOAT3& Position() const { return m_position; }
    const DirectX::XMFLOAT3& Target() const { return m_target; }
    float Distance() const { return m_distance; }
    float& Yaw() { return m_yaw; }
    float& Pitch() { return m_pitch; }
    float Yaw() const { return m_yaw; }
    float Pitch() const { return m_pitch; }

private:
    void UpdatePosition();

    DirectX::XMFLOAT3 m_position{0.0f, 0.25f, -4.0f};
    DirectX::XMFLOAT3 m_target{0.0f, 0.25f, 0.0f};
    float m_distance = 4.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
};
} // namespace YRender
