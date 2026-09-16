#!/usr/bin/env python3
"""keybinds: writes KEYBINDS.md, a reference of what the merged wheel can bind in SnowRunner.

Sources: the game's initial.pak (the binding slots a custom wheel gets and their English names), dimerge.log
(which merged number is which physical control, from the proxy's own placement lines), the Steam cloud
user_settings.cfg (what is bound right now) and every Context.Action input link the game data mentions.

  keybinds.py [--bin <Sources\\Bin>] [--cfg <user_settings.cfg>] [--out KEYBINDS.md]

--bin defaults to the first SnowRunner install found through Steam; the pak, dimerge.log and dimerge.ini are
taken from there. --cfg defaults to the first Steam profile's SnowRunner settings file.
"""
import datetime
import json
import os
import re
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pakpatch"))
from wheel_slots import parse_index, ALL_SLOTS, steam_paks  # noqa: E402

AXES = ["X", "Y", "Z", "Rx", "Ry", "Rz", "Slider0", "Slider1"]
HAT = ["up", "right", "down", "left"]
# hidden defaults of the game's wheel presets per axis slot (initial.cache_block, see the readme)
AXIS_DEFAULTS = {"Rx": "camera zoom and minimap X by default", "Ry": "minimap Y by default",
                 "Slider0": "camera rotation X by default", "Slider1": "camera rotation Y by default"}


