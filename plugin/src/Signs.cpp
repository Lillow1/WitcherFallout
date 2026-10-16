#include "Signs.h"

#include "witcher/signs.h"

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace witcher::plugin::signs
{
	namespace
	{
		constexpr const char* kScript = "WitcherSigns";  // BindNativeMethod wants a null-terminated name

		// Papyrus natives run on the script VM's threads, the snapshot is
		// written on the main thread: everything here is behind one lock.
		struct State
		{
			std::mutex lock;
			std::unique_ptr<SignBook> book;
			const SignsSettings* settings{ nullptr };

			View view{};
			std::vector<Candidate> candidates;
			std::vector<RE::ActorHandle> handles;
			ActorId lockTarget{ 0 };
			bool haveSnapshot{ false };

			float lastCost{ 0.0f };
			std::vector<RE::ActorHandle> lastTargets;
		};

		State& Get()
		{
			static State state;
			return state;
		}

		// ---- natives (WitcherSigns.psc) ----

		std::int32_t GetSelectedSign(std::monostate)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			return s.book ? static_cast<std::int32_t>(s.book->Selected()) : 0;
		}

		std::int32_t SelectSign(std::monostate, std::int32_t a_sign)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			return s.book ? static_cast<std::int32_t>(s.book->Select(a_sign)) : 0;
		}

		std::int32_t CycleSign(std::monostate, std::int32_t a_direction)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			return s.book ? static_cast<std::int32_t>(s.book->Cycle(a_direction)) : 0;
		}

		std::string_view GetSignName(std::monostate, std::int32_t a_sign)
		{
			if (a_sign < 0 || a_sign >= static_cast<std::int32_t>(kSignCount)) {
				return ""sv;
			}
			return SignName(static_cast<Sign>(a_sign));
		}

		float GetAltHoldTime(std::monostate)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			return s.settings ? s.settings->altHoldTime : 0.4f;
		}

		float GetCooldown(std::monostate, std::int32_t a_sign)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			if (!s.book || a_sign < 0 || a_sign >= static_cast<std::int32_t>(kSignCount)) {
				return 0.0f;
			}
			return s.book->CooldownRemaining(static_cast<Sign>(a_sign));
		}

		// Returns a CastResult code: 0 ok, 1 cooling down, 2 not enough AP, 3 no target.
		// On 0, the script spends GetLastCost() AP and applies effects to GetLastTargets().
		std::int32_t TryCast(std::monostate, bool a_alt, float a_currentAP)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			s.lastCost = 0.0f;
			s.lastTargets.clear();
			if (!s.book || !s.haveSnapshot) {
				return static_cast<std::int32_t>(CastResult::kNoTarget);
			}

			const auto out = s.book->TryCast(a_alt, a_currentAP, s.view, s.candidates, s.lockTarget);
			if (out.result == CastResult::kOk) {
				s.lastCost = out.cost;
				for (const auto i : out.targets) {
					s.lastTargets.push_back(s.handles[i]);
				}
				logger::info("cast {}{} -> {} target(s), {} AP", SignName(out.sign), out.alt ? " (alt)" : "", out.targets.size(), out.cost);
			}
			return static_cast<std::int32_t>(out.result);
		}

		float GetLastCost(std::monostate)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			return s.lastCost;
		}

		std::vector<RE::Actor*> GetLastTargets(std::monostate)
		{
			auto& s = Get();
			std::scoped_lock l{ s.lock };
			std::vector<RE::Actor*> out;
			for (const auto& handle : s.lastTargets) {
				// An actor can unload between the cast and this call; skip it if so.
				if (const auto actor = handle.get()) {
					out.push_back(actor.get());
				}
			}
			return out;
		}
	}

	void Init(const SignsSettings& a_settings)
	{
		auto& s = Get();
		std::scoped_lock l{ s.lock };
		s.settings = std::addressof(a_settings);
		s.book = std::make_unique<SignBook>(a_settings);
	}

	void OnFrame(float a_dt, const GameState& a_game, ActorId a_lockTarget)
	{
		auto& s = Get();
		std::scoped_lock l{ s.lock };
		if (!s.book) {
			return;
		}
		s.book->Tick(a_dt);
		s.view = a_game.view;
		s.candidates = a_game.candidates;
		s.handles = a_game.handles;
		s.lockTarget = a_lockTarget;
		s.haveSnapshot = true;
	}

	void OnGameLoaded()
	{
		auto& s = Get();
		std::scoped_lock l{ s.lock };
		if (s.book) {
			s.book->Reset();
		}
		s.haveSnapshot = false;
		s.lastTargets.clear();
	}

	bool F4SEAPI RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}
		a_vm->BindNativeMethod(kScript, "GetSelectedSign", GetSelectedSign, true);
		a_vm->BindNativeMethod(kScript, "SelectSign", SelectSign, true);
		a_vm->BindNativeMethod(kScript, "CycleSign", CycleSign, true);
		a_vm->BindNativeMethod(kScript, "GetSignName", GetSignName, true);
		a_vm->BindNativeMethod(kScript, "GetAltHoldTime", GetAltHoldTime, true);
		a_vm->BindNativeMethod(kScript, "GetCooldown", GetCooldown, true);
		a_vm->BindNativeMethod(kScript, "TryCast", TryCast, true);
		a_vm->BindNativeMethod(kScript, "GetLastCost", GetLastCost, true);
		a_vm->BindNativeMethod(kScript, "GetLastTargets", GetLastTargets, true);
		logger::info("registered {} Papyrus functions", kScript);
		return true;
	}
}
