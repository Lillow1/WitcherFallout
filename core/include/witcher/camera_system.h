#pragma once

// Top-level camera controller. The engine adapter fills a FrameSnapshot each
// frame, calls Tick, and applies the FrameResult. Everything in here is
// engine-free and covered by the tests in /tests.

#include "witcher/camera_rig.h"
#include "witcher/directional.h"
#include "witcher/lock_on.h"
#include "witcher/settings.h"
#include "witcher/targeting.h"

#include <optional>
#include <span>

namespace witcher
{
	struct FrameSnapshot
	{
		float dt{ 0.0f };
		View view{};  // eye = player head; yaw = where the CAMERA looks (used for targeting); pitch = player pitch
		std::optional<float> bodyYaw;  // where the character faces, if different from view.yaw
		CameraMode mode{ CameraMode::kExploration };
		std::span<const Candidate> candidates;

		float lookX{ 0.0f };          // horizontal look input this frame
		bool attackStarted{ false };  // edge
		bool lockPressed{ false };    // edge
		bool shoulderPressed{ false };  // edge
		float trauma{ 0.0f };         // camera shake to add this frame (from hits taken/dealt)

		bool directionalAllowed{ false };  // third person, not in a scene
		bool directionalTogglePressed{ false };  // edge
		Vec2 move{};                       // raw movement input, camera-relative
	};

	struct FrameResult
	{
		RigOutput rig{};
		LockEvent lockEvent{ LockEvent::kNone };
		ActorId lockTarget{ 0 };
		DirectionalOutput directional{};
	};

	class CameraSystem
	{
	public:
		explicit CameraSystem(const Settings& a_settings);

		FrameResult Tick(const FrameSnapshot& a_frame);

		// Drop the lock and snap framing (load game, fast travel, entering a menu-driven scene).
		void Reset(CameraMode a_mode);

		bool DirectionalEnabled() const { return _directionalOn; }
		const LockOnController& LockOn() const { return _lockOn; }
		const CameraRig& Rig() const { return _rig; }

	private:
		const Settings* _settings;
		LockOnController _lockOn;
		CameraRig _rig;
		DirectionalController _directional;
		bool _directionalOn;
	};
}
