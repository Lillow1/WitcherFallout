#include "witcher/camera_system.h"

namespace witcher
{
	CameraSystem::CameraSystem(const Settings& a_settings) :
		_settings(&a_settings),
		_lockOn(a_settings.lockOn),
		_rig(a_settings.camera, a_settings.softLock),
		_directional(a_settings.directional),
		_directionalOn(a_settings.directional.enabled)
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

		const float bodyYaw = a_frame.bodyYaw.value_or(a_frame.view.yaw);

		RigInput in{};
		in.dt = a_frame.dt;
		in.eye = a_frame.view.eye;
		in.playerYaw = bodyYaw;
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

		if (a_frame.directionalTogglePressed) {
			_directionalOn = !_directionalOn;
		}

		DirectionalInput dir{};
		dir.dt = a_frame.dt;
		dir.allowed = a_frame.directionalAllowed && _directionalOn;
		dir.mode = a_frame.mode;
		dir.move = a_frame.move;
		dir.cameraYaw = a_frame.view.yaw;
		dir.bodyYaw = bodyYaw;
		dir.attackStarted = a_frame.attackStarted;
		dir.locked = in.lockTarget.has_value();
		if (result.rig.driveYaw) {
			dir.forcedYaw = result.rig.yaw;
		}
		result.directional = _directional.Update(dir);

		// Directional movement owns the body heading while it's active.
		if (result.directional.active) {
			result.rig.driveYaw = result.directional.driveBody;
			result.rig.yaw = result.directional.bodyYaw;
		}
		return result;
	}

	void CameraSystem::Reset(CameraMode a_mode)
	{
		_lockOn.Release();
		_rig.ResetFraming(a_mode);
		_directional.Reset();
	}
}
