# dimerge 2.0 handoff

Written 2026-09-16 01:50, repo section added 02:10, keybinds block 23:40, start screen block 2026-09-17 11:40, start over block 2026-09-18 00:05. Durable notes also live in the auto-memory file `snowrunner-dimerge.md` (see `MEMORY.md` in the Claude memory folder); this file is the project-local view. Newest session block first.

## 2026-09-18 00:05: wizard start over (the user's pick instead of a back key), field note

- The user asked how hard a back/previous key would be. Assessment given: back between screens is easy, redoing one device inside the learn passes is moderate (the wizard hands out free wheel axes as it goes, and the modifier and knob prompts watch raw keys). Their call: "adding it at the end as redo or something is fine". Built: after the plan lines and before anything is written, "Press Enter to write dimerge.ini, or type r to start over"; the end menu gained "[r] start over" (the live view is where a wrong pedal shows up). Both return `kStartOver` from `Wizard()`, and `StartScreen()` runs the wizard again from "Plug in and switch on every controller", which re-enumerates, so a controller switched on late is picked up. No per-step back, no b key.
- Build 507392 bytes (23:58), /W4 clean, copied to `test\`, zip rebuilt (537381 bytes). Rerun offline: the start screen and the pak subcommand. NOT run: the two start-over paths themselves, which sit behind the learn passes and need the rig at the keyboard; the user's next wizard run is their first test.
- Field note from the user (2026-09-17 evening): the 11:28 exe "seems to work" on their side; the pak in the game was under test when this block was written, so open item 1 stays open until they report the new rows and the game loading the C++-written pak.
- Noticed, not changed: `Install` overwrites `dimerge.ini.bak` on every install, so a second install (more likely now that start over exists) replaces the backup of a hand-made ini with the previous wizard ini. Keeping the first backup is a two-line change; the user's call.
- README step 2 and the CHANGELOG setup bullet mention the start over.

## 2026-09-17 11:40: one installer with a start screen, wording approved in chat, zip without Python

- The user reviewed the wording of every new screen in chat before the build and approved it. The mod.io upload is the same zip as the GitHub release (an exe inside a zip is fine there), so the loose-file drop-in is no longer the mod.io product.
- `dimerge-setup.exe` (507392 bytes, copied to `test\`) opens on a start screen: 1 = the controller wizard, 2 = the wheel binding slots; no default ("Type 1 or 2 and press Enter" repeats on Enter alone). A double-clicked run (the program owns its console: `GetConsoleProcessList` == 1) ends every path with "Press Enter to close."; from a shell nothing is added. Wizard changes: section header "Merge controllers", the controllers are read only after "Plug in and switch on every controller, then press Enter" (a late plug-in used to be missed), "at least two controllers", the end menu names [p] "add crane, engine and HUD rows to the wheel bindings", install says "Unzip the whole download into a folder and run dimerge-setup.exe from there" when dinput8.dll is not beside the exe and "Run option 1 first, it writes that file" when the ini is missing.
- Game folder pick shared by install and the pak menu (`PickGameFolder` in dimerge-setup.cpp): the Steam list as before, plus a pasted path accepted at the same prompt (quotes and forward slashes tidied, a trailing Sources\Bin, preload\paks\client or initial.pak stripped, accepted when SnowRunner.exe or initial.pak is under it; the Epic version's route); with no Steam install only the paste prompt. q quits at every prompt, and end of input quits (AskLine returns q) instead of taking a default. The install picker therefore lists game folders now, not Bin folders.
- Pak menu rewritten to the approved text: "This pak has:" table, [a]/[c]/[e]/[h]/[r]/[q] with Enter = a, or "[r] puts the untouched pak back, [q] quits" when every set is present. Apply receives only the sets the pak lacks, prints where the new rows appear (a `rows` text per set in `Sets()`), and REFRESHES `initial.pak.orig` whenever the pak carries no set at all (a stock pak after a game update): the old rule kept the first backup forever, so [r] after an update would have installed the previous version's pak. Lock messages by cause: sharing violation = "The pak is in use. Close SnowRunner and run this again.", anything else = "The pak is not writable ... run this program as administrator". [r] without a backup points at Steam's verify.
- Offline runs on a scratchpad copy of the vanilla pak, never a real install: start screen (Enter alone and x repeat, end of input exits), the pak subcommand, a bad pasted path, add all (1.0 s, 30881146 bytes, orig == vanilla), rerun with everything present, stale-orig refresh after a simulated update (orig == vanilla again), crane then engine on a partly patched pak (backup kept, one row line each), restore (pak == vanilla), hud with no backup at all ("Copy of the pak saved ... already carried some sets"), restore without a backup, the pak held open by another program, the pak read-only. Not run: option 1 itself (the learn passes need the rig at the keyboard).
- Package: `tools\package.ps1` copies no Python any more (zip = dimerge-setup.exe, dinput8.dll, tools\dienum.exe, README, LICENSE; 537181 bytes). README Install rewritten for the start screen with the SmartScreen line ("Windows protected your PC": More info, Run anyway), choice 2 named in the wheel slots section, the Python scripts "in the repository"; CHANGELOG bullets updated. `dimerge-setup.cpp` includes `src\dimerge.h` for the version in the title.
- MISTAKE, repaired: a probe of the OLD exe (`printf 'q\n' | dimerge-setup.exe pak`) hit the old pick prompt, where q was no answer and fell through to install 1, and the end of input then took the old default "add every missing set": the vanilla install's initial.pak (steamapps\common\Snowrunner) was patched at 11:20. Restored at 11:25 from the initial.pak.orig the tool had written (cmp byte-identical, Sep 8 timestamp kept), the backup removed again at 11:33; the play copy (SnowRunner5) was never touched. The new menu cannot do this, but the rule stands: no piped answers against a real install's menu, ever; the scratchpad copy is for that.
- Unchanged: the play install, its pak and settings file, dinput8.dll, dienum, the Python tools in the repository.

## 2026-09-16 23:40: keybinds for 2.0, SMT numbers confirmed, pak pre-check, install detection fix

- `tools\keybinds\keybinds.py` now reads the 2.0 log in full: the `+mod2` and higher layer suffixes (the old pattern knew only `+mod`, so the PXN's second layer, 96 to 121, was missing), knob placement lines, and the layer header lines, so a layered button reads "PXNCB1 button 3 with its button 7 held" (or "with GX100 button 14 held", "with wheelbase button 5 held") instead of "with the modifier held". Runs of layered buttons fold into one table row like plain ones. The pak patch note names `dimerge-setup pak` next to the Python script.
- `docs\KEYBINDS.md` written from the play install (the 01:11 log, the pak, the settings file): 109 merged numbers, 93 slots, 34 bound. Two findings from the bound column: the game accepted a second-layer number (GarageGlobalMap = 121, PXN button 25 with button 6 held), so the `+mod2` layer binds; three numbers carry two slots in different contexts (58 = CraneArrowLower in CRANE and CraneTurnOn in GAME, 52 = AWD and CraneAttachCargo, 53 = DiffLock and CraneAnchor). Whether those doubles are meant is the user's call; the game keeps them apart by context.
- Open item 2 is closed: the user rewrote `SMT.ini` at 01:12 with the wizard numbers (GX100 gates and collar layer on 9 to 13 and 24 to 33, AWD 40, DIFF LOCK 41, CLUTCH still `SASW.a.4.p`). The clutch index is still right: the 01:11 log shows the add-on tagging the same wheelbase axis instances as under 1.x (X, Y, Rx, Ry, Rz) plus the injected Z, which the proxy enumerates after the wheelbase's own objects, so OIS axis 4 stays Rz.
- The installed `dinput8.dll` (00:06) and `src\out` (01:49) come from the same sources; nothing in the tree is newer than the installed build. README read with the no-ai-slop rules: nothing to cut.
- Pak pre-check on the play copy's own pak (`pakfile` into the scratchpad, nothing installed): the C++ tool and `wheel_slots.py` make the same three inserts (the hud set; crane and engine are already there), the patched cache block is byte-identical between the two, all 12253 other entries are untouched, Python's zip test passes; the pak grows by 187 KB because the cache block is recompressed with fixed Huffman (1.21 MB against the game's 1.03 MB). The menu itself (`dimerge-setup pak`, driven with a pick and `q`) reads the real pak and reports crane present, engine present, hud missing. What is left for the game to prove is only that it reads the C++-written stream.
- Detection bug found by that run and fixed in both tools: every install was listed twice, once through the registry's `c:/program files (x86)/steam` spelling and once through the canonical path in `libraryfolders.vdf`, because the two were compared as plain strings; `wheel_slots.py` also matched the folder name case-sensitively, so the vanilla install, spelled `Snowrunner` on disk, was missing from its list, and it sorted case-sensitively. Now one spelling, paths compared without case, folder names matched and sorted without case. `dimerge-setup.exe` rebuilt (497664 bytes) and copied to `test\`; the install picker uses the same scan. On this PC the pak menu lists the vanilla install as 1 and the play copy as 2, so the play copy needs an explicit 2 (Enter takes 1).
- Untouched: the play install, the settings file, the pak.

## 2026-09-16 02:10: repository created

`andizla/dimerge` on GitHub, private, branch `main`, first commit b18237f with 24 tracked files (sources, tools, docs; binaries, `release\` and the `test\` scratch except its ini are ignored). `tools\release.ps1` and `CHANGELOG.md` added; `README-1.x.md` and `KEYBINDS-1.x.md` moved to `docs\`. Nothing released yet: the CHANGELOG header still says unreleased on purpose, and the release script refuses until it is dated.

## What this folder is

`C:\Games\dimerge2` is the 2.0 tree of dimerge, the SnowRunner `dinput8.dll` proxy that merges several DirectInput devices into the force-feedback wheel. It was started on 2026-09-15 as its own folder so the 1.x project (`C:\Games\SnowRunner-dimerge`, still complete and untouched) stays available if 2.0 is dropped. The user (handle andizla) plans a public release from the private repo `andizla/dimerge` (see Repo and workflow); making it public is the user's call.

```
src\                 the proxy: dimerge.h, dimerge_config.cpp, dimerge_main.cpp, dimerge_device.cpp, dinput8.def, build.bat -> src\out\dinput8.dll
tools\setup\         dimerge-setup.cpp + pak_slots.cpp, build.bat -> dimerge-setup.exe (wizard, list, test, install, uninstall, pak, pakfile, pakexport)
tools\dienum\        dienum.exe: raw device listing and live values (run beside a dinput8.dll for the merged view)
tools\pakpatch\      wheel_slots.py: the Python form of the pak patch (same sets, same bytes); crane_slots.py was removed from this tree
tools\keybinds\      keybinds.py: writes KEYBINDS.md from the pak, dimerge.log and the Steam settings file
tools\package.ps1    builds everything and writes release\dimerge-2.0\ + release\dimerge-2.0.zip (-SkipBuild reuses the binaries)
tools\release.ps1    tags the pushed commit and publishes a GitHub release with the zip (see Repo and workflow)
test\                scratch: dinput8.dll + dienum.exe + dimerge-setup.exe + a dimerge.ini with three layer kinds and two knobs, for offline proxy checks (only the ini is tracked)
README.md, LICENSE, CHANGELOG.md   the release docs (MIT)
docs\                KEYBINDS.md (generated for this rig, 2.0 numbers), README-1.x.md and KEYBINDS-1.x.md (reference copies from 1.x)
release\             the current package (not tracked; the zip ships through GitHub Releases)
```

## Repo and workflow

GitHub: `https://github.com/andizla/dimerge`, branch `main`, created 2026-09-16 as a private repo; `gh repo edit andizla/dimerge --visibility public` makes it public when the user says so. Modelled on TXRWM but with one repo, because the source is the product and there is no live game tree to stage.

