#include "witcher/directional.h"

#include <algorithm>
#include <cmath>

namespace witcher
{
	DirectionalOutput DirectionalController::Update(const DirectionalInput& a_in)
	{
		DirectionalOutput out{};
		out.bodyYaw = a_in.bodyYaw;

		if (!a_in.allowed) {
			Reset();
			return out;
		}
		out.active = true;

		const float dt = a_in.dt;
		const float magnitude = a_in.move.Length();
		const bool moving = magnitude >= _settings->moveDeadzone;
		const float moveYaw = WrapAngle(a_in.cameraYaw + std::atan2(a_in.move.x, a_in.move.y));
		const bool aiming = a_in.mode == CameraMode::kCombat && _settings->rangedFacesCamera;

		if (a_in.attackStarted && !aiming) {
			_holdRemaining = _settings->attackHoldTime;
			if (!a_in.forcedYaw) {
				// No target to snap to: swing where you're heading, or where you're looking.
				_snapYaw = moving ? moveYaw : a_in.cameraYaw;
				_snapRemaining = std::max(0.001f, _settings->attackSnapTime);
			}
		}

		float body = a_in.bodyYaw;
		bool drive = false;

		if (a_in.forcedYaw) {
			body = *a_in.forcedYaw;
			drive = true;
			_snapRemaining = 0.0f;
		} else if (aiming) {
			body = TurnToward(body, a_in.cameraYaw, DegToRad(_settings->aimTurnRateDeg) * dt);
			drive = true;
		} else if (_snapRemaining > 0.0f) {
			// Close a share of the remaining arc so the turn lands exactly on time.
			const float t = std::min(1.0f, dt / _snapRemaining);
			body = WrapAngle(body + AngleDelta(body, _snapYaw) * t);
			drive = true;
		} else if (_holdRemaining <= 0.0f && moving) {
			body = TurnToward(body, moveYaw, DegToRad(_settings->turnRateDeg) * dt);
			drive = true;
		}

		_snapRemaining = std::max(0.0f, _snapRemaining - dt);
		_holdRemaining = std::max(0.0f, _holdRemaining - dt);

		// Keep the camera where it is in the world while the body turns under it,
		// except when locked on: then swing it back behind the character.
		float offset = AngleDelta(body, a_in.cameraYaw);
		if (a_in.locked) {
			offset = SmoothDampAngle(offset, 0.0f, _offsetVel, _settings->lockRecenterTime, dt);
		} else {
			_offsetVel = 0.0f;
		}

		out.driveBody = drive;
		out.bodyYaw = body;
		out.cameraOffset = offset;
		return out;
	}

	void DirectionalController::Reset()
	{
		_snapRemaining = 0.0f;
		_holdRemaining = 0.0f;
		_offsetVel = 0.0f;
	}

	Vec2 RemapMoveInput(Vec2 a_move, float a_cameraYaw, float a_bodyYaw)
	{
		const float magnitude = std::min(1.0f, a_move.Length());
		if (magnitude < 1e-4f) {
			return a_move;
		}
		const float world = a_cameraYaw + std::atan2(a_move.x, a_move.y);
		const float local = AngleDelta(a_bodyYaw, world);
		return { std::sin(local) * magnitude, std::cos(local) * magnitude };
	}
}
