Scriptname WitcherQuenEffect extends ActiveMagicEffect
{Quen: the shield soaks a set number of hits, then breaks.
Put on the Quen magic effects; the effect's own damage resistance does the soaking.}

Int Property HitsToBreak = 1 Auto Const
{1 for normal Quen, more for the held (alternate) cast.}

Actor shielded
int hitsTaken = 0

Event OnEffectStart(Actor akTarget, Actor akCaster)
	shielded = akTarget
	hitsTaken = 0
	RegisterForHitEvent(akTarget)
EndEvent

Event OnHit(ObjectReference akTarget, ObjectReference akAggressor, Form akSource, Projectile akProjectile, bool abPowerAttack, bool abSneakAttack, bool abBashAttack, bool abHitBlocked, string apMaterial)
	hitsTaken += 1
	If hitsTaken >= HitsToBreak
		Dispel()
	Else
		RegisterForHitEvent(shielded)  ; hit events are one-shot: listen for the next one
	EndIf
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
	UnregisterForAllHitEvents()
EndEvent
