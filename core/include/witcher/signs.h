#pragma once

// Witcher Signs: which Sign is selected, what casting it costs, and who it
// hits. Engine-free; the plugin feeds it the same actor snapshot the camera
// uses, and a Papyrus script applies the actual effects.

#include "witcher/math.h"
#include "witcher/settings.h"
#include "witcher/targeting.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace witcher
{
	enum class Sign : std::int32_t
	{
		kAard,   // telekinetic blast: knockback
		kIgni,   // fire cone
		kYrden,  // slowing trap
		kQuen,   // shield on yourself
		kAxii,   // calms one target

		kCount
	};

	static_assert(static_cast<std::size_t>(Sign::kCount) == kSignCount, "update kSignCount in settings.h");

	std::string_view SignName(Sign a_sign);

	enum class CastResult : std::int32_t
	{
		kOk = 0,
		kCooldown = 1,
		kNoActionPoints = 2,
		kNoTarget = 3,  // only kSingle Signs need a target; cones and areas can be cast at nothing
	};

	struct CastOutcome
	{
		CastResult result{ CastResult::kOk };
		Sign sign{ Sign::kAard };
		bool alt{ false };
		float cost{ 0.0f };
		std::vector<std::size_t> targets;  // indices into the candidates passed to TryCast
	};

	// Targets a Sign would hit right now, nearest first.
	std::vector<std::size_t> SelectSignTargets(
		SignShape a_shape,
		const SignMode& a_mode,
		bool a_affectsNonHostile,
		bool a_aimAtLockTarget,
		const View& a_view,
		std::span<const Candidate> a_candidates,
		ActorId a_lockTarget);

	class SignBook
	{
	public:
		explicit SignBook(const SignsSettings& a_settings) :
			_settings(&a_settings)
		{}

		Sign Selected() const { return _selected; }
		Sign Select(std::int32_t a_sign);   // out-of-range values are ignored
		Sign Cycle(std::int32_t a_direction);  // +1 next, -1 previous, wraps around

		void Tick(float a_dt);
		float CooldownRemaining(Sign a_sign) const { return _cooldowns[Index(a_sign)]; }

		// Checks cooldown, AP and targets for the selected Sign. On success
		// starts the cooldown; the caller spends `cost` AP and applies effects.
		CastOutcome TryCast(bool a_alt, float a_currentAP, const View& a_view, std::span<const Candidate> a_candidates, ActorId a_lockTarget);

		void Reset();

	private:
		static std::size_t Index(Sign a_sign) { return static_cast<std::size_t>(a_sign); }

		const SignsSettings* _settings;
		Sign _selected{ Sign::kAard };
		std::array<float, kSignCount> _cooldowns{};
	};
}
