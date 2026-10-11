#pragma once

// The only code that touches game memory. Reads a snapshot of the player and
// nearby actors, and writes the camera system's output back.

#include "PCH.h"

#include "witcher/camera_rig.h"
#include "witcher/directional.h"
#include "witcher/targeting.h"

#include <vector>

namespace witcher::plugin
{
	// Camera yaw = body yaw + kFreeRotationSign * ThirdPersonState::freeRotation.x.
	// Flip to -1 if, with directional movement on, the camera turns the
	// opposite way to the mouse or swings away while the character turns.
	inline constexpr float kFreeRotationSign = 1.0f;

	struct GameState
	{
		View view{};      // view.yaw = camera yaw
		float bodyYaw{ 0.0f };
		CameraMode mode{ CameraMode::kExploration };
		bool thirdPerson{ false };
		Vec2 move{};      // raw movement input (PlayerControls::data.moveInputVec)
		std::vector<Candidate> candidates;
	};

	// False when there's no player or camera yet (main menu, loading).
	bool ReadGameState(GameState& a_out, float a_searchRadius);

	// The active third-person camera state, or null in any other camera state.
	RE::ThirdPersonState* CurrentThirdPerson();

	class Applier
	{
	public:
		// Player rotation (lock-on, attack snap) plus shake.
		void ApplyRotation(const RigOutput& a_rig);

		// Third-person shoulder offset, pull-back and FOV.
		void ApplyFraming(const RigOutput& a_rig, bool a_thirdPerson);

		// Free-rotation camera offset for directional movement. When it turns
		// off, folds the offset back into the body so the view doesn't jump.
		void ApplyDirectional(const DirectionalOutput& a_dir);

		// Forget shake and FOV state (after a load) without touching the game.
		void Reset();

	private:
		float _lastShakeYaw{ 0.0f };
		float _lastShakePitch{ 0.0f };
		float _savedFov{ 0.0f };  // vanilla FOV while we override it, 0 = not overriding
		bool _ownsFreeRotation{ false };
	};
}
