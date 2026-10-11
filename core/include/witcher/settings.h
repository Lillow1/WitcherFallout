#pragma once

// Tunables for the camera system, loaded from Data/F4SE/Plugins/WitcherCamera.ini.
// Every value has a default so a missing or partial INI still works.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace witcher
{
	// Third-person framing. Units are game units (about 1.4 cm each).
	struct FramingPreset
	{
		float shoulderX{ 0.0f };  // + right of the character
		float shoulderZ{ 0.0f };  // + up
		float distance{ 0.0f };   // extra pull-back from vanilla zoom
		float fov{ 0.0f };        // 0 = leave the game's FOV alone
	};

	struct TargetingSettings
	{
		float maxDistance{ 2000.0f };     // acquire range
		float maxAngleDeg{ 60.0f };       // half-angle of the acquire cone
		float angleWeight{ 0.7f };        // score weighting, lower score wins
		float distanceWeight{ 0.3f };
		bool hostileOnly{ true };
	};

	struct LockOnSettings
	{
		TargetingSettings targeting{};
		float breakDistance{ 2600.0f };   // lock drops beyond this
		float losGraceSeconds{ 1.5f };    // how long a target may be hidden before the lock drops
		float switchThreshold{ 0.6f };    // accumulated look input needed to flick to the next target
		float switchCooldown{ 0.35f };    // seconds between flicks
		float switchDecay{ 4.0f };        // how fast unused look input bleeds off (per second)
		bool releaseOnTargetDeath{ false };  // false = hop to the next nearby enemy, like Witcher 3
	};

	struct SoftLockSettings
	{
		bool enabled{ true };
		TargetingSettings targeting{ 350.0f, 35.0f, 0.6f, 0.4f, true };
		float snapTime{ 0.08f };          // seconds to turn toward the target when an attack starts
	};

	struct CameraSettings
	{
		FramingPreset exploration{ 35.0f, 5.0f, 0.0f, 0.0f };
		FramingPreset combat{ 45.0f, 0.0f, 20.0f, 0.0f };
		FramingPreset melee{ 30.0f, -5.0f, 60.0f, 0.0f };
		float framingBlendTime{ 0.45f };

		float lockYawSmoothTime{ 0.12f };
		float lockPitchSmoothTime{ 0.18f };
		float lockPitchBiasDeg{ 8.0f };   // look slightly down at the target, so the ground is visible
		float lockPitchMinDeg{ -25.0f };
		float lockPitchMaxDeg{ 30.0f };
		float lockMinDistance{ 60.0f };   // stop re-aiming when closer than this, to avoid spinning

		float groupRadius{ 1200.0f };      // hostiles within this range count as "nearby"
		float groupZoomPerEnemy{ 15.0f };  // pull back per extra hostile nearby
		float groupZoomMax{ 90.0f };
		float largeTargetZoom{ 80.0f };   // extra pull-back at targetScale >= 2 (deathclaws, behemoths)

		float shakeMaxYawDeg{ 1.5f };
		float shakeMaxPitchDeg{ 1.2f };
		float shakeDecay{ 2.5f };         // trauma lost per second
		float shakeFrequency{ 18.0f };
	};

	struct InputSettings
	{
		std::uint32_t lockOnKey{ 0x5A };       // Z (Windows virtual-key code, what FO4 reports)
		std::uint32_t shoulderSwapKey{ 0x43 };  // C
		std::uint32_t lockOnGamepad{ 0x0080 };  // right thumbstick click (XInput mask)
		float mouseFlickScale{ 0.006f };  // raw mouse counts -> flick units (about 100 counts = one switch)
		float stickFlickScale{ 6.0f };    // right stick held fully for ~0.1 s = one switch
	};

	struct Settings
	{
		CameraSettings camera{};
		LockOnSettings lockOn{};
		SoftLockSettings softLock{};
		InputSettings input{};
	};

	// Parses INI text. Unknown keys and malformed lines are skipped and
	// reported in a_warnings (if given) instead of failing the whole load.
	Settings ParseSettings(std::string_view a_ini, std::vector<std::string>* a_warnings = nullptr);
}
