#include "Camera.h"
#include "D3D12Util.h"

Camera::Camera()
: m_pitch(0.0f),
m_yaw(0.0f),
m_speed(0.05f)
{
	UpdateLook();
	m_viewMatrix = XMMatrixIdentity();
	m_projectionMatrix = XMMatrixIdentity();
	m_viewProjectionMatrix = XMMatrixIdentity();
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
