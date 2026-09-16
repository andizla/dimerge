// dimerge-setup: the console wizard and installer for dimerge.
//
//   dimerge-setup              wizard: pick the wheel and the devices to merge, learn their controls, write dimerge.ini
//   dimerge-setup list         every attached game controller with its instance id
//   dimerge-setup test [s]     watch the merged wheel through the dinput8.dll next to this program
//   dimerge-setup install [Bin folder]     copy the proxy and ini into the game's Bin (found through Steam when omitted)
//   dimerge-setup uninstall [Bin folder]   remove them again and restore a chained dinput8.dll
//   dimerge-setup pak                      add wheel binding slots to the game's initial.pak
//
// Everything the wizard learns ends up as plain lines in dimerge.ini, so the file can still be edited by hand afterwards.
#define DIRECTINPUT_VERSION 0x0800
#define INITGUID
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dinput.h>
#include <shlwapi.h>
#include <cstdio>
#include <cwchar>
#include <conio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")

int PakSlotsMenu();                        // pak_slots.cpp
int PakCommand(int argc, wchar_t** argv);  // pak_slots.cpp: pak, pakfile, pakexport

static const wchar_t* kSlot[8] = { L"X", L"Y", L"Z", L"Rx", L"Ry", L"Rz", L"Slider0", L"Slider1" };

struct Dev {
    GUID instance{}, product{};
    std::wstring name;
    DWORD type = 0;
    bool ff = false;
    bool hasAxis[8] = {};
    int buttons = 0, povs = 0;
    IDirectInputDevice8W* dev = nullptr;
};

static IDirectInput8W* g_di = nullptr;
static std::vector<Dev> g_devs;

static std::wstring ExeDir() { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); PathRemoveFileSpecW(p); return std::wstring(p) + L"\\"; }
static bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static std::wstring GuidText(const GUID& g) { wchar_t b[64]; StringFromGUID2(g, b, 64); return b; }
static WORD Vid(const Dev& d) { return LOWORD(d.product.Data1); }
static WORD Pid(const Dev& d) { return HIWORD(d.product.Data1); }
static std::wstring IdText(const Dev& d) {
    // identical products need the instance id, a unique product does with VID:PID
    int same = 0; for (auto& o : g_devs) if (o.product.Data1 == d.product.Data1) same++;
    if (same > 1) return GuidText(d.instance);
    wchar_t b[16]; swprintf_s(b, L"%04X:%04X", Vid(d), Pid(d)); return b;
}

// DirectInput is loaded by hand: the system copy for the wizard, so an ini already next to the tool does not hide
// the devices it is about to learn; the proxy copy next to the tool for the test view.
typedef HRESULT(WINAPI* DI8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
static DI8Create g_create = nullptr;
static bool g_proxyLoaded = false;
static void DropDevices() {
    for (auto& d : g_devs) if (d.dev) d.dev->Release();
    g_devs.clear();
}
static bool LoadDI(bool throughProxy) {
    wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring path = throughProxy ? ExeDir() + L"dinput8.dll" : std::wstring(sys) + L"\\dinput8.dll";
    HMODULE h = LoadLibraryW(path.c_str());
    if (h) g_create = (DI8Create)GetProcAddress(h, "DirectInput8Create");
    if (!g_create) { wprintf(L"cannot load %s\n", path.c_str()); return false; }
    g_proxyLoaded = throughProxy;
    return true;
}

static BOOL CALLBACK EnumCB(LPCDIDEVICEINSTANCEW di, LPVOID) {
    Dev d; d.instance = di->guidInstance; d.product = di->guidProduct; d.name = di->tszProductName; d.type = di->dwDevType;
    if (FAILED(g_di->CreateDevice(di->guidInstance, &d.dev, nullptr))) return DIENUM_CONTINUE;
    d.dev->SetDataFormat(&c_dfDIJoystick2);
    d.dev->SetCooperativeLevel(GetConsoleWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    DIDEVCAPS caps{}; caps.dwSize = sizeof(caps);
    if (SUCCEEDED(d.dev->GetCapabilities(&caps))) { d.ff = (caps.dwFlags & DIDC_FORCEFEEDBACK) != 0; d.povs = (int)caps.dwPOVs; }
    for (int i = 0; i < 8; i++) { DIDEVICEOBJECTINSTANCEW oi{}; oi.dwSize = sizeof(oi); d.hasAxis[i] = SUCCEEDED(d.dev->GetObjectInfo(&oi, DIJOFS_X + i * 4, DIPH_BYOFFSET)); }
    for (int i = 0; i < 128; i++) { DIDEVICEOBJECTINSTANCEW oi{}; oi.dwSize = sizeof(oi); if (SUCCEEDED(d.dev->GetObjectInfo(&oi, DIJOFS_BUTTON(i), DIPH_BYOFFSET))) d.buttons = i + 1; }
    g_devs.push_back(d);
    return DIENUM_CONTINUE;
}

static bool Enumerate(bool throughProxy = false) {
    if (g_create && g_proxyLoaded != throughProxy) { DropDevices(); if (g_di) g_di->Release(); g_di = nullptr; g_create = nullptr; }
    if (!g_create && !LoadDI(throughProxy)) return false;
    if (!g_di && FAILED(g_create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&g_di, nullptr))) { wprintf(L"DirectInput is not available.\n"); return false; }
    DropDevices();
    g_di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumCB, nullptr, DIEDFL_ATTACHEDONLY);
    return true;
}

