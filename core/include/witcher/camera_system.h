#pragma once

// Top-level camera controller. The engine adapter fills a FrameSnapshot each
// frame, calls Tick, and applies the FrameResult. Everything in here is
// engine-free and covered by the tests in /tests.

#include "witcher/camera_rig.h"
#include "witcher/lock_on.h"
#include "witcher/settings.h"
#include "witcher/targeting.h"

#include <span>

namespace witcher
{
	struct FrameSnapshot
	{
		float dt{ 0.0f };
		View view{};  // eye = player head; yaw/pitch = player's current rotation
		CameraMode mode{ CameraMode::kExploration };
		std::span<const Candidate> candidates;

		float lookX{ 0.0f };          // horizontal look input this frame
		bool attackStarted{ false };  // edge
		bool lockPressed{ false };    // edge
		bool shoulderPressed{ false };  // edge
		float trauma{ 0.0f };         // camera shake to add this frame (from hits taken/dealt)
	};

	struct FrameResult
	{
		RigOutput rig{};
		LockEvent lockEvent{ LockEvent::kNone };
		ActorId lockTarget{ 0 };
	};

	class CameraSystem
	{
	public:
		explicit CameraSystem(const Settings& a_settings);

		FrameResult Tick(const FrameSnapshot& a_frame);

		// Drop the lock and snap framing (load game, fast travel, entering a menu-driven scene).
		void Reset(CameraMode a_mode);

		const LockOnController& LockOn() const { return _lockOn; }
		const CameraRig& Rig() const { return _rig; }

	private:
		const Settings* _settings;
		LockOnController _lockOn;
		CameraRig _rig;
	};
}
