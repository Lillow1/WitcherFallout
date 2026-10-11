#pragma once

// Hard lock-on state machine: acquire, keep, switch by flicking, release.

#include "witcher/settings.h"
#include "witcher/targeting.h"

#include <span>

namespace witcher
{
	enum class LockEvent
	{
		kNone,
		kAcquired,
		kSwitched,  // flicked, or hopped to a new enemy after the old one died
		kReleased
	};

	class LockOnController
	{
	public:
		explicit LockOnController(const LockOnSettings& a_settings) :
			_settings(&a_settings)
		{}

		// Lock-on key pressed. Locks the best target, or releases the current one.
		LockEvent Toggle(const View& a_view, std::span<const Candidate> a_candidates);

		// Call once per frame. a_lookX is this frame's horizontal look input
		// (mouse delta scaled to roughly -1..1 per flick, or the right stick X).
		LockEvent Update(float a_dt, const View& a_view, std::span<const Candidate> a_candidates, float a_lookX);

		void Release();

		bool IsLocked() const { return _target != 0; }
		ActorId Target() const { return _target; }

	private:
		LockEvent Acquire(ActorId a_id, LockEvent a_event);

		const LockOnSettings* _settings;
		ActorId _target{ 0 };
		float _hiddenTime{ 0.0f };
		float _flickAccum{ 0.0f };
		float _cooldown{ 0.0f };
	};
}
