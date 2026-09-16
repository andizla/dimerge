# dimerge

SnowRunner reads one DirectInput steering wheel. Pedals, shifters, button boxes and joysticks on their own USB plugs can be bound in the menu but do nothing in the truck, or they flip the control scheme to the gamepad. dimerge is a `dinput8.dll` proxy that lives in the game's `Sources\Bin` folder and hands the game one device: the wheelbase, with the axes, buttons and hats of the other devices written into its data. The wheelbase itself is passed through untouched, so force feedback works as before.

Any DirectInput devices can be merged. Built and played with a SIMAGIC Alpha wheelbase, SIMSONN Plus X pedals, a PXN-CB1 button box and a GX100 H-shifter.

## Install

1. Unzip the release anywhere and plug in every controller.
2. Run `dimerge-setup.exe`. It lists the attached controllers, asks which one is the wheel (the force feedback device) and which ones to merge, then learns each device: leave the wheel alone and move its own controls so the free axes are known, then move every axis of each merged device through its travel; each pass ends on Enter. After a device's pass it asks for modifiers (a shifter's pull collar, a layer switch, held on that device, another merged device or the wheel; several per device), and at the end for rotary knobs that should drive an axis. It writes `dimerge.ini` next to itself and ends in a small menu: test view, install, pak slots, quit. Axes that read full scale at rest (some pedal sets) are marked `Invert` from that pass.
3. `dimerge-setup test` shows the merged wheel live for 15 seconds, so the pedals and buttons can be checked before the game sees them.
4. `dimerge-setup install` copies `dinput8.dll` and `dimerge.ini` into the game's `Sources\Bin`. It finds the install through Steam; give the folder as an argument otherwise. If that folder already has a `dinput8.dll` (the Ultimate ASI Loader or another mod), it is renamed to `dinput8_chain.dll` and dimerge loads it, so the other mod keeps working.
5. Start the game. Settings, Controls, steering wheel tab: the merged buttons and axes bind like the wheel's own. `dimerge.log` next to the game shows what was merged.

`dimerge-setup uninstall` removes the files and puts a chained `dinput8.dll` back. By hand: delete `dinput8.dll`, `dimerge.ini` and `dimerge.log` from `Sources\Bin`.

The setup tool writes fixed numbers: the merged devices take blocks at the top of the 128 button numbers (a shift layer gets a block of its own) and the wheel's own buttons keep the low ones. A device left unplugged one day does not move the bindings of the others; the log says it was not attached and the proxy looks for it every two seconds. Wheelbases tend to declare far more buttons than the rim has, which is why the low numbers are not taken as a guide.

## dimerge.ini

Every line the setup tool writes can be edited by hand. Axis names are `X, Y, Z, Rx, Ry, Rz, Slider0, Slider1`. Lines starting with `;` are comments.

```ini
[dimerge]
Enabled=1
LogLevel=1            ; 0 quiet, 1 normal, 2 every call (grows fast, short runs only)
HideMerged=1          ; hide the merged devices from the game's own enumeration
CrashProbe=1          ; log fatal exceptions with module and offset, minidump the first two

[Primary]
Device=0483:0522      ; the wheelbase
DeadAxes=Rx,Ry        ; wheelbase axes nothing is plugged into; merged axes may take them over

[Merge.Pedals]
Device=DDFD:6011
Axes=X->Slider0,Y->Y,Z->Rz
Invert=none
Buttons=none
POVs=none

[Merge.ButtonBox]
Device=36E6:8001
Axes=none
Buttons=64            ; the box's buttons 0.. become merged buttons 64..
POVs=buttons:90       ; its hat becomes buttons 90 to 93 (up, right, down, left)
```

`Device` takes three spellings: `VID:PID` (the first attached device with that product id), `VID:PID#2` (the second identical device) or the instance id in braces as `dimerge-setup list` prints it, which pins one physical unit.

`Axes` is `none`, `auto`, or a list. `X->Slider0` puts the source device's X on the merged Slider0. A source name alone (`X,Y`) is placed automatically. Automatic placement takes free slots in the order `Y, Z, Rz, Slider0, Slider1, Rx, Ry, X` (`AxisOrder=` under `[dimerge]` changes that), skipping wheelbase axes that exist unless `DeadAxes` names them.

`Buttons` is `none`, `auto` (the first free numbers) or the first merged number. `POVs` (or `Hats`) is `none`, `auto`, a hat slot number, `buttons:N` (four buttons from N: up, right, down, left; a diagonal presses two) or `buttons:auto`. SnowRunner binds buttons on a wheel but not extra hats, so a hat or mini joystick is best turned into buttons.

`Invert=Z` flips a source axis inside the range the game set, for pedals that read full scale at rest.

Keep pedals off `Rx` and `Ry`. The game's wheel presets carry hidden defaults per axis slot: `Rx` is camera zoom and minimap movement, `Ry` is minimap movement, the two sliders are camera rotation, and `Y`, `Z`, `Rz` are throttle, brake and clutch. A pedal on `Rx` moves the camera through a default that never shows in the binding menu. After moving an axis, bind it again in the menu: the game stores the slot number (buttons 0 to 127, axes 128 to 135 in slot order, hat directions 136 to 139).

### Shift layers

A held button can switch a device's buttons to a second set of numbers, for a six-gate shifter with a pull collar (6+6) or any box with a layer switch:

```ini
[Merge.Shifter]
Device=04B0:5750
Buttons=64
Layer=14->80,hide     ; with the shifter's own button 14 held, gate N lands on 80 + N; hide keeps 14 itself from the game
Layer=ButtonBox:3->96 ; with button 3 of the [Merge.ButtonBox] device held, gate N lands on 96 + N
Layer=Primary:5->112  ; with wheelbase button 5 held, on 112 + N
```