static void List() {
    wprintf(L"\n  ID device                                   id           axes                 buttons hats ffb\n");
    for (size_t i = 0; i < g_devs.size(); i++) {
        const Dev& d = g_devs[i];
        std::wstring axes; for (int a = 0; a < 8; a++) if (d.hasAxis[a]) { if (!axes.empty()) axes += L" "; axes += kSlot[a]; }
        wprintf(L"  %-2zu %-40.40s %-12s %-20s %-7d %-4d %s\n", i + 1, d.name.c_str(), IdText(d).c_str(), axes.c_str(), d.buttons, d.povs, d.ff ? L"yes" : L"");
        wprintf(L"     instance %s\n", GuidText(d.instance).c_str());
    }
    if (g_devs.empty()) wprintf(L"  no game controllers attached\n");
}

static std::wstring Ask(const wchar_t* prompt, const wchar_t* def) {
    wprintf(L"%s", prompt); if (def && *def) wprintf(L" [%s]", def); wprintf(L": ");
    wchar_t buf[512] = {}; if (!fgetws(buf, 512, stdin)) return def ? def : L"";
    std::wstring s = buf; while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' ')) s.pop_back();
    size_t a = s.find_first_not_of(L' '); s = a == std::wstring::npos ? L"" : s.substr(a);
    return s.empty() && def ? def : s;
}
static std::vector<int> Numbers(const std::wstring& s, int max) {
    std::vector<int> out; std::wstringstream ss(s); std::wstring item;
    while (std::getline(ss, item, L',')) { int n = _wtoi(item.c_str()); if (n >= 1 && n <= max && std::find(out.begin(), out.end(), n) == out.end()) out.push_back(n); }
    return out;
}

