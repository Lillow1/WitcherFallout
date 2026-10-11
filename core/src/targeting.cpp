#include "witcher/targeting.h"

#include <cmath>
#include <limits>

namespace witcher
{
	namespace
	{
		bool PassesBasicFilter(const View& a_view, const Candidate& a_c, const TargetingSettings& a_s)
		{
			if (a_c.id == 0 || a_c.dead || !a_c.visible) {
				return false;
			}
			if (a_s.hostileOnly && !a_c.hostile) {
				return false;
			}
			return (a_c.aimPoint - a_view.eye).Length() <= a_s.maxDistance;
		}
	}

	bool IsEligible(const View& a_view, const Candidate& a_c, const TargetingSettings& a_s)
	{
		if (!PassesBasicFilter(a_view, a_c, a_s)) {
			return false;
		}
		const float off = std::abs(AngleDelta(a_view.yaw, YawTo(a_view.eye, a_c.aimPoint)));
		return off <= DegToRad(a_s.maxAngleDeg);
	}

	float ScoreCandidate(const View& a_view, const Candidate& a_c, const TargetingSettings& a_s)
	{
		const float off = std::abs(AngleDelta(a_view.yaw, YawTo(a_view.eye, a_c.aimPoint)));
		const float dist = (a_c.aimPoint - a_view.eye).Length();
		const float angleNorm = off / std::max(0.001f, DegToRad(a_s.maxAngleDeg));
		const float distNorm = dist / std::max(1.0f, a_s.maxDistance);
		return a_s.angleWeight * angleNorm + a_s.distanceWeight * distNorm;
	}

	std::optional<std::size_t> SelectBest(const View& a_view, std::span<const Candidate> a_candidates, const TargetingSettings& a_s)
	{
		std::optional<std::size_t> best;
		float bestScore = std::numeric_limits<float>::max();
		for (std::size_t i = 0; i < a_candidates.size(); ++i) {
			const auto& c = a_candidates[i];
			if (!IsEligible(a_view, c, a_s)) {
				continue;
			}
			const float score = ScoreCandidate(a_view, c, a_s);
			if (score < bestScore) {
				bestScore = score;
				best = i;
			}
		}
		return best;
	}

	std::optional<std::size_t> SelectNext(const View& a_view, std::span<const Candidate> a_candidates, ActorId a_current, SwitchDirection a_dir, const TargetingSettings& a_s)
	{
		const auto cur = Find(a_candidates, a_current);
		// Without a current target, measure from where the camera is looking.
		const float refYaw = cur ? YawTo(a_view.eye, a_candidates[*cur].aimPoint) : a_view.yaw;

		std::optional<std::size_t> best;
		float bestDelta = std::numeric_limits<float>::max();
		for (std::size_t i = 0; i < a_candidates.size(); ++i) {
			const auto& c = a_candidates[i];
			if (c.id == a_current || !PassesBasicFilter(a_view, c, a_s)) {
				continue;
			}
			// Positive delta = clockwise = to the right.
			float delta = AngleDelta(refYaw, YawTo(a_view.eye, c.aimPoint));
			if (a_dir == SwitchDirection::kLeft) {
				delta = -delta;
			}
			if (delta <= 0.0f) {
				continue;
			}
			if (delta < bestDelta) {
				bestDelta = delta;
				best = i;
			}
		}
		return best;
	}

	std::optional<std::size_t> Find(std::span<const Candidate> a_candidates, ActorId a_id)
	{
		if (a_id == 0) {
			return std::nullopt;
		}
		for (std::size_t i = 0; i < a_candidates.size(); ++i) {
			if (a_candidates[i].id == a_id) {
				return i;
			}
		}
		return std::nullopt;
	}
}
