#pragma once

// The only code that touches game memory. Reads a snapshot of the player and
// nearby actors, and writes the camera system's output back.

#include "PCH.h"

#include "witcher/camera_rig.h"
#include "witcher/targeting.h"

#include <vector>

namespace witcher::plugin
{
	struct GameState
	{
		View view{};
		CameraMode mode{ CameraMode::kExploration };
		bool thirdPerson{ false };
		std::vector<Candidate> candidates;
		std::vector<RE::ActorHandle> handles;  // handles[i] is candidates[i]'s actor
	};

	// False when there's no player or camera yet (main menu, loading).
	bool ReadGameState(GameState& a_out, float a_searchRadius);

	class Applier
	{
	public:
		// Player rotation (lock-on, attack snap) plus shake.
		void ApplyRotation(const RigOutput& a_rig);

		// Third-person shoulder offset, pull-back and FOV.
		void ApplyFraming(const RigOutput& a_rig, bool a_thirdPerson);

		// Forget shake and FOV state (after a load) without touching the game.
		void Reset();

	private:
		float _lastShakeYaw{ 0.0f };
		float _lastShakePitch{ 0.0f };
		float _savedFov{ 0.0f };  // vanilla FOV while we override it, 0 = not overriding
	};
}
