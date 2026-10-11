#pragma once

// Choosing what the camera locks onto. Pure functions over a snapshot of
// nearby actors, so the engine adapter only has to fill in Candidate structs.

#include "witcher/math.h"
#include "witcher/settings.h"

#include <cstdint>
#include <optional>
#include <span>

namespace witcher
{
	using ActorId = std::uint32_t;  // engine handle; 0 = none

	struct Candidate
	{
		ActorId id{ 0 };
		Vec3 aimPoint{};       // torso or head, wherever the camera should look
		bool hostile{ false };
		bool dead{ false };
		bool visible{ true };  // line of sight from the camera
		float scale{ 1.0f };   // 1 = human-sized, 2+ = large creature
	};

	struct View
	{
		Vec3 eye{};      // camera or player head position
		float yaw{ 0.0f };
		float pitch{ 0.0f };
	};

	enum class SwitchDirection
	{
		kLeft,
		kRight
	};

	// True when a candidate can be locked onto at all.
	bool IsEligible(const View& a_view, const Candidate& a_c, const TargetingSettings& a_s);

	// Lower is better. Mixes how far off-center the candidate is with how far away it is.
	float ScoreCandidate(const View& a_view, const Candidate& a_c, const TargetingSettings& a_s);

	// Best target in the acquire cone, or nullopt.
	std::optional<std::size_t> SelectBest(const View& a_view, std::span<const Candidate> a_candidates, const TargetingSettings& a_s);

	// Nearest eligible target to the left or right of a_current, measured by
	// bearing from the eye. Ignores the acquire cone so you can flick to an
	// enemy at your side, but still respects distance and hostility.
	std::optional<std::size_t> SelectNext(const View& a_view, std::span<const Candidate> a_candidates, ActorId a_current, SwitchDirection a_dir, const TargetingSettings& a_s);

	// Index of a_id in a_candidates, or nullopt.
	std::optional<std::size_t> Find(std::span<const Candidate> a_candidates, ActorId a_id);
}
