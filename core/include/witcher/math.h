#pragma once

// Engine-agnostic math for the camera system.
//
// Angle conventions match Fallout 4 (Creation Engine) references:
//   - Units are radians.
//   - Yaw (angle.z) 0 faces +Y ("north") and increases clockwise toward +X.
//   - Pitch (angle.x) is positive when looking DOWN.

#include <algorithm>
#include <cmath>

namespace witcher
{
	inline constexpr float kPi = 3.14159265358979323846f;
	inline constexpr float kTwoPi = 2.0f * kPi;

	constexpr float DegToRad(float a_deg) { return a_deg * (kPi / 180.0f); }
	constexpr float RadToDeg(float a_rad) { return a_rad * (180.0f / kPi); }

	struct Vec3
	{
		float x{ 0.0f };
		float y{ 0.0f };
		float z{ 0.0f };

		constexpr Vec3 operator+(const Vec3& a_rhs) const { return { x + a_rhs.x, y + a_rhs.y, z + a_rhs.z }; }
		constexpr Vec3 operator-(const Vec3& a_rhs) const { return { x - a_rhs.x, y - a_rhs.y, z - a_rhs.z }; }
		constexpr Vec3 operator*(float a_s) const { return { x * a_s, y * a_s, z * a_s }; }

		float Length() const { return std::sqrt(x * x + y * y + z * z); }
		float Length2D() const { return std::sqrt(x * x + y * y); }
	};

	struct Vec2
	{
		float x{ 0.0f };  // right
		float y{ 0.0f };  // forward

		float Length() const { return std::sqrt(x * x + y * y); }
	};

	inline Vec3 Lerp(const Vec3& a_from, const Vec3& a_to, float a_t)
	{
		return a_from + (a_to - a_from) * a_t;
	}

	// Wraps an angle into (-pi, pi].
	inline float WrapAngle(float a_rad)
	{
		float r = std::fmod(a_rad + kPi, kTwoPi);
		if (r <= 0.0f) {
			r += kTwoPi;
		}
		return r - kPi;
	}

	// Signed shortest rotation from a_from to a_to, in (-pi, pi].
	inline float AngleDelta(float a_from, float a_to)
	{
		return WrapAngle(a_to - a_from);
	}

	// Yaw that faces from a_from toward a_to.
	inline float YawTo(const Vec3& a_from, const Vec3& a_to)
	{
		const Vec3 d = a_to - a_from;
		return std::atan2(d.x, d.y);
	}

	// Pitch that looks from a_from toward a_to (positive = down).
	inline float PitchTo(const Vec3& a_from, const Vec3& a_to)
	{
		const Vec3 d = a_to - a_from;
		return -std::atan2(d.z, d.Length2D());
	}

	// Steps a_from toward a_to by at most a_maxStep along the shortest arc.
	inline float TurnToward(float a_from, float a_to, float a_maxStep)
	{
		const float d = AngleDelta(a_from, a_to);
		if (std::abs(d) <= a_maxStep) {
			return WrapAngle(a_to);
		}
		return WrapAngle(a_from + (d > 0.0f ? a_maxStep : -a_maxStep));
	}

	// Critically damped spring (same model as Unity's SmoothDamp). Frame-rate
	// independent: identical results whether stepped at 30 or 144 fps.
	inline float SmoothDamp(float a_current, float a_target, float& a_velocity, float a_smoothTime, float a_dt)
	{
		if (a_dt <= 0.0f) {
			return a_current;
		}
		a_smoothTime = std::max(0.0001f, a_smoothTime);
		const float omega = 2.0f / a_smoothTime;
		const float x = omega * a_dt;
		const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
		const float change = a_current - a_target;
		const float temp = (a_velocity + omega * change) * a_dt;
		a_velocity = (a_velocity - omega * temp) * exp;
		float result = a_target + (change + temp) * exp;

		// Prevent overshoot.
		if ((a_target - a_current > 0.0f) == (result > a_target)) {
			result = a_target;
			a_velocity = (result - a_target) / a_dt;
		}
		return result;
	}

	// SmoothDamp along the shortest arc between two angles.
	inline float SmoothDampAngle(float a_current, float a_target, float& a_velocity, float a_smoothTime, float a_dt)
	{
		const float target = a_current + AngleDelta(a_current, a_target);
		return WrapAngle(SmoothDamp(a_current, target, a_velocity, a_smoothTime, a_dt));
	}
}
