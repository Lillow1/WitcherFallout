// Dependency-free tests for the camera core. Build and run on any OS:
//   cmake -S fo4-witcher -B build && cmake --build build && ctest --test-dir build

#include "witcher/camera_system.h"
#include "witcher/math.h"
#include "witcher/settings.h"
#include "witcher/targeting.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>

using namespace witcher;

namespace
{
	int g_failures = 0;
	std::vector<std::pair<const char*, std::function<void()>>>& Registry()
	{
		static std::vector<std::pair<const char*, std::function<void()>>> r;
		return r;
	}
	struct Reg
	{
		Reg(const char* n, std::function<void()> f) { Registry().emplace_back(n, std::move(f)); }
	};

#define TEST(name)                     \
	void name();                       \
	const Reg reg_##name(#name, name); \
	void name()

#define CHECK(cond)                                                          \
	do {                                                                     \
		if (!(cond)) {                                                       \
			std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
			++g_failures;                                                    \
		}                                                                    \
	} while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::abs((a) - (b)) <= (eps))

	constexpr float kEps = 1e-4f;

	Candidate Enemy(ActorId id, Vec3 pos, float scale = 1.0f)
	{
		return { id, pos, true, false, true, scale };
	}

	View LookingNorth()
	{
		return { { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f };
	}

	// Runs a CameraSystem for a_seconds, feeding the rig's rotation back in
	// as the player's rotation, like the game would.
	FrameResult Simulate(CameraSystem& a_sys, FrameSnapshot a_frame, float a_seconds, float a_dt = 1.0f / 60.0f)
	{
		FrameResult r{};
		for (float t = 0.0f; t < a_seconds; t += a_dt) {
			a_frame.dt = a_dt;
			r = a_sys.Tick(a_frame);
			if (r.rig.driveYaw) {
				a_frame.view.yaw = r.rig.yaw;
			}
			if (r.rig.drivePitch) {
				a_frame.view.pitch = r.rig.pitch;
			}
			a_frame.lockPressed = false;
			a_frame.shoulderPressed = false;
			a_frame.attackStarted = false;
			a_frame.trauma = 0.0f;
		}
		return r;
	}
}

// ---- math ---------------------------------------------------------------

TEST(WrapAngle_StaysInRange)
{
	CHECK_NEAR(WrapAngle(0.0f), 0.0f, kEps);
	CHECK_NEAR(WrapAngle(kTwoPi), 0.0f, kEps);
	CHECK_NEAR(WrapAngle(kPi + 0.5f), -kPi + 0.5f, kEps);
	CHECK_NEAR(WrapAngle(-kPi - 0.5f), kPi - 0.5f, kEps);
	CHECK_NEAR(WrapAngle(7.0f * kTwoPi + 0.25f), 0.25f, 1e-3f);
}

TEST(AngleDelta_TakesShortestArc)
{
	CHECK_NEAR(AngleDelta(DegToRad(170.0f), DegToRad(-170.0f)), DegToRad(20.0f), kEps);
	CHECK_NEAR(AngleDelta(DegToRad(-170.0f), DegToRad(170.0f)), DegToRad(-20.0f), kEps);
}

TEST(YawPitch_MatchCreationEngineConventions)
{
	const Vec3 o{};
	CHECK_NEAR(YawTo(o, { 0, 100, 0 }), 0.0f, kEps);              // north
	CHECK_NEAR(YawTo(o, { 100, 0, 0 }), kPi / 2.0f, kEps);        // east = clockwise
	CHECK_NEAR(YawTo(o, { -100, 0, 0 }), -kPi / 2.0f, kEps);      // west
	CHECK(PitchTo(o, { 0, 100, -50 }) > 0.0f);                     // below = positive pitch
	CHECK(PitchTo(o, { 0, 100, 50 }) < 0.0f);
}

TEST(SmoothDamp_ConvergesWithoutOvershoot)
{
	float v = 0.0f, vel = 0.0f;
	float maxSeen = 0.0f;
	for (int i = 0; i < 600; ++i) {
		v = SmoothDamp(v, 10.0f, vel, 0.2f, 1.0f / 60.0f);
		maxSeen = std::max(maxSeen, v);
	}
	CHECK_NEAR(v, 10.0f, 1e-3f);
	CHECK(maxSeen <= 10.0f + 1e-4f);
}

TEST(SmoothDamp_IsFrameRateIndependent)
{
	// Same 0.5 s of simulated time at both rates.
	auto run = [](int fps) {
		const float dt = 1.0f / static_cast<float>(fps);
		float v = 0.0f, vel = 0.0f;
		for (int i = 0; i < fps / 2; ++i) {
			v = SmoothDamp(v, 1.0f, vel, 0.2f, dt);
		}
		return v;
	};
	CHECK_NEAR(run(30), run(144), 0.02f);
}

