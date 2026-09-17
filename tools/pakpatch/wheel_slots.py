#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""wheel_slots: adds binding slots for custom steering wheels to SnowRunner's initial.pak.

A wheel the game does not know by vendor id gets the "Custom" preset, and the controls menu offers that preset a
fixed list of slots: the inputs map of steering_wheel_input_mapper.sso inside initial.cache_block. The stock list
has no slot for moving the crane, entering crane mode, attaching cargo, the anchor, the engine or the HUD toggle,
so those stay on the keyboard. This tool inserts the slots of the chosen sets, the matching rows of the wheel
settings screen and their controllers, an English string for every new name, and fixes the cache block's index
(a text-relative offset and a size per source entry). Names that already exist are left alone, so a run on a
patched pak adds only what is missing, and a game update (which replaces the pak) is answered by running it again.

  wheel_slots.py --list                         the sets and their slots
  wheel_slots.py [--pak P] [--sets a,b]         dry run: validates and writes the patched files to a work folder
  wheel_slots.py --apply [--pak P] [--sets a,b] backs the pak up once (<pak>.orig) and rewrites it; game closed
  wheel_slots.py --restore [--pak P]            puts the backup back
  wheel_slots.py --export <pak> <folder> [--sets a,b]
                                                release build: patches the given pak in memory only and writes the
                                                files in pak layout to the folder (drop-in for WinRAR users); use
                                                the vanilla Steam install, not a modded copy

