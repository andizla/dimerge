# Changelog

## [2.0.0] unreleased

- Any DirectInput device can be merged: devices are named by `VID:PID`, `VID:PID#n` or instance GUID, axes are placed by rule or automatically, buttons and hats take fixed or free numbers.
- Shift layers: any number per device, with the modifier on the device itself, on another merged device or on the wheelbase.
- Knobs: a rotary encoder's two pulse buttons drive a merged axis, pulse or hold mode, read through the device's event buffer.
- `dimerge-setup.exe` opens with a choice between merging controllers and the wheel binding slots. The wizard learns the devices, shows the plan, writes the ini or starts over, shows the merged wheel live, installs into the game (chaining an existing `dinput8.dll`) and uninstalls. The game folder comes from Steam or from a pasted path.
- Wheel binding slots for `initial.pak` built into the setup tool: crane movement and mode, attach and anchor, engine, HUD toggle. The untouched pak is kept as `initial.pak.orig`, refreshed after a game update, and can be put back. The release zip carries no Python; `tools\pakpatch\wheel_slots.py` in the repository is the same patch as a script.
- `Chain=` loads another proxy (an ASI loader) underneath; the setup tool sets it up when a `dinput8.dll` is already in the game folder.
- `tools\keybinds\keybinds.py` (repository only) reads the merged numbers from `dimerge.log`, shift layers and knobs included, and names the modifier behind each layered button.

1.x was the private single-rig build this grew out of: the same proxy core, one shift layer, a Python-only pak patch.
