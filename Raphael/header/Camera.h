#pragma once
#include "DirectXMath.h"

using namespace DirectX;

class Camera
{
public:
	Camera();

	XMVECTOR GetPosition() { return m_position; }

	XMMATRIX GetViewMatrix() const { return m_viewMatrix; }
	XMMATRIX GetProjectionMatrix() const { return m_projectionMatrix; }
	XMMATRIX GetViewProjectionMatrix() const { return m_viewProjectionMatrix; }

	XMVECTOR GetUp() const { return m_up; }
	XMVECTOR GetLook() const { return m_look; }

	float GetPitch() const { return m_pitch; }
	float GetYaw() const { return m_yaw; }

	float GetSpeed() const { return m_speed; }

	void SetPosition(const XMVECTOR& position) { m_position = position; }

	void SetUp(const XMVECTOR& up) { m_up = up; }
	void SetLook(const XMVECTOR& look) { m_look = look; }
	void UpdateLook();

	void UpdateViewMatrix();
	void SetProjectionMatrix(float fovY, float aspectRatio, float nearZ, float farZ);
	void SetProjectionMatrix(const XMMATRIX& projectionMatrix) { m_projectionMatrix = projectionMatrix; }

	void SetPitch(float pitch);
	void SetYaw(float yaw) { m_yaw = yaw; }

	// Set camera speed	
	void SetSpeed(float speed) { m_speed = speed; }

	// Camera movement methods
	void MoveForward();
	void MoveBackward();
	void MoveLeft();
	void MoveRight();
	void MoveUpDown(float delta);


private:
	XMMATRIX m_viewMatrix;
	XMMATRIX m_projectionMatrix;
	XMMATRIX m_viewProjectionMatrix;
	
	XMVECTOR m_position;
	XMVECTOR m_up;
	XMVECTOR m_look;

	float m_pitch;
	float m_yaw;

	float m_speed;
};

