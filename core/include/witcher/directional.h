#pragma once

// True directional movement: the camera orbits freely and the character
// turns to face the way it's running, instead of strafing and backpedalling.
//
// The engine side has two hooks:
//   - each frame, Update() decides the character's heading and how far the
//     camera is rotated away from it (ThirdPersonState free rotation);
//   - when the movement system reads the player's input, RemapMoveInput()
//     turns "camera-relative" input into "character-relative" input.

#include "witcher/camera_rig.h"
#include "witcher/math.h"
#include "witcher/settings.h"

#include <optional>

namespace witcher
{
	struct DirectionalInput
	{
		float dt{ 0.0f };
		bool allowed{ false };  // third person, feature on, not in a menu or scene
		CameraMode mode{ CameraMode::kExploration };
		Vec2 move{};            // raw movement input: x right, y forward, relative to the camera
		float cameraYaw{ 0.0f };  // where the camera looks (world)
		float bodyYaw{ 0.0f };    // where the character faces (world)
		bool attackStarted{ false };
		bool locked{ false };
		std::optional<float> forcedYaw;  // lock-on or soft-lock turn from the camera rig, already smoothed
	};

	struct DirectionalOutput
	{
		bool active{ false };
		bool driveBody{ false };  // write bodyYaw to the character this frame
		float bodyYaw{ 0.0f };
		float cameraOffset{ 0.0f };  // camera yaw minus body yaw; becomes ThirdPersonState::freeRotation.x
	};

	class DirectionalController
	{
	public:
		explicit DirectionalController(const DirectionalSettings& a_settings) :
			_settings(&a_settings)
		{}

		DirectionalOutput Update(const DirectionalInput& a_in);
		void Reset();

	private:
		const DirectionalSettings* _settings;
		float _snapRemaining{ 0.0f };
		float _snapYaw{ 0.0f };
		float _holdRemaining{ 0.0f };
		float _offsetVel{ 0.0f };
	};

	// Movement input is read relative to the character's facing. The player
	// means it relative to the camera, so rotate it by the camera/body gap.
	// Running straight at the camera while facing it becomes plain "forward".
	Vec2 RemapMoveInput(Vec2 a_move, float a_cameraYaw, float a_bodyYaw);
}
