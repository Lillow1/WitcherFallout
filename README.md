# Witcher Camera (Fallout 4)

Witcher 3-style third-person camera for **Fallout 4 1.10.163 (old-gen / downgraded)** with **F4SE 0.6.23**.

- **Lock-on**: press Z (or right stick click) with a weapon drawn. The player turns to face the target smoothly, with the view tilted slightly down.
  - Flick the mouse or right stick to switch to the next enemy left or right.
  - When the target dies, the lock hops to the next enemy.
  - The lock drops when you holster or the target gets too far away.
- **Soft lock**: when you start an attack near an enemy without a lock, you turn into them over about 0.08 s.
- **Framing**: separate over-the-shoulder setups for exploring, ranged combat and melee, blended smoothly.
  - C swaps shoulders.
  - The camera pulls back for groups and for big creatures like deathclaws.
- **Directional movement** (toggle with N): the camera orbits freely and the character turns to face where it runs. Press back and you turn around and run at the camera instead of backpedalling.
  - Standing still, the camera orbits and the character keeps its facing.
  - Melee attacks go toward the soft-lock target, else where you're running, else where the camera looks. Facing is held briefly during the swing.
  - Locked on, the character faces the target and strafes around it, and the camera swings in behind.
  - With a gun drawn you face the crosshair and strafe, like vanilla (`bRangedFacesCamera`).
- **Shake**: trauma-based camera shake. The system is wired up, but nothing triggers it yet (see below).

## Layout

| Path | What |
|---|---|
| `core/` | Engine-free camera logic: targeting, lock-on, camera rig, directional movement, settings. Builds on any OS. |
| `tests/` | 33 tests for the core, no dependencies. |
| `plugin/src/` | F4SE plugin. The only code that touches game memory (`Engine.cpp`, `Movement.cpp`) or input (`Input.cpp`). |
| `config/WitcherCamera.ini` | All tunables, commented. |

All the feel is decided in `core/`, where it's unit-tested. The plugin only reads a snapshot of the game each frame and writes the result back. To try a camera behavior, change `core/` and run the tests; no game launch needed.

## Build

Run the core tests anywhere:

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Build the plugin on Windows with Visual Studio 2022, CMake and vcpkg:

1. Clone the **original** CommonLibF4 into `extern/CommonLibF4`:
   ```sh
   git clone https://github.com/Ryan-rsm-McKenzie/CommonLibF4 extern/CommonLibF4
   ```
   Don't use the NG forks (alandtse, etc.). They export only `F4SEPlugin_Version`, which F4SE 0.6.23 ignores. That's the same reason LooksMenu 1.7.x won't load on this setup.
2. Set `VCPKG_ROOT`. Optionally set `Fallout4Path` to auto-copy the DLL and INI after each build.
3. Configure and build:
   ```sh
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
4. Install as an MO2 mod:
   - `F4SE/Plugins/WitcherCamera.dll`
   - `F4SE/Plugins/WitcherCamera.ini`

   It needs the 1.10.163 Address Library. The plugin refuses to load on any other game version instead of crashing.

The log is at `Documents/My Games/Fallout4/F4SE/WitcherCamera.log`. Lock-on events are logged there.

## How it hooks in

- **Per-frame update**: vtable hooks on `TESCameraState::Update` (vfunc 0xB) for `ThirdPersonState` and `FirstPersonState`. The active state's update runs once per frame on the main thread, before it reads the player's rotation and shoulder offset.
  - Don't hook `PlayerCamera::Update` (TESCamera vfunc 0x3) instead: the game calls it directly, not through the vtable, so that hook never fires.
- **Directional movement**, in two parts:
  - The camera is split from the body with the engine's own free-rotation mode (`ThirdPersonState::freeRotationEnabled` / `freeRotation.x`, the holstered "orbit" camera). Each tick sets the body heading and writes `freeRotation.x` so the camera stays put in the world while the body turns.
  - A vtable hook on `IMovementPlayerControls::GetControllerOutput` (slot 1) of `PlayerControls`. While the movement system reads input, `moveInputVec` is temporarily rotated from camera-relative to body-relative, then restored. The right vtable is found by reading MSVC's RTTI locator (base offset 0x30), not by list order. If it can't be verified, directional movement stays off and the log says so.
- **Lock-on and soft lock**: these steer the player's own yaw and pitch (`data.angle`). The vanilla third-person camera follows the player, so collision, animations and first-person all keep working.
- **Framing**: writes `ThirdPersonState::targetShoulderOffset`. The engine already eases toward that target.
- **Nearby actors**: read from `ProcessLists::highActorHandles`. This CommonLibF4 doesn't define `ProcessLists`, so `Engine.cpp` uses the 1.10.163 address ID and field offset from alandtse/CommonLibF4.
- **Input**: an observer at the front of `MenuControls::handlers`. It never consumes events, so the game still gets every key.
  - Keyboard `idCode`s are Windows virtual-key codes (Z = `0x5A`), not DirectInput scan codes.

## Check first in game

Confirmed in game (2026-10-11): lock-on holds while strafing, and positive `fDistance` pulls the camera back (`targetShoulderOffset.y = -fDistance`).

Still to check:

1. **Mouse while locked.** Mouse movement is damped back toward the target rather than blocked, so it may feel slightly springy. If that bothers you, zero `PlayerControls::data.lookInputVec` while locked.
2. **Controller binding.** Right stick click may clash with your current bindings. Change `iLockOnGamepad` in the INI.

Directional movement (new, not yet run in game). Check these in order:

1. **Hooks load.** The log should say `hooked PlayerControls::GetControllerOutput`, then `movement hook live` once you move. If it says the vtable wasn't found, nothing else applies.
2. **Free-rotation direction.** Holster, stand still and move the mouse: only the camera should orbit. If the camera turns the opposite way to the mouse, or swings away whenever the character turns, flip `kFreeRotationSign` in `Engine.h` to `-1`.
3. **Running back.** Press S: the character should turn around (over ~0.25 s) and run toward the camera, with the camera staying still. If it runs sideways or circles, the input remap is fighting the engine. Report what you see.
4. **Engine pulling the body back.** If the character keeps snapping back to face the camera while running, the game is resetting free rotation when you move. We'd then also hook `ThirdPersonState::ProcessWeaponDrawnChange` / `SetFreeRotationMode`.
5. **Locked-on pitch.** With free rotation on, the camera's up/down may come from `freeRotation.y` instead of the player's pitch. If the lock-on no longer tilts the view toward the target, tell me.
6. **Threads.** Compare the thread numbers in the two log lines from step 1. If they differ, the movement hook runs off the main thread and needs a lock around the input swap.

## Known gaps / next steps

- **Line of sight isn't checked yet.** Every nearby enemy counts as visible, so the lock drops only on distance or death. It needs an LOS function address for 1.10.163.
- **Nothing triggers shake yet.** It needs a hit event sink (`TESHitEvent`) to feed in trauma.
- **The "attack started" trigger is `PrimaryAttack` only.** Power attacks and bash aren't hooked up yet.
- **The crosshair is still shown** in melee directional mode. It should hide while running and come back when aiming.
- **Not yet merged with the Ciri project.** It wasn't reachable from this session. Once it's available, its hook setup and logging can replace `main.cpp`/`Plugin.cpp`. Only `Engine.cpp`, `Movement.cpp` and `Input.cpp` touch the game.
