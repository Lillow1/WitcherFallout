Scriptname WitcherSignsQuest extends Quest
{Casts Witcher Signs. Hotkeys and effects live here; targeting, costs and
cooldowns come from WitcherCamera.dll through the WitcherSigns natives.
Attach to a Start Game Enabled quest and fill the properties (see signs/README.md).}

Actor Property PlayerRef Auto Const Mandatory
ActorValue Property ActionPoints Auto Const Mandatory

Spell[] Property SignSpells Auto Const Mandatory
{Per-target effect of a normal cast, by Sign: 0 Aard, 1 Igni, 2 Yrden, 3 Quen, 4 Axii.}
Spell[] Property AltSignSpells Auto Const Mandatory
{Per-target effect of a held (alternate) cast, same order.}
Spell[] Property SignCastFX Auto Const
{Optional: visual-only spell cast on the player for every successful cast, same order.
Lets Aard and Igni show something even when they hit nobody.}

Float Property AardForce = 6.0 Auto Const
{Knockback for a normal Aard.}
Float Property AardAltForce = 3.0 Auto Const
{Knockback for the 360-degree held Aard.}

; Windows virtual-key codes (what F4SE's RegisterForKey uses).
Int Property CastKey = 88 Auto Const
{X. Tap to cast; hold for the alternate cast.}
Int Property NextSignKey = 66 Auto Const
{B}
Int Property PrevSignKey = 86 Auto Const
{V}

Int Property SIGN_AARD = 0 AutoReadOnly Hidden
Int Property SIGN_QUEN = 3 AutoReadOnly Hidden

Int Property RESULT_OK = 0 AutoReadOnly Hidden
Int Property RESULT_COOLDOWN = 1 AutoReadOnly Hidden
Int Property RESULT_NO_AP = 2 AutoReadOnly Hidden
Int Property RESULT_NO_TARGET = 3 AutoReadOnly Hidden


Event OnQuestInit()
	RegisterKeys()
	ShowSelected(WitcherSigns.GetSelectedSign())
EndEvent

; Re-register after each load so the hotkeys survive saves and plugin updates.
Event Actor.OnPlayerLoadGame(Actor akSender)
	RegisterKeys()
EndEvent

Function RegisterKeys()
	RegisterForKey(CastKey)
	RegisterForKey(NextSignKey)
	RegisterForKey(PrevSignKey)
	RegisterForRemoteEvent(PlayerRef, "OnPlayerLoadGame")
EndFunction

Event OnKeyDown(int aiKeyCode)
	If !CanAct()
		Return
	EndIf
	If aiKeyCode == NextSignKey
		ShowSelected(WitcherSigns.CycleSign(1))
	ElseIf aiKeyCode == PrevSignKey
		ShowSelected(WitcherSigns.CycleSign(-1))
	EndIf
EndEvent

; Casting happens on release, so the hold time decides normal or alternate.
Event OnKeyUp(int aiKeyCode, float afHoldTime)
	If aiKeyCode == CastKey && CanAct()
		Cast(afHoldTime >= WitcherSigns.GetAltHoldTime())
	EndIf
EndEvent

bool Function CanAct()
	Return !Utility.IsInMenuMode() && !PlayerRef.IsDead()
EndFunction

; None for a missing array or a slot that wasn't filled.
Spell Function SpellAt(Spell[] akSpells, int aiIndex)
	If akSpells && aiIndex < akSpells.Length
		Return akSpells[aiIndex]
	EndIf
	Return None
EndFunction

Function ShowSelected(int aiSign)
	Debug.Notification("Sign: " + WitcherSigns.GetSignName(aiSign))
EndFunction

Function Cast(bool abAlt)
	int sign = WitcherSigns.GetSelectedSign()
	int result = WitcherSigns.TryCast(abAlt, PlayerRef.GetValue(ActionPoints))

	If result == RESULT_COOLDOWN
		Return  ; mashing the key shouldn't spam messages
	ElseIf result == RESULT_NO_AP
		Debug.Notification("Not enough Action Points for " + WitcherSigns.GetSignName(sign))
		Return
	ElseIf result == RESULT_NO_TARGET
		Debug.Notification(WitcherSigns.GetSignName(sign) + ": no target")
		Return
	EndIf

	PlayerRef.DamageValue(ActionPoints, WitcherSigns.GetLastCost())

	Spell fx = SpellAt(SignCastFX, sign)
	If fx
		fx.Cast(PlayerRef, PlayerRef)
	EndIf

	Spell effect = SpellAt(SignSpells, sign)
	If abAlt
		effect = SpellAt(AltSignSpells, sign)
	EndIf
	If !effect
		Debug.Trace("WitcherSignsQuest: no spell set for " + WitcherSigns.GetSignName(sign) + " (alt " + abAlt + ")", 1)
		Return
	EndIf

	If sign == SIGN_QUEN
		effect.Cast(PlayerRef, PlayerRef)
		Return
	EndIf

	Actor[] targets = WitcherSigns.GetLastTargets()
	int i = 0
	While i < targets.Length
		Actor target = targets[i]
		If sign == SIGN_AARD
			If abAlt
				PlayerRef.PushActorAway(target, AardAltForce)
			Else
				PlayerRef.PushActorAway(target, AardForce)
			EndIf
		EndIf
		effect.Cast(PlayerRef, target)
		i += 1
	EndWhile
EndFunction
