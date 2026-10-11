#pragma once

// Per-frame camera brain. Takes a snapshot of the game state and returns
// what the engine adapter should apply: player rotation (while locked or
// snapping to an attack target), third-person framing, and shake offsets.
//
// Fallout 4's third-person camera follows the player's heading, so lock-on
// works by steering the player's yaw and pitch. That keeps the vanilla
// camera, collision and animations intact and needs no camera hooks.

#include "witcher/math.h"
#include "witcher/settings.h"

#include <optional>

namespace witcher
{
	enum class CameraMode
	{
		kExploration,  // weapon holstered
		kCombat,       // ranged weapon drawn or in combat
		kMelee         // melee weapon or fists drawn
	};

	struct RigInput
	{
		float dt{ 0.0f };
		Vec3 eye{};
		float playerYaw{ 0.0f };
		float playerPitch{ 0.0f };
		CameraMode mode{ CameraMode::kExploration };

		std::optional<Vec3> lockTarget;  // set while hard-locked
		float lockTargetScale{ 1.0f };
		int nearbyHostiles{ 0 };

		bool attackStarted{ false };         // edge: an attack began this frame
		std::optional<Vec3> softLockTarget;  // best soft-lock target, if any
	};

	struct RigOutput
	{
		bool driveYaw{ false };
		bool drivePitch{ false };
		float yaw{ 0.0f };
		float pitch{ 0.0f };

		FramingPreset framing{};  // fov 0 = leave the game's FOV alone

		float shakeYaw{ 0.0f };   // additive, apply to the camera only
		float shakePitch{ 0.0f };
	};

	class CameraRig
	{
	public:
		CameraRig(const CameraSettings& a_camera, const SoftLockSettings& a_softLock);

		RigOutput Update(const RigInput& a_in);

		void SwapShoulder() { _rightShoulder = !_rightShoulder; }
		bool IsRightShoulder() const { return _rightShoulder; }

		// 0..1. Heavy hit ~0.5, staggered by a deathclaw ~0.9. Stacks, capped at 1.
		void AddTrauma(float a_amount);

		// Snap framing straight to the current mode (after loading a save or a teleport).
		void ResetFraming(CameraMode a_mode);

	private:
		const FramingPreset& PresetFor(CameraMode a_mode) const;
		void UpdateFraming(const RigInput& a_in, RigOutput& a_out);
		void UpdateRotation(const RigInput& a_in, RigOutput& a_out);
		void UpdateShake(float a_dt, RigOutput& a_out);

		const CameraSettings* _camera;
		const SoftLockSettings* _softLock;

		bool _rightShoulder{ true };
		float _shoulderSign{ 1.0f };
		float _shoulderVel{ 0.0f };

		FramingPreset _framing{};
		FramingPreset _framingVel{};
		bool _framingInit{ false };

		float _yawVel{ 0.0f };
		float _pitchVel{ 0.0f };
		bool _wasLocked{ false };

		float _snapRemaining{ 0.0f };
		float _snapYaw{ 0.0f };

		float _trauma{ 0.0f };
		float _shakeTime{ 0.0f };
	};
}
