#include "Plugin.h"

#include "Engine.h"
#include "Input.h"

#include "witcher/camera_system.h"
#include "witcher/settings.h"

#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>

namespace witcher::plugin
{
	namespace
	{
		constexpr auto kSettingsPath = "Data/F4SE/Plugins/WitcherCamera.ini";
		constexpr float kMaxFrameTime = 0.1f;  // clamp after hitches so springs don't explode

		struct State
		{
			Settings settings{};
			std::unique_ptr<CameraSystem> system;
			Applier applier{};
			GameState game{};
			std::chrono::steady_clock::time_point last{};
			bool resetPending{ true };
		};

		State& Get()
		{
			static State state;
			return state;
		}

		Settings LoadSettings()
		{
			std::ifstream file{ kSettingsPath };
			if (!file) {
				logger::warn("{} not found, using defaults", kSettingsPath);
				return {};
			}
			std::stringstream buf;
			buf << file.rdbuf();

			std::vector<std::string> warnings;
			auto settings = ParseSettings(buf.str(), &warnings);
			for (const auto& w : warnings) {
				logger::warn("{}: {}", kSettingsPath, w);
			}
			logger::info("loaded {}", kSettingsPath);
			return settings;
		}

		const char* ToString(LockEvent a_event)
		{
			switch (a_event) {
			case LockEvent::kAcquired:
				return "acquired";
			case LockEvent::kSwitched:
				return "switched";
			case LockEvent::kReleased:
				return "released";
			default:
				return "none";
			}
		}

		void Tick()
		{
			auto& s = Get();
			if (!s.system) {
				return;
			}

			const auto now = std::chrono::steady_clock::now();
			float dt = std::chrono::duration<float>(now - s.last).count();
			s.last = now;

			const auto ui = RE::UI::GetSingleton();
			if (ui && ui->menuMode > 0) {
				return;  // paused: keep state, don't steer
			}
			if (!ReadGameState(s.game, s.settings.lockOn.breakDistance + 500.0f)) {
				s.resetPending = true;
				return;
			}
			if (s.resetPending) {
				s.system->Reset(s.game.mode);
				s.applier.Reset();
				s.resetPending = false;
				dt = 0.0f;
			}
			dt = std::clamp(dt, 0.0f, kMaxFrameTime);

			const auto input = InputHandler::GetSingleton()->Consume();
			const auto& in = s.settings.input;

			FrameSnapshot frame{};
			frame.dt = dt;
			frame.view = s.game.view;
			frame.mode = s.game.mode;
			frame.candidates = s.game.candidates;
			frame.lookX = input.mouseX * in.mouseFlickScale + input.stickX * in.stickFlickScale * dt;
			frame.attackStarted = input.attackPressed;
			frame.lockPressed = input.lockPressed;
			frame.shoulderPressed = input.shoulderPressed;

			const auto result = s.system->Tick(frame);
			if (result.lockEvent != LockEvent::kNone) {
				logger::info("lock {} (target {:08X})", ToString(result.lockEvent), result.lockTarget);
			}

			s.applier.ApplyRotation(result.rig);
			s.applier.ApplyFraming(result.rig, s.game.thirdPerson);
		}

		// The game calls PlayerCamera::Update directly, never through its vtable,
		// so hooking it there never fires. The active camera state's Update is
		// a real virtual call, once per frame on the main thread, before the
		// state reads the player's rotation and shoulder offset.
		// Tag only gives each hooked vtable its own `func`.
		template <int Tag>
		struct CameraStateUpdateHook
		{
			static void thunk(RE::TESCameraState* a_this, RE::BSTSmartPointer<RE::TESCameraState>& a_next)
			{
				Tick();
				func(a_this, a_next);
			}

			static inline REL::Relocation<decltype(thunk)> func;

			static void Install(std::array<REL::ID, 1> a_vtbl, const char* a_name)
			{
				REL::Relocation<std::uintptr_t> vtbl{ a_vtbl[0] };
				func = vtbl.write_vfunc(0xB, thunk);  // TESCameraState::Update
				logger::info("hooked {}::Update", a_name);
			}
		};
	}

	void Install()
	{
		auto& s = Get();
		if (s.system) {
			return;
		}
		s.settings = LoadSettings();
		s.system = std::make_unique<CameraSystem>(s.settings);
		s.last = std::chrono::steady_clock::now();

		auto input = InputHandler::GetSingleton();
		input->SetSettings(s.settings.input);
		input->Register();

		CameraStateUpdateHook<0>::Install(RE::VTABLE::ThirdPersonState, "ThirdPersonState");
		CameraStateUpdateHook<1>::Install(RE::VTABLE::FirstPersonState, "FirstPersonState");
	}

	void OnGameLoaded()
	{
		Get().resetPending = true;
	}
}