TEST(SmoothDampAngle_CrossesSeam)
{
	float a = DegToRad(170.0f), vel = 0.0f;
	for (int i = 0; i < 600; ++i) {
		a = SmoothDampAngle(a, DegToRad(-170.0f), vel, 0.1f, 1.0f / 60.0f);
	}
	CHECK_NEAR(a, DegToRad(-170.0f), 1e-3f);
}

// ---- targeting ----------------------------------------------------------

TEST(SelectBest_PrefersCenteredOverCloser)
{
	TargetingSettings s{};
	const Candidate cands[] = {
		Enemy(1, { 300, 300, 0 }),  // 45 degrees right, close
		Enemy(2, { 0, 800, 0 }),    // dead ahead, farther
	};
	const auto best = SelectBest(LookingNorth(), cands, s);
	CHECK(best && cands[*best].id == 2);
}

TEST(SelectBest_SkipsIneligible)
{
	TargetingSettings s{};
	Candidate dead = Enemy(1, { 0, 200, 0 });
	dead.dead = true;
	Candidate friendly = Enemy(2, { 0, 300, 0 });
	friendly.hostile = false;
	Candidate hidden = Enemy(3, { 0, 400, 0 });
	hidden.visible = false;
	const Candidate behind = Enemy(4, { 0, -200, 0 });
	const Candidate far = Enemy(5, { 0, 5000, 0 });
	const Candidate cands[] = { dead, friendly, hidden, behind, far };
	CHECK(!SelectBest(LookingNorth(), cands, s));

	s.hostileOnly = false;
	const auto best = SelectBest(LookingNorth(), cands, s);
	CHECK(best && cands[*best].id == 2);
}

TEST(SelectNext_PicksNearestInDirection)
{
	TargetingSettings s{};
	const Candidate cands[] = {
		Enemy(1, { 0, 500, 0 }),     // current, ahead
		Enemy(2, { 200, 500, 0 }),   // slightly right
		Enemy(3, { 500, 200, 0 }),   // far right
		Enemy(4, { -300, 500, 0 }),  // left
	};
	auto r = SelectNext(LookingNorth(), cands, 1, SwitchDirection::kRight, s);
	CHECK(r && cands[*r].id == 2);
	r = SelectNext(LookingNorth(), cands, 1, SwitchDirection::kLeft, s);
	CHECK(r && cands[*r].id == 4);
	r = SelectNext(LookingNorth(), cands, 3, SwitchDirection::kRight, s);
	CHECK(!r || cands[*r].id != 3);
}

// ---- lock-on ------------------------------------------------------------

TEST(LockOn_ToggleAcquiresAndReleases)
{
	LockOnSettings s{};
	LockOnController lock(s);
	const Candidate cands[] = { Enemy(7, { 0, 500, 0 }) };
	CHECK(lock.Toggle(LookingNorth(), cands) == LockEvent::kAcquired);
	CHECK(lock.Target() == 7);
	CHECK(lock.Toggle(LookingNorth(), cands) == LockEvent::kReleased);
	CHECK(!lock.IsLocked());
}

TEST(LockOn_HopsToNextEnemyOnDeath)
{
	LockOnSettings s{};
	LockOnController lock(s);
	Candidate cands[] = { Enemy(1, { 0, 500, 0 }), Enemy(2, { 100, 600, 0 }) };
	lock.Toggle(LookingNorth(), cands);
	CHECK(lock.Target() == 1);
	cands[0].dead = true;
	CHECK(lock.Update(0.016f, LookingNorth(), cands, 0.0f) == LockEvent::kSwitched);
	CHECK(lock.Target() == 2);

	s.releaseOnTargetDeath = true;
	cands[1].dead = true;
	CHECK(lock.Update(0.016f, LookingNorth(), cands, 0.0f) == LockEvent::kReleased);
}

TEST(LockOn_BreaksAfterLosGraceAndDistance)
{
	LockOnSettings s{};
	LockOnController lock(s);
	Candidate cands[] = { Enemy(1, { 0, 500, 0 }) };
	lock.Toggle(LookingNorth(), cands);
	cands[0].visible = false;
	CHECK(lock.Update(s.losGraceSeconds * 0.5f, LookingNorth(), cands, 0.0f) == LockEvent::kNone);
	cands[0].visible = true;  // reappearing resets the timer
	CHECK(lock.Update(0.1f, LookingNorth(), cands, 0.0f) == LockEvent::kNone);
	cands[0].visible = false;
	CHECK(lock.Update(s.losGraceSeconds * 0.9f, LookingNorth(), cands, 0.0f) == LockEvent::kNone);
	CHECK(lock.Update(s.losGraceSeconds * 0.2f, LookingNorth(), cands, 0.0f) == LockEvent::kReleased);

	cands[0] = Enemy(1, { 0, 500, 0 });
	lock.Toggle(LookingNorth(), cands);
	cands[0].aimPoint = { 0, s.breakDistance + 10.0f, 0 };
	CHECK(lock.Update(0.016f, LookingNorth(), cands, 0.0f) == LockEvent::kReleased);
}

