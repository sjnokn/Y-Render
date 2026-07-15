#include "Scene/Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace YRender
{
void Camera::Update(float dt, const InputState& input, bool acceptInput, float lookDeltaX, float lookDeltaY, float wheelDelta)
{
    if (!acceptInput)
    {
        return;
    }

    const float moveSpeed = (input.IsDown(VK_SHIFT) ? 3.0f : 1.25f) * dt;
    const float lookSpeed = 1.6f * dt;
    const float mouseLookSpeed = 0.004f;

    m_yaw += lookDeltaX * mouseLookSpeed;
    m_pitch += lookDeltaY * mouseLookSpeed;
    if (input.IsDown(VK_LEFT))
    {
        m_yaw -= lookSpeed;
    }
    if (input.IsDown(VK_RIGHT))
    {
        m_yaw += lookSpeed;
    }
    if (input.IsDown(VK_UP))
    {
        m_pitch -= lookSpeed;
    }
    if (input.IsDown(VK_DOWN))
    {
        m_pitch += lookSpeed;
    }
    m_pitch = std::clamp(m_pitch, -1.45f, 1.45f);

    const XMVECTOR viewForward = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&m_target), XMLoadFloat3(&m_position)));
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), viewForward));
    XMVECTOR target = XMLoadFloat3(&m_target);

    if (input.IsDown('W'))
    {
        m_distance -= moveSpeed * 2.0f;
    }
    if (input.IsDown('S'))
    {
        m_distance += moveSpeed * 2.0f;
    }
    if (input.IsDown('D'))
    {
        target += right * moveSpeed;
    }
    if (input.IsDown('A'))
    {
        target -= right * moveSpeed;
    }
    if (input.IsDown('E'))
    {
        target += XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * moveSpeed;
    }
    if (input.IsDown('Q'))
    {
        target -= XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * moveSpeed;
    }
    if (std::abs(wheelDelta) > 0.0f)
    {
        m_distance *= std::pow(0.86f, wheelDelta);
    }

    m_distance = std::clamp(m_distance, 0.75f, 30.0f);
    XMStoreFloat3(&m_target, target);
    UpdatePosition();
}

XMMATRIX Camera::ViewProjection(float aspect) const
{
    const XMVECTOR position = XMLoadFloat3(&m_position);
    const XMMATRIX view = XMMatrixLookAtLH(position, XMLoadFloat3(&m_target), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspect, 0.05f, 200.0f);
    return view * projection;
}

void Camera::SetOrbit(const XMFLOAT3& target, float distance, float yaw, float pitch)
{
    m_target = target;
    m_distance = std::clamp(distance, 0.75f, 30.0f);
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, -1.45f, 1.45f);
    UpdatePosition();
}

void Camera::UpdatePosition()
{
    const float horizontalDistance = std::cos(m_pitch) * m_distance;
    m_position.x = m_target.x + std::sin(m_yaw) * horizontalDistance;
    m_position.y = m_target.y + std::sin(m_pitch) * m_distance;
    m_position.z = m_target.z - std::cos(m_yaw) * horizontalDistance;
}
} // namespace YRender