--pak defaults to the first SnowRunner install found through Steam. --sets defaults to every set.
"""
import os
import shutil
import struct
import sys
import time
import zipfile

BS = chr(92)
CACHE = "initial.cache_block"
CRLF = b"\r\n"
MAPPER = b"steering_wheel_input_mapper.sso"
SETTINGS = b"ui_settings_controller.sso"

# A slot: name, string key, English text (None = the key exists in the game already), target input link,
# direction (None = forward vector, a word = that vector direction, "ACTION" = plain action) and game context.
SETS = {
    "crane": {
        "about": "moving the crane, crane mode, attaching cargo and the anchor",
        "after": "CraneArrowLower",
        "slots": [
            ("CraneMoveForward", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_FORWARD", "Move Crane Forward", "Crane.moveXZplane", None, "CRANE"),
            ("CraneMoveBackward", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_BACKWARD", "Move Crane Backward", "Crane.moveXZplane", "BACKWARD", "CRANE"),
            ("CraneMoveLeft", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_LEFT", "Move Crane Left", "Crane.moveXZplane", "LEFT", "CRANE"),
            ("CraneMoveRight", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_RIGHT", "Move Crane Right", "Crane.moveXZplane", "RIGHT", "CRANE"),
            ("CraneTurnOn", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MODE", "Enter Crane Mode", "Crane.TurnOn", "ACTION", "GAME"),
            ("CraneAttachCargo", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_ATTACH", "Attach or Detach Cargo", "Crane.AttachCargo", "ACTION", "CRANE"),
            ("CraneAnchor", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_ANCHOR", "Crane Anchor", "Crane.EnableAnchor", "ACTION", "CRANE"),
        ],
        # crane legend rows whose hint is hidden for wheels in the stock data
        "unhide": ["UI_CRANE_ANCHOR"],
    },
    "engine": {
        "about": "the engine start/stop slot the stock data carries but never shows",
        "complete": "StartEngine",
    },
    "hud": {
        "about": "the HUD toggle (keyboard H)",
        "after": "ToggleGameCamera",
        "slots": [
            ("HudVisibility", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CUSTOM_ADDON_ACTION_HUD_VISIBILITY", None, "Exploration.ToggleHudVisibility", "ACTION", "GAME"),
        ],
    },
}
ALL_SLOTS = [s for st in SETS.values() for s in st.get("slots", [])] + [("StartEngine", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_ENGINE", None, "Truck.EngineSolo", "ACTION", "GAME")]


def lines(rows):
    return b"".join(r.encode("ascii") + CRLF for r in rows)


def slot_block(name, key, target, direction, ctx):
    if direction == "ACTION":
        rows = ["   %s   =   {" % name,
                '      uiName   =   "%s"' % key,
                "      targetInputsToRemap   =   [",
                "         {",
                '            inputLink   =   "%s"' % target,
                "         }",
                "      ]"]
    else:
        rows = ["   %s   =   {" % name,
                '      uiName   =   "%s"' % key,
                "      targetInputsToRemapInVector   =   [",
                "         {",
                "            inputLink   =   {",
                '               inputLink   =   "%s"' % target,
                '               __type   =   "InputLinkMr"',
                "            }"]
        if direction:
            rows.append('            direction   =   "%s"' % direction)
        rows += ["         }", "      ]"]
    rows += ["      gameContexts   =   [", '         "%s"' % ctx, "      ]",
             '      __type   =   "SteeringWheelInputDescription"', "   }"]
    return lines(rows)


def menu_row(name):
    return lines(["            %s   =   {" % name,
                  '               name   =   "SteeringWheelInput%s"' % name,
                  '               __type   =   "UiSettingScreenElementInteractive"',
                  "            }"])


def controller(name, key):
    return lines(["   SteeringWheelInput%s   =   {" % name,
                  '      header   =   "%s"' % key,
                  '      inputDescriptionLink   =   "%s"' % name,
                  '      __type   =   "UiOneSettingSteeringWheelInputShortcutController"',
                  "   }"])


def parse_index(d):
    """Header: name table at 0x4e, then a flag byte and N u64 offsets, a flag byte and N u32 sizes, a flag byte and
    N u32 zeros, then the text area the offsets point into."""
    u32 = lambda o: struct.unpack_from("<I", d, o)[0]
    u64 = lambda o: struct.unpack_from("<Q", d, o)[0]
    p = 0x4e
    names = []
    while True:
        n = u32(p)
        s = d[p + 4:p + 4 + n]
        if n == 0 or n > 300 or not s.startswith(b"<"):
            break
        names.append(s)
        p += 4 + n
    N = len(names)
    if d[p] != 1:
        raise SystemExit("unexpected byte after the name table")
    t1 = p + 1
    offs = [u64(t1 + 8 * i) for i in range(N)]
    t2 = t1 + 8 * N
    if d[t2] != 1:
        raise SystemExit("unexpected byte after the offset table")
    t2 += 1
    sizes = [u32(t2 + 4 * i) for i in range(N)]
    t3 = t2 + 4 * N
    if d[t3] != 1:
        raise SystemExit("unexpected byte after the size table")
    t3 += 1
    text0 = t3 + 4 * N
    if any(offs[i] + sizes[i] != offs[i + 1] for i in range(N - 1)) or offs[-1] + sizes[-1] != len(d) - text0:
        raise SystemExit("index does not describe the text area, format changed")
    return names, t1, t2, text0, offs, sizes


class Entry:
    """one text object of the cache block: its byte range and block lookups inside it"""

    def __init__(self, d, names, text0, offs, sizes, suffix):
        k = [i for i, n in enumerate(names) if n.endswith(suffix)]
        if len(k) != 1:
            raise SystemExit("entry %s not found once in the cache block" % suffix.decode())
        self.k = k[0]
        self.d = d
        self.start = text0 + offs[self.k]
        self.end = self.start + sizes[self.k]

    def block_end(self, head, indent):
        """position after the closing line of the block that starts with the given header line; None when absent"""
        a = self.d.find(CRLF + head + CRLF, self.start, self.end)
        if a < 0:
            return None
        if self.d.find(CRLF + head + CRLF, a + 1, self.end) >= 0:
            raise SystemExit("block header not unique: %r" % head[:60])
        close = CRLF + b" " * indent + b"}" + CRLF
        e = self.d.find(close, a, self.end)
        if e < 0:
            raise SystemExit("block never closes: %r" % head[:60])
        return e + len(close)

    def has(self, head):
        return self.d.find(CRLF + head + CRLF, self.start, self.end) >= 0


def slot_head(name):
    return ("   %s   =   {" % name).encode()


def row_head(name):
    return ("            %s   =   {" % name).encode() + CRLF + ('               name   =   "SteeringWheelInput%s"' % name).encode()


def ctrl_head(name):
    return ("   SteeringWheelInput%s   =   {" % name).encode()


def patch_cache(d, sets):
    names, t1, t2, text0, offs, sizes = parse_index(d)
    N = len(names)
    mapper = Entry(d, names, text0, offs, sizes, MAPPER)
    settings = Entry(d, names, text0, offs, sizes, SETTINGS)
    ops = []    # (position, bytes replaced, new bytes), positions in the original text
    for set_name in sets:
        st = SETS[set_name]
        if "slots" in st:
            missing = [s for s in st["slots"] if not mapper.has(slot_head(s[0]))]
            if missing:
                after = st["after"]
                pos = mapper.block_end(slot_head(after), 3)
                row = settings.block_end(row_head(after), 12)
                ctrl = settings.block_end(ctrl_head(after), 3)
                if pos is None or row is None or ctrl is None:
                    raise SystemExit("set %s: the stock slot %s it follows is missing" % (set_name, after))
                ops.append((pos, 0, b"".join(slot_block(n, key, tgt, dr, ctx) for n, key, _, tgt, dr, ctx in missing)))
                ops.append((row, 0, b"".join(menu_row(n) for n, *_ in missing)))
                ops.append((ctrl, 0, b"".join(controller(n, key) for n, key, *_ in missing)))
            for name, *_ in st["slots"]:
                # a slot may be present without its screen row and controller (a half-applied run); add those alone
                if mapper.has(slot_head(name)) and not settings.has(ctrl_head(name)) and name not in [m[0] for m in missing]:
                    raise SystemExit("slot %s exists but its menu entries do not; restore the backup and run again" % name)
        if st.get("complete") == "StartEngine":
            # the stock StartEngine slot has no target, no row in the wheel settings screen and no controller
            old_engine = lines(["   StartEngine   =   {", '      uiName   =   "UI_SETTINGS_CONTROL_LAYOUT_ACTION_ENGINE"',
                                "      gameContexts   =   [", '         "GAME"', "      ]",
                                '      __type   =   "SteeringWheelInputDescription"', "   }"])
            new_engine = lines(["   StartEngine   =   {", '      uiName   =   "UI_SETTINGS_CONTROL_LAYOUT_ACTION_ENGINE"',
                                "      targetInputsToRemap   =   [", "         {", '            inputLink   =   "Truck.EngineSolo"', "         }", "      ]",
                                "      gameContexts   =   [", '         "GAME"', "      ]",
                                '      __type   =   "SteeringWheelInputDescription"', "   }"])
            a = d.find(old_engine, mapper.start, mapper.end)
            if a >= 0:
                ops.append((a, len(old_engine), new_engine))
            if not settings.has(ctrl_head("StartEngine")):
                row = settings.block_end(row_head("Headlights"), 12)
                ctrl = settings.block_end(ctrl_head("Handbrake"), 3)
                if row is None or ctrl is None:
                    raise SystemExit("engine: the Headlights row or the Handbrake controller is missing")
                ops.append((row, 0, menu_row("StartEngine")))
                ops.append((ctrl, 0, controller("StartEngine", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_ENGINE")))
        for key in st.get("unhide", []):
            start = 0
            while True:
                i = d.find(('text   =   "%s"' % key).encode(), start)
                if i < 0:
                    break
                stop = d.find(b'__type   =   "NavLegendHintController"', i)
                j = d.find(b"excludedDevices   =   [", i, stop)
                if j >= 0:
                    line = d.find(b'"steering_wheel",' + CRLF, j, stop)
                    if line >= 0:
                        ls = d.rfind(CRLF, j, line) + 2
                        ops.append((ls, line + len(b'"steering_wheel",' + CRLF) - ls, b""))
                start = i + 1
    if not ops:
        return None
    offs, sizes = list(offs), list(sizes)
    for pos, old, new in ops:
        delta = len(new) - old
        k = max(i for i in range(N) if offs[i] <= pos - text0)
        sizes[k] += delta
        for i in range(k + 1, N):
            offs[i] += delta
        print("  %s %d bytes in [%d] %s" % ("insert" if old == 0 else "replace", len(new) if old == 0 else old, k, names[k].decode("latin1")))
    out = bytearray(d)
    for pos, old, new in sorted(ops, reverse=True):
        out[pos:pos + old] = new
    for i in range(N):
        struct.pack_into("<Q", out, t1 + 8 * i, offs[i])
        struct.pack_into("<I", out, t2 + 4 * i, sizes[i])
    parse_index(bytes(out))
    return bytes(out)


def patch_strings(d, sets):
    slots = [s for n in sets for s in SETS[n].get("slots", [])]
    add = "".join('\r\n%s\t\t\t\t"%s"' % (key, t) for _, key, t, *_ in slots if t and (key + "\t").encode("utf-16-le") not in d)
    return d + add.encode("utf-16-le") if add else None


def rebuild_pak(src, dst, replaced):
    with zipfile.ZipFile(src) as zin, zipfile.ZipFile(dst, "w", allowZip64=True) as zout:
        for info in zin.infolist():
            name = info.orig_filename
            data = replaced.get(name)
            if data is None:
                data = zin.read(info)
            zi = zipfile.ZipInfo(name, date_time=info.date_time)
            zi.filename = name          # keep the stored separators, ZipInfo() would turn them into slashes
            zi.compress_type = info.compress_type
            zi.external_attr = info.external_attr
            zi.create_system = info.create_system
            zout.writestr(zi, data)
    with zipfile.ZipFile(src) as a, zipfile.ZipFile(dst) as b:
        na = [i.orig_filename for i in a.infolist()]
        nb = [i.orig_filename for i in b.infolist()]
        if na != nb:
            raise SystemExit("entry names changed during the rebuild")
        for ia, ib in zip(a.infolist(), b.infolist()):
            if ia.orig_filename not in replaced and ia.CRC != ib.CRC:
                raise SystemExit("content changed for " + ia.orig_filename)


def load(pak):
    """the cache block and every language's strings file, keyed by the stored entry name"""
    with zipfile.ZipFile(pak) as z:
        cache = z.read(CACHE)
        strings = {i.orig_filename: z.read(i) for i in z.infolist()
                   if i.orig_filename.startswith("[strings]") and i.orig_filename.endswith(".str")}
    return cache, strings


