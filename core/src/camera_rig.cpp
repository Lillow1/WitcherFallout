#include "witcher/camera_rig.h"

#include <algorithm>
#include <cmath>

namespace witcher
{
	CameraRig::CameraRig(const CameraSettings& a_camera, const SoftLockSettings& a_softLock) :
		_camera(&a_camera),
		_softLock(&a_softLock)
	{}

	RigOutput CameraRig::Update(const RigInput& a_in)
	{
		RigOutput out{};
		out.yaw = a_in.playerYaw;
		out.pitch = a_in.playerPitch;

		UpdateFraming(a_in, out);
		UpdateRotation(a_in, out);
		UpdateShake(a_in.dt, out);
		return out;
	}

	void CameraRig::AddTrauma(float a_amount)
	{
		_trauma = std::clamp(_trauma + a_amount, 0.0f, 1.0f);
	}

	void CameraRig::ResetFraming(CameraMode a_mode)
	{
		_framing = PresetFor(a_mode);
		_framingVel = {};
		_framingInit = true;
	}

	const FramingPreset& CameraRig::PresetFor(CameraMode a_mode) const
	{
		switch (a_mode) {
		case CameraMode::kCombat:
			return _camera->combat;
		case CameraMode::kMelee:
			return _camera->melee;
		case CameraMode::kExploration:
		default:
			return _camera->exploration;
		}
	}

	void CameraRig::UpdateFraming(const RigInput& a_in, RigOutput& a_out)
	{
		if (!_framingInit) {
			ResetFraming(a_in.mode);
		}

		FramingPreset goal = PresetFor(a_in.mode);

		// Pull back for crowds and for big creatures so the fight stays readable.
		if (a_in.mode != CameraMode::kExploration) {
			const float extra = static_cast<float>(std::max(0, a_in.nearbyHostiles - 1));
			goal.distance += std::min(extra * _camera->groupZoomPerEnemy, _camera->groupZoomMax);
		}
		if (a_in.lockTarget) {
			const float t = std::clamp(a_in.lockTargetScale - 1.0f, 0.0f, 1.0f);
			goal.distance += t * _camera->largeTargetZoom;
		}

		const float dt = a_in.dt;
		const float blend = _camera->framingBlendTime;
		_shoulderSign = SmoothDamp(_shoulderSign, _rightShoulder ? 1.0f : -1.0f, _shoulderVel, blend * 0.6f, dt);
		_framing.shoulderX = SmoothDamp(_framing.shoulderX, goal.shoulderX, _framingVel.shoulderX, blend, dt);
		_framing.shoulderZ = SmoothDamp(_framing.shoulderZ, goal.shoulderZ, _framingVel.shoulderZ, blend, dt);
		_framing.distance = SmoothDamp(_framing.distance, goal.distance, _framingVel.distance, blend, dt);

		// fov 0 means "don't touch". Only blend between two real values.
		if (goal.fov <= 0.0f) {
			_framing.fov = 0.0f;
			_framingVel.fov = 0.0f;
		} else if (_framing.fov <= 0.0f) {
			_framing.fov = goal.fov;
		} else {
			_framing.fov = SmoothDamp(_framing.fov, goal.fov, _framingVel.fov, blend, dt);
		}

		a_out.framing = _framing;
		a_out.framing.shoulderX *= _shoulderSign;
	}

	void CameraRig::UpdateRotation(const RigInput& a_in, RigOutput& a_out)
	{
		const float dt = a_in.dt;

		if (a_in.lockTarget) {
			if (!_wasLocked) {
				_yawVel = 0.0f;
				_pitchVel = 0.0f;
			}
			_wasLocked = true;
			_snapRemaining = 0.0f;

			const Vec3 target = *a_in.lockTarget;
			float goalYaw = a_in.playerYaw;
			// Re-aiming at point-blank range makes the camera whip around as
			// the target steps past you; hold the heading instead.
			if ((target - a_in.eye).Length2D() >= _camera->lockMinDistance) {
				goalYaw = YawTo(a_in.eye, target);
			}
			const float goalPitch = std::clamp(
				PitchTo(a_in.eye, target) + DegToRad(_camera->lockPitchBiasDeg),
				DegToRad(_camera->lockPitchMinDeg),
				DegToRad(_camera->lockPitchMaxDeg));

			a_out.driveYaw = true;
			a_out.drivePitch = true;
			a_out.yaw = SmoothDampAngle(a_in.playerYaw, goalYaw, _yawVel, _camera->lockYawSmoothTime, dt);
			a_out.pitch = SmoothDamp(a_in.playerPitch, goalPitch, _pitchVel, _camera->lockPitchSmoothTime, dt);
			return;
		}
		_wasLocked = false;

		// Soft lock: when an attack starts near an enemy, turn into it quickly.
		if (_softLock->enabled && a_in.attackStarted && a_in.softLockTarget) {
			_snapYaw = YawTo(a_in.eye, *a_in.softLockTarget);
			_snapRemaining = std::max(0.001f, _softLock->snapTime);
		}
		if (_snapRemaining > 0.0f) {
			// Close a share of the remaining arc each frame so the turn lands
			// exactly when the snap time runs out, at any frame rate.
			const float t = std::min(1.0f, dt / _snapRemaining);
			a_out.driveYaw = true;
			a_out.yaw = WrapAngle(a_in.playerYaw + AngleDelta(a_in.playerYaw, _snapYaw) * t);
			_snapRemaining = std::max(0.0f, _snapRemaining - dt);
		}
	}

	void CameraRig::UpdateShake(float a_dt, RigOutput& a_out)
	{
		if (_trauma <= 0.0f) {
			return;
		}
		_shakeTime += a_dt;
		const float strength = _trauma * _trauma;  // squared feels better than linear
		const float t = _shakeTime * _camera->shakeFrequency;
		// Cheap smooth noise: incommensurate sines.
		const float nx = 0.6f * std::sin(t * 1.00f) + 0.4f * std::sin(t * 2.31f + 1.7f);
		const float ny = 0.6f * std::sin(t * 1.13f + 4.2f) + 0.4f * std::sin(t * 2.79f + 0.3f);
		a_out.shakeYaw = DegToRad(_camera->shakeMaxYawDeg) * strength * nx;
		a_out.shakePitch = DegToRad(_camera->shakeMaxPitchDeg) * strength * ny;
		_trauma = std::max(0.0f, _trauma - _camera->shakeDecay * a_dt);
	}
}