The loop, as in TXRWM: edit, build (`tools\package.ps1`, or the single `build.bat` that matters), prepend a dated block to this file, then from `C:\Games\dimerge2`: `git add -A`, `git commit -m "<one line, house style, no Co-Authored-By trailer>"`, `git push`. Every piece of prose and every comment goes through the no-ai-slop rules (no em dash, no spaced hyphen as a separator).

Release: date the `## [x.y.z]` header in `CHANGELOG.md` (its text becomes the release notes), commit and push, then `tools\release.ps1 -Version 2.0.0 -Message "..."` (`-DryRun` first). It checks the header version against `DIMERGE_VERSION`, builds, copies the zip to `release\dimerge-<version>.zip`, tags `v<version>` and runs `gh release create`. gh needs keyring access, so under the Claude Code sandbox run it with the sandbox off. Build outputs, exes, dlls and `release\` are ignored by git.

Builds need Visual Studio 2022 Community (x64, static CRT). Run the `.bat` files through PowerShell (`& "C:\Games\dimerge2\src\build.bat"`); `cmd //c` from the Bash tool fails. `package.ps1` clears the release folder's contents rather than the folder, because a shell parked inside it blocks deletion.

## Proxy (2.0) in one screen

- `[Primary] Device=` and `[Merge.X] Device=` take `VID:PID`, `VID:PID#n` or `{instance GUID}`.
- `Axes=none|auto|X->Slider0,Y,Z->Rz` (a source alone is placed automatically in `AxisOrder`, default `Y,Z,Rz,Slider0,Slider1,Rx,Ry,X`, skipping live wheelbase axes unless `DeadAxes` names them). `Invert=` flips sources. `Buttons=none|auto|N`. `POVs=none|auto|N|buttons:N|buttons:auto`.
- Shift layers: `Layer=<mod>-><start>[,hide]`, several per device, first held wins; `<mod>` = `N` (own button), `Section:N` (another merge section), `Primary:N` (wheelbase button, read from the game's own state buffer). `Shift=/ShiftButtons=/ShiftHide=` still parse as the first layer.
- Knobs: `Knob=L,R-><axis|auto>[,hold][,step=N][,pulse=ms]`. Pulse (default) deflects fully for 100 ms per click then returns to centre; hold counts clicks. Clicks come from a 64-entry DirectInput event buffer on the merged device, so detents between game polls are not lost. A knob axis has no device object behind it (`Injected.knob`), range questions are answered from what the game set.
- `Chain=` or an automatic `dinput8_chain.dll` loads another proxy (ASI loader). `NonExclusiveModules` (default `smt.asi,smt.addon64`) get shared instead of exclusive access. `CrashProbe` logs fatal exceptions and dumps the first two.
- The log prints one placement line per merged input, `+mod`/`+modN` for layers, `knob L/R` for knobs, and a summary with anything that could not be placed.

Verified: the 2.0 dll ran the game with the 1.x ini unchanged (63 inputs, SMT add-on downgraded to shared, clean exit); the three layer kinds plus two knobs map offline (`test\`, 95 inputs, nothing left out). Not yet seen in the game: a cross-device or wheelbase modifier, a knob.

## Wizard (dimerge-setup.exe)

Since 2026-09-17 a double-click opens a start screen first: 1 is the wizard below, 2 is the pak menu (Pak slots); no default, and every exit of a double-clicked run ends with "Press Enter to close.". The wizard flow: "Plug in and switch on every controller, then press Enter", then list devices (ID column, Enter takes the bracketed default), pick the wheel and the merges, learn the wheel's own controls (free axes become `DeadAxes`), then per device: move the axes (passes end on Enter, 120 s ceiling), then "Modifier for its buttons?" in a loop watching every device (own-device modifiers get `,hide`), then after all devices the knob prompts (turn left, turn right; next free axis). Numbers are fixed, allocated from the top of 0..127 downwards (the wheelbase declares 116 buttons but the rim has none real, so declared counts are ignored), pedals go to the free wheel axes in the proxy's order (Y, Z, Rz on this rig, which matches the game's PS-family preset defaults). Ends in a menu: `[t]` live view through the proxy dll beside the exe, `[i]` install (Steam library detection, a foreign `dinput8.dll` becomes `dinput8_chain.dll`, an existing ini is kept as `dimerge.ini.bak`, manifest `dimerge-install.txt`), `[p]` pak slots, `[r]` start over (also offered once before the ini is written), `[q]`.

The wizard loads the system `dinput8.dll` by hand (LoadLibrary), so a rerun beside its own ini still sees every device; `test` switches to the proxy dll. Lessons already fixed: DirectInput returns centred defaults (32767) until the first report, so rest positions are read after a 400 ms settle; the modifier prompt watches the whole time it is open.

Field status: three wizard runs by the user; the third produced a correct ini (DeadAxes, pedals, blocks) and the live view confirmed pedals and box; the collar layer, cross-device modifiers and knobs were added after that run and are untested by hand.

## Pak slots

Sets `crane` (7 slots, in daily use), `engine` (completes the stock StartEngine slot, in daily use), `hud` (HudVisibility -> Exploration.ToggleHudVisibility, reuses the game's own string key; new, untested in the game). `pak_slots.cpp` is a full port of `wheel_slots.py` with its own zip reader/writer, inflate and a fixed-Huffman LZ77 deflate; on the vanilla pak it produces a cache block and 13 strings files byte-identical to the Python tool, copies every other entry byte for byte, and Python's zipfile passes it (1.8 s, pak 29.07 -> 30.88 MB because fixed Huffman compresses less than zlib). Untested: the game loading a pak written by it (a single-block fixed-Huffman stream is standard deflate; `[r]` restores `initial.pak.orig` if it fails).

The mod.io drop-in for 1.x was exported from the vanilla install (`steamapps\common\SnowRunner`); `pakexport` does the same without Python. Never export from the play copy: its pak carries the Real Life mod.

## The user's play install right now

- `steamapps\common\SnowRunner5\Sources\Bin`: `dinput8.dll` = 2.0 proxy; `dimerge.ini` = a wizard ini (GX100 buttons from 8 with collar layer at 24, PXN buttons from 40, joystick as buttons 66..69, two PXN-button layers at 70 and 96, pedals `X->Y,Y->Z,Z->Rz`, `DeadAxes=Y,Rx,Ry,Rz,Slider0,Slider1`); the 1.x ini is `dimerge.ini.bak` there. Everything was rebound in the game menu to the new numbers. SMT runs as the pinned ReShade add-on `SMT.addon64`; the user rewrote its `SMT.ini` with the wizard numbers on 2026-09-16 01:12.
- Steam settings file `userdata\72265044\1465360\remote\user_settings.cfg`: on 2026-09-16 01:09 the three axis slots Camera Rotation X/Y and Camera Zoom were cut out with `C:\Games\SnowRunner-dimerge\tools\steam\restore_settings.py` (Steam closed; backups in `C:\Games\SnowRunner-dimerge\extras`, stamp 20260916-010905). That fixed the camera following the clutch, confirmed by the user. Rule: never bind a button number to an axis slot (camera rotation, zoom, minimap movement); the menu's unbind does not always remove the entry, the restore script route does; never hand-edit the cfg without it (Steam's `remotecache.vdf` record must match or the game resets all settings). Launching through Steam after a restore shows "Unable to Sync" once; Play anyway, or launch outside Steam as the user normally does.

## Open items

1. Field test in the game, on the 2026-09-17 exe: a cross-device modifier (a PXN button layering the GX100), a knob on a camera axis, start screen 2 adding `hud` to the play copy (type 2 at the folder pick on this PC, Enter takes 1 = the vanilla copy), and the game accepting the C++-written pak (the file itself is verified against the Python patch, see the 23:40 block). Seen so far in the settings file: a second own-device layer number binds (GarageGlobalMap = 121).
2. Release: make the repo public when the user says so, date the CHANGELOG header and run `tools\release.ps1` for v2.0.0, rebuild the mod.io drop-in with `pakexport` from the vanilla pak once `hud` is confirmed in the game.
3. Nice to have: dynamic Huffman in the deflate (smaller pak).
4. After the next rebinding: `python tools\keybinds\keybinds.py --bin "<SnowRunner5>\Sources\Bin" --out docs\KEYBINDS.md`.

## Tooling notes for the next session

- The GateGuard hook denies the first Bash per idle period, the first Write/Edit per file and anything matching its destructive regex (`rm -rf` and the like) once; the identical retry passes. State the facts it asks for and retry.
- Never drive `dimerge-setup.exe pak` (or start screen 2) with piped answers against a real install. For tests, copy the vanilla pak to `<scratch>\game\preload\paks\client\initial.pak` and paste `<scratch>\game` at the folder pick; the 2026-09-17 block says why.
- Python heredocs in the Bash tool work when the script contains no unmatched quotes; the Write tool into the scratchpad is the fallback. Python needs `C:/` paths, not `/c/`.
- The Edit tool needs a Read of the file in the same session first.
- House style for everything outward-facing and for comments: no em dash, no spaced hyphen as a separator, the no-ai-slop skill loaded before writing docs.
