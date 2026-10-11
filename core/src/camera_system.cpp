#include "witcher/camera_system.h"

namespace witcher
{
	CameraSystem::CameraSystem(const Settings& a_settings) :
		_settings(&a_settings),
		_lockOn(a_settings.lockOn),
		_rig(a_settings.camera, a_settings.softLock)
	{}

	FrameResult CameraSystem::Tick(const FrameSnapshot& a_frame)
	{
		FrameResult result{};
		const auto& cands = a_frame.candidates;

		// Lock-on is a combat tool: holstering drops it, and it can't start out of combat.
		if (a_frame.mode == CameraMode::kExploration) {
			if (_lockOn.IsLocked()) {
				_lockOn.Release();
				result.lockEvent = LockEvent::kReleased;
			}
		} else if (a_frame.lockPressed) {
			result.lockEvent = _lockOn.Toggle(a_frame.view, cands);
		}

		if (const auto ev = _lockOn.Update(a_frame.dt, a_frame.view, cands, a_frame.lookX); ev != LockEvent::kNone) {
			result.lockEvent = ev;
		}

		if (a_frame.shoulderPressed) {
			_rig.SwapShoulder();
		}
		if (a_frame.trauma > 0.0f) {
			_rig.AddTrauma(a_frame.trauma);
		}

		RigInput in{};
		in.dt = a_frame.dt;
		in.eye = a_frame.view.eye;
		in.playerYaw = a_frame.view.yaw;
		in.playerPitch = a_frame.view.pitch;
		in.mode = a_frame.mode;
		in.attackStarted = a_frame.attackStarted;

		for (const auto& c : cands) {
			if (c.hostile && !c.dead && (c.aimPoint - a_frame.view.eye).Length() <= _settings->camera.groupRadius) {
				++in.nearbyHostiles;
			}
		}

		if (const auto idx = Find(cands, _lockOn.Target())) {
			in.lockTarget = cands[*idx].aimPoint;
			in.lockTargetScale = cands[*idx].scale;
		} else if (a_frame.attackStarted && a_frame.mode != CameraMode::kExploration) {
			if (const auto soft = SelectBest(a_frame.view, cands, _settings->softLock.targeting)) {
				in.softLockTarget = cands[*soft].aimPoint;
			}
		}

		result.rig = _rig.Update(in);
		result.lockTarget = _lockOn.Target();
		return result;
	}

	void CameraSystem::Reset(CameraMode a_mode)
	{
		_lockOn.Release();
		_rig.ResetFraming(a_mode);
	}
}
