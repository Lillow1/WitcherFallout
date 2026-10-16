Scriptname WitcherSigns Native Hidden
{Witcher Signs natives, provided by WitcherCamera.dll (F4SE plugin).
Signs by number: 0 Aard, 1 Igni, 2 Yrden, 3 Quen, 4 Axii.}

; The selected Sign.
int Function GetSelectedSign() native global

; Selects a Sign by number and returns the selection (unchanged if out of range).
int Function SelectSign(int aiSign) native global

; Selects the next (1) or previous (-1) Sign, wrapping around. Returns the new one.
int Function CycleSign(int aiDirection) native global

string Function GetSignName(int aiSign) native global

; Seconds the cast key must be held for the alternate cast ([Signs] fAltHold in the INI).
float Function GetAltHoldTime() native global

; Seconds until this Sign can be cast again.
float Function GetCooldown(int aiSign) native global

; Checks cooldown, Action Points and targets for the selected Sign and starts its cooldown.
; Returns 0 ok, 1 cooling down, 2 not enough AP, 3 no target.
; On 0: spend GetLastCost() AP and apply effects to GetLastTargets().
int Function TryCast(bool abAlt, float afCurrentAP) native global

float Function GetLastCost() native global

; Who the last successful TryCast hit, nearest first. Can be empty for cones and areas.
Actor[] Function GetLastTargets() native global
