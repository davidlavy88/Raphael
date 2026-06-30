#pragma once
#include "DirectXMath.h"

class Camera
{
public:
    Camera() = default;

    void Initialize(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& up, float pitch, float yaw, float speed);
    void Reset();

    DirectX::XMVECTOR GetPosition() const { return m_position; }

    DirectX::XMMATRIX GetViewMatrix() const { return m_viewMatrix; }
    DirectX::XMMATRIX GetProjectionMatrix() const { return m_projectionMatrix; }
    DirectX::XMMATRIX GetViewProjectionMatrix() const { return m_viewProjectionMatrix; }

    DirectX::XMVECTOR GetUp() const { return m_up; }
    DirectX::XMVECTOR GetLook() const { return m_look; }

    float GetPitch() const { return m_pitch; }
    float GetYaw() const { return m_yaw; }

    float GetSpeed() const { return m_speed; }

    void SetPosition(const DirectX::XMVECTOR& position) { m_position = position; }

    void SetUp(const DirectX::XMVECTOR& up) { m_up = up; }
    void SetLook(const DirectX::XMVECTOR& look) { m_look = look; }
    void UpdateLook();

    void UpdateViewMatrix();
    void SetProjectionMatrix(float fovY, float aspectRatio, float nearZ, float farZ);
    void SetProjectionMatrix(const DirectX::XMMATRIX& projectionMatrix) { m_projectionMatrix = projectionMatrix; }

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
    DirectX::XMMATRIX m_viewMatrix;
    DirectX::XMMATRIX m_projectionMatrix;
    DirectX::XMMATRIX m_viewProjectionMatrix;
    
    DirectX::XMVECTOR m_position;
    DirectX::XMVECTOR m_up;
    DirectX::XMVECTOR m_look;

    float m_pitch;
    float m_yaw;

    float m_speed;

    // Initial values for Reset
    DirectX::XMVECTOR m_initialPosition;
    DirectX::XMVECTOR m_initialUp;
    float m_initialPitch;
    float m_initialYaw;
    float m_initialSpeed;
};

