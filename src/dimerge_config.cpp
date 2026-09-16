// dimerge_config.cpp: reads dimerge.ini.
#include "dimerge.h"
#include <fstream>
#include <sstream>
#include <cctype>

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

static const char* kSlotNames[8] = { "X", "Y", "Z", "Rx", "Ry", "Rz", "Slider0", "Slider1" };
const char* AxisSlotName(int slot) { return (slot >= 0 && slot < 8) ? kSlotNames[slot] : "?"; }
int AxisSlotFromName(const std::string& name) {
    std::string n = Lower(Trim(name));
    for (int i = 0; i < 8; i++) if (n == Lower(kSlotNames[i])) return i;
    if (n == "s0" || n == "slider") return 6;
    if (n == "s1") return 7;
    return -1;
}

std::string DevIdText(const DevId& id) {
    static const GUID zero = {};
    if (memcmp(&id.instance, &zero, sizeof(GUID)) != 0) return GuidStr(id.instance);
    char b[32]; sprintf_s(b, id.index ? "%04X:%04X#%d" : "%04X:%04X", id.vid, id.pid, id.index);
    return b;
}

// Device: VID:PID | VID:PID#n | {GUID}
static bool ParseDevId(const std::string& v, DevId& id) {
    std::string t = Trim(v);
    if (!t.empty() && t.front() == '{') {
        std::wstring w(t.begin(), t.end());
        if (FAILED(CLSIDFromString(w.c_str(), &id.instance))) return false;
        id.set = true; return true;
    }
    unsigned a = 0, b = 0; int n = 0;
    int got = sscanf_s(t.c_str(), "%x:%x#%d", &a, &b, &n);
    if (got < 2) got = sscanf_s(t.c_str(), "%x/%x#%d", &a, &b, &n);
    if (got < 2 || n < 0) return false;
    id.vid = (WORD)a; id.pid = (WORD)b; id.index = got == 3 ? n : 0; id.set = true;
    return true;
}