def patched(cache, strings, sets):
    """patched cache block (None if nothing was missing) and the strings files that gained keys"""
    new_cache = patch_cache(cache, sets)
    new_strings = {name: patch_strings(data, sets) for name, data in strings.items()}
    return new_cache, {name: data for name, data in new_strings.items() if data is not None}


def write_files(folder, cache, strings):
    """pak layout on disk: the cache block at the root, the strings files in a [strings] folder"""
    os.makedirs(os.path.join(folder, "[strings]"), exist_ok=True)
    if cache is not None:
        open(os.path.join(folder, CACHE), "wb").write(cache)
    for name, data in strings.items():
        open(os.path.join(folder, "[strings]", name.split(BS)[-1]), "wb").write(data)


def steam_paks():
    """initial.pak of every SnowRunner install Steam knows, the default library first"""
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as k:
            steam = winreg.QueryValueEx(k, "SteamPath")[0]
    except OSError:
        return []
    # the registry spells the Steam folder "c:/program files (x86)/steam" and libraryfolders.vdf lists the same
    # library as "C:\Program Files (x86)\Steam": one spelling, and paths compared without case, or every
    # install shows up twice
    libs, seen = [], set()
    for lib in [steam.replace("/", BS)] + vdf_paths(steam):
        key = os.path.normcase(os.path.normpath(lib))
        if key not in seen:
            seen.add(key)
            libs.append(lib)
    out, seen = [], set()
    for lib in libs:
        common = os.path.join(lib, "steamapps", "common")
        if not os.path.isdir(common):
            continue
        for name in sorted(os.listdir(common), key=str.lower):
            pak = os.path.join(common, name, "preload", "paks", "client", "initial.pak")
            key = os.path.normcase(os.path.normpath(pak))
            if name.lower().startswith("snowrunner") and os.path.exists(pak) and key not in seen:
                seen.add(key)
                out.append(pak)
    return out