// One sampling pass over a device: which axes moved, where they rest, and which buttons and hats were used.
struct Learned {
    bool moved[8] = {}; LONG rest[8] = {}, lo[8] = {}, hi[8] = {};
    bool button[128] = {}; bool hat[4] = {};
    int buttonsUsed = 0, hatsUsed = 0, axesMoved = 0;
};
static bool Sample(Dev& d, DIJOYSTATE2& st) {
    d.dev->Poll();
    if (d.dev->GetDeviceState(sizeof(st), &st) == DI_OK) return true;
    d.dev->Acquire(); Sleep(20);
    return d.dev->GetDeviceState(sizeof(st), &st) == DI_OK;
}
static bool EnterPressed() {
    while (_kbhit()) { int c = _getch(); if (c == '\r' || c == '\n') return true; }
    return false;
}
// DirectInput answers with centred defaults (32767 on every axis) until the device's first report arrives, so the
// first reads after Acquire say nothing about the rest position. Read for a moment and keep the last state.
static bool Settle(Dev& d, DIJOYSTATE2& st) {
    bool ok = false;
    DWORD end = GetTickCount() + 400;
    while ((LONG)(end - GetTickCount()) > 0) { ok = Sample(d, st) || ok; Sleep(20); }
    return ok;
}
static Learned Learn(Dev& d, int seconds) {
    Learned L;
    d.dev->Acquire();
    DIJOYSTATE2 base{};
    if (!Settle(d, base)) { wprintf(L"    (no data from this device)\n"); return L; }
    for (int a = 0; a < 8; a++) { LONG v; memcpy(&v, (BYTE*)&base + a * 4, 4); L.rest[a] = L.lo[a] = L.hi[a] = v; }
    DWORD end = GetTickCount() + (DWORD)seconds * 1000;
    while ((LONG)(end - GetTickCount()) > 0 && !EnterPressed()) {
        DIJOYSTATE2 st{};
        if (Sample(d, st)) {
            for (int a = 0; a < 8; a++) {
                if (!d.hasAxis[a]) continue;
                LONG v; memcpy(&v, (BYTE*)&st + a * 4, 4);
                L.lo[a] = std::min(L.lo[a], v); L.hi[a] = std::max(L.hi[a], v);
                if (std::abs(v - L.rest[a]) > 3000 && !L.moved[a]) { L.moved[a] = true; L.axesMoved++; wprintf(L"    axis %s moved\n", kSlot[a]); }
            }
            for (int b = 0; b < 128; b++) if ((st.rgbButtons[b] & 0x80) && !L.button[b]) { L.button[b] = true; L.buttonsUsed++; wprintf(L"    button %d\n", b); }
            for (int h = 0; h < 4; h++) if (LOWORD(st.rgdwPOV[h]) != 0xFFFF && !L.hat[h]) { L.hat[h] = true; L.hatsUsed++; wprintf(L"    hat %d\n", h); }
        }
        Sleep(10);
    }
    d.dev->Unacquire();
    return L;
}
static bool RestsAtFullScale(const Learned& L, int a) {
    LONG span = L.hi[a] - L.lo[a];
    return span > 3000 && (L.rest[a] - L.lo[a]) > span * 9 / 10;   // rests at the top of what it reached: reads full scale untouched
}
// A button pressed on any of the given devices while the prompt is open, watched until Enter. count says how many
// different buttons were seen; dev and button name the last one.
struct Held { int dev = -1; int button = -1; int count = 0; };
static Held HeldButtonAny(std::vector<Dev*>& devs, const wchar_t* prompt) {
    wprintf(L"%s: ", prompt);
    for (Dev* d : devs) d->dev->Acquire();
    std::vector<std::vector<bool>> seen(devs.size(), std::vector<bool>(128, false));
    DWORD end = GetTickCount() + 120000;
    while ((LONG)(end - GetTickCount()) > 0 && !EnterPressed()) {
        for (size_t k = 0; k < devs.size(); k++) { DIJOYSTATE2 st{}; if (Sample(*devs[k], st)) for (int b = 0; b < 128; b++) if (st.rgbButtons[b] & 0x80) seen[k][b] = true; }
        Sleep(10);
    }
    wprintf(L"\n");
    for (Dev* d : devs) d->dev->Unacquire();
    Held h;
    for (size_t k = 0; k < devs.size(); k++) for (int b = 0; b < 128; b++) if (seen[k][b]) { h.dev = (int)k; h.button = b; h.count++; }
    return h;
}

static int Test(int seconds);
static int Install(int argc, wchar_t** argv);

