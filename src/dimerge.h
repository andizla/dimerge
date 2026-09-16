// dimerge: a dinput8.dll proxy that presents several DirectInput devices to the game as one.
// The primary device (the force-feedback wheelbase) is passed through untouched, so FFB effects reach it
// natively. Axes, buttons and hats of the other devices are written into the primary's data block.
#pragma once
#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dinput.h>
#include <string>
#include <vector>
#include <cstring>
#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL 0x80000000
#endif

#define DIMERGE_VERSION "2.0"

enum class Kind { Axis, Button, Pov };
enum MapMode { MapNone = 0, MapAuto = 1, MapExplicit = 2, MapButtons = 3 };   // MapExplicit: axis rules, or a start index for buttons/hats; MapButtons: a hat becomes four buttons

struct AxisRule { int src; int dst; };          // DIJOYSTATE2 axis slots 0..7: X Y Z Rx Ry Rz Slider0 Slider1; dst -1 = place this axis automatically

// A shift layer: while its modifier is held, the device's buttons land on start + N instead of their plain numbers.
// The modifier sits on this device (modSec -1), on the wheelbase (-2) or on another merged device (its index).
struct Layer { int modSec = -1; int modButton = -1; int start = -1; bool hide = false; std::string modName; };
// A rotary knob: two source buttons, one per direction, drive a merged axis. Pulse mode deflects the axis fully for
// pulseMs after each click and returns to the centre; hold mode moves it by stepPct of the range per click and keeps it.
struct Knob { int left = -1, right = -1; int dst = -1; bool hold = false; int stepPct = 10; int pulseMs = 100; };

// Which physical device a section means. Three spellings in the ini: VID:PID (the first attached device with that
// product id), VID:PID#n (the nth one, for identical devices) or {instance GUID} as dienum prints it.
struct DevId {
    WORD vid = 0, pid = 0;
    int index = 0;                 // 0 = any instance, n = the nth in enumeration order
    GUID instance = {};            // all zero = not used
    bool set = false;
};

struct DevSpec {
    std::string name;
    DevId id;
    bool enabled = true;
    int axesMode = MapAuto;    std::vector<AxisRule> axes;
    int buttonsMode = MapAuto; int buttonsStart = 0;
    int povsMode = MapAuto;    int povsStart = 0;   // MapButtons with povsStart -1: four free buttons are taken automatically
    std::vector<int> invert;   // source axis slots whose direction is flipped (rest reads full scale)
    std::vector<Layer> layers; // shift layers in ini order; the first held modifier wins
    std::vector<Knob> knobs;   // rotary knobs driving merged axes
    int shiftButton = -1;      // the 1.x spelling of the first layer (Shift, ShiftButtons, ShiftHide), folded into layers after the file is read
    int shiftStart = -1;
    bool shiftHide = false;
};

struct Config {
    bool enabled = true;
    int logLevel = 1;                 // 0 quiet, 1 normal, 2 every call
    bool hideMerged = true;           // hide the merged devices from the game's own enumeration
    std::vector<std::wstring> nonExclusiveModules = { L"smt.asi", L"smt.addon64" };   // DirectInput clients created from these modules never get exclusive access
    bool crashProbe = true;           // log fatal-looking exceptions with module and offset, minidump the first two
    std::wstring chain;               // another dinput8 proxy to load instead of the system DLL (an ASI loader for instance); empty = auto-detect dinput8_chain.dll
    DevId prim;
    std::vector<int> deadAxes;        // primary axis slots nothing is plugged into; merged axes may take them over
    std::vector<int> axisOrder = { 1, 2, 5, 6, 7, 3, 4, 0 };   // preference for automatic axis placement: Y Z Rz Slider0 Slider1 Rx Ry X
    std::vector<DevSpec> merges;
};

// dimerge_config.cpp
bool LoadConfig(const std::wstring& path, Config& cfg, std::string& err);
int  AxisSlotFromName(const std::string& name);   // -1 when unknown
const char* AxisSlotName(int slot);
std::string DevIdText(const DevId& id);

// dimerge_main.cpp
void LogF(int level, const char* fmt, ...);
int  LogLevel();
#define LOG(lvl, ...) do { if (LogLevel() >= (lvl)) LogF((lvl), __VA_ARGS__); } while (0)
const Config& Cfg();
IDirectInput8W* InternalDI();          // the proxy's own real IDirectInput8W, never handed to the game
const DIDATAFORMAT* Joy2Format();      // a private copy of c_dfDIJoystick2 (this module cannot link dinput8.lib)
const GUID* SlotGuid(int slot);
std::string Narrow(const wchar_t* s);
std::string GuidStr(const GUID& g);

// dimerge_device.cpp
HRESULT CreateWrappedDI(REFIID riid, void* real, void** out, bool foreign);

inline bool ProductMatches(const GUID& product, WORD vid, WORD pid) {
    static const BYTE tail[8] = { 0x00, 0x00, 'P', 'I', 'D', 'V', 'I', 'D' };
    return product.Data1 == (DWORD)(((DWORD)pid << 16) | vid) && product.Data2 == 0 && product.Data3 == 0
        && memcmp(product.Data4, tail, 8) == 0;
}
// True when a device seen during one enumeration pass is the one an id means. 'seen' counts the pass's earlier
// matches of the same product id, so the "#n" spelling can pick the nth identical device.
inline bool IdMatches(const DevId& id, const GUID& product, const GUID& instance, int& seen) {
    if (!id.set) return false;
    static const GUID zero = {};
    if (memcmp(&id.instance, &zero, sizeof(GUID)) != 0) return memcmp(&id.instance, &instance, sizeof(GUID)) == 0;
    if (!ProductMatches(product, id.vid, id.pid)) return false;
    seen++;
    return id.index == 0 || seen == id.index;
}
inline DWORD SlotOfs(int slot) { return (DWORD)slot * 4; }        // DIJOYSTATE2: lX..rglSlider[1]
inline DWORD PovOfs(int i) { return 32 + (DWORD)i * 4; }          // rgdwPOV
inline DWORD ButtonOfs(int i) { return 48 + (DWORD)i; }           // rgbButtons
