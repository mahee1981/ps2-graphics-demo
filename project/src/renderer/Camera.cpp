#include "renderer/Camera.hpp"



Camera::Camera(ps2math::Vec4 startPosition, ps2math::Vec4 startUp, float startYaw, float startPitch, float movementSpeed, float turnSpeed)
: _position(startPosition)
, _front(0.0f, 0.0f, -1.0f, 1.0f)
, _worldUp(startUp)
, _yaw(startYaw)
, _pitch(startPitch)
, _movementSpeed(movementSpeed)
, _turnSpeed(turnSpeed)
{
    update();
}

void Camera::MotionControl(const Input::PadJoy &leftStick, float deltaTime)
{
	// Normalize stick [-1,1]
	float x = (float(leftStick.h) - 127.0f) / 127.0f; // strafe
	float y = (float(leftStick.v) - 127.0f) / 127.0f; // forward/back

	// Apply deadzone
	constexpr float deadzone = 0.1f;
	float magnitude = sqrtf(x * x + y * y);

	if (magnitude < deadzone)
		return;

	float scale = (magnitude - deadzone) / (1.0f - deadzone);
	x = (x / magnitude) * scale;
	y = (y / magnitude) * scale;

	// Camera-space movement
	ps2math::Vec4 moveDir = (_right * (-x) + _front * y).Normalize();

	// Apply movement
	_position += moveDir * (_movementSpeed * deltaTime * magnitude);

}

void Camera::RotationControl(const Input::PadJoy &rightStick, float deltaTime)
{
	// Normalize stick input to [-1, 1], center at 127.5
    float horzInput = (float(rightStick.h) - 127.5f) / 127.5f;
    float vertInput = (float(rightStick.v) - 127.5f) / 127.5f;

    // Deadzone to avoid drift
    constexpr float deadzone = 0.1f;
    if (fabs(horzInput) < deadzone) horzInput = 0.0f;
    if (fabs(vertInput) < deadzone) vertInput = 0.0f;

    // Apply rotation with speed and deltaTime
    _yaw   -= horzInput * _turnSpeed * deltaTime;
    _pitch -= vertInput * _turnSpeed * deltaTime;

    // Clamp pitch to avoid flipping
    if (_pitch > 89.0f) _pitch = 89.0f;
    if (_pitch < -89.0f) _pitch = -89.0f;

    update();
}

ps2math::Mat4 Camera::CalculateViewMatrix() const
{
    return ps2math::Mat4::LookAt(_position, _position + _front, _up).Transpose();

}

void Camera::update()
{
    _front.x = cosf(Utils::ToRadians(_yaw)) * cosf(Utils::ToRadians(_pitch));
	_front.y = sinf(Utils::ToRadians(_pitch));
	_front.z = sinf(Utils::ToRadians(_yaw)) * cosf(Utils::ToRadians(_pitch));
	_front.w = 0.0f;


	_front = _front.Normalize();
	_right = ps2math::CrossProduct(_worldUp, _front).Normalize();
	_up = ps2math::CrossProduct(_front, _right).Normalize();
}