// Axes: none | auto | "X->Slider0,Y,Z->Rz" (a source slot alone is placed automatically).
static bool ParseAxes(const std::string& v, DevSpec& d, std::string& err) {
    std::string l = Lower(Trim(v));
    if (l == "none") { d.axesMode = MapNone; return true; }
    if (l == "auto" || l.empty()) { d.axesMode = MapAuto; return true; }
    d.axesMode = MapExplicit; d.axes.clear();
    std::stringstream ss(v); std::string item;
    while (std::getline(ss, item, ',')) {
        size_t p = item.find("->");
        AxisRule r{ AxisSlotFromName(p == std::string::npos ? item : item.substr(0, p)), p == std::string::npos ? -1 : AxisSlotFromName(item.substr(p + 2)) };
        if (r.src < 0 || (p != std::string::npos && r.dst < 0)) { err = "unknown axis name in '" + Trim(item) + "'"; return false; }
        d.axes.push_back(r);
    }
    return true;
}
// Buttons / POVs: none | auto | <first merged index>.
static bool ParseIndexed(const std::string& v, int& mode, int& start, std::string& err) {
    std::string l = Lower(Trim(v));
    if (l == "none") { mode = MapNone; return true; }
    if (l == "auto" || l.empty()) { mode = MapAuto; return true; }
    char* end = nullptr; long n = strtol(l.c_str(), &end, 10);
    if (!end || *end || n < 0 || n > 127) { err = "expected none, auto or a number 0..127, got '" + Trim(v) + "'"; return false; }
    mode = MapExplicit; start = (int)n; return true;
}
static std::vector<std::string> Split(const std::string& v) {
    std::vector<std::string> parts; std::stringstream ss(v); std::string item;
    while (std::getline(ss, item, ',')) parts.push_back(Trim(item));
    return parts;
}
static bool Number(const std::string& s, long lo, long hi, long& out) {
    char* end = nullptr; out = strtol(s.c_str(), &end, 10);
    return end && *end == 0 && !s.empty() && out >= lo && out <= hi;
}
// Layer: <modifier>-><first merged button>[,hide]. The modifier is N (a button of this device), Name:N (button N of
// another merge section) or Primary:N (a wheelbase button).
static bool ParseLayer(const std::string& v, Layer& L, std::string& err) {
    std::vector<std::string> parts = Split(v);
    size_t p = parts.empty() ? std::string::npos : parts[0].find("->");
    if (p == std::string::npos) { err = "Layer wants modifier->start, got '" + Trim(v) + "'"; return false; }
    std::string mod = Trim(parts[0].substr(0, p)), start = Trim(parts[0].substr(p + 2));
    size_t c = mod.find(':');
    if (c != std::string::npos) { L.modName = Lower(Trim(mod.substr(0, c))); mod = Trim(mod.substr(c + 1)); }
    long b = 0, s = 0;
    if (!Number(mod, 0, 127, b)) { err = "Layer modifier wants a button number 0..127, got '" + mod + "'"; return false; }
    if (!Number(start, 0, 127, s)) { err = "Layer start wants a merged button number 0..127, got '" + start + "'"; return false; }
    L.modButton = (int)b; L.start = (int)s;
    for (size_t i = 1; i < parts.size(); i++) {
        std::string o = Lower(parts[i]);
        if (o == "hide") L.hide = true; else if (!o.empty()) { err = "unknown Layer option '" + parts[i] + "'"; return false; }
    }
    return true;
}
// Knob: <left button>,<right button>-><axis|auto>[,hold][,step=N][,pulse=ms]
static bool ParseKnob(const std::string& v, Knob& k, std::string& err) {
    std::vector<std::string> parts = Split(v);
    size_t p = parts.size() < 2 ? std::string::npos : parts[1].find("->");
    if (p == std::string::npos) { err = "Knob wants left,right->axis, got '" + Trim(v) + "'"; return false; }
    long l = 0, r = 0;
    if (!Number(parts[0], 0, 127, l) || !Number(Trim(parts[1].substr(0, p)), 0, 127, r)) { err = "Knob buttons want numbers 0..127"; return false; }
    std::string dst = Lower(Trim(parts[1].substr(p + 2)));
    k.left = (int)l; k.right = (int)r; k.dst = dst == "auto" ? -1 : AxisSlotFromName(dst);
    if (dst != "auto" && k.dst < 0) { err = "unknown axis '" + dst + "' in Knob"; return false; }
    for (size_t i = 2; i < parts.size(); i++) {
        std::string o = Lower(parts[i]); long n;
        if (o == "hold") k.hold = true;
        else if (o.rfind("step=", 0) == 0) { if (!Number(o.substr(5), 1, 100, n)) { err = "Knob step wants 1..100 (percent of the range)"; return false; } k.stepPct = (int)n; }
        else if (o.rfind("pulse=", 0) == 0) { if (!Number(o.substr(6), 10, 5000, n)) { err = "Knob pulse wants 10..5000 (ms)"; return false; } k.pulseMs = (int)n; }
        else if (!o.empty()) { err = "unknown Knob option '" + parts[i] + "'"; return false; }
    }
    return true;
}
static bool ParseSlotList(const std::string& v, std::vector<int>& out, std::string& err) {
    std::stringstream ss(v); std::string item;
    while (std::getline(ss, item, ',')) {
        int s = AxisSlotFromName(item);
        if (s < 0) { err = "unknown axis '" + Trim(item) + "'"; return false; }
        out.push_back(s);
    }
    return true;
}

