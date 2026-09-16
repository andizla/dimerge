# dimerge for SnowRunner

SnowRunner reads one DirectInput steering wheel. Pedals, shifters and button boxes on their own USB plugs can be bound in the menu but do nothing in the truck, or they flip the control scheme. dimerge is a dinput8.dll proxy that lives in the game's Bin folder and hands the game one device: the wheelbase, with the axes, buttons and hats of the other devices written into its data. The wheelbase itself is passed through untouched, so force feedback works as before.

## Files

- `bin\dinput8.dll` and `bin\dimerge.ini`: copy both into `Sources\Bin` next to SnowRunner.exe. Installed there on 2026-09-12.
- `src\`: the source and `build.bat` (Visual Studio 2022 Community, x64, static CRT). The build writes `src\out\dinput8.dll`.
- `tools\dienum.exe`: lists every DirectInput device with its VID:PID, axes, buttons, hats and force feedback effects. `dienum watch VID:PID 10` prints live values for ten seconds. Run it from a folder that holds dinput8.dll and it shows the merged view instead.
- `tools\pakpatch\crane_slots.py`: adds wheel binding slots for crane movement and crane mode to the game's `initial.pak` (see Crane on the wheel). `extras\initial.pak.orig` is the untouched pak.
- `release\WheelCraneBindings\` and `release\WheelCraneBindings_build25096372.zip`: the drop-in mod.io package, the patched `initial.cache_block` and `[strings]` folder plus a readme; users drop both into their `initial.pak` with WinRAR. Built with `crane_slots.py --export` from the vanilla Steam install (`steamapps\common\SnowRunner`, build 25096372), never from the play copy: `SnowRunner5` carries the Real Life mod's `initial.pak`, and `extras\initial.pak.orig` is that modded pak, not vanilla. After a game update, export again from the fresh vanilla pak and re-zip.
- `smt-addon\`: the manual transmission mod rebuilt as a ReShade add-on, with `build.bat` and the two switch scripts (see Manual transmission mod).
- `tools\steam\restore_settings.py`: puts a saved copy of the game's settings file back so the game reads it (see Game settings file).
- `KEYBINDS.md`: every slot the wheel can bind, with menu name, game input, context and current binding, plus the merged key numbers and every input link in the game data. `tools\keybinds\keybinds.py` regenerates it.

## Current mapping (bin\dimerge.ini)

| Device | Id | Merged as |
|---|---|---|
| SIMAGIC Alpha wheelbase | 0483:0522 | primary, untouched, force feedback native |
| SIMSONN Plus X pedals | DDFD:6011 | X, Y, Z onto the base's Throttle (Slider0), Brake (Y) and Wheel Right Pedal (Rz) slots |
| PXN-CB1 button box | 36E6:8001 | buttons 64 to 89, the joystick as buttons 90 to 93 (up, right, down, left) |
| GX100 shifter | 04B0:5750 | buttons 96 to 111; with the pull collar pulled, gates land on 112 to 117 (shift layer) |

The merged devices are hidden from the game (HideMerged=1). Devices not listed in the ini stay visible and untouched, the 8BitDo receiver for example.

## Checking a run

`dimerge.log` in Bin is rewritten on every launch. It lists the devices the game enumerated and which one became the primary, the merge table, and the game's DirectInput calls: data format, cooperative level, acquire, the first state reads, effects created. `LogLevel=2` logs every call and grows fast, so use it for a short run only.

## Changing the layout

Axis names are X, Y, Z, Rx, Ry, Rz, Slider0, Slider1. A rule such as `Axes=X->Slider0` puts the source device's X axis on the merged Slider0. `Buttons=64` starts a device's buttons at merged button 64, `Buttons=auto` takes the first free ones (the base leaves 116 to 127 free), and `none` skips that kind of input. `DeadAxes` names wheelbase axes that auto placement may take over.

SnowRunner binds buttons on a wheel but not extra hats, so a hat or mini joystick is best turned into buttons: `POVs=buttons:90` makes its four directions merged buttons 90 to 93 (up, right, down, left; a diagonal presses two). `POVs=0` shares the rim's own hat instead (the merged hat speaks only while it is pushed), `POVs=auto` takes the first free hat slot, `none` skips it.

Keep pedals off the Rx and Ry slots. The game's wheel presets carry hidden defaults per axis slot (read from `initial.cache_block` in `initial.pak`): Rx is camera zoom and minimap camera movement, Ry is minimap movement, the two sliders are camera rotation, and Y, Z and Rz are throttle, brake and clutch. A pedal on Rx moves the camera through a default you never see in the binding menu. After moving a pedal, bind it again in the menu (the binding stores the slot number: buttons are 0 to 127, axes 128 to 135 in slot order, hat directions 136 to 139).

If a pedal shows up on the wrong axis in the game, swap the right-hand names in the Pedals rule. If the game ignores buttons above some number, lower the start indexes; the rim uses the low numbers, and `dienum watch 0483:0522` shows which ones.

## Shift layer (6+6 on the GX100)

The GX100 is a six-gate H-shifter with a pull collar under the knob. Its firmware offers "6+3": with a "Comb.Switch" option on, collar plus gate 1, 5 or 6 becomes R, gear 7 or gear 8, gates 2 to 4 stay plain, and the collar itself is never reported. There is no 6+6 in the firmware, so the proxy does it: `Shift=14` names the source button that acts as the modifier (the collar, reported as button 14 once "Pull Button" is on), `ShiftButtons=112` is the first merged button of the shifted set (gate N lands on 112 + N - 1), and `ShiftHide=1` keeps the collar itself away from the game. The layer is decided when a gate engages and kept until it releases, so letting go of the collar while in gear changes nothing.

Shifter settings for this: both Comb switches off and Pull Button on, then Apply in the GX100 tool, or `tools\gx100\gx100cfg.py apply --comb1 0 --comb2 0 --pull 1`. Measured with `dienum watch 04B0:5750 15` on 2026-09-12: with a Comb switch on, collar plus gates 1 to 6 read 10, 1, 2, 3, 6, 7; with Pull Button on, 14 joins every line; with the switches off, the plain gate plus 14. The "S Gear" dropdown in the tool (AUTO, ON, OFF) is the sequential mode; OFF is the safe setting for H-pattern use. In the game, bind the shifted gears from wheel buttons 112 to 117.

`Invert=Z` under a Merge section flips a source axis inside the range the game set, for pedals that read full scale at rest. Not needed for the Simsonn set today.

## Crane on the wheel (pak patch)

A wheel the game does not know by vendor id gets the "Custom" preset, and the settings menu offers that preset a fixed list of slots: the `inputs` map of `steering_wheel_input_mapper.sso`, serialized in `initial.pak` as `initial.cache_block`. The stock list has winch lift and lower, cargo rotation, arm lift and lower, but no slot for moving the crane, entering crane mode, attaching cargo or the anchor, so those stayed on the keyboard. `tools\pakpatch\crane_slots.py` adds seven slots (Move Crane Forward, Backward, Left, Right; Enter Crane Mode; Attach or Detach Cargo; Crane Anchor), their rows in the wheel settings screen, the menu controllers and English strings, drops the wheel exclusion from the legend's anchor row so the HUD shows that binding, then fixes the cache block's index (a text-relative offset and a size per source entry). It adds only what is missing, so it can be run again. First applied on 2026-09-13; the rows appeared and were bound the same night. The untouched pak is `extras\initial.pak.orig`, `crane_slots.py --restore` puts it back, and Steam's file verification does the same. A game update replaces the pak: run `crane_slots.py --apply` again afterwards (the anchors are text, so it survives layout changes unless the slot list itself changes).

In the game: Settings, Controls, steering wheel tab, crane section. Winch lift and lower and cargo rotation had wheel slots all along, named "Lift/Lower Crane Winch" and "Pull Crane Winch to the left/right". The joystick also fills the four D-pad slots (transmission up and down, winch point selection, menu navigation, see `KEYBINDS.md`); if that gets in the way in crane mode, move those to the rim's own hat (136 to 139).

The functions menu (V) is a different case. The stock data has an Engine slot that never reached the menu (no row in the wheel screen, no controller, no target input); the patch completes it, so "Engine" sits after Headlights in the game controls section and fires Truck.EngineSolo. The addon operations (outriggers, suspension mode, restore crane) are the game's custom addon actions and bind through the "Custom addon action" slots, and Remove cargo starts from the menu with Accept confirming. Recover, Refuel, Repair, Change truck and Delete trailers exist only as menu entries, with no game input to bind.

## Manual transmission mod

SnowRunner Manual Transmission (EvanTrow's fork of drafty46/SMT, v1.6) replaces the automatic gearbox with numbered gears, clutch and stalling. It is an `.asi` plugin and its release ships the Ultimate ASI Loader under the name `dinput8.dll`, which would replace the proxy. Installed here as `Sources\Bin\version.dll` (the same loader under a name the game also imports) plus `Sources\Bin\SMT.asi`; copies sit in `extras\SMT-1.6` with the original zip. Uninstall by deleting those two files from Bin; `SMT.ini` and `SMT_LOG.txt` appear there after the first run.

The mod reads controllers itself through DirectInput, so inside the game it goes through the proxy and sees the merged wheel, hidden devices excluded. It asks for exclusive access, which the game already holds, so the proxy hands clients created from the modules listed in `NonExclusiveModules` (default `SMT.asi`) a shared view instead; the log line for that client says "foreign client: exclusive downgraded to shared". Its bindings name inputs by vendor abbreviation, so the gates appear as `SASW.b.96` to `SASW.b.101` and the shifted layer as `SASW.b.112` to `SASW.b.117`.

The stock build crashes the game at boot next to ReShade: its overlay creates a throwaway D3D11 device and swap chain to find the Present hook, and the ReShade chain (Luma, RenoDX) initialises on that dummy. Without ReShade it runs. So the installed `SMT.asi` is a headless build made here from the mod's source (MIT): the overlay unit is replaced by a version that looks up the game window and skips the D3D hook; shifting logic, controller reading, ini and named pipe are unchanged. Files in `extras\SMT-1.6`: `SMT-headless.asi` (installed), `SMT-overlay.asi` (the stock overlay built with the same host guard, for menu sessions), `gui_headless.cpp`, `dllmain_headless.cpp`, `build_headless.bat` and `build_overlay.bat` (drop into the mod's `DLL` folder, build with `/p:PlatformToolset=v143`), the original zip and the PowerShell module.

To see the menu anyway (bindings with input capture, options, gear status), run `extras\SMT-1.6\menu-session-start.bat` with the game closed. It parks ReShade as `dxgi.dll.menu-session` and installs the overlay build as `SMT.asi`; in the game, Delete opens the menu and its Save button writes `SMT.ini`. `menu-session-end.bat` puts the headless build and ReShade back, and the saved ini carries over. The headless build never writes the ini itself, so a binding set to `FOUND` in it is captured for the session only.

The ReShade add-on build in `smt-addon` turns the same mod into `SMT.addon64`, which ReShade loads next to Luma and RenoDX. Its menu is the Manual Transmission window inside the ReShade overlay, opened with Home, and the gear display draws through ReShade, so it needs no throwaway device, no ASI loader and no menu session. Shifting logic, controller reading, ini and pipe are the same code as the headless build. Only `src\gui.cpp` and `src\dllmain.cpp` are rewritten, and `deps` holds ReShade 6.8.0's add-on headers with the Dear ImGui 1.92.5 header they require. `build.bat` writes `out\SMT.addon64`. The module pins itself once ReShade has registered it: ReShade unloads and reloads every add-on when the game drops a graphics device, which SnowRunner does during start-up, and the first build got unmapped under its own start-up thread that way (boot crash on 2026-09-14). Pinned, it stays mapped, ReShade marks it external and finds it again on the next pass, and `SMT_LOG.txt` says `addon: module pinned`. ReShade's log then carries one warning that the add-on was not unregistered, which is the expected trace of that.

Field-verified on 2026-09-14: the pinned build registered, stayed loaded through three add-on reloads during start-up, took shared wheel access from the proxy and ran a full session, and the user reports it boots and works. The pin was the only fix the first boot needed.

Switching takes one script each way, with the game closed. `switch-to-addon.bat` parks `version.dll` and `SMT.asi` in Bin as `.off` files and copies the add-on in, and `switch-to-asi.bat` removes the add-on and restores both. The add-on refuses to start while `SMT.asi` is loaded, and `NonExclusiveModules` in `dimerge.ini` lists both file names. The first boot of the add-on checks, in order:

1. `ReShade.log` shows `Loading add-on from` with `SMT.addon64`, then `Registered add-on "SnowRunner Manual Transmission"`.
2. `SMT_LOG.txt` shows `ReShade add-on build`, a nonzero `addon: game window`, `Processing input` and `addon: ready`.
3. `dimerge.log` names `smt.addon64` as the foreign module and downgrades its exclusive request.
4. Home opens the overlay with a Manual Transmission window: the vehicle line, the three tables and Save config.
5. With that window open, a wheel button and a key bind, and Save config writes `SMT.ini`.
6. With the overlay closed, the gates shift, and collar plus gates give R and gears 7 to 11.
7. SHOW MENU opens and closes the overlay. The current ini has it on Delete. ReShade's effect toggle moved from Delete to Scroll Lock on 2026-09-13, so the two no longer collide.
8. The game quits the way it always does.

With no menu, bindings live in `Sources\Bin\SMT.ini` (the defaults are kept in `SMT.ini.defaults`). The proxy log shows what its input library sees, and the names follow from that: buttons are `SASW.b.<wheel button>` and axes `SASW.a.<n>.p` where n counts the wheel's non-slider axes in order (X 0, Y 1, Rx 2, Ry 3, Rz 4). Prefilled: GEAR 1 to 6 = `SASW.b.96` to `.101`, GEAR R = `SASW.b.112` (collar plus gate 1, where the firmware puts reverse too), GEAR 7 to 11 = `SASW.b.113` to `.117` (collar plus gates 2 to 6), CLUTCH = `SASW.a.4.p`, options DISABLE GAME SHIFTING and REQUIRE GEAR HELD on (lever in neutral means neutral), REQUIRE CLUTCH off. Turn REQUIRE CLUTCH on once the clutch binding is confirmed. Status without the overlay: `Import-Module extras\SMT-1.6\SnowRunnerMT.psm1; Get-SMTStatus`.

The proxy echoes the application-data tag a client sets on a merged input back in that input's buffered events, which is how this library tells axes apart; without it the pedal axes would be invisible to the mod.

## GX100 tool

`tools\gx100\gx100cfg.py` speaks the shifter's own protocol, recovered from the vendor program (a packed Python app). It needs `pip install hidapi`.

- `gx100cfg.py status 10` prints the gate and button mask and the six raw sensor values while you move the lever.
- `gx100cfg.py apply --comb1 0 --comb2 0 --seq off --pull 0` sends the settings; values you leave out come from the vendor tool's `cfg.ini`, which the command updates afterwards so both tools agree. `--hadj` and `--sadj` are the two "Adj" numbers, `--seq auto|on|off` the sequential mode.
- `gx100cfg.py calib h start`, move through every gate, then `gx100cfg.py calib h end`; `calib s` does the same for the sequential axis.

Report layout: a 16-byte input report with the button mask in bytes 0 and 1 (bits 0 to 5 gates, 6 and 7 gears 7 and 8, 8 and 9 shift down and up, 10 reverse, 11 to 14 buttons), six raw values in bytes 3 to 14 and the pull collar in byte 15. Output reports start with a command byte: 1 for calibration (1 to 4 = H start, H end, sequential start, sequential end), 2 for settings.

## Exit crash

Every exit so far crashes inside the NVIDIA driver while the game destroys its D3D11 device. The three proxy dumps from 2026-09-13, after sessions of 2, 14 and 48 minutes, show the same stack: d3d11 hands the teardown to the driver, the driver calls `NvCamera64.dll` (NVIDIA's game filters and photo mode), and releasing a shader there faults in `nvwgf2umx.dll`. No dimerge, ReShade, Luma or SMT code is on that stack. The game's crash handler then writes its own record to `Documents\My Games\SnowRunner\base\ssl_crash_dump`, starts `crash_reporter.exe` (the "Crash Dump Sending Utility" window) and keeps the game process alive until that window closes. Until 2026-09-12 the reporter died at start, because the ASI loader injected SMT into it, so the window only began to appear once SMT got its host guard.

Since 2026-09-13 the reporter in Bin is renamed to `crash_reporter.exe.off`, so the game ends after the crash without the window. Rename it back to restore crash reports. Turning off NVIDIA's game filters and photo mode in the NVIDIA App keeps `NvCamera64.dll` out of the game, which should remove the crash itself. An exit without a new exception line in `dimerge.log` and without a new `dimerge_crash_*.dmp` would confirm that.

## Game settings file

The game keeps its settings, wheel bindings included, in `C:\Program Files (x86)\Steam\userdata\72265044\1465360\remote\user_settings.cfg` and reads it through Steam's cloud layer. Steam records each file's size and SHA1 in `remotecache.vdf` one folder up and trusts that record when the game reads the file. After a hand edit the record no longer matched, and the game wrote factory defaults on the next two launches. Change bindings in the game menu only.

To put a saved copy back, close the game and Steam, then run `tools\steam\restore_settings.py` with the copy's path. It backs up the current file and the record into `extras`, writes the copy and sets the record's size, SHA1 and time to match. On 2026-09-13 this brought back the 02:07 settings after the reset, and the game read them whole on the next launch.

The reset also dropped settings that never show up as bindings: the wheel id (`steeringWheelCurrentUid`, "LogitechPs4" here) and the three pedal inverts. Without them, camera rotation followed the clutch pedal again. Restoring the file with them fixed it. Which of the two carries the fix is not known.

## Uninstall

Delete `dinput8.dll`, `dimerge.ini` and `dimerge.log` from `Sources\Bin`.

## How it works

The game imports DirectInput8Create from dinput8.dll and Windows loads the copy in the game folder first. The proxy loads the real `System32\dinput8.dll`, wraps IDirectInput8 and, for the wheelbase only, IDirectInputDevice8. EnumDevices skips the merged devices. The wrapped device forwards every call to the wheelbase, effects included, and after each GetDeviceState or GetDeviceData it polls the merged devices (opened non-exclusive, background) and writes their values at the offsets of the game's data format. Property calls for merged objects, ranges and dead zones for instance, are translated to the device they belong to. Buffered input for merged devices is generated from state changes.
