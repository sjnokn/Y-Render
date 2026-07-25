#include "Scene/Camera.h"

#include <algorithm>
#include <cmath>

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

    const glm::vec3 viewForward = glm::normalize(m_target - m_position);
    const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), viewForward));
    glm::vec3 target = m_target;

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
        target += glm::vec3(0.0f, 1.0f, 0.0f) * moveSpeed;
    }
    if (input.IsDown('Q'))
    {
        target -= glm::vec3(0.0f, 1.0f, 0.0f) * moveSpeed;
    }
    if (std::abs(wheelDelta) > 0.0f)
    {
        m_distance *= std::pow(0.86f, wheelDelta);
    }

    m_distance = std::clamp(m_distance, 0.75f, 30.0f);
    m_target = target;
    UpdatePosition();
}

glm::mat4 Camera::ViewProjection(float aspect) const
{
    // Phase 4: column-vector convention. Projection is applied AFTER view, so
    // the column-vector product is projection*view (projection first, view
    // second) - the reverse of the old DX view*projection row-vector form.
    // Left-handed + [0,1] depth matches the previous XMMatrixPerspectiveFovLH /
    // XMMatrixLookAtLH behaviour exactly.
    const glm::mat4 view = glm::lookAtLH(m_position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspectiveLH_ZO(glm::radians(45.0f), aspect, 0.05f, 200.0f);
    return projection * view;
}

void Camera::SetOrbit(const glm::vec3& target, float distance, float yaw, float pitch)
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
    // The minus sign encodes the LH orbit (camera sits on -Z for yaw=0).
    // It MUST NOT be flipped.
    m_position.z = m_target.z - std::cos(m_yaw) * horizontalDistance;
}
} // namespace YRender