bool LoadConfig(const std::wstring& path, Config& cfg, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open ini"; return false; }
    std::string line, section; DevSpec* cur = nullptr; int lineNo = 0;
    while (std::getline(f, line)) {
        lineNo++;
        size_t c = line.find_first_of(";#");
        size_t brace = line.find('{');
        if (c != std::string::npos && !(brace != std::string::npos && brace < c && line.find('}', c) != std::string::npos)) line = line.substr(0, c);   // a '#' inside {GUID} is not a comment
        line = Trim(line);
        if (line.empty()) continue;
        std::string where = "line " + std::to_string(lineNo) + ": ";
        if (line.front() == '[' && line.back() == ']') {
            std::string raw = Trim(line.substr(1, line.size() - 2));
            section = Lower(raw);
            cur = nullptr;
            if (section.rfind("merge.", 0) == 0) { cfg.merges.push_back(DevSpec()); cur = &cfg.merges.back(); cur->name = raw.substr(6); }
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) { err = where + "expected key=value"; return false; }
        std::string key = Lower(Trim(line.substr(0, eq))), val = Trim(line.substr(eq + 1));
        std::string e;
        if (section == "dimerge") {
            if (key == "enabled") cfg.enabled = val != "0";
            else if (key == "loglevel") cfg.logLevel = atoi(val.c_str());
            else if (key == "hidemerged") cfg.hideMerged = val != "0";
            else if (key == "crashprobe") cfg.crashProbe = val != "0";
            else if (key == "chain") { std::string t = Lower(val); cfg.chain = (t == "none" || t == "auto") ? L"" : std::wstring(val.begin(), val.end()); }
            else if (key == "axisorder") { cfg.axisOrder.clear(); if (!ParseSlotList(val, cfg.axisOrder, e)) { err = where + e; return false; } }
            else if (key == "nonexclusivemodules") {
                cfg.nonExclusiveModules.clear();
                std::stringstream ss(val); std::string item;
                while (std::getline(ss, item, ',')) { std::string t = Lower(Trim(item)); if (!t.empty() && t != "none") cfg.nonExclusiveModules.push_back(std::wstring(t.begin(), t.end())); }
            }
        } else if (section == "primary") {
            if (key == "device") { if (!ParseDevId(val, cfg.prim)) { err = where + "Device wants VID:PID, VID:PID#n or {GUID}"; return false; } }
            else if (key == "deadaxes") { if (!ParseSlotList(val, cfg.deadAxes, e)) { err = where + e; return false; } }
        } else if (cur) {
            if (key == "device") { if (!ParseDevId(val, cur->id)) { err = where + "Device wants VID:PID, VID:PID#n or {GUID}"; return false; } }
            else if (key == "enabled") cur->enabled = val != "0";
            else if (key == "axes") { if (!ParseAxes(val, *cur, e)) { err = where + e; return false; } }
            else if (key == "shift") { cur->shiftButton = (Lower(val) == "none") ? -1 : atoi(val.c_str()); }
            else if (key == "shiftbuttons") { cur->shiftStart = (Lower(val) == "none") ? -1 : atoi(val.c_str()); if (cur->shiftStart > 127) { err = where + "ShiftButtons wants 0..127"; return false; } }
            else if (key == "shifthide") { cur->shiftHide = val != "0"; }
            else if (key == "layer") { Layer L; if (!ParseLayer(val, L, e)) { err = where + e; return false; } cur->layers.push_back(L); }
            else if (key == "knob") { Knob k; if (!ParseKnob(val, k, e)) { err = where + e; return false; } cur->knobs.push_back(k); }
            else if (key == "invert") {
                std::string l = Lower(val);
                if (l != "none" && !l.empty() && !ParseSlotList(val, cur->invert, e)) { err = where + e; return false; }
            }
            else if (key == "buttons") { if (!ParseIndexed(val, cur->buttonsMode, cur->buttonsStart, e)) { err = where + e; return false; } }
            else if (key == "povs" || key == "hats") {
                std::string l = Lower(val);
                if (l.rfind("buttons:", 0) == 0) {
                    cur->povsMode = MapButtons;
                    std::string rest = Trim(l.substr(8));
                    if (rest == "auto" || rest.empty()) cur->povsStart = -1;
                    else { cur->povsStart = atoi(rest.c_str()); if (cur->povsStart < 0 || cur->povsStart > 124) { err = where + "buttons:N wants N in 0..124, or buttons:auto"; return false; } }
                } else if (!ParseIndexed(val, cur->povsMode, cur->povsStart, e)) { err = where + e; return false; }
            }
        }
    }
    if (!cfg.prim.set) { err = "[Primary] Device is missing"; return false; }
    for (auto& m : cfg.merges) {
        if (!m.id.set) { err = "[Merge." + m.name + "] Device is missing"; return false; }
        if (m.shiftButton >= 0 && m.shiftStart >= 0) { Layer L; L.modButton = m.shiftButton; L.start = m.shiftStart; L.hide = m.shiftHide; m.layers.insert(m.layers.begin(), L); }
        for (auto& L : m.layers) {
            if (L.modName.empty()) L.modSec = -1;
            else if (L.modName == "primary" || L.modName == "wheel" || L.modName == "wheelbase") L.modSec = -2;
            else {
                L.modSec = -3;
                for (int i = 0; i < (int)cfg.merges.size(); i++) if (Lower(cfg.merges[i].name) == L.modName) L.modSec = i;
                if (L.modSec == -3) { err = "[Merge." + m.name + "] Layer names a section that does not exist: '" + L.modName + "'"; return false; }
            }
        }
    }
    return true;
}
