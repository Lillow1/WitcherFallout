# Witcher Signs

Five Signs, each with a normal (tap) and an alternate (hold) cast:

| Sign | Tap | Hold |
|---|---|---|
| **Aard** | Knockback cone in front of you | Weaker blast all around you |
| **Igni** | Short fire cone | Longer, narrower stream |
| **Yrden** | Slows enemies around you | Wider circle |
| **Quen** | Shield that soaks one hit | Shield that soaks three hits |
| **Axii** | Calms one enemy (your lock-on target, else the one ahead) | Longer, from further away |

Controls: **X** casts, tap or hold. **B** / **V** pick the next / previous Sign. Every cast spends Action Points.

## How it's split

| Part | Where | Does |
|---|---|---|
| Plugin (`WitcherCamera.dll`) | `core/src/signs.cpp`, `plugin/src/Signs.cpp` | Which Sign is selected, AP and cooldown checks, and who gets hit: cone, area, or single target, preferring your lock-on target. Unit-tested. |
| Papyrus | `signs/Scripts/Source/User/` | Hotkeys, spending AP, and applying effects: `Spell.Cast` on each target, `PushActorAway` for Aard. |
| Your plugin (`.esp`) | Creation Kit | The spells and magic effects: damage, burning, slow, shield, calm, and visuals. |

The plugin can't cast spells itself: none of the CommonLibF4 versions have casting for 1.10.163. Papyrus can, so it does that part.

Costs, ranges, cone widths, cooldowns and target limits are in the `[Signs.*]` sections of `WitcherCamera.ini`.

## Setting up the Creation Kit side

You need F4SE's script sources (your "F4SE Scripts 0.6.23" mod) where the CK's Papyrus compiler can find them, because the scripts use F4SE's `RegisterForKey`.

### 1. Copy the scripts
Copy the three `.psc` files from `signs/Scripts/Source/User/` into `Fallout 4/Data/Scripts/Source/User/`.

### 2. Magic effects (Magic Effect, under Magic in the Object Window)

| Editor ID | Archetype | Settings |
|---|---|---|
| `WS_AardFX` | Value Modifier, Health | Detrimental, Hostile. Magnitude 5, duration 0: a tap of damage so the hit registers. Add an impact visual. |
| `WS_IgniFX` | Value Modifier, Health | Detrimental, Hostile. Resist value: Energy Resist. 8 per second for 4 s. Effect shader: copy a vanilla fire shader (search "Fire" under Effect Shaders). |
| `WS_YrdenFX` | Value Modifier, SpeedMult | Detrimental, Recover. Magnitude 50 for 8 s. The alt version lasts 12 s. A slight purple shader helps you read it. |
| `WS_QuenFX` | Value Modifier, DamageResist | Recover. Magnitude 500 for 20 s. Script: `WitcherQuenEffect`, `HitsToBreak` = 1. Add a second effect on EnergyResist with the same values. |
| `WS_QuenAltFX` | as Quen | 30 s, `HitsToBreak` = 3. |
| `WS_AxiiFX` | Calm | Hostile. Magnitude 100 (max level affected) for 15 s. The alt version lasts 30 s. |

If **Calm** isn't in your archetype list, tell me and I'll give you a script-based version.

### 3. Spells (Spell, under Magic)

Make 10 spells: one normal and one alt for each Sign. Each uses its magic effect from step 2.
- **Type:** Spell. **Casting:** Fire and Forget.
- **Delivery:** Target Actor. For Quen, use **Self**.
- Untick "Auto-Calculate" and set the cost to 0. The plugin charges Action Points instead.

Optionally, make 5 visual-only spells (Self delivery, with a casting effect such as a flash or particles). These go in `SignCastFX` and play on every cast, even when nothing is hit.

### 4. The quest
1. Make a new quest, `WitcherSignsQuest`, and tick **Start Game Enabled**.
2. In its Scripts tab, add `WitcherSignsQuest` and fill in the properties:
   - `PlayerRef`: the player reference (`PlayerRef`).
   - `ActionPoints`: the `ActionPoints` actor value.
   - `SignSpells`: the 5 normal spells **in this order**: Aard, Igni, Yrden, Quen, Axii.
   - `AltSignSpells`: the 5 alt spells, same order.
   - `SignCastFX` (optional): visual spells, same order.
   - The keys (`CastKey` and so on) default to X / B / V. They're Windows virtual-key codes.
3. Save your plugin, enable it, and start a **new game** or load a save made before the quest existed. Start Game Enabled quests start on load.

## Check in game

1. **Hotkeys:** press B. A "Sign: Igni" notification means F4SE's key events work. If nothing appears, check that the F4SE scripts are installed and the key codes are right.
2. **Natives:** `WitcherCamera.log` should say `registered WitcherSigns Papyrus functions`. Each cast logs `cast Aard -> N target(s), 30 AP`.
3. **Aard near a raider:** they should go flying and your AP should drop. If the log says 0 targets with an enemy right in front of you, tell me.
4. **Hold X** for half a second for the alternate cast. The log shows `(alt)`.

If casting does nothing, look in `Documents/My Games/Fallout4/Logs/Script/Papyrus.0.log`. A missing spell slot shows up there as `WitcherSignsQuest: no spell set for ...`.

## Not done yet
- **Signs ignore walls.** Targeting doesn't check line of sight yet; it's the same gap as lock-on.
- **No cast animation.** The visual-only `SignCastFX` spells stand in for now.
- **No HUD element.** The selected Sign only shows as a notification.
- **Witcher 3 behaviours left for later:** Yrden as a trap that stays where you put it, Igni's alternate as a held stream, and Axii in dialogue.
- **AP sharing:** Signs spend the same Action Points as sprinting and VATS. When your combat work comes back, the costs may need rebalancing with dodges and strong attacks.