def vdf_paths(steam):
    """the library folders listed in steamapps/libraryfolders.vdf (backslashes escaped in the file)"""
    out = []
    try:
        for line in open(os.path.join(steam, "steamapps", "libraryfolders.vdf"), encoding="utf-8", errors="replace"):
            if '"path"' in line:
                out.append(line.split('"')[3].replace(BS + BS, BS))
    except OSError:
        pass
    return out


def main():
    args = sys.argv[1:]
    sets = list(SETS)
    pak = None
    export = None
    mode = "--dry-run"
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--sets":
            sets = [s.strip() for s in args[i + 1].split(",") if s.strip()]
            i += 1
        elif a == "--pak":
            pak = args[i + 1]
            i += 1
        elif a == "--export":
            mode = a
            export = (args[i + 1], args[i + 2])
            i += 2
        elif a in ("--apply", "--restore", "--dry-run", "--list"):
            mode = a
        else:
            raise SystemExit("unknown argument " + a + "\n" + __doc__)
        i += 1
    unknown = [s for s in sets if s not in SETS]
    if unknown:
        raise SystemExit("unknown set(s) %s; known: %s" % (", ".join(unknown), ", ".join(SETS)))
    if mode == "--list":
        for name, st in SETS.items():
            print("%-8s %s" % (name, st["about"]))
            for n, key, text, tgt, dr, ctx in st.get("slots", []):
                print("         %-20s %-40s %s%s [%s]" % (n, text or "(" + key + ")", tgt, "" if dr in (None, "ACTION") else " " + dr.lower(), ctx))
            if "complete" in st:
                print("         %-20s %-40s %s [%s]" % ("StartEngine", "Engine", "Truck.EngineSolo", "GAME"))
        return
    if mode == "--export":
        # release build: patch the given pak (the vanilla one) in memory and write the files in pak layout, nothing else
        src, out = export
        cache, strings = load(src)
        new_cache, new_strings = patched(cache, strings, sets)
        write_files(out, new_cache if new_cache is not None else cache, {n: new_strings.get(n, strings[n]) for n in strings})
        print("release files from %s written to %s (%d strings files, cache %s)" % (src, out, len(strings), "patched" if new_cache is not None else "already patched"))
        return
    if pak is None:
        found = steam_paks()
        if not found:
            raise SystemExit("no SnowRunner install found through Steam; give --pak <path to initial.pak>")
        pak = found[0]
        if len(found) > 1:
            print("several installs found, using the first; --pak picks another:\n  " + "\n  ".join(found))
    backup = pak + ".orig"
    if mode == "--restore":
        if not os.path.exists(backup):
            raise SystemExit("no backup at " + backup)
        shutil.copy2(backup, pak)
        print("restored", pak, "from", backup)
        return
    work = os.path.join(os.environ.get("TEMP", "."), "dimerge-pakpatch")
    os.makedirs(work, exist_ok=True)
    cache, strings = load(pak)
    print("%s: cache block %d bytes, %d strings files, sets %s" % (pak, len(cache), len(strings), ",".join(sets)))
    new_cache, new_strings = patched(cache, strings, sets)
    if new_cache is None and not new_strings:
        print("nothing to do, the pak already carries everything")
        return
    write_files(work, new_cache, new_strings)
    replaced = dict(new_strings)
    if new_cache is not None:
        replaced[CACHE] = new_cache
        print("cache block +%d bytes" % (len(new_cache) - len(cache)))
    if new_strings:
        print("strings patched: %s" % ", ".join(sorted(n.split(BS)[-1] for n in new_strings)))
    print("patched files in", work)
    if mode != "--apply":
        print("dry run only; use --apply to rewrite the pak")
        return
    try:
        with open(pak, "ab"):
            pass
    except OSError:
        raise SystemExit("the pak is locked, close the game first")
    if not os.path.exists(backup):
        shutil.copy2(pak, backup)
        print("backup written:", backup)
    tmp = pak + ".dimerge-new"
    t = time.time()
    rebuild_pak(pak, tmp, replaced)
    os.replace(tmp, pak)
    print("pak rewritten in %.1f s: %s" % (time.time() - t, pak))


if __name__ == "__main__":
    main()