static int Wizard() {
    wprintf(L"dimerge setup\n=============\nPlug in and switch on every controller first. Attached game controllers:\n");
    if (!Enumerate()) return 1;
    List();
    if (g_devs.size() < 2) { wprintf(L"\nMerging needs at least two devices. Nothing written.\n"); return 1; }
    wprintf(L"\nAnswer with the ID from the first column. Enter alone takes the value in brackets.\n");
    int ffCount = 0, ffIdx = 0; for (size_t i = 0; i < g_devs.size(); i++) if (g_devs[i].ff) { ffCount++; ffIdx = (int)i + 1; }
    wchar_t def[16] = L""; if (ffCount == 1) swprintf_s(def, L"%d", ffIdx);
    int prim = 0;
    while (prim < 1) { std::vector<int> n = Numbers(Ask(L"ID of the wheel the game will see (the force feedback device)", def), (int)g_devs.size()); if (!n.empty()) prim = n[0]; }
    std::wstring others; for (size_t i = 0; i < g_devs.size(); i++) if ((int)i + 1 != prim) { if (!others.empty()) others += L","; others += std::to_wstring(i + 1); }
    std::vector<int> merges;
    while (merges.empty()) { merges = Numbers(Ask(L"IDs of the devices to merge into it, comma separated", others.c_str()), (int)g_devs.size()); merges.erase(std::remove(merges.begin(), merges.end(), prim), merges.end()); }

    int steps = 1 + (int)merges.size(), step = 1;
    Dev& P = g_devs[prim - 1];
    std::vector<Dev*> all; all.push_back(&P); for (int m : merges) all.push_back(&g_devs[m - 1]);   // the wheel first, then the merged devices
    wprintf(L"\nStep %d of %d: the wheel, %s\nLeave every control alone and press Enter. Then move the rim and anything plugged into the base itself\n(its own pedals or paddles), and press Enter again when done. Axes that stay still are free for the merged devices.\n", step, steps, P.name.c_str());
    Ask(L"Press Enter to start", L"");
    wprintf(L"Move the wheel's own controls now; press Enter when done.\n");
    Learned pl = Learn(P, 120);
    std::wstring dead;
    for (int a = 0; a < 8; a++) if (P.hasAxis[a] && !pl.moved[a]) { if (!dead.empty()) dead += L","; dead += kSlot[a]; }
    wprintf(L"  free axes on the wheel: %s\n", dead.empty() ? L"none" : dead.c_str());

    // Fixed numbers rather than "auto", so a device left unplugged one day does not move the others' bindings. The
    // merged devices take blocks at the top of the 128 button numbers and the wheel's own keep the low ones: a
    // wheelbase often declares far more buttons than its rim has, so its declared count says nothing about free numbers.
    static const int order[8] = { 1, 2, 5, 6, 7, 3, 4, 0 };   // the proxy's own placement order: Y Z Rz Slider0 Slider1 Rx Ry X
    std::vector<int> freeSlots; for (int s : order) if (!pl.moved[s]) freeSlots.push_back(s);
    struct Mod { int dev; int button; };                      // dev indexes 'all': 0 the wheel, 1.. the merged devices
    struct KnobPlan { int left, right, slot; };
    struct Plan { Dev* d; std::wstring sec, axes, invert; std::vector<Mod> mods; std::vector<KnobPlan> knobs; int need = 0, buttons = -1, hatButtons = -1, layerButtons = -1; };
    std::vector<Plan> plans;
    for (int m : merges) {
        Dev& D = g_devs[m - 1];
        wprintf(L"\nStep %d of %d: %s\nLeave it alone and press Enter. Then move each of its axes through the full travel (pedals, sticks, levers) and\npress Enter again when done. Buttons and hats are merged as they are; pressing them is not needed.\n", ++step, steps, D.name.c_str());
        Ask(L"Press Enter to start", L"");
        wprintf(L"Move its axes now; press Enter when done.\n");
        Learned L = Learn(D, 120);
        Plan p{ &D };
        p.sec = D.name; p.sec.erase(std::remove_if(p.sec.begin(), p.sec.end(), [](wchar_t c) { return !iswalnum(c); }), p.sec.end());
        if (p.sec.empty()) p.sec = L"Device" + std::to_wstring(plans.size() + 1);
        for (auto& q : plans) if (q.sec == p.sec) p.sec += std::to_wstring(plans.size() + 1);
        for (int a = 0; a < 8; a++) if (D.hasAxis[a] && L.moved[a]) {
            if (!p.axes.empty()) p.axes += L",";
            p.axes += kSlot[a];
            if (!freeSlots.empty()) { p.axes += std::wstring(L"->") + kSlot[freeSlots.front()]; freeSlots.erase(freeSlots.begin()); }
            else wprintf(L"  no free wheel axis left for %s; the proxy looks for one at launch\n", kSlot[a]);
            if (RestsAtFullScale(L, a)) { if (!p.invert.empty()) p.invert += L","; p.invert += kSlot[a]; }
        }
        if (D.buttons > 0) {
            // modifiers: while one is held, this device's buttons get a second set of numbers (a shift layer); the
            // modifier may sit on this device, on another merged device or on the wheel
            for (;;) {
                Held h = HeldButtonAny(all, p.mods.empty()
                    ? L"  Modifier for its buttons (a shifter's pull collar, a layer switch, on this or any other device)? Hold it and press Enter; Enter alone means none"
                    : L"  Another modifier? Hold it and press Enter; Enter alone to continue");
                if (h.count == 0) break;
                if (h.count > 1) { wprintf(L"  %d different buttons were pressed, one is needed; try again\n", h.count); continue; }
                bool dup = false; for (auto& q : p.mods) if (q.dev == h.dev && q.button == h.button) dup = true;
                if (dup) { wprintf(L"  that button is already a modifier here\n"); continue; }
                p.mods.push_back({ h.dev, h.button });
                wprintf(L"  modifier %d: %s button %d\n", (int)p.mods.size(), all[h.dev]->name.c_str(), h.button);
            }
        }
        p.need = ((D.buttons * (1 + (int)p.mods.size()) + 4 * D.povs + 7) / 8) * 8;
        plans.push_back(p);
    }

    // rotary knobs: a knob that clicks as it turns sends a button per direction; two such buttons can drive an axis
    bool firstKnob = true;
    while (!freeSlots.empty()) {
        Held l = HeldButtonAny(all, firstKnob
            ? L"\nRotary knobs: a knob that clicks as it turns can drive an axis instead (camera zoom or rotation in the game).\nTurn one LEFT a few clicks and press Enter; Enter alone if there is none"
            : L"Another knob? Turn it LEFT a few clicks and press Enter; Enter alone to finish");
        firstKnob = false;
        if (l.count == 0) break;
        if (l.count > 1) { wprintf(L"  %d different buttons pulsed; turn one knob only\n", l.count); continue; }
        if (l.dev == 0) { wprintf(L"  that is the wheel's own button; knobs on merged devices only\n"); continue; }
        Held r = HeldButtonAny(all, L"  Now turn the same knob RIGHT a few clicks and press Enter");
        if (r.count != 1 || r.dev != l.dev || r.button == l.button) { wprintf(L"  that did not read as the other direction of the same knob; skipped\n"); continue; }
        int slot = freeSlots.front(); freeSlots.erase(freeSlots.begin());
        plans[l.dev - 1].knobs.push_back({ l.button, r.button, slot });
        wprintf(L"  knob on %s: buttons %d (left) and %d (right) -> axis %s\n", all[l.dev]->name.c_str(), l.button, r.button, kSlot[slot]);
    }

    int total = 0; for (auto& p : plans) total += p.need;
    if (total > 128) wprintf(L"\nThe merged devices need %d button numbers of the 128; they get automatic placement, which fills what is free.\n", total);
    else {
        int top = 128;
        for (auto it = plans.rbegin(); it != plans.rend(); ++it) {
            if (it->need == 0) continue;
            top -= it->need;
            it->buttons = top;
            it->hatButtons = top + it->d->buttons;
            it->layerButtons = it->hatButtons + 4 * it->d->povs;   // one block of the device's button count per modifier follows
        }
        int rimMax = -1; for (int b = 0; b < 128; b++) if (pl.button[b]) rimMax = b;
        if (rimMax >= top) wprintf(L"\nThe wheel's own button %d was pressed during its pass and lies inside the merged numbers (%d and up); both would fire together.\n", rimMax, top);
    }
    wprintf(L"\n");
    for (auto& p : plans) {
        Dev& D = *p.d;
        wchar_t bt[80] = L"none", ht[80] = L"none";
        if (D.buttons > 0) { if (p.buttons >= 0) swprintf_s(bt, L"%d to %d", p.buttons, p.buttons + D.buttons - 1); else wcscpy_s(bt, L"automatic"); }
        if (D.povs > 0) { if (p.hatButtons >= 0) swprintf_s(ht, L"four buttons each from %d", p.hatButtons); else wcscpy_s(ht, L"four buttons each, automatic"); }
        wprintf(L"%s: axes %s%s; buttons %s; hats %s", D.name.c_str(), p.axes.empty() ? L"none" : p.axes.c_str(), p.invert.empty() ? L"" : (L" (inverted: " + p.invert + L")").c_str(), bt, ht);
        for (size_t k = 0; k < p.mods.size() && p.layerButtons >= 0; k++) {
            int start = p.layerButtons + (int)k * D.buttons;
            wprintf(L"; with %s button %d held: %d to %d", all[p.mods[k].dev]->name.c_str(), p.mods[k].button, start, start + D.buttons - 1);
        }
        for (auto& k : p.knobs) wprintf(L"; knob %d/%d -> axis %s", k.left, k.right, kSlot[k.slot]);
        wprintf(L"\n");
    }

    std::wstring out = ExeDir() + L"dimerge.ini";
    std::wofstream f(out);
    f << L"; dimerge.ini written by dimerge-setup. Every line can be edited by hand; dimerge.log in the game folder shows the result.\n";
    f << L"[dimerge]\nEnabled=1\nLogLevel=1\nHideMerged=1\nCrashProbe=1\n\n";
    f << L"[Primary]\n; " << P.name << L"\nDevice=" << IdText(P) << L"\n";
    if (!dead.empty()) f << L"DeadAxes=" << dead << L"\n";
    f << L"\n";
    for (size_t pi = 0; pi < plans.size(); pi++) {
        Plan& p = plans[pi];
        f << L"[Merge." << p.sec << L"]\n; " << p.d->name << L"\nDevice=" << IdText(*p.d) << L"\n";
        f << L"Axes=" << (p.axes.empty() ? L"none" : p.axes) << L"\n";
        if (!p.invert.empty()) f << L"Invert=" << p.invert << L"\n";
        if (p.d->buttons == 0) f << L"Buttons=none\n"; else if (p.buttons >= 0) f << L"Buttons=" << p.buttons << L"\n"; else f << L"Buttons=auto\n";
        if (p.d->povs == 0) f << L"POVs=none\n"; else if (p.hatButtons >= 0) f << L"POVs=buttons:" << p.hatButtons << L"\n"; else f << L"POVs=buttons:auto\n";
        for (size_t k = 0; k < p.mods.size() && p.layerButtons >= 0; k++) {
            const Mod& m = p.mods[k];
            int start = p.layerButtons + (int)k * p.d->buttons;
            f << L"Layer=";
            if (m.dev == 0) f << L"Primary:";
            else if (m.dev - 1 != (int)pi) f << plans[m.dev - 1].sec << L":";
            f << m.button << L"->" << start;
            if (m.dev - 1 == (int)pi) f << L",hide";   // a modifier on the device itself is not a button the game should see
            f << L"\n";
        }
        for (auto& k : p.knobs) f << L"Knob=" << k.left << L"," << k.right << L"->" << kSlot[k.slot] << L"\n";
        f << L"\n";
    }
    f.close();
    wprintf(L"\nWritten: %s\n", out.c_str());
    for (;;) {
        std::wstring c = Ask(L"\nNext: [t] show the merged wheel live for 15 s, [i] install into the game, [p] add wheel binding slots to the game's initial.pak, [q] quit", L"q");
        if (c == L"t" || c == L"T") Test(15);
        else if (c == L"i" || c == L"I") Install(1, nullptr);
        else if (c == L"p" || c == L"P") PakSlotsMenu();
        else break;
    }
    return 0;
}

