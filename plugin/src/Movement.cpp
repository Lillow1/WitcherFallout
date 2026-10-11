#include "Movement.h"

#include "Engine.h"

#include "witcher/directional.h"

#include <atomic>
#include <functional>
#include <optional>
#include <span>
#include <thread>

namespace witcher::plugin::movement
{
	namespace
	{
		std::atomic<bool> g_active{ false };
		std::atomic<bool> g_loggedFirstCall{ false };

		// IMovementPlayerControls sits at +0x30 inside PlayerControls
		// (see RE/Bethesda/PlayerControls.h).
		constexpr std::uint32_t kMovementInterfaceOffset = 0x30;

		// PlayerControls has seven vtables, one per base class. Pick the one
		// whose MSVC RTTI locator (stored just before the vtable) says it
		// belongs to the base at a_offset, instead of trusting list order.
		std::optional<REL::ID> FindVtableForBaseOffset(std::span<const REL::ID> a_ids, std::uint32_t a_offset)
		{
			for (const auto& id : a_ids) {
				REL::Relocation<std::uintptr_t> vtbl{ id };
				const auto entries = reinterpret_cast<const std::uintptr_t*>(vtbl.address());
				// RTTICompleteObjectLocator: signature, offset, cdOffset, ...
				const auto locator = reinterpret_cast<const std::uint32_t*>(entries[-1]);
				if (locator && locator[1] == a_offset) {
					return id;
				}
			}
			return std::nullopt;
		}

		struct GetControllerOutputHook
		{
			// The movement system asks the player's controls for this frame's
			// input here. We hand it camera-relative input rotated into the
			// character's frame, then put the real input back.
			static void thunk(RE::IMovementPlayerControls* a_this, std::uint32_t a_id, RE::PlayerControlsMovementData& a_out)
			{
				if (!g_loggedFirstCall.exchange(true)) {
					logger::info("movement hook live (thread {})", std::hash<std::thread::id>{}(std::this_thread::get_id()));
				}

				const auto controls = RE::PlayerControls::GetSingleton();
				const auto player = RE::PlayerCharacter::GetSingleton();
				const auto third = g_active.load() ? CurrentThirdPerson() : nullptr;
				if (!controls || !player || !third || !third->freeRotationEnabled) {
					func(a_this, a_id, a_out);
					return;
				}

				auto& move = controls->data.moveInputVec;
				const auto saved = move;
				const float body = player->data.angle.z;
				const float camera = WrapAngle(body + kFreeRotationSign * third->freeRotation.x);
				const auto remapped = RemapMoveInput({ saved.x, saved.y }, camera, body);
				move.x = remapped.x;
				move.y = remapped.y;

				func(a_this, a_id, a_out);

				move = saved;
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	bool Install()
	{
		const auto id = FindVtableForBaseOffset(RE::VTABLE::PlayerControls, kMovementInterfaceOffset);
		if (!id) {
			logger::error("PlayerControls movement vtable not found; directional movement disabled");
			return false;
		}
		REL::Relocation<std::uintptr_t> vtbl{ *id };
		GetControllerOutputHook::func = vtbl.write_vfunc(0x1, GetControllerOutputHook::thunk);  // GetControllerOutput
		logger::info("hooked PlayerControls::GetControllerOutput (main thread {})",
			std::hash<std::thread::id>{}(std::this_thread::get_id()));
		return true;
	}

	void SetActive(bool a_active)
	{
		g_active.store(a_active);
	}
}