One `Layer=` line per modifier; the first held modifier in file order wins. The layer is decided when a button engages and kept until it releases, so letting go of the collar while in gear changes nothing. The 1.x spelling `Shift=14`, `ShiftButtons=112`, `ShiftHide=1` still reads as the first layer. The setup tool writes these lines from the modifiers held at its prompts.

### Knobs

A rotary knob that clicks as it turns reports a button per direction. `Knob=20,21->Rx` turns that pair (left, right) into the merged Rx axis: each click deflects the axis fully for 100 ms and it returns to the centre, which is what camera zoom and camera rotation expect from a stick (`pulse=150` changes the length). `Knob=22,23->Slider0,hold,step=5` keeps the position instead, moving 5 percent of the range per click, for a knob used as a virtual throttle. `auto` in place of the axis name takes the next free one. The knob's buttons are read through DirectInput's event buffer, so a click that falls between two of the game's polls still counts.

### Other proxies and the manual transmission mod

`Chain=` under `[dimerge]` names another `dinput8.dll` proxy to load instead of the system one; without it, a `dinput8_chain.dll` next to the proxy is loaded when present. That is how the setup tool keeps an ASI loader working.

Programs that read the wheel themselves through DirectInput inside the game (SnowRunner Manual Transmission, for one) see the merged wheel too. They ask for exclusive access the game already holds, so the proxy hands clients created from the modules listed in `NonExclusiveModules` a shared view instead. The default is `smt.asi,smt.addon64`; the log line for such a client says "foreign client: exclusive downgraded to shared".

## Wheel slots the game does not offer

A wheel the game does not know by vendor id gets the "Custom" preset, and the controls menu offers that preset a fixed list of slots. The stock list has no slot for moving the crane, entering crane mode, attaching cargo, the anchor, the engine or the HUD toggle, so those stay on the keyboard. `dimerge-setup pak` (also the wizard's `[p]` choice) adds them to the game's `initial.pak` in three sets, `crane`, `engine` and `hud`: it shows which sets the pak carries, patches with the game closed, keeps the untouched pak next to it as `initial.pak.orig` and can put that back. The new rows appear in the steering wheel tab: crane movement, mode, attach and anchor in the crane section, Engine after Headlights, HUD after Toggle Camera. A game update replaces the pak; run it again afterwards, it adds only what is missing. The crane and engine sets are in daily use; the HUD slot is new in 2.0.

`dimerge-setup pakfile <initial.pak> <out.pak> [sets]` patches a copy instead, and `dimerge-setup pakexport <initial.pak> <folder> [sets]` writes the patched `initial.cache_block` and `[strings]` files as loose files, the drop-in package for people who drag them into `initial.pak` with WinRAR. Build those from the vanilla Steam install, not from a modded pak. `tools\pakpatch\wheel_slots.py` is the same patch in Python, for anyone who prefers a script.

`tools\keybinds\keybinds.py` writes `KEYBINDS.md`: every slot the wheel can bind with its menu name, game input and context, what is bound right now, which merged number is which physical control (from `dimerge.log`), and every input link the game data mentions. `docs\KEYBINDS.md` is that file for the rig dimerge was built on.

## Checking a run

`dimerge.log` in `Sources\Bin` is rewritten on every launch. It lists the devices the game enumerated and which one became the primary, then one line per merged input ("ButtonBox button 3 -> game button slot 67"), a summary of anything that could not be placed and why, and the game's DirectInput calls: data format, cooperative level, acquire, the first state reads, effects created.

If a pedal shows up on the wrong axis in the game, swap the right-hand names in its `Axes` rule. If the game ignores buttons above some number, lower the start numbers; `tools\dienum.exe watch VID:PID 10` prints a device's live values for ten seconds, and run from a folder that holds `dinput8.dll` it shows the merged view.

Camera Rotation X, Camera Rotation Y, Camera Zoom and the minimap movement slots are axis slots. A button number bound there makes the game read some other axis in its place (the clutch pedal, in the case that took an evening to find), so leave them empty or give them a real axis such as a knob; the joystick belongs on the two "Turn camera" button slots. Unbinding a wheel slot in the menu does not always remove the entry from the settings file, so a slot that keeps acting after it shows as empty is best checked there.

Bindings live in the game's Steam Cloud settings file (`userdata\<id>\1465360\remote\user_settings.cfg`). Change them in the game menu only: Steam keeps a size and hash record of that file, and a hand edit makes the game write factory defaults on the next launch.

## Building

Visual Studio 2022 Community, x64, static CRT. `src\build.bat` writes `src\out\dinput8.dll`, `tools\setup\build.bat` and `tools\dienum\build.bat` the two tools, and `tools\package.ps1` builds everything and writes the release folder and zip.

## How it works

The game imports `DirectInput8Create` from `dinput8.dll` and Windows loads the copy in the game folder first. The proxy loads the real `System32\dinput8.dll` (or the chained proxy), wraps `IDirectInput8` and, for the wheelbase only, `IDirectInputDevice8`. `EnumDevices` skips the merged devices. The wrapped device forwards every call to the wheelbase, effects included, and after each `GetDeviceState` or `GetDeviceData` it polls the merged devices (opened non-exclusive, background) and writes their values at the offsets of the game's data format. Property calls for merged objects, ranges and dead zones for instance, are translated to the device they belong to. Buffered input for merged devices is generated from state changes.

MIT license, see `LICENSE`.