static int Test(int seconds) {
    // DirectInput8Create from the dinput8.dll next to this program is the proxy, so this watches the merged view
    if (!Exists(ExeDir() + L"dinput8.dll") || !Exists(ExeDir() + L"dimerge.ini")) { wprintf(L"test needs dinput8.dll and dimerge.ini next to dimerge-setup.exe\n"); return 1; }
    if (!Enumerate(true)) return 1;
    Dev* ffd = nullptr; for (auto& d : g_devs) if (d.ff) ffd = &d;
    if (!ffd) ffd = g_devs.empty() ? nullptr : &g_devs[0];
    if (!ffd) { wprintf(L"no device to watch\n"); return 1; }
    wprintf(L"watching \"%s\" through the proxy for %d s; the merged inputs show up in its values\n", ffd->name.c_str(), seconds);
    ffd->dev->Acquire();
    DIJOYSTATE2 last{}; bool first = true;
    Settle(*ffd, last);
    DWORD end = GetTickCount() + (DWORD)seconds * 1000;
    while ((LONG)(end - GetTickCount()) > 0) {
        DIJOYSTATE2 s{};
        if (!Sample(*ffd, s)) { Sleep(50); continue; }
        if (first || memcmp(&s, &last, sizeof(s)) != 0) {
            wprintf(L"X=%6ld Y=%6ld Z=%6ld Rx=%6ld Ry=%6ld Rz=%6ld S0=%6ld S1=%6ld POV=%6ld btn:", s.lX, s.lY, s.lZ, s.lRx, s.lRy, s.lRz, s.rglSlider[0], s.rglSlider[1], (long)s.rgdwPOV[0]);
            for (int i = 0; i < 128; i++) if (s.rgbButtons[i] & 0x80) wprintf(L" %d", i);
            wprintf(L"\n"); last = s; first = false;
        }
        Sleep(20);
    }
    ffd->dev->Unacquire();
    return 0;
}