def read_ini(path):
    sections, cur = {}, None
    for raw in open(path, encoding="utf-8", errors="replace"):
        line = raw.split(";")[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            cur = line[1:-1]
            sections[cur] = {}
        elif "=" in line and cur:
            k, v = line.split("=", 1)
            sections[cur][k.strip().lower()] = v.strip()
    return sections


def number_map_from_log(path):
    """merged key number -> physical control, from the proxy's placement lines:
    '  Section  button 3  -> game button slot 67 ...', '  Section  hat 0  up  -> game button slot 90 ...'"""
    m = {}
    pat = re.compile(r"^\s*(\S+)\s+(axis|button|hat)\s+(\d+)\s*(\+mod|up|right|down|left)?\s*-> game (axis|button|hat)\s+slot (\d+)")
    for line in open(path, encoding="utf-8", errors="replace"):
        line = re.sub(r"^\S+ \[\d+\] ", "", line.rstrip())    # time and thread id
        g = pat.match(line)
        if not g:
            continue
        sec, kind, idx, extra, gkind, slot = g.group(1), g.group(2), int(g.group(3)), g.group(4) or "", g.group(5), int(g.group(6))
        if kind == "axis":
            what = "%s axis %s" % (sec, AXES[idx] if idx < 8 else idx)
        elif kind == "hat":
            what = "%s hat %d %s" % (sec, idx, extra)
        else:
            what = "%s button %d%s" % (sec, idx, " with the modifier held" if extra == "+mod" else "")
        if gkind == "button":
            m[slot] = what
        elif gkind == "axis":
            m[128 + slot] = what + " (%s)" % AXES[slot]
        elif gkind == "hat" and slot == 0:
            for i, d in enumerate(HAT):
                m[136 + i] = "%s hat %d %s" % (sec, idx, d)
    return m


def describe(key, nmap, primary):
    if key in nmap:
        return nmap[key]
    if key < 128:
        return "%s button %d" % (primary, key)
    if key < 136:
        ax = AXES[key - 128]
        extra = AXIS_DEFAULTS.get(ax)
        return "%s axis %s%s" % (primary, ax, " (" + extra + ")" if extra else "")
    if key < 140:
        return "%s hat %s" % (primary, HAT[key - 136])
    return "key %d" % key


def read_strings(z):
    name = [n for n in z.namelist() if n.endswith("strings_english.str")][0]
    text = z.read(name).decode("utf-16-le", errors="replace")
    out = {}
    for line in text.splitlines():
        m = re.match(r'^(\S+)\s+"(.*)"\s*$', line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


def read_slots(cache):
    names, t1, t2, text0, offs, sizes = parse_index(cache)
    k = [i for i, n in enumerate(names) if n.endswith(b"steering_wheel_input_mapper.sso")][0]
    entry = cache[text0 + offs[k]:text0 + offs[k] + sizes[k]].decode("latin1").split("\r\n")
    slots, cur, i = [], None, 0
    while i < len(entry):
        line = entry[i]
        ind = len(line) - len(line.lstrip(" "))
        s = line.strip()
        if ind == 0 and s.startswith("inputs   ="):
            cur = "in"
        elif cur == "in" and ind == 0 and s.startswith("}"):
            break
        elif cur == "in" and ind == 3 and s.endswith("{"):
            slot = {"name": s.split()[0], "ui": "", "targets": [], "contexts": []}
            slots.append(slot)
            j = i + 1
            while j < len(entry) and not (len(entry[j]) - len(entry[j].lstrip(" ")) == 3 and entry[j].strip() == "}"):
                t = entry[j].strip()
                m = re.match(r'^uiName   =   "(.*)"$', t)
                if m:
                    slot["ui"] = m.group(1)
                m = re.match(r'^inputLink   =   "(.*)"$', t)
                if m:
                    slot["targets"].append(m.group(1))
                m = re.match(r'^direction   =   "(.*)"$', t)
                if m and slot["targets"]:
                    slot["targets"][-1] += " " + m.group(1).lower()
                m = re.match(r'^"([A-Z_]+)"$', t)
                if m:
                    slot["contexts"].append(m.group(1))
                j += 1
            i = j
        i += 1
    return slots


def read_links(cache):
    links = {}
    for m in re.finditer(rb'inputLink   =   "([A-Za-z0-9]+)\.([A-Za-z0-9_]+)"', cache):
        links.setdefault(m.group(1).decode(), set()).add(m.group(2).decode())
    return links


def read_bindings(path):
    try:
        cfg = json.load(open(path, encoding="utf-8"))
        return cfg["steeringWheelPresets"]["Custom"]
    except Exception:
        text = open(path, encoding="utf-8", errors="replace").read()
        return {m.group(1): {"device": m.group(2), "key": int(m.group(3))}
                for m in re.finditer(r'"([A-Za-z0-9]+)":\{"device":"([a-z_]+)","key":(\d+)\}', text)}


def steam_cfg():
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as k:
            steam = winreg.QueryValueEx(k, "SteamPath")[0]
    except OSError:
        return None
    root = os.path.join(steam, "userdata")
    if not os.path.isdir(root):
        return None
    for user in sorted(os.listdir(root)):
        p = os.path.join(root, user, "1465360", "remote", "user_settings.cfg")
        if os.path.exists(p):
            return p
    return None


def main():
    args = sys.argv[1:]
    bin_dir = cfg = None
    out = "KEYBINDS.md"
    i = 0
    while i < len(args):
        if args[i] == "--bin":
            bin_dir = args[i + 1]
        elif args[i] == "--cfg":
            cfg = args[i + 1]
        elif args[i] == "--out":
            out = args[i + 1]
        else:
            raise SystemExit(__doc__)
        i += 2
    if bin_dir is None:
        paks = steam_paks()
        if not paks:
            raise SystemExit("no SnowRunner install found through Steam; give --bin <Sources\\Bin>")
        bin_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(paks[0])))), "Sources", "Bin")
    pak = os.path.join(os.path.dirname(os.path.dirname(bin_dir)), "preload", "paks", "client", "initial.pak")
    log = os.path.join(bin_dir, "dimerge.log")
    ini = os.path.join(bin_dir, "dimerge.ini")
    if cfg is None:
        cfg = steam_cfg()
    primary = "wheelbase"
    if os.path.exists(ini):
        for sec, kv in read_ini(ini).items():
            if sec == "Primary" and "device" in kv:
                primary = "wheelbase " + kv["device"]
    nmap = number_map_from_log(log) if os.path.exists(log) else {}
    with zipfile.ZipFile(pak) as z:
        cache = z.read("initial.cache_block")
        strings = read_strings(z)
    slots = read_slots(cache)
    links = read_links(cache)
    bound = read_bindings(cfg) if cfg and os.path.exists(cfg) else {}
    patched = {s[0] for s in ALL_SLOTS}
    o = []
    o.append("# SnowRunner keybinds on the merged wheel\n")
    o.append("\nGenerated %s by tools/keybinds/keybinds.py from %s, %s and %s. Run it again after changing bindings or the ini.\n"
             % (datetime.date.today().isoformat(), pak, log if nmap else "no dimerge.log (numbers of the merged controls unknown)", cfg or "no settings file"))
    o.append("\n## Merged key numbers\n\nThe game stores a wheel binding as a key number: buttons 0 to 127, axes 128 to 135 (X, Y, Z, Rx, Ry, Rz, Slider0, Slider1), hat directions 136 to 139 (up, right, down, left). What the proxy put on each number in its last run:\n")
    o.append("\n| Number | Physical control |\n|---|---|\n")
    used = sorted(set(list(nmap.keys()) + [128, 136, 137, 138, 139]))
    rows = []
    for k in used:
        d = describe(k, nmap, primary)
        generic = re.match(r"^(.*) button (\d+)$", d)
        if rows and generic and rows[-1][1] == k - 1 and rows[-1][2] == generic.group(1) and rows[-1][4] == int(generic.group(2)) - 1:
            rows[-1][1] = k
            rows[-1][4] = int(generic.group(2))
        else:
            rows.append([k, k, generic.group(1) if generic else None, d, int(generic.group(2)) if generic else 0])
    for a, b, base, d, last in rows:
        if a == b:
            o.append("| %d | %s |\n" % (a, d))
        else:
            o.append("| %d to %d | %s buttons %d to %d |\n" % (a, b, base, last - (b - a), last))
    o.append("| other 0 to 127 | %s buttons, native |\n" % primary)
    o.append("\n## Binding slots of a custom wheel\n\nEvery slot the settings menu offers for a wheel the game does not know by vendor id. Slots marked added come from tools/pakpatch/wheel_slots.py; the stock game has no wheel slot for moving the crane, entering crane mode, the engine or the HUD toggle.\n")
    o.append("\n| Slot | Menu name | Game input | Context | Bound now |\n|---|---|---|---|---|\n")
    for s in slots:
        b = bound.get(s["name"])
        now = ""
        if isinstance(b, dict) and "key" in b:
            now = "%d = %s" % (b["key"], describe(int(b["key"]), nmap, primary))
        name = s["name"] + (" (added)" if s["name"] in patched else "")
        ui = strings.get(s["ui"], s["ui"])
        o.append("| %s | %s | %s | %s | %s |\n" % (name, ui, ", ".join(s["targets"]), ", ".join(s["contexts"]), now))
    stray = [k for k in bound if k not in {s["name"] for s in slots}]
    if stray:
        o.append("\nBound in the settings file but not a slot any more: %s.\n" % ", ".join(stray))
    o.append("\n## Every input link in the game data\n\nAll Context.Action names the game data mentions (HUD hints, presets, menus). A wheel slot targets one of these; anything without a slot above is keyboard or gamepad only for a custom wheel unless a slot is added the way wheel_slots.py does it.\n")
    for ctx in sorted(links):
        o.append("\n- **%s**: %s\n" % (ctx, ", ".join(sorted(links[ctx]))))
    open(out, "w", encoding="utf-8", newline="\n").write("".join(o))
    print("wrote %s: %d slots (%d bound), %d merged numbers, %d contexts, %d links" % (out, len(slots), sum(1 for s in slots if s["name"] in bound), len(nmap), len(links), sum(len(v) for v in links.values())))


if __name__ == "__main__":
    main()