TEST(LockOn_FlickSwitchesButDriftDoesNot)
{
	LockOnSettings s{};
	LockOnController lock(s);
	const Candidate cands[] = { Enemy(1, { 0, 500, 0 }), Enemy(2, { 300, 500, 0 }) };
	lock.Toggle(LookingNorth(), cands);

	// Slow drift: small input every frame, bleeds off faster than it builds.
	for (int i = 0; i < 300; ++i) {
		lock.Update(1.0f / 60.0f, LookingNorth(), cands, 0.05f);
	}
	CHECK(lock.Target() == 1);

	// Deliberate flick right.
	CHECK(lock.Update(1.0f / 60.0f, LookingNorth(), cands, s.switchThreshold + 0.1f) == LockEvent::kSwitched);
	CHECK(lock.Target() == 2);

	// Immediate flick back is blocked by the cooldown.
	CHECK(lock.Update(1.0f / 60.0f, LookingNorth(), cands, -(s.switchThreshold + 0.1f)) == LockEvent::kNone);
	CHECK(lock.Target() == 2);
}

// ---- camera system ------------------------------------------------------

TEST(System_LockSteersPlayerTowardTarget)
{
	Settings settings{};
	CameraSystem sys(settings);
	const Candidate cands[] = { Enemy(1, { 400, 400, -60 }) };  // 45 degrees right, slightly below

	FrameSnapshot f{};
	f.view = LookingNorth();
	f.view.yaw = DegToRad(20.0f);  // inside the 60 degree acquire cone
	f.mode = CameraMode::kMelee;
	f.candidates = cands;
	f.lockPressed = true;

	const auto r = Simulate(sys, f, 1.5f);
	CHECK(r.lockTarget == 1);
	CHECK(r.rig.driveYaw && r.rig.drivePitch);
	CHECK_NEAR(r.rig.yaw, DegToRad(45.0f), DegToRad(0.5f));
	CHECK(r.rig.pitch > 0.0f);  // looking down at the target plus bias
	CHECK(r.rig.pitch <= DegToRad(settings.camera.lockPitchMaxDeg) + kEps);
}

TEST(System_HolsteringReleasesLock)
{
	Settings settings{};
	CameraSystem sys(settings);
	const Candidate cands[] = { Enemy(1, { 0, 500, 0 }) };
	FrameSnapshot f{};
	f.dt = 1.0f / 60.0f;
	f.view = LookingNorth();
	f.mode = CameraMode::kCombat;
	f.candidates = cands;
	f.lockPressed = true;
	CHECK(sys.Tick(f).lockEvent == LockEvent::kAcquired);

	f.lockPressed = false;
	f.mode = CameraMode::kExploration;
	const auto r = sys.Tick(f);
	CHECK(r.lockEvent == LockEvent::kReleased);
	CHECK(!r.rig.driveYaw);
}

TEST(System_CannotLockOutOfCombat)
{
	Settings settings{};
	CameraSystem sys(settings);
	const Candidate cands[] = { Enemy(1, { 0, 500, 0 }) };
	FrameSnapshot f{};
	f.dt = 1.0f / 60.0f;
	f.view = LookingNorth();
	f.mode = CameraMode::kExploration;
	f.candidates = cands;
	f.lockPressed = true;
	CHECK(sys.Tick(f).lockTarget == 0);
}

TEST(System_SoftLockSnapsOnAttackOnly)
{
	Settings settings{};
	CameraSystem sys(settings);
	const Candidate cands[] = { Enemy(1, { 100, 200, 0 }) };  // ~27 degrees right, in melee range

	FrameSnapshot f{};
	f.view = LookingNorth();
	f.mode = CameraMode::kMelee;
	f.candidates = cands;

	auto r = Simulate(sys, f, 0.5f);
	CHECK(!r.rig.driveYaw);  // no attack, no snap

	f.attackStarted = true;
	FrameResult last{};
	float yaw = 0.0f;
	for (int i = 0; i < 30; ++i) {
		f.dt = 1.0f / 60.0f;
		f.view.yaw = yaw;
		last = sys.Tick(f);
		if (last.rig.driveYaw) {
			yaw = last.rig.yaw;
		}
		f.attackStarted = false;
	}
	CHECK_NEAR(yaw, YawTo({}, cands[0].aimPoint), DegToRad(0.5f));
	CHECK(!last.rig.driveYaw);  // snap finished, player has control again
}

