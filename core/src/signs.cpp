#include "witcher/signs.h"

#include <algorithm>
#include <cmath>

namespace witcher
{
	std::string_view SignName(Sign a_sign)
	{
		switch (a_sign) {
		case Sign::kAard:
			return "Aard";
		case Sign::kIgni:
			return "Igni";
		case Sign::kYrden:
			return "Yrden";
		case Sign::kQuen:
			return "Quen";
		case Sign::kAxii:
			return "Axii";
		default:
			return "";
		}
	}

	std::array<SignSpec, kSignCount> DefaultSignSpecs()
	{
		std::array<SignSpec, kSignCount> s{};
		// Aard: a cone that knocks people over; held, a weaker blast all around you.
		s[0] = { SignShape::kCone, { 30.0f, 1.0f, 600.0f, 70.0f, 6 }, { 50.0f, 2.0f, 400.0f, 360.0f, 8 }, false };
		// Igni: a short fire cone; held, a longer, narrower stream.
		s[1] = { SignShape::kCone, { 30.0f, 1.0f, 500.0f, 50.0f, 4 }, { 55.0f, 2.0f, 700.0f, 30.0f, 6 }, false };
		// Yrden: slows everyone around you; held, a wider circle.
		s[2] = { SignShape::kArea, { 35.0f, 1.5f, 350.0f, 360.0f, 8 }, { 50.0f, 3.0f, 500.0f, 360.0f, 10 }, false };
		// Quen: a shield on you; held, a stronger one.
		s[3] = { SignShape::kSelf, { 40.0f, 1.5f, 0.0f, 0.0f, 0 }, { 60.0f, 3.0f, 0.0f, 0.0f, 0 }, false };
		// Axii: calms one enemy; held, longer and from further away.
		s[4] = { SignShape::kSingle, { 35.0f, 1.5f, 800.0f, 30.0f, 1 }, { 50.0f, 3.0f, 1000.0f, 30.0f, 1 }, false };
		return s;
	}

	namespace
	{
		bool Affectable(const Candidate& a_c, bool a_affectsNonHostile)
		{
			return a_c.id != 0 && !a_c.dead && a_c.visible && (a_c.hostile || a_affectsNonHostile);
		}

		float Distance(const View& a_view, const Candidate& a_c)
		{
			return (a_c.aimPoint - a_view.eye).Length2D();
		}
	}

	std::vector<std::size_t> SelectSignTargets(
		SignShape a_shape,
		const SignMode& a_mode,
		bool a_affectsNonHostile,
		bool a_aimAtLockTarget,
		const View& a_view,
		std::span<const Candidate> a_candidates,
		ActorId a_lockTarget)
	{
		std::vector<std::size_t> out;
		if (a_shape == SignShape::kSelf || a_mode.maxTargets == 0) {
			return out;
		}

		const auto lock = Find(a_candidates, a_lockTarget);
		const bool lockUsable = lock && Affectable(a_candidates[*lock], a_affectsNonHostile) &&
		                        Distance(a_view, a_candidates[*lock]) <= a_mode.range;

		if (a_shape == SignShape::kSingle) {
			if (lockUsable) {
				out.push_back(*lock);
				return out;
			}
			TargetingSettings t{};
			t.maxDistance = a_mode.range;
			t.maxAngleDeg = a_mode.coneDeg * 0.5f;
			t.hostileOnly = !a_affectsNonHostile;
			if (const auto best = SelectBest(a_view, a_candidates, t)) {
				out.push_back(*best);
			}
			return out;
		}

		// Cones aim where the camera looks, or at the lock-on target when there is one.
		float aimYaw = a_view.yaw;
		if (a_shape == SignShape::kCone && a_aimAtLockTarget && lockUsable) {
			aimYaw = YawTo(a_view.eye, a_candidates[*lock].aimPoint);
		}
		const float halfCone = DegToRad(std::min(a_mode.coneDeg, 360.0f) * 0.5f);
		const bool fullCircle = a_shape == SignShape::kArea || a_mode.coneDeg >= 360.0f;

		for (std::size_t i = 0; i < a_candidates.size(); ++i) {
			const auto& c = a_candidates[i];
			if (!Affectable(c, a_affectsNonHostile) || Distance(a_view, c) > a_mode.range) {
				continue;
			}
			if (!fullCircle && std::abs(AngleDelta(aimYaw, YawTo(a_view.eye, c.aimPoint))) > halfCone) {
				continue;
			}
			out.push_back(i);
		}

		std::sort(out.begin(), out.end(), [&](std::size_t a, std::size_t b) {
			return Distance(a_view, a_candidates[a]) < Distance(a_view, a_candidates[b]);
		});
		if (out.size() > a_mode.maxTargets) {
			out.resize(a_mode.maxTargets);
		}
		return out;
	}

	Sign SignBook::Select(std::int32_t a_sign)
	{
		if (a_sign >= 0 && a_sign < static_cast<std::int32_t>(kSignCount)) {
			_selected = static_cast<Sign>(a_sign);
		}
		return _selected;
	}

	Sign SignBook::Cycle(std::int32_t a_direction)
	{
		const auto n = static_cast<std::int32_t>(kSignCount);
		const auto step = a_direction < 0 ? -1 : 1;
		const auto next = (static_cast<std::int32_t>(_selected) + step + n) % n;
		_selected = static_cast<Sign>(next);
		return _selected;
	}

	void SignBook::Tick(float a_dt)
	{
		for (auto& c : _cooldowns) {
			c = std::max(0.0f, c - a_dt);
		}
	}

	CastOutcome SignBook::TryCast(bool a_alt, float a_currentAP, const View& a_view, std::span<const Candidate> a_candidates, ActorId a_lockTarget)
	{
		CastOutcome out{};
		out.sign = _selected;
		out.alt = a_alt;

		const auto& spec = _settings->specs[Index(_selected)];
		const auto& mode = a_alt ? spec.alt : spec.normal;
		out.cost = mode.apCost;

		if (_cooldowns[Index(_selected)] > 0.0f) {
			out.result = CastResult::kCooldown;
			return out;
		}
		if (a_currentAP < mode.apCost) {
			out.result = CastResult::kNoActionPoints;
			return out;
		}

		out.targets = SelectSignTargets(spec.shape, mode, spec.affectsNonHostile, _settings->aimAtLockTarget, a_view, a_candidates, a_lockTarget);
		if (spec.shape == SignShape::kSingle && out.targets.empty()) {
			out.result = CastResult::kNoTarget;  // nothing spent: casting Axii at air would just waste AP
			return out;
		}

		_cooldowns[Index(_selected)] = mode.cooldown;
		out.result = CastResult::kOk;
		return out;
	}

	void SignBook::Reset()
	{
		_cooldowns.fill(0.0f);
	}
}
