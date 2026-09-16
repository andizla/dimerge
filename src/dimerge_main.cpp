// dimerge_main.cpp: DLL entry, the dinput8 exports, logging, config and the private joystick2 format.
#define INITGUID
#include "dimerge.h"
#include <cstdio>
#include <share.h>
#include <cstdarg>
#include <mutex>
#include <algorithm>
#include <cwctype>
#include <dbghelp.h>

typedef HRESULT(WINAPI* PFN_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(WINAPI* PFN_DllCanUnloadNow)();
typedef HRESULT(WINAPI* PFN_DllGetClassObject)(REFCLSID, REFIID, LPVOID*);
typedef HRESULT(WINAPI* PFN_DllRegister)();
typedef LPCDIDATAFORMAT(WINAPI* PFN_GetdfDIJoystick)();

static HMODULE g_self = nullptr, g_real = nullptr;
static PFN_DirectInput8Create g_realCreate = nullptr;
static PFN_DllCanUnloadNow g_realCanUnload = nullptr;
static PFN_DllGetClassObject g_realGetClassObject = nullptr;
static PFN_DllRegister g_realRegister = nullptr, g_realUnregister = nullptr;
static PFN_GetdfDIJoystick g_realGetdf = nullptr;
static Config g_cfg; static bool g_cfgOk = false;
static FILE* g_log = nullptr; static int g_logLevel = 1;
static std::mutex g_logMx;
static std::once_flag g_once;
static IDirectInput8W* g_internal = nullptr;
static std::wstring g_dir;
static LONG g_probeCount = 0;
typedef BOOL(WINAPI* PFN_MiniDumpWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

// Sees every exception before the game does. Logs the fatal-looking ones and dumps the first two; never handles them.
static LONG CALLBACK CrashProbe(EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c != EXCEPTION_ACCESS_VIOLATION && c != EXCEPTION_ILLEGAL_INSTRUCTION && c != EXCEPTION_STACK_OVERFLOW && c != EXCEPTION_PRIV_INSTRUCTION && c != EXCEPTION_INT_DIVIDE_BY_ZERO) return EXCEPTION_CONTINUE_SEARCH;
    LONG n = InterlockedIncrement(&g_probeCount);
    if (n > 20) return EXCEPTION_CONTINUE_SEARCH;
    void* addr = ep->ExceptionRecord->ExceptionAddress;
    HMODULE m = nullptr; wchar_t path[MAX_PATH] = L"?";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &m) && m) GetModuleFileNameW(m, path, MAX_PATH);
    const char* kind = c == EXCEPTION_ACCESS_VIOLATION ? (ep->ExceptionRecord->ExceptionInformation[0] == 1 ? " write" : ep->ExceptionRecord->ExceptionInformation[0] == 8 ? " execute" : " read") : "";
    LOG(1, "exception %08x%s at %p = %s+0x%llx, thread %lu, target %p", c, kind, addr, Narrow(path).c_str(), (unsigned long long)((BYTE*)addr - (BYTE*)m), GetCurrentThreadId(),
        c == EXCEPTION_ACCESS_VIOLATION ? (void*)ep->ExceptionRecord->ExceptionInformation[1] : nullptr);
    if (n <= 2) {
        HMODULE dbg = LoadLibraryW(L"dbghelp.dll");
        PFN_MiniDumpWriteDump fn = dbg ? (PFN_MiniDumpWriteDump)GetProcAddress(dbg, "MiniDumpWriteDump") : nullptr;
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t name[64]; swprintf_s(name, L"dimerge_crash_%02d%02d%02d_%ld.dmp", st.wHour, st.wMinute, st.wSecond, n);
        HANDLE h = fn ? CreateFileW((g_dir + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
        if (h != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei = { GetCurrentThreadId(), ep, FALSE };
            BOOL ok = fn(GetCurrentProcess(), GetCurrentProcessId(), h, (MINIDUMP_TYPE)(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithUnloadedModules), &mei, nullptr, nullptr);
            CloseHandle(h);
            LOG(1, "crash dump %s: %s", Narrow(name).c_str(), ok ? "written" : "failed");
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int LogLevel() { return g_logLevel; }
void LogF(int, const char* fmt, ...) {
    std::lock_guard<std::mutex> lk(g_logMx);
    if (!g_log) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_log, "%02d:%02d:%02d.%03d [%5lu] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId());
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}
std::string Narrow(const wchar_t* s) {
    if (!s) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, s, -1, &out[0], n, nullptr, nullptr);
    return out;
}
std::string GuidStr(const GUID& g) {
    char b[64]; sprintf_s(b, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return b;
}
const Config& Cfg() { return g_cfg; }

// ---- a private copy of c_dfDIJoystick2 (linking dinput8.lib from a module named dinput8.dll would import ourselves)
static const GUID* kAxisGuids[8] = { &GUID_XAxis, &GUID_YAxis, &GUID_ZAxis, &GUID_RxAxis, &GUID_RyAxis, &GUID_RzAxis, &GUID_Slider, &GUID_Slider };
const GUID* SlotGuid(int slot) { return (slot >= 0 && slot < 8) ? kAxisGuids[slot] : &GUID_Unknown; }
static DIOBJECTDATAFORMAT g_joy2Objs[164];
static DIDATAFORMAT g_joy2 = {};
const DIDATAFORMAT* Joy2Format() {
    if (g_joy2.dwSize) return &g_joy2;
    int n = 0;
    auto add = [&](const GUID* g, DWORD ofs, DWORD type, DWORD flags) { g_joy2Objs[n].pguid = g; g_joy2Objs[n].dwOfs = ofs; g_joy2Objs[n].dwType = type; g_joy2Objs[n].dwFlags = flags; n++; };
    for (int i = 0; i < 8; i++) add(kAxisGuids[i], SlotOfs(i), DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION);
    for (int i = 0; i < 4; i++) add(&GUID_POV, PovOfs(i), DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0);
    for (int i = 0; i < 128; i++) add(nullptr, ButtonOfs(i), DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0);
    const DWORD base[3] = { FIELD_OFFSET(DIJOYSTATE2, lVX), FIELD_OFFSET(DIJOYSTATE2, lAX), FIELD_OFFSET(DIJOYSTATE2, lFX) };
    const DWORD aspect[3] = { DIDOI_ASPECTVELOCITY, DIDOI_ASPECTACCEL, DIDOI_ASPECTFORCE };
    for (int k = 0; k < 3; k++) for (int i = 0; i < 8; i++) add(kAxisGuids[i], base[k] + (DWORD)i * 4, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, aspect[k]);
    g_joy2.dwSize = sizeof(DIDATAFORMAT); g_joy2.dwObjSize = sizeof(DIOBJECTDATAFORMAT); g_joy2.dwFlags = DIDF_ABSAXIS;
    g_joy2.dwDataSize = sizeof(DIJOYSTATE2); g_joy2.dwNumObjs = (DWORD)n; g_joy2.rgodf = g_joy2Objs;
    return &g_joy2;
}

IDirectInput8W* InternalDI() {
    if (!g_internal && g_realCreate) {
        HRESULT hr = g_realCreate(g_self, DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&g_internal, nullptr);
        if (FAILED(hr)) { LOG(1, "internal DirectInput8Create failed 0x%08x", hr); g_internal = nullptr; }
    }
    return g_internal;
}

static bool FileExists(const std::wstring& p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }

static void Init() {
    wchar_t path[MAX_PATH] = {}; GetModuleFileNameW(g_self, path, MAX_PATH);
    std::wstring dir = path; size_t sl = dir.find_last_of(L"\\/"); dir = sl == std::wstring::npos ? L"" : dir.substr(0, sl + 1);
    g_dir = dir;
    std::string err;
    g_cfgOk = LoadConfig(dir + L"dimerge.ini", g_cfg, err);
    g_logLevel = g_cfgOk ? g_cfg.logLevel : 1;
    if (g_logLevel > 0) g_log = _wfsopen((dir + L"dimerge.log").c_str(), L"w", _SH_DENYNO);   // shared, so the log can be read while the game runs
    // The real DirectInput normally comes from System32. A game folder that already had a dinput8.dll before
    // dimerge (an ASI loader is the usual case) keeps it as dinput8_chain.dll, and dimerge loads that instead,
    // which forwards to System32 itself and still loads whatever it used to load.
    std::wstring real;
    if (!g_cfg.chain.empty()) real = g_cfg.chain.find(L'\\') == std::wstring::npos && g_cfg.chain.find(L'/') == std::wstring::npos ? dir + g_cfg.chain : g_cfg.chain;
    else if (FileExists(dir + L"dinput8_chain.dll")) real = dir + L"dinput8_chain.dll";
    else { wchar_t sys[MAX_PATH] = {}; GetSystemDirectoryW(sys, MAX_PATH); real = std::wstring(sys) + L"\\dinput8.dll"; }
    g_real = LoadLibraryW(real.c_str());
    if (g_real) {
        g_realCreate = (PFN_DirectInput8Create)GetProcAddress(g_real, "DirectInput8Create");
        g_realCanUnload = (PFN_DllCanUnloadNow)GetProcAddress(g_real, "DllCanUnloadNow");
        g_realGetClassObject = (PFN_DllGetClassObject)GetProcAddress(g_real, "DllGetClassObject");
        g_realRegister = (PFN_DllRegister)GetProcAddress(g_real, "DllRegisterServer");
        g_realUnregister = (PFN_DllRegister)GetProcAddress(g_real, "DllUnregisterServer");
        g_realGetdf = (PFN_GetdfDIJoystick)GetProcAddress(g_real, "GetdfDIJoystick");
    }
    wchar_t exe[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    LOG(1, "dimerge " DIMERGE_VERSION " loaded into %s", Narrow(exe).c_str());
    LOG(1, "real dinput8: %s -> %s", Narrow(real.c_str()).c_str(), g_realCreate ? "ok" : "MISSING");
    if (g_cfgOk && g_cfg.crashProbe) AddVectoredExceptionHandler(1, CrashProbe);
    if (!g_cfgOk) { LOG(1, "dimerge.ini: %s. Passthrough only, nothing merged.", err.c_str()); return; }
    std::string order; for (int s : g_cfg.axisOrder) { if (!order.empty()) order += ","; order += AxisSlotName(s); }
    LOG(1, "config: enabled=%d hideMerged=%d primary=%s merges=%zu axisOrder=%s", g_cfg.enabled, g_cfg.hideMerged, DevIdText(g_cfg.prim).c_str(), g_cfg.merges.size(), order.c_str());
    for (auto& m : g_cfg.merges) {
        std::string ax = m.axesMode == MapNone ? "none" : m.axesMode == MapAuto ? "auto" : "";
        if (m.axesMode == MapExplicit) for (auto& r : m.axes) { if (!ax.empty()) ax += ","; ax += AxisSlotName(r.src); if (r.dst >= 0) { ax += "->"; ax += AxisSlotName(r.dst); } }
        std::string inv; for (int s : m.invert) { if (!inv.empty()) inv += ","; inv += AxisSlotName(s); }
        for (auto& L : m.layers)
            LOG(1, "  merge %s: shift layer: modifier %s button %d, shifted buttons from %d%s", m.name.c_str(),
                L.modSec == -1 ? "own" : L.modSec == -2 ? "wheelbase" : g_cfg.merges[L.modSec].name.c_str(), L.modButton, L.start, L.hide ? ", modifier hidden" : "");
        for (auto& k : m.knobs)
            LOG(1, "  merge %s: knob: buttons %d,%d -> axis %s, %s", m.name.c_str(), k.left, k.right, k.dst < 0 ? "auto" : AxisSlotName(k.dst), k.hold ? "hold" : "pulse");
        char pov[32];
        if (m.povsMode == MapButtons) sprintf_s(pov, m.povsStart < 0 ? "buttons:auto" : "buttons:%d", m.povsStart);
        else if (m.povsMode == MapExplicit) sprintf_s(pov, "start %d", m.povsStart);
        else strcpy_s(pov, m.povsMode == MapNone ? "none" : "auto");
        LOG(1, "  merge %s = %s enabled=%d axes=%s invert=%s buttons=%s%d povs=%s", m.name.c_str(), DevIdText(m.id).c_str(), m.enabled, ax.c_str(), inv.empty() ? "none" : inv.c_str(),
            m.buttonsMode == MapNone ? "none" : m.buttonsMode == MapAuto ? "auto" : "start ", m.buttonsMode == MapExplicit ? m.buttonsStart : 0, pov);
    }
}

// True when a module from the NonExclusiveModules list sits anywhere on the current call stack.
static bool ForeignCaller(std::wstring* who) {
    void* frames[24] = {};
    USHORT n = CaptureStackBackTrace(1, 24, frames, nullptr);
    for (USHORT i = 0; i < n; i++) {
        HMODULE m = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)frames[i], &m) || !m) continue;
        wchar_t path[MAX_PATH] = {}; GetModuleFileNameW(m, path, MAX_PATH);
        std::wstring base = path; size_t sl = base.find_last_of(L"\\/"); if (sl != std::wstring::npos) base = base.substr(sl + 1);
        for (auto& c : base) c = (wchar_t)towlower(c);
        for (auto& f : g_cfg.nonExclusiveModules) if (base == f) { if (who) *who = base; return true; }
    }
    return false;
}

static const char* IidName(REFIID r) {
    if (r == IID_IDirectInput8W) return "IDirectInput8W"; if (r == IID_IDirectInput8A) return "IDirectInput8A"; return "other";
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    std::call_once(g_once, Init);
    if (!g_realCreate) return DIERR_GENERIC;
    HRESULT hr = g_realCreate(hinst, ver, riid, out, outer);
    std::wstring who; bool foreign = g_cfgOk && ForeignCaller(&who);
    LOG(1, "DirectInput8Create(version=%08x, %s) -> 0x%08x%s%s", ver, IidName(riid), hr, foreign ? " from foreign module " : "", foreign ? Narrow(who.c_str()).c_str() : "");
    if (FAILED(hr) || !out || !*out || !g_cfgOk || !g_cfg.enabled) return hr;
    void* wrapped = nullptr;
    if (SUCCEEDED(CreateWrappedDI(riid, *out, &wrapped, foreign))) *out = wrapped;
    return hr;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() { std::call_once(g_once, Init); return g_realCanUnload ? g_realCanUnload() : S_FALSE; }
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) { std::call_once(g_once, Init); return g_realGetClassObject ? g_realGetClassObject(rclsid, riid, ppv) : CLASS_E_CLASSNOTAVAILABLE; }
extern "C" HRESULT WINAPI DllRegisterServer() { std::call_once(g_once, Init); return g_realRegister ? g_realRegister() : E_FAIL; }
extern "C" HRESULT WINAPI DllUnregisterServer() { std::call_once(g_once, Init); return g_realUnregister ? g_realUnregister() : E_FAIL; }
extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick() { std::call_once(g_once, Init); return g_realGetdf ? g_realGetdf() : nullptr; }

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_self = hinst; DisableThreadLibraryCalls(hinst); }
    else if (reason == DLL_PROCESS_DETACH) { if (g_log) { LOG(1, "unloading"); fclose(g_log); g_log = nullptr; } }
    return TRUE;
}
