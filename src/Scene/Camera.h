#pragma once

#include "Core/Input.h"

#include <DirectXMath.h>

namespace YRender
{
class Camera
{
public:
    void Update(float dt, const InputState& input, bool acceptInput);
    DirectX::XMMATRIX ViewProjection(float aspect) const;

    DirectX::XMFLOAT3& Position() { return m_position; }
    const DirectX::XMFLOAT3& Position() const { return m_position; }
    float& Yaw() { return m_yaw; }
    float& Pitch() { return m_pitch; }
    float Yaw() const { return m_yaw; }
    float Pitch() const { return m_pitch; }

private:
    DirectX::XMFLOAT3 m_position{0.0f, 2.0f, -6.0f};
    float m_yaw = 0.0f;
    float m_pitch = -0.15f;
};
} // namespace YRender