TEST(System_FramingBlendsAndSwapsShoulder)
{
	Settings settings{};
	CameraSystem sys(settings);
	FrameSnapshot f{};
	f.view = LookingNorth();
	f.mode = CameraMode::kExploration;

	auto r = Simulate(sys, f, 0.1f);
	CHECK_NEAR(r.rig.framing.shoulderX, settings.camera.exploration.shoulderX, 0.01f);

	f.mode = CameraMode::kMelee;
	r = Simulate(sys, f, 3.0f);
	CHECK_NEAR(r.rig.framing.shoulderX, settings.camera.melee.shoulderX, 0.05f);
	CHECK_NEAR(r.rig.framing.distance, settings.camera.melee.distance, 0.05f);

	f.shoulderPressed = true;
	r = Simulate(sys, f, 3.0f);
	CHECK_NEAR(r.rig.framing.shoulderX, -settings.camera.melee.shoulderX, 0.05f);
}

TEST(System_GroupAndLargeTargetPullBack)
{
	Settings settings{};
	CameraSystem sys(settings);
	const Candidate cands[] = {
		Enemy(1, { 0, 500, 0 }, 2.5f),
		Enemy(2, { 200, 500, 0 }),
		Enemy(3, { -200, 500, 0 }),
	};
	FrameSnapshot f{};
	f.view = LookingNorth();
	f.mode = CameraMode::kMelee;
	f.candidates = cands;
	f.lockPressed = true;

	const auto r = Simulate(sys, f, 4.0f);
	CHECK(r.lockTarget == 1);
	const float expected = settings.camera.melee.distance + 2.0f * settings.camera.groupZoomPerEnemy + settings.camera.largeTargetZoom;
	CHECK_NEAR(r.rig.framing.distance, expected, 0.1f);
}

TEST(System_ShakeDecaysToZero)
{
	Settings settings{};
	CameraSystem sys(settings);
	FrameSnapshot f{};
	f.dt = 1.0f / 60.0f;
	f.view = LookingNorth();
	f.trauma = 0.8f;
	bool shook = false;
	for (int i = 0; i < 10; ++i) {
		const auto r = sys.Tick(f);
		shook |= std::abs(r.rig.shakeYaw) > 0.0f;
		f.trauma = 0.0f;
	}
	CHECK(shook);
	const auto r = Simulate(sys, f, 2.0f);
	CHECK(r.rig.shakeYaw == 0.0f && r.rig.shakePitch == 0.0f);
}

// ---- settings -----------------------------------------------------------

TEST(Settings_ParsesAndWarns)
{
	const char* ini = R"(
; comment
[Framing.Melee]
fShoulderX = 12.5   ; trailing comment
fDistance=80

[LockOn]
fBreakDistance = 3000
bReleaseOnTargetDeath = true
fMystery = 1
fSwitchCooldown = abc

[Input]
iLockOnKey = 0x10
)";
	std::vector<std::string> warnings;
	const auto s = ParseSettings(ini, &warnings);
	CHECK_NEAR(s.camera.melee.shoulderX, 12.5f, kEps);
	CHECK_NEAR(s.camera.melee.distance, 80.0f, kEps);
	CHECK_NEAR(s.camera.melee.shoulderZ, Settings{}.camera.melee.shoulderZ, kEps);  // untouched default
	CHECK_NEAR(s.lockOn.breakDistance, 3000.0f, kEps);
	CHECK(s.lockOn.releaseOnTargetDeath);
	CHECK_NEAR(s.lockOn.switchCooldown, Settings{}.lockOn.switchCooldown, kEps);
	CHECK(s.input.lockOnKey == 0x10);
	CHECK(warnings.size() == 2);
}

TEST(Settings_ShippedIniIsClean)
{
	std::ifstream file{ WITCHER_CONFIG_INI };
	CHECK(file.good());
	std::stringstream buf;
	buf << file.rdbuf();
	std::vector<std::string> warnings;
	const auto s = ParseSettings(buf.str(), &warnings);
	for (const auto& w : warnings) {
		std::printf("  warning: %s\n", w.c_str());
	}
	CHECK(warnings.empty());
	CHECK(s.input.lockOnKey == 0x5A);
}

int main()
{
	for (const auto& [name, fn] : Registry()) {
		const int before = g_failures;
		fn();
		std::printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", name);
	}
	std::printf("\n%zu tests, %d failed checks\n", Registry().size(), g_failures);
	return g_failures == 0 ? 0 : 1;
}
