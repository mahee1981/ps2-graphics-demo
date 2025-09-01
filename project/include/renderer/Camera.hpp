#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "VU0Math/vec4.hpp"
#include "VU0Math/mat4.hpp"
#include "input/padman.hpp"
#include "utils.hpp"

#include <cmath>


/*
This camera system is a fly like camera that suits most purposes and works well with Euler angles,
but be careful when creating different camera systems like an FPS camera, or a flight simulation camera.
Each camera system has its own tricks and quirks so be sure to read up on them.
For example, this fly camera doesn't allow for pitch values higher than or equal to 90 degrees
and a static up vector of (0,1,0) doesn't work when we take roll values into account.
*/
class Camera
{
public:
	Camera(ps2math::Vec4 startPosition, ps2math::Vec4 startUp, float startYaw, float startPitch, float movementSpeed, float turnSpeed);

	void MotionControl(const Input::PadJoy &leftAnalogStick, float deltaTime);
	void RotationControl(const Input::PadJoy &rightAnalogStick, float deltaTime);

	// ps2math::Vec4 GetCameraPosition() const;
	// ps2math::Vec4 GetCameraDirection() const;
	ps2math::Mat4 CalculateViewMatrix() const;

private:
	ps2math::Vec4 _position; //world position of the camera

	ps2math::Vec4 _front; // where is camera looking at
	ps2math::Vec4 _up; // up from camera
	ps2math::Vec4 _right; // right of camera

	ps2math::Vec4 _worldUp; // where the sky is in the world

	float _yaw;
	float _pitch;
	float _movementSpeed;
	float _turnSpeed;

	void update();

};

#endif
