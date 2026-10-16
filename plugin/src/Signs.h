#pragma once

#include "PCH.h"

#include "Engine.h"

#include "witcher/settings.h"
#include "witcher/targeting.h"

// Witcher Signs, plugin side. The camera tick feeds in each frame's actor
// snapshot; the Papyrus script (WitcherSignsQuest) calls the WitcherSigns
// native functions to check a cast and get its targets, then applies the
// effects with vanilla Papyrus (Spell.Cast, PushActorAway).
namespace witcher::plugin::signs
{
	void Init(const SignsSettings& a_settings);

	// Main thread, once per gameplay frame.
	void OnFrame(float a_dt, const GameState& a_game, ActorId a_lockTarget);

	void OnGameLoaded();

	// For F4SE::PapyrusInterface::Register.
	bool F4SEAPI RegisterFunctions(RE::BSScript::IVirtualMachine* a_vm);
}
