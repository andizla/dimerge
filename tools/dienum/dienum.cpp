// dienum: list every attached DirectInput8 game controller with type, VID/PID, caps and objects.
// "dienum watch VID:PID [seconds]" prints live DIJOYSTATE2 values of one device.
#define DIRECTINPUT_VERSION 0x0800
#define INITGUID
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "ole32.lib")

static const wchar_t* Axis(const GUID& g) {
    if (g == GUID_XAxis) return L"X";   if (g == GUID_YAxis) return L"Y";   if (g == GUID_ZAxis) return L"Z";
    if (g == GUID_RxAxis) return L"Rx"; if (g == GUID_RyAxis) return L"Ry"; if (g == GUID_RzAxis) return L"Rz";
    if (g == GUID_Slider) return L"Slider"; if (g == GUID_Button) return L"Button"; if (g == GUID_POV) return L"POV";
    if (g == GUID_Key) return L"Key"; return L"?";
}
static BOOL CALLBACK EnumObj(LPCDIDEVICEOBJECTINSTANCEW o, LPVOID) {
    const wchar_t* t = (o->dwType & DIDFT_AXIS) ? L"axis" : (o->dwType & DIDFT_BUTTON) ? L"button" : (o->dwType & DIDFT_POV) ? L"pov" : L"other";
    wprintf(L"    %-6s inst=%3u ofs=%4u %-6s flags=%08x usage=%04x/%04x ff=%u \"%s\"\n", t, DIDFT_GETINSTANCE(o->dwType), o->dwOfs, Axis(o->guidType), o->dwFlags, o->wUsagePage, o->wUsage, o->dwFFMaxForce, o->tszName);
    return DIENUM_CONTINUE;
}
static BOOL CALLBACK EnumEff(LPCDIEFFECTINFOW e, LPVOID) {
    wprintf(L"    effect \"%s\" type=%08x static=%08x dynamic=%08x\n", e->tszName, e->dwEffType, e->dwStaticParams, e->dwDynamicParams);
    return DIENUM_CONTINUE;
}
static BOOL CALLBACK EnumDev(LPCDIDEVICEINSTANCEW d, LPVOID p) {
    IDirectInput8W* di = (IDirectInput8W*)p;
    wchar_t gi[64], gp[64]; StringFromGUID2(d->guidInstance, gi, 64); StringFromGUID2(d->guidProduct, gp, 64);
    wprintf(L"\n== \"%s\" product=\"%s\"\n   devType=%08x (type 0x%02x subtype 0x%02x) usage=%04x/%04x\n   inst=%s prod=%s\n",
        d->tszInstanceName, d->tszProductName, d->dwDevType, d->dwDevType & 0xff, (d->dwDevType >> 8) & 0xff, d->wUsagePage, d->wUsage, gi, gp);
    IDirectInputDevice8W* dev = nullptr;
    HRESULT hr = di->CreateDevice(d->guidInstance, &dev, nullptr);
    if (FAILED(hr)) { wprintf(L"   CreateDevice failed 0x%08x\n", hr); return DIENUM_CONTINUE; }
    HRESULT hf = dev->SetDataFormat(&c_dfDIJoystick2);
    wprintf(L"   SetDataFormat(c_dfDIJoystick2) -> 0x%08x\n", hf);
    {
        wprintf(L"   occupied DIJOYSTATE2 offsets:");
        for (DWORD o = 0; o < 48; o += 4) {
            DIDEVICEOBJECTINSTANCEW oi = {}; oi.dwSize = sizeof(oi);
            if (SUCCEEDED(dev->GetObjectInfo(&oi, o, DIPH_BYOFFSET))) wprintf(L" %u=%s", o, oi.tszName);
        }
        int nb = 0, first = -1, last = -1;
        for (DWORD o = 48; o < 176; o++) {
            DIDEVICEOBJECTINSTANCEW oi = {}; oi.dwSize = sizeof(oi);
            if (SUCCEEDED(dev->GetObjectInfo(&oi, o, DIPH_BYOFFSET))) { nb++; if (first < 0) first = (int)o; last = (int)o; }
        }
        wprintf(L" buttons=%d (ofs %d..%d)\n", nb, first, last);
    }
    DIPROPDWORD pd = {}; pd.diph.dwSize = sizeof(pd); pd.diph.dwHeaderSize = sizeof(DIPROPHEADER); pd.diph.dwHow = DIPH_DEVICE;
    if (SUCCEEDED(dev->GetProperty(DIPROP_VIDPID, &pd.diph))) wprintf(L"   VID_%04X PID_%04X\n", LOWORD(pd.dwData), HIWORD(pd.dwData));
    DIPROPGUIDANDPATH gpath = {}; gpath.diph.dwSize = sizeof(gpath); gpath.diph.dwHeaderSize = sizeof(DIPROPHEADER); gpath.diph.dwHow = DIPH_DEVICE;
    if (SUCCEEDED(dev->GetProperty(DIPROP_GUIDANDPATH, &gpath.diph))) wprintf(L"   path=%s\n", gpath.wszPath);
    DIDEVCAPS caps = {}; caps.dwSize = sizeof(caps);
    if (SUCCEEDED(dev->GetCapabilities(&caps)))
        wprintf(L"   caps: axes=%u buttons=%u povs=%u flags=%08x%s ffSamplePeriod=%u ffMinTimeRes=%u\n", caps.dwAxes, caps.dwButtons, caps.dwPOVs, caps.dwFlags,
            (caps.dwFlags & DIDC_FORCEFEEDBACK) ? L" FORCEFEEDBACK" : L"", caps.dwFFSamplePeriod, caps.dwFFMinTimeResolution);
    dev->EnumObjects(EnumObj, nullptr, DIDFT_ALL);
    if (caps.dwFlags & DIDC_FORCEFEEDBACK) dev->EnumEffects(EnumEff, nullptr, DIEFT_ALL);
    dev->Release();
    return DIENUM_CONTINUE;
}
struct WatchCtx { DWORD vidpid; IDirectInput8W* di; IDirectInputDevice8W* dev; };
static BOOL CALLBACK EnumWatch(LPCDIDEVICEINSTANCEW d, LPVOID p) {
    WatchCtx* w = (WatchCtx*)p;
    if (d->guidProduct.Data1 != w->vidpid) return DIENUM_CONTINUE;
    if (SUCCEEDED(w->di->CreateDevice(d->guidInstance, &w->dev, nullptr))) return DIENUM_STOP;
    return DIENUM_CONTINUE;
}
static int Watch(const wchar_t* vp, int seconds) {
    unsigned vid = 0, pid = 0; swscanf_s(vp, L"%x:%x", &vid, &pid);
    WatchCtx w = { (DWORD)((pid << 16) | vid), nullptr, nullptr };
    if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&w.di, nullptr))) return 1;
    w.di->EnumDevices(DI8DEVCLASS_ALL, EnumWatch, &w, DIEDFL_ATTACHEDONLY);
    if (!w.dev) { wprintf(L"device %s not found\n", vp); return 1; }
    w.dev->SetDataFormat(&c_dfDIJoystick2);
    w.dev->SetCooperativeLevel(GetConsoleWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    w.dev->Acquire();
    DIJOYSTATE2 last = {}; bool first = true; DWORD end = GetTickCount() + (DWORD)seconds * 1000;
    while (GetTickCount() < end) {
        DIJOYSTATE2 s = {}; w.dev->Poll();
        if (w.dev->GetDeviceState(sizeof(s), &s) != DI_OK) { w.dev->Acquire(); Sleep(50); continue; }
        if (first || memcmp(&s, &last, sizeof(s)) != 0) {
            wprintf(L"X=%6ld Y=%6ld Z=%6ld Rx=%6ld Ry=%6ld Rz=%6ld S0=%6ld S1=%6ld POV=%6ld btn:", s.lX, s.lY, s.lZ, s.lRx, s.lRy, s.lRz, s.rglSlider[0], s.rglSlider[1], (long)s.rgdwPOV[0]);
            for (int i = 0; i < 128; i++) if (s.rgbButtons[i] & 0x80) wprintf(L" %d", i);
            wprintf(L"\n"); last = s; first = false;
        }
        Sleep(20);
    }
    w.dev->Unacquire(); w.dev->Release(); w.di->Release(); return 0;
}
int wmain(int argc, wchar_t** argv) {
    if (argc > 2 && wcscmp(argv[1], L"watch") == 0) return Watch(argv[2], argc > 3 ? _wtoi(argv[3]) : 10);
    IDirectInput8W* di = nullptr;
    HRESULT hr = DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&di, nullptr);
    if (FAILED(hr)) { wprintf(L"DirectInput8Create failed 0x%08x\n", hr); return 1; }
    DWORD cls = (argc > 1 && wcscmp(argv[1], L"all") == 0) ? DI8DEVCLASS_ALL : DI8DEVCLASS_GAMECTRL;
    hr = di->EnumDevices(cls, EnumDev, di, DIEDFL_ATTACHEDONLY);
    wprintf(L"\nEnumDevices -> 0x%08x\n", hr);
    di->Release();
    return 0;
}
