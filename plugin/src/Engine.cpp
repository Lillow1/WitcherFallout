#include "Engine.h"

namespace witcher::plugin
{
	namespace
	{
		// Height of the camera pivot above the player's feet, in game units.
		constexpr float kEyeHeight = 110.0f;
		// Typical human bound height, used to turn bounds into a size scale.
		constexpr float kHumanHeight = 128.0f;
		// Aim a little below the top of the bounds: chest/head height.
		constexpr float kAimHeightFraction = 0.75f;

		// Weapon animation types from the WEAP record. 0-6 are melee
		// (hand to hand, swords, daggers, axes, maces, two-handers).
		constexpr std::int8_t kLastMeleeWeaponType = 6;

		// ProcessLists isn't in this CommonLibF4. Address ID and offset are
		// for 1.10.163, taken from alandtse/CommonLibF4 (ProcessLists.h).
		RE::BSTArray<RE::ActorHandle>* HighActorHandles()
		{
			static REL::Relocation<std::byte**> singleton{ REL::ID(1569706) };
			std::byte* lists = *singleton;
			return lists ? reinterpret_cast<RE::BSTArray<RE::ActorHandle>*>(lists + 0x40) : nullptr;
		}

		Vec3 ToVec(const RE::NiPoint3& a_p)
		{
			return { a_p.x, a_p.y, a_p.z };
		}

		bool HasMeleeWeaponOut(RE::PlayerCharacter* a_player)
		{
			const auto process = a_player->currentProcess;
			const auto middleHigh = process ? process->middleHigh : nullptr;
			if (!middleHigh) {
				return false;
			}
			RE::BSAutoLock lock{ middleHigh->equippedItemsLock };
			for (const auto& item : middleHigh->equippedItems) {
				const auto weapon = item.item.object ? item.item.object->As<RE::TESObjectWEAP>() : nullptr;
				if (weapon && item.equipIndex.index == 0) {
					return weapon->weaponData.type <= kLastMeleeWeaponType;
				}
			}
			return true;  // nothing in the weapon slot: fists
		}

		CameraMode ReadMode(RE::PlayerCharacter* a_player)
		{
			if (!a_player->GetWeaponMagicDrawn()) {
				return CameraMode::kExploration;
			}
			return HasMeleeWeaponOut(a_player) ? CameraMode::kMelee : CameraMode::kCombat;
		}

		RE::ThirdPersonState* ThirdPerson(RE::PlayerCamera* a_camera)
		{
			const auto state = a_camera ? a_camera->currentState.get() : nullptr;
			if (!state || *state->id != RE::CameraState::k3rdPerson) {
				return nullptr;
			}
			return static_cast<RE::ThirdPersonState*>(state);
		}

		// The third-person state object, even while another state is active.
		RE::ThirdPersonState* AnyThirdPerson(RE::PlayerCamera* a_camera)
		{
			return a_camera ? static_cast<RE::ThirdPersonState*>(a_camera->cameraStates[RE::CameraState::k3rdPerson].get()) : nullptr;
		}
	}

	RE::ThirdPersonState* CurrentThirdPerson()
	{
		return ThirdPerson(RE::PlayerCamera::GetSingleton());
	}

