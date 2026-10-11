#include "witcher/lock_on.h"

#include <algorithm>
#include <cmath>

namespace witcher
{
	LockEvent LockOnController::Toggle(const View& a_view, std::span<const Candidate> a_candidates)
	{
		if (IsLocked()) {
			Release();
			return LockEvent::kReleased;
		}
		const auto best = SelectBest(a_view, a_candidates, _settings->targeting);
		if (!best) {
			return LockEvent::kNone;
		}
		return Acquire(a_candidates[*best].id, LockEvent::kAcquired);
	}

	LockEvent LockOnController::Update(float a_dt, const View& a_view, std::span<const Candidate> a_candidates, float a_lookX)
	{
		if (!IsLocked()) {
			return LockEvent::kNone;
		}

		_cooldown = std::max(0.0f, _cooldown - a_dt);

		const auto idx = Find(a_candidates, _target);
		const bool gone = !idx || a_candidates[*idx].dead;
		if (gone) {
			if (!_settings->releaseOnTargetDeath) {
				if (const auto next = SelectBest(a_view, a_candidates, _settings->targeting)) {
					return Acquire(a_candidates[*next].id, LockEvent::kSwitched);
				}
			}
			Release();
			return LockEvent::kReleased;
		}

		const auto& target = a_candidates[*idx];
		if ((target.aimPoint - a_view.eye).Length() > _settings->breakDistance) {
			Release();
			return LockEvent::kReleased;
		}

		_hiddenTime = target.visible ? 0.0f : _hiddenTime + a_dt;
		if (_hiddenTime > _settings->losGraceSeconds) {
			Release();
			return LockEvent::kReleased;
		}

		// Flick to switch: look input accumulates and bleeds off over time, so a
		// quick deliberate flick switches but slow aim drift never does.
		_flickAccum += a_lookX;
		const float decay = _settings->switchDecay * a_dt;
		_flickAccum = _flickAccum > 0.0f ? std::max(0.0f, _flickAccum - decay) : std::min(0.0f, _flickAccum + decay);

		if (_cooldown <= 0.0f && std::abs(_flickAccum) >= _settings->switchThreshold) {
			const auto dir = _flickAccum > 0.0f ? SwitchDirection::kRight : SwitchDirection::kLeft;
			_flickAccum = 0.0f;
			_cooldown = _settings->switchCooldown;
			if (const auto next = SelectNext(a_view, a_candidates, _target, dir, _settings->targeting)) {
				return Acquire(a_candidates[*next].id, LockEvent::kSwitched);
			}
		}

		return LockEvent::kNone;
	}

	void LockOnController::Release()
	{
		_target = 0;
		_hiddenTime = 0.0f;
		_flickAccum = 0.0f;
	}

	LockEvent LockOnController::Acquire(ActorId a_id, LockEvent a_event)
	{
		_target = a_id;
		_hiddenTime = 0.0f;
		_flickAccum = 0.0f;
		return a_event;
	}
}