// ---- install
static std::vector<std::wstring> SteamBins() {
    std::vector<std::wstring> out;
    wchar_t steam[MAX_PATH] = {}; DWORD n = sizeof(steam);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, steam, &n) != ERROR_SUCCESS) return out;
    std::vector<std::wstring> libs = { steam };
    std::wifstream vdf(std::wstring(steam) + L"\\steamapps\\libraryfolders.vdf");
    std::wstring line;
    while (std::getline(vdf, line)) {
        size_t p = line.find(L"\"path\"");
        if (p == std::wstring::npos) continue;
        size_t a = line.find(L'"', p + 6), b = a == std::wstring::npos ? a : line.find(L'"', a + 1);
        if (a == std::wstring::npos || b == std::wstring::npos) continue;
        std::wstring path = line.substr(a + 1, b - a - 1);
        std::wstring fixed; for (size_t i = 0; i < path.size(); i++) { if (path[i] == L'\\' && i + 1 < path.size() && path[i + 1] == L'\\') i++; fixed += path[i]; }
        libs.push_back(fixed);
    }
    for (auto& lib : libs) {
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((lib + L"\\steamapps\\common\\SnowRunner*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            std::wstring bin = lib + L"\\steamapps\\common\\" + fd.cFileName + L"\\Sources\\Bin";
            if (Exists(bin + L"\\SnowRunner.exe") && std::find(out.begin(), out.end(), bin) == out.end()) out.push_back(bin);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return out;
}
std::vector<std::wstring> SteamGameFolders() {   // pak_slots.cpp asks for these
    std::vector<std::wstring> out;
    for (auto& bin : SteamBins()) out.push_back(bin.substr(0, bin.size() - wcslen(L"\\Sources\\Bin")));
    return out;
}
static bool IsDimerge(const std::wstring& dll) {
    std::ifstream f(dll, std::ios::binary); if (!f) return false;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return data.find("dimerge " ) != std::string::npos && data.find("dimerge.ini") != std::string::npos;
}
static std::wstring PickBin(int argc, wchar_t** argv) {
    if (argc > 2) return argv[2];
    std::vector<std::wstring> bins = SteamBins();
    if (bins.empty()) { wprintf(L"No SnowRunner install found through Steam. Give the Bin folder as an argument.\n"); return L""; }
    wprintf(L"Game folders found:\n"); for (size_t i = 0; i < bins.size(); i++) wprintf(L"  %zu  %s\n", i + 1, bins[i].c_str());
    std::vector<int> n = Numbers(Ask(L"Install into", L"1"), (int)bins.size());
    return n.empty() ? L"" : bins[n[0] - 1];
}
static int Install(int argc, wchar_t** argv) {
    std::wstring bin = PickBin(argc, argv); if (bin.empty()) return 1;
    if (bin.back() != L'\\') bin += L'\\';
    std::wstring src = ExeDir();
    if (!Exists(src + L"dinput8.dll")) { wprintf(L"dinput8.dll is missing next to dimerge-setup.exe\n"); return 1; }
    if (!Exists(src + L"dimerge.ini")) { wprintf(L"dimerge.ini is missing next to dimerge-setup.exe; run the wizard first\n"); return 1; }
    std::wstring existing = bin + L"dinput8.dll";
    bool chained = Exists(bin + L"dinput8_chain.dll");   // a previous install already moved the folder's own proxy aside
    if (Exists(existing) && !IsDimerge(existing)) {
        if (chained) { wprintf(L"%s already has a dinput8_chain.dll; sort that out by hand first.\n", bin.c_str()); return 1; }
        if (!MoveFileW(existing.c_str(), (bin + L"dinput8_chain.dll").c_str())) { wprintf(L"could not rename the existing dinput8.dll (is the game running?)\n"); return 1; }
        chained = true;
        wprintf(L"The folder had its own dinput8.dll (an ASI loader or another mod). It is now dinput8_chain.dll and dimerge loads it.\n");
    }
    if (!CopyFileW((src + L"dinput8.dll").c_str(), existing.c_str(), FALSE)) { wprintf(L"could not copy dinput8.dll (is the game running?)\n"); return 1; }
    if (Exists(bin + L"dimerge.ini") && CopyFileW((bin + L"dimerge.ini").c_str(), (bin + L"dimerge.ini.bak").c_str(), FALSE))
        wprintf(L"The folder's dimerge.ini was kept as dimerge.ini.bak; hand-made lines such as a shift layer live there.\n");
    if (!CopyFileW((src + L"dimerge.ini").c_str(), (bin + L"dimerge.ini").c_str(), FALSE)) { wprintf(L"could not copy dimerge.ini\n"); return 1; }
    std::wofstream manifest(bin + L"dimerge-install.txt");
    if (chained) manifest << L"chained dinput8.dll -> dinput8_chain.dll\n";
    manifest << L"copied dinput8.dll\ncopied dimerge.ini\n";
    wprintf(L"Installed into %s\nStart the game, then bind the merged buttons and axes in its controls menu. dimerge.log there shows what was merged.\n", bin.c_str());
    return 0;
}
static int Uninstall(int argc, wchar_t** argv) {
    std::wstring bin = PickBin(argc, argv); if (bin.empty()) return 1;
    if (bin.back() != L'\\') bin += L'\\';
    if (!Exists(bin + L"dimerge-install.txt") && !Exists(bin + L"dimerge.ini")) { wprintf(L"dimerge is not installed in %s\n", bin.c_str()); return 1; }
    if (Exists(bin + L"dinput8.dll") && IsDimerge(bin + L"dinput8.dll")) DeleteFileW((bin + L"dinput8.dll").c_str());
    if (Exists(bin + L"dinput8_chain.dll") && !Exists(bin + L"dinput8.dll")) MoveFileW((bin + L"dinput8_chain.dll").c_str(), (bin + L"dinput8.dll").c_str());
    for (const wchar_t* f : { L"dimerge.ini", L"dimerge.log", L"dimerge-install.txt" }) DeleteFileW((bin + f).c_str());
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((bin + L"dimerge_crash_*.dmp").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) { do { DeleteFileW((bin + fd.cFileName).c_str()); } while (FindNextFileW(h, &fd)); FindClose(h); }
    wprintf(L"Removed from %s\n", bin.c_str());
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    std::wstring cmd = argc > 1 ? argv[1] : L"";
    if (cmd == L"list") { if (!Enumerate()) return 1; List(); return 0; }
    if (cmd == L"test") return Test(argc > 2 ? _wtoi(argv[2]) : 15);
    if (cmd == L"install") return Install(argc, argv);
    if (cmd == L"uninstall") return Uninstall(argc, argv);
    if (cmd == L"pak" || cmd == L"pakfile" || cmd == L"pakexport") return PakCommand(argc, argv);
    if (!cmd.empty()) { wprintf(L"usage: dimerge-setup [list | test [seconds] | install [Bin] | uninstall [Bin] | pak | pakfile <in> <out> [sets] | pakexport <in> <folder> [sets]]\n"); return 1; }
    return Wizard();
}