	bool ReadGameState(GameState& a_out, float a_searchRadius)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!player || !camera || !player->GetParentCell() || !player->Get3D()) {
			return false;
		}

		const Vec3 feet = ToVec(player->data.location);
		a_out.view.eye = { feet.x, feet.y, feet.z + kEyeHeight };
		const auto third = ThirdPerson(camera);
		const float freeRot = third && third->freeRotationEnabled ? kFreeRotationSign * third->freeRotation.x : 0.0f;
		a_out.bodyYaw = player->data.angle.z;
		a_out.view.yaw = WrapAngle(a_out.bodyYaw + freeRot);
		a_out.view.pitch = player->data.angle.x;
		a_out.mode = ReadMode(player);
		a_out.thirdPerson = third != nullptr;

		const auto controls = RE::PlayerControls::GetSingleton();
		a_out.move = controls ? Vec2{ controls->data.moveInputVec.x, controls->data.moveInputVec.y } : Vec2{};

		a_out.candidates.clear();
		const auto handles = HighActorHandles();
		if (!handles) {
			return true;
		}

		for (auto handle : *handles) {  // by value: native_handle() is non-const here
			const auto ptr = handle.get();
			const auto actor = ptr.get();
			if (!actor || actor == player || actor->IsDeleted() || !actor->Get3D()) {
				continue;
			}

			const Vec3 pos = ToVec(actor->data.location);
			if ((pos - feet).Length() > a_searchRadius) {
				continue;
			}

			const float height = actor->GetBoundMax().z - actor->GetBoundMin().z;
			const float h = height > 1.0f ? height : kHumanHeight;

			Candidate c{};
			c.id = handle.native_handle();
			c.aimPoint = { pos.x, pos.y, pos.z + h * kAimHeightFraction };
			c.hostile = actor->GetHostileToActor(player);
			c.dead = actor->IsDead(true);
			c.visible = true;  // TODO: needs a line-of-sight call; lock drops on distance until then
			c.scale = h / kHumanHeight;
			a_out.candidates.push_back(c);
		}
		return true;
	}

	void Applier::ApplyRotation(const RigOutput& a_rig)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const bool shaking = a_rig.shakeYaw != 0.0f || a_rig.shakePitch != 0.0f || _lastShakeYaw != 0.0f || _lastShakePitch != 0.0f;
		if (!a_rig.driveYaw && !a_rig.drivePitch && !shaking) {
			return;  // leave the player's rotation entirely to the game
		}
		auto& angle = player->data.angle;

		// Take last frame's shake back out before applying anything new, so
		// shake never accumulates into the player's real heading.
		float yaw = angle.z - _lastShakeYaw;
		float pitch = angle.x - _lastShakePitch;
		if (a_rig.driveYaw) {
			yaw = a_rig.yaw;
		}
		if (a_rig.drivePitch) {
			pitch = a_rig.pitch;
		}

		angle.z = WrapAngle(yaw + a_rig.shakeYaw);
		angle.x = pitch + a_rig.shakePitch;
		_lastShakeYaw = a_rig.shakeYaw;
		_lastShakePitch = a_rig.shakePitch;
	}

	void Applier::ApplyFraming(const RigOutput& a_rig, bool a_thirdPerson)
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera) {
			return;
		}

		if (const auto third = a_thirdPerson ? ThirdPerson(camera) : nullptr) {
			// The engine blends currentShoulderOffset toward this target itself.
			third->targetShoulderOffset.x = a_rig.framing.shoulderX;
			third->targetShoulderOffset.y = -a_rig.framing.distance;
			third->targetShoulderOffset.z = a_rig.framing.shoulderZ;
		}

		if (a_rig.framing.fov > 0.0f) {
			if (_savedFov <= 0.0f) {
				_savedFov = camera->worldFOV;
			}
			camera->worldFOV = a_rig.framing.fov;
		} else if (_savedFov > 0.0f) {
			camera->worldFOV = _savedFov;
			_savedFov = 0.0f;
		}
	}

	void Applier::ApplyDirectional(const DirectionalOutput& a_dir)
	{
		const auto camera = RE::PlayerCamera::GetSingleton();
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!camera || !player) {
			return;
		}

		if (a_dir.active) {
			if (const auto third = ThirdPerson(camera)) {
				// Free rotation is the engine's own "orbit the character" mode:
				// look input turns the camera, and the body is left alone.
				third->freeRotationEnabled = true;
				third->freeRotation.x = kFreeRotationSign * a_dir.cameraOffset;
				_ownsFreeRotation = true;
			}
			return;
		}

		if (!_ownsFreeRotation) {
			return;
		}
		// Hand control back: turn the body to where the camera was looking.
		if (const auto third = AnyThirdPerson(camera)) {
			auto& angle = player->data.angle;
			angle.z = WrapAngle(angle.z + kFreeRotationSign * third->freeRotation.x);
			third->freeRotation.x = 0.0f;
			third->freeRotationEnabled = false;
		}
		_ownsFreeRotation = false;
	}

	void Applier::Reset()
	{
		_lastShakeYaw = 0.0f;
		_lastShakePitch = 0.0f;
		_savedFov = 0.0f;
	}
}
