#include "Camera.h"
#include "D3D12Util.h"

void Camera::Initialize(const XMVECTOR& position, const XMVECTOR& up, float pitch, float yaw, float speed)
{
    m_initialPosition = position;
    m_initialUp = up;
    m_initialPitch = pitch;
    m_initialYaw = yaw;
    m_initialSpeed = speed;

    m_position = position;
    m_up = up;
    m_pitch = pitch;
    m_yaw = yaw;
    m_speed = speed;

    UpdateLook();
}

void Camera::Reset()
{
    m_position = m_initialPosition;
    m_up = m_initialUp;
    m_pitch = m_initialPitch;
    m_yaw = m_initialYaw;
    m_speed = m_initialSpeed;

    UpdateLook();
    UpdateViewMatrix();
}

void Camera::UpdateLook()
{
    m_look = XMVectorSet(
        cosf(m_pitch) * sinf(m_yaw),
        sinf(m_pitch),
        cosf(m_pitch) * cosf(m_yaw),
        0.0f
    );
}

void Camera::UpdateViewMatrix()
{
    m_viewMatrix = XMMatrixLookAtLH(m_position, m_position + m_look, m_up);
    // Update view-projection matrix
    m_viewProjectionMatrix = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);
}

void Camera::SetProjectionMatrix(float fovY, float aspectRatio, float nearZ, float farZ)
{
    m_projectionMatrix = XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearZ, farZ);
    // Update view-projection matrix
    m_viewProjectionMatrix = XMMatrixMultiply(m_viewMatrix, m_projectionMatrix);
}

void Camera::SetPitch(float pitch)
{
    m_pitch = pitch;
    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    m_pitch = D3D12Util::Clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);
}

void Camera::MoveForward()
{
    m_position += m_look * m_speed;
}

void Camera::MoveBackward()
{
    m_position -= m_look * m_speed;
}

void Camera::MoveLeft()
{
    m_position += XMVector3Normalize(XMVector3Cross(m_look, m_up)) * m_speed;
}

void Camera::MoveRight()
{
    m_position -= XMVector3Normalize(XMVector3Cross(m_look, m_up)) * m_speed;
}

void Camera::MoveUpDown(float delta)
{
    m_position += m_up * delta * m_speed;
}
