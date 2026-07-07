#include "Scene/Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace YRender
{
void Camera::Update(float dt, const InputState& input, bool acceptInput)
{
    if (!acceptInput)
    {
        return;
    }

    const float moveSpeed = (input.IsDown(VK_SHIFT) ? 8.0f : 3.5f) * dt;
    const float lookSpeed = 1.6f * dt;
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
        m_pitch += lookSpeed;
    }
    if (input.IsDown(VK_DOWN))
    {
        m_pitch -= lookSpeed;
    }
    m_pitch = std::clamp(m_pitch, -1.45f, 1.45f);

    XMVECTOR forward = XMVector3Normalize(XMVectorSet(std::sin(m_yaw), 0.0f, std::cos(m_yaw), 0.0f));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward));
    XMVECTOR position = XMLoadFloat3(&m_position);

    if (input.IsDown('W'))
    {
        position += forward * moveSpeed;
    }
    if (input.IsDown('S'))
    {
        position -= forward * moveSpeed;
    }
    if (input.IsDown('D'))
    {
        position += right * moveSpeed;
    }
    if (input.IsDown('A'))
    {
        position -= right * moveSpeed;
    }
    if (input.IsDown('E'))
    {
        position += XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * moveSpeed;
    }
    if (input.IsDown('Q'))
    {
        position -= XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * moveSpeed;
    }

    XMStoreFloat3(&m_position, position);
}

XMMATRIX Camera::ViewProjection(float aspect) const
{
    const XMVECTOR position = XMLoadFloat3(&m_position);
    const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
        std::cos(m_pitch) * std::sin(m_yaw),
        std::sin(m_pitch),
        std::cos(m_pitch) * std::cos(m_yaw),
        0.0f));
    const XMMATRIX view = XMMatrixLookToLH(position, forward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.05f, 200.0f);
    return view * projection;
}
} // namespace YRender
