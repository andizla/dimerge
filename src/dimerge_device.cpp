// SPDX-License-Identifier: GPL-3.0-only
// dimerge_device.cpp: the IDirectInput8 and IDirectInputDevice8 wrappers, in both A and W flavours.
#include "dimerge.h"
#include <deque>
#include <mutex>
#include <algorithm>
#include <stdexcept>

// ---------- A / W traits ----------
template<bool W> struct TR;
template<> struct TR<true> {
    using IDI = IDirectInput8W; using IDev = IDirectInputDevice8W;
    using DevInst = DIDEVICEINSTANCEW; using ObjInst = DIDEVICEOBJECTINSTANCEW; using EffInfo = DIEFFECTINFOW;
    using EnumDevCB = LPDIENUMDEVICESCALLBACKW; using EnumObjCB = LPDIENUMDEVICEOBJECTSCALLBACKW; using EnumEffCB = LPDIENUMEFFECTSCALLBACKW;
    using EnumSemCB = LPDIENUMDEVICESBYSEMANTICSCBW; using ActionFmt = DIACTIONFORMATW; using ImgHdr = DIDEVICEIMAGEINFOHEADERW; using CfgParams = DICONFIGUREDEVICESPARAMSW;
    using Char = WCHAR;
    static const IID& IidDI() { return IID_IDirectInput8W; }
    static const IID& IidDev() { return IID_IDirectInputDevice8W; }
    static void CopyName(Char* dst, size_t n, const wchar_t* src) { wcsncpy_s(dst, n, src, _TRUNCATE); }
    static std::string Str(const Char* s) { return Narrow(s); }
};
template<> struct TR<false> {
    using IDI = IDirectInput8A; using IDev = IDirectInputDevice8A;
    using DevInst = DIDEVICEINSTANCEA; using ObjInst = DIDEVICEOBJECTINSTANCEA; using EffInfo = DIEFFECTINFOA;
    using EnumDevCB = LPDIENUMDEVICESCALLBACKA; using EnumObjCB = LPDIENUMDEVICEOBJECTSCALLBACKA; using EnumEffCB = LPDIENUMEFFECTSCALLBACKA;
    using EnumSemCB = LPDIENUMDEVICESBYSEMANTICSCBA; using ActionFmt = DIACTIONFORMATA; using ImgHdr = DIDEVICEIMAGEINFOHEADERA; using CfgParams = DICONFIGUREDEVICESPARAMSA;
    using Char = CHAR;
    static const IID& IidDI() { return IID_IDirectInput8A; }
    static const IID& IidDev() { return IID_IDirectInputDevice8A; }
    static void CopyName(Char* dst, size_t n, const wchar_t* src) { WideCharToMultiByte(CP_ACP, 0, src, -1, dst, (int)n, nullptr, nullptr); dst[n - 1] = 0; }
    static std::string Str(const Char* s) { return s; }
};

static const char* KindName(Kind k) { return k == Kind::Axis ? "axis" : k == Kind::Button ? "button" : "pov"; }
static DWORD KindTypeBits(Kind k) { return k == Kind::Axis ? DIDFT_AXIS : k == Kind::Button ? DIDFT_BUTTON : DIDFT_POV; }
static DWORD KindSynthType(Kind k) { return k == Kind::Axis ? DIDFT_ABSAXIS : k == Kind::Button ? DIDFT_PSHBUTTON : DIDFT_POV; }

// ---------- a merged (secondary) device and its objects ----------
struct SrcObj { Kind kind; int index; DWORD srcOfs; DWORD srcType; GUID guid; std::wstring name; WORD usagePage, usage; };
struct Secondary {
    IDirectInputDevice8W* dev = nullptr; const DevSpec* spec = nullptr; std::wstring name;
    std::vector<SrcObj> objs; DIJOYSTATE2 state{}; DIJOYSTATE2 prev{};
    bool acquired = false, haveState = false; DWORD nextAcquireTry = 0; DWORD lostLogged = 0;
    bool buffered = false;                    // buffered input on, so knob clicks between two game polls are not lost
    signed char latch[128], prevLatch[128];   // per source button: layer chosen when it engaged (-1 released, 0 plain, N layer N)
    struct KnobState { int dir = 0; DWORD until = 0; int clicks = 0; DWORD lastOut = 0; bool haveOut = false; };
    std::vector<KnobState> knobs;             // one per Knob rule of the spec
    Secondary() { memset(latch, -1, sizeof(latch)); memset(prevLatch, -1, sizeof(prevLatch)); }
};
struct Injected {
    int sec, obj; Kind kind; int slot;      // slot: axis 0..7, pov 0..3 or button 0..127 in the game's format
    DWORD gameOfs = 0xFFFFFFFF; bool overrides = false; DWORD id = 0; std::wstring name;
    int povDir = -1;                        // >= 0: this button is one direction of a hat (0 up, 1 right, 2 down, 3 left)
    bool invert = false;                    // axis direction flipped inside the range the game set
    int layer = -1;                         // buttons of a device with shift layers: 0 plain, N = layer N
    int knob = -1;                          // >= 0: an axis driven by a rotary knob (index into the spec's Knob rules); no source object then
    UINT_PTR appData = (UINT_PTR)-1;        // DIPROP_APPDATA the client set on this input, echoed in its events
};
struct FmtEntry { Kind kind; int slot; DWORD ofs; bool occupied = false; DWORD primId = 0; };

// ---------- the merged device ----------
template<bool W>
class MergedDevice final : public TR<W>::IDev {
    using T = TR<W>;
    using IDev = typename T::IDev;
    ULONG m_ref = 1;
    IDev* m_prim;
    typename T::DevInst m_inst;
    std::vector<Secondary> m_secs;
    std::vector<Injected> m_inj;
    std::vector<FmtEntry> m_entries;              // the game's format, classified
    std::vector<DIOBJECTDATAFORMAT> m_fmtObjs; std::vector<GUID> m_fmtGuids; DIDATAFORMAT m_fmt{};
    bool m_haveFmt = false;
    DWORD m_bufSize = 0; std::deque<DIDEVICEOBJECTDATA> m_events; DWORD m_seq = 0x40000000; bool m_overflow = false;
    std::recursive_mutex m_mx;
    bool m_foreign = false;                       // created by a mod, not by the game: never exclusive
    DWORD m_nextRescan = 0;                       // next look for merged devices that were absent so far
    unsigned m_stateCalls = 0, m_dataCalls = 0, m_effects = 0;
    std::vector<BYTE> m_primBuf; bool m_primNeeded = false;   // the wheelbase's last raw state, kept when a layer's modifier sits on it
    LONG m_rangeMin[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, m_rangeMax[8] = { 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535 };   // per axis slot, as the game set them

public:
    MergedDevice(IDev* prim, const typename T::DevInst& inst, bool foreign) : m_prim(prim), m_inst(inst), m_foreign(foreign) {
        LOG(2, "merged device: opening the merged devices");
        OpenSecondaries();
        LOG(2, "merged device: %zu merged devices open, setting the format", m_secs.size());
        CopyFormat(Joy2Format());
        if (SUCCEEDED(m_prim->SetDataFormat(Joy2Format()))) { m_haveFmt = true; ComputeMapping(); }
        else LOG(1, "primary SetDataFormat(joystick2) failed at construction; mapping waits for the game's format");
    }
    ~MergedDevice() {
        for (auto& s : m_secs) if (s.dev) { s.dev->Unacquire(); s.dev->Release(); }
        if (m_prim) m_prim->Release();
    }

private:
    // --- secondaries
    struct EnumCtx { MergedDevice* self; std::vector<int> seen; EnumCtx(MergedDevice* s) : self(s), seen(Cfg().merges.size(), 0) {} };
    static BOOL CALLBACK EnumSecCB(LPCDIDEVICEINSTANCEW d, LPVOID p) {
        EnumCtx* ctx = (EnumCtx*)p; MergedDevice* self = ctx->self;
        for (size_t si = 0; si < Cfg().merges.size(); si++) {
            const DevSpec& spec = Cfg().merges[si];
            if (!spec.enabled || !IdMatches(spec.id, d->guidProduct, d->guidInstance, ctx->seen[si])) continue;
            bool dup = false; for (auto& s : self->m_secs) if (s.spec == &spec) dup = true;
            if (dup) continue;
            IDirectInputDevice8W* dev = nullptr;
            HRESULT hr = InternalDI()->CreateDevice(d->guidInstance, &dev, nullptr);
            if (FAILED(hr)) { LOG(1, "merge %s: CreateDevice failed 0x%08x", spec.name.c_str(), hr); continue; }
            Secondary s; s.dev = dev; s.spec = &spec; s.name = d->tszProductName;
            dev->SetDataFormat(Joy2Format());
            dev->SetCooperativeLevel(nullptr, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
            if (!spec.knobs.empty()) {
                DIPROPDWORD bp{}; bp.diph.dwSize = sizeof(bp); bp.diph.dwHeaderSize = sizeof(DIPROPHEADER); bp.diph.dwHow = DIPH_DEVICE; bp.dwData = 64;
                s.buffered = SUCCEEDED(dev->SetProperty(DIPROP_BUFFERSIZE, &bp.diph));
            }
            for (int i = 0; i < 8 + 4 + 128; i++) {
                Kind k = i < 8 ? Kind::Axis : i < 12 ? Kind::Pov : Kind::Button;
                int idx = i < 8 ? i : i < 12 ? i - 8 : i - 12;
                DWORD ofs = k == Kind::Axis ? SlotOfs(idx) : k == Kind::Pov ? PovOfs(idx) : ButtonOfs(idx);
                DIDEVICEOBJECTINSTANCEW oi{}; oi.dwSize = sizeof(oi);
                if (FAILED(dev->GetObjectInfo(&oi, ofs, DIPH_BYOFFSET))) continue;
                s.objs.push_back(SrcObj{ k, idx, ofs, oi.dwType, oi.guidType, oi.tszName, oi.wUsagePage, oi.wUsage });
            }
            int na = 0, nb = 0, np = 0; for (auto& o : s.objs) (o.kind == Kind::Axis ? na : o.kind == Kind::Button ? nb : np)++;
            LOG(1, "merge %s: opened \"%s\" %s axes=%d buttons=%d povs=%d", spec.name.c_str(), Narrow(d->tszProductName).c_str(), GuidStr(d->guidInstance).c_str(), na, nb, np);
            s.knobs.resize(spec.knobs.size());
            self->m_secs.push_back(std::move(s));
        }
        return DIENUM_CONTINUE;
    }
    void OpenSecondaries() {
        IDirectInput8W* di = InternalDI();
        if (!di) { LOG(1, "no internal DirectInput; nothing merged"); return; }
        EnumCtx c(this);
        di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumSecCB, &c, DIEDFL_ATTACHEDONLY);
        for (const DevSpec& spec : Cfg().merges) {
            bool found = false; for (auto& s : m_secs) if (s.spec == &spec) found = true;
            if (spec.enabled && !found) LOG(1, "merge %s: device %s is not attached, skipped (looked for again every 2 s)", spec.name.c_str(), DevIdText(spec.id).c_str());
        }
    }

    // a configured device that was absent at creation is looked for every two seconds
    void RescanMissing() {
        bool missing = false;
        for (const DevSpec& spec : Cfg().merges) { if (!spec.enabled) continue; bool found = false; for (auto& s : m_secs) if (s.spec == &spec) found = true; if (!found) missing = true; }
        if (!missing) return;
        DWORD now = GetTickCount();
        if ((LONG)(now - m_nextRescan) < 0) return;
        m_nextRescan = now + 2000;
        IDirectInput8W* di = InternalDI();
        if (!di) return;
        size_t before = m_secs.size();
        EnumCtx c(this);
        di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumSecCB, &c, DIEDFL_ATTACHEDONLY);
        if (m_secs.size() != before) { LOG(1, "a merged device appeared late, mapping rebuilt"); ComputeMapping(); }
    }

    // --- the game's data format
    void CopyFormat(LPCDIDATAFORMAT f) {
        m_fmt = *f; m_fmtObjs.assign(f->rgodf, f->rgodf + f->dwNumObjs); m_fmtGuids.clear(); m_fmtGuids.reserve(f->dwNumObjs);
        for (auto& o : m_fmtObjs) if (o.pguid) { m_fmtGuids.push_back(*o.pguid); o.pguid = &m_fmtGuids.back(); }
        m_fmt.rgodf = m_fmtObjs.data();
        // classify entries into slots
        m_entries.clear();
        int sliders = 0, genericAxes = 0, povs = 0, buttons = 0; bool axisUsed[8] = {};
        for (auto& o : m_fmtObjs) {
            DWORD t = DIDFT_GETTYPE(o.dwType), aspect = o.dwFlags & DIDOI_ASPECTMASK;
            if ((t & DIDFT_AXIS) && (aspect == 0 || aspect == DIDOI_ASPECTPOSITION)) {
                int slot = -1;
                if (o.pguid) {
                    const GUID& g = *o.pguid;
                    if (g == GUID_XAxis) slot = 0; else if (g == GUID_YAxis) slot = 1; else if (g == GUID_ZAxis) slot = 2;
                    else if (g == GUID_RxAxis) slot = 3; else if (g == GUID_RyAxis) slot = 4; else if (g == GUID_RzAxis) slot = 5;
                    else if (g == GUID_Slider) slot = 6 + (sliders++);
                } else { while (genericAxes < 8 && axisUsed[genericAxes]) genericAxes++; slot = genericAxes < 8 ? genericAxes : -1; }
                if (slot < 0 || slot > 7 || axisUsed[slot]) continue;
                axisUsed[slot] = true; m_entries.push_back(FmtEntry{ Kind::Axis, slot, o.dwOfs });
            } else if (t & DIDFT_POV) { if (povs < 4) m_entries.push_back(FmtEntry{ Kind::Pov, povs++, o.dwOfs }); }
            else if (t & DIDFT_BUTTON) { if (buttons < 128) m_entries.push_back(FmtEntry{ Kind::Button, buttons++, o.dwOfs }); }
        }
    }
    FmtEntry* Entry(Kind k, int slot) { for (auto& e : m_entries) if (e.kind == k && e.slot == slot) return &e; return nullptr; }
    static bool IsDead(int slot) { for (int d : Cfg().deadAxes) if (d == slot) return true; return false; }
    bool Taken(Kind k, int slot) { for (auto& i : m_inj) if (i.kind == k && i.slot == slot) return true; return false; }

    int m_skipped = 0;
    // the first free axis slot in the configured preference order (unoccupied, or occupied but declared dead)
    int FreeAxisSlot() {
        for (int slot : Cfg().axisOrder) {
            FmtEntry* e = Entry(Kind::Axis, slot);
            if (!e || Taken(Kind::Axis, slot) || (e->occupied && !IsDead(slot))) continue;
            return slot;
        }
        return -1;
    }
    // the first run of n free button slots
    int FreeButtonRun(int n) {
        for (int start = 0; start + n <= 128; start++) {
            bool ok = true;
            for (int k = 0; k < n && ok; k++) { FmtEntry* e = Entry(Kind::Button, start + k); if (!e || e->occupied || Taken(Kind::Button, start + k)) ok = false; }
            if (ok) return start;
        }
        return -1;
    }

    void ComputeMapping() {
        m_skipped = 0;
        m_primNeeded = false;
        for (auto& m : Cfg().merges) for (auto& L : m.layers) if (L.modSec == -2) m_primNeeded = true;
        // which format slots does the primary occupy?
        DWORD maxInst[3] = { 0, 0, 0 };
        for (auto& e : m_entries) {
            typename T::ObjInst oi{}; oi.dwSize = sizeof(oi);
            e.occupied = SUCCEEDED(m_prim->GetObjectInfo(&oi, e.ofs, DIPH_BYOFFSET));
            e.primId = e.occupied ? oi.dwType : 0;
            if (e.occupied) { DWORD in = DIDFT_GETINSTANCE(oi.dwType) + 1; DWORD& m = maxInst[(int)e.kind]; if (in > m) m = in; }
        }
        m_inj.clear();
        for (int si = 0; si < (int)m_secs.size(); si++) {
            Secondary& s = m_secs[si]; const DevSpec& spec = *s.spec;
            static const char* dirNames[4] = { " up", " right", " down", " left" };
            auto add = [&](int objIdx, Kind k, int slot, bool explicitRule, int povDir = -1, int layer = -1) {
                FmtEntry* e = Entry(k, slot);
                if (!e) { m_skipped++; LOG(1, "merge %s: %s %d has no %s slot %d in the game's format, skipped", spec.name.c_str(), KindName(k), s.objs[objIdx].index, KindName(k), slot); return; }
                if (Taken(k, slot)) { m_skipped++; LOG(1, "merge %s: %s slot %d already taken by another merged input, skipped", spec.name.c_str(), KindName(k), slot); return; }
                if (e->occupied && k == Kind::Axis && !IsDead(slot) && !explicitRule) return;
                Injected in; in.sec = si; in.obj = objIdx; in.kind = k; in.slot = slot; in.gameOfs = e->ofs; in.overrides = e->occupied;
                in.id = DIDFT_MAKEINSTANCE(maxInst[(int)k] + (DWORD)m_inj.size()) | KindSynthType(k);
                in.name = s.name + L" " + s.objs[objIdx].name;
                in.povDir = povDir; in.layer = layer;
                in.invert = k == Kind::Axis && std::find(spec.invert.begin(), spec.invert.end(), s.objs[objIdx].index) != spec.invert.end();
                if (povDir >= 0) { static const wchar_t* dirW[4] = { L" up", L" right", L" down", L" left" }; in.name += dirW[povDir]; }
                if (layer == 1) in.name += L" (shifted)";
                else if (layer > 1) { wchar_t ln[24]; swprintf_s(ln, L" (layer %d)", layer); in.name += ln; }
                m_inj.push_back(in);
                char mod[12] = ""; if (layer == 1) strcpy_s(mod, " +mod"); else if (layer > 1) sprintf_s(mod, " +mod%d", layer);
                LOG(1, "  %-14s %-6s %-3d%-6s -> game %-6s slot %-3d ofs %-3u%s%s%s%s", spec.name.c_str(), povDir >= 0 ? "hat" : KindName(k), s.objs[objIdx].index, povDir >= 0 ? dirNames[povDir] : mod, KindName(k), slot, e->ofs,
                    e->occupied ? " (takes over the wheelbase's own input there)" : "", (e->occupied && k == Kind::Button) ? " (OR-ed)" : "", in.invert ? " (inverted)" : "", layer >= 1 ? " (shift layer)" : "");
            };
            for (int oi = 0; oi < (int)s.objs.size(); oi++) {
                const SrcObj& o = s.objs[oi];
                if (o.kind == Kind::Axis) {
                    if (spec.axesMode == MapNone) continue;
                    bool wanted = spec.axesMode == MapAuto; int dst = -1;
                    if (spec.axesMode == MapExplicit) for (auto& r : spec.axes) if (r.src == o.index) { wanted = true; dst = r.dst; }
                    if (!wanted) continue;
                    if (dst < 0) dst = FreeAxisSlot();
                    if (dst < 0) { m_skipped++; LOG(1, "merge %s: no free axis slot for %s, skipped (name a dead wheelbase axis in DeadAxes, or an explicit Axes rule)", spec.name.c_str(), Narrow(o.name.c_str()).c_str()); continue; }
                    add(oi, Kind::Axis, dst, spec.axesMode == MapExplicit);
                } else if (o.kind == Kind::Button) {
                    if (spec.buttonsMode == MapNone) continue;
                    bool ownMod = false, hidden = false;
                    for (auto& L : spec.layers) if (L.modSec == -1 && L.modButton == o.index) { ownMod = true; if (L.hide) hidden = true; }
                    if (hidden) continue;
                    bool layered = !spec.layers.empty() && !ownMod;   // a modifier of its own device keeps its plain number only
                    if (layered) for (int li = 0; li < (int)spec.layers.size(); li++) add(oi, Kind::Button, spec.layers[li].start + o.index, true, -1, li + 1);
                    if (spec.buttonsMode == MapExplicit) { add(oi, Kind::Button, spec.buttonsStart + o.index, true, -1, layered ? 0 : -1); continue; }
                    int slot = FreeButtonRun(1);
                    if (slot < 0) { m_skipped++; LOG(1, "merge %s: no free button slot for button %d, the 128 slots are used up", spec.name.c_str(), o.index); continue; }
                    add(oi, Kind::Button, slot, false, -1, layered ? 0 : -1);
                } else {
                    if (spec.povsMode == MapNone) continue;
                    if (spec.povsMode == MapButtons) {
                        int start = spec.povsStart >= 0 ? spec.povsStart + o.index * 4 : FreeButtonRun(4);
                        if (start < 0) { m_skipped++; LOG(1, "merge %s: no run of four free buttons for hat %d", spec.name.c_str(), o.index); continue; }
                        for (int d = 0; d < 4; d++) add(oi, Kind::Button, start + d, true, d);
                        continue;
                    }
                    if (spec.povsMode == MapExplicit) { add(oi, Kind::Pov, spec.povsStart + o.index, true); continue; }
                    bool placed = false;
                    for (int slot = 0; slot < 4 && !placed; slot++) { FmtEntry* e = Entry(Kind::Pov, slot); if (!e || e->occupied || Taken(Kind::Pov, slot)) continue; add(oi, Kind::Pov, slot, false); placed = true; }
                    if (!placed) { m_skipped++; LOG(1, "merge %s: no free hat slot for hat %d (POVs=buttons:auto turns it into four buttons instead)", spec.name.c_str(), o.index); }
                }
            }
            // knobs: an axis of the game's format driven by two of this device's buttons, no device object behind it
            for (int ki = 0; ki < (int)spec.knobs.size(); ki++) {
                const Knob& kn = spec.knobs[ki];
                int dst = kn.dst >= 0 ? kn.dst : FreeAxisSlot();
                FmtEntry* e = dst >= 0 ? Entry(Kind::Axis, dst) : nullptr;
                if (!e) { m_skipped++; LOG(1, "merge %s: no axis slot for knob %d,%d (%s)", spec.name.c_str(), kn.left, kn.right, dst < 0 ? "none free" : "not in the game's format"); continue; }
                if (Taken(Kind::Axis, dst)) { m_skipped++; LOG(1, "merge %s: axis slot %d for knob %d,%d already taken by another merged input", spec.name.c_str(), dst, kn.left, kn.right); continue; }
                Injected in; in.sec = si; in.obj = -1; in.knob = ki; in.kind = Kind::Axis; in.slot = dst; in.gameOfs = e->ofs; in.overrides = e->occupied;
                in.id = DIDFT_MAKEINSTANCE(maxInst[0] + (DWORD)m_inj.size()) | KindSynthType(Kind::Axis);
                wchar_t nm[48]; swprintf_s(nm, L" knob %d/%d", kn.left, kn.right); in.name = s.name + nm;
                m_inj.push_back(in);
                LOG(1, "  %-14s knob   %-3d/%-4d -> game axis   slot %-3d ofs %-3u (%s)%s", spec.name.c_str(), kn.left, kn.right, dst, e->ofs,
                    kn.hold ? "hold" : "pulse", e->occupied ? " (takes over the wheelbase's own input there)" : "");
            }
        }
        LOG(1, "mapping: %zu merged inputs into the wheelbase (format %u bytes, %u entries)%s", m_inj.size(), m_fmt.dwDataSize, m_fmt.dwNumObjs, m_skipped ? "" : ", nothing left out");
        if (m_skipped) LOG(1, "mapping: %d merged inputs could not be placed, see the lines above", m_skipped);
    }

    Injected* Find(DWORD how, DWORD obj) {
        if (how == DIPH_BYOFFSET) { for (auto& i : m_inj) if (i.gameOfs == obj) return &i; }
        else if (how == DIPH_BYID) {
            for (auto& i : m_inj) if (i.id == obj) return &i;
            for (auto& e : m_entries) if (e.occupied && e.primId == obj) { for (auto& i : m_inj) if (i.gameOfs == e.ofs) return &i; }
        }
        return nullptr;
    }
    static bool PovCentered(DWORD v) { return LOWORD(v) == 0xFFFF; }
    static DWORD ValueIn(const DIJOYSTATE2& st, const SrcObj& o, int povDir, int layer = -1, const signed char* latch = nullptr) {
        if (o.kind == Kind::Button) { if (!(st.rgbButtons[o.index] & 0x80)) return 0; if (layer >= 0 && latch && latch[o.index] != layer) return 0; return 0x80; }
        DWORD v; memcpy(&v, (const BYTE*)&st + o.srcOfs, 4);
        if (povDir < 0) return v;
        if (PovCentered(v)) return 0;
        DWORD a = v % 36000;   // hundredths of a degree, 0 = up, clockwise; a diagonal presses two directions
        bool hit = povDir == 0 ? (a >= 31500 || a <= 4500) : povDir == 1 ? (a >= 4500 && a <= 13500) : povDir == 2 ? (a >= 13500 && a <= 22500) : (a >= 22500 && a <= 31500);
        return hit ? 0x80 : 0;
    }
    bool PrimaryButton(int n) const {
        for (auto& e : m_entries) if (e.kind == Kind::Button && e.slot == n) return e.occupied && e.ofs < m_primBuf.size() && (m_primBuf[e.ofs] & 0x80) != 0;
        return false;
    }
    bool ModifierHeld(const Layer& L, const DIJOYSTATE2& own) const {
        if (L.modButton < 0 || L.modButton > 127) return false;
        if (L.modSec == -1) return (own.rgbButtons[L.modButton] & 0x80) != 0;
        if (L.modSec == -2) return PrimaryButton(L.modButton);
        if (L.modSec < 0 || L.modSec >= (int)Cfg().merges.size()) return false;
        for (auto& o : m_secs) if (o.spec == &Cfg().merges[L.modSec]) return o.haveState && (o.state.rgbButtons[L.modButton] & 0x80) != 0;
        return false;
    }
    // the axis value a knob stands at: pulse mode deflects fully for a moment after a click, hold mode counts clicks
    DWORD KnobOut(const Secondary& s, const Injected& i) const {
        const Knob& kn = s.spec->knobs[i.knob]; const Secondary::KnobState& ks = s.knobs[i.knob];
        LONGLONG mn = m_rangeMin[i.slot], mx = m_rangeMax[i.slot], centre = (mn + mx) / 2;
        if (kn.hold) { LONGLONG v = centre + (mx - mn) * kn.stepPct / 100 * ks.clicks; return (DWORD)(LONG)std::max(mn, std::min(mx, v)); }
        if ((LONG)(GetTickCount() - ks.until) < 0) return (DWORD)(LONG)(ks.dir > 0 ? mx : mn);
        return (DWORD)(LONG)centre;
    }
    DWORD OutValue(const Injected& i, const Secondary& s, const DIJOYSTATE2& st, bool previous = false) const {
        if (i.knob >= 0) return KnobOut(s, i);
        DWORD v = ValueIn(st, s.objs[i.obj], i.povDir, i.layer, previous ? s.prevLatch : s.latch);
        if (i.invert && i.kind == Kind::Axis) v = (DWORD)(m_rangeMin[i.slot] + m_rangeMax[i.slot] - (LONG)v);
        return v;
    }
    DWORD SrcValue(const Secondary& s, const Injected& i) const { return OutValue(i, s, s.state); }

    // --- polling the merged devices and generating buffered events
    void Refresh() {
        RescanMissing();
        DWORD now = GetTickCount();
        for (int si = 0; si < (int)m_secs.size(); si++) {
            Secondary& s = m_secs[si];
            if (!s.dev) continue;
            if (!s.acquired) {
                if ((LONG)(now - s.nextAcquireTry) < 0) continue;
                HRESULT hr = s.dev->Acquire();
                if (FAILED(hr)) { s.nextAcquireTry = now + 1000; if (s.lostLogged++ < 3) LOG(1, "merge %s: Acquire failed 0x%08x", s.spec->name.c_str(), hr); continue; }
                s.acquired = true;
            }
            s.dev->Poll();
            DIJOYSTATE2 st{};
            HRESULT hr = s.dev->GetDeviceState(sizeof(st), &st);
            if (hr != DI_OK) { LOG(2, "merge %s: GetDeviceState 0x%08x, will reacquire", s.spec->name.c_str(), hr); s.acquired = false; s.nextAcquireTry = now + 200; continue; }
            s.prev = s.state; s.state = st;
            if (!s.spec->layers.empty()) {
                memcpy(s.prevLatch, s.latch, sizeof(s.latch));
                int mod = 0;   // the first held modifier in ini order wins
                for (int li = 0; li < (int)s.spec->layers.size() && mod == 0; li++) if (ModifierHeld(s.spec->layers[li], st)) mod = li + 1;
                for (int b = 0; b < 128; b++) {
                    bool down = (st.rgbButtons[b] & 0x80) != 0, was = (s.prev.rgbButtons[b] & 0x80) != 0;
                    if (!down) s.latch[b] = -1; else if (!was || s.latch[b] < 0) s.latch[b] = (signed char)mod;
                }
            }
            if (!s.spec->knobs.empty()) {
                // clicks per knob since the last poll: from the device's event buffer when it has one (a detent that
                // presses and releases between two game polls still counts), else from state edges
                int net[16] = {};
                bool fromEvents = false;
                if (s.buffered) {
                    DIDEVICEOBJECTDATA ev[64]; DWORD n = 64;
                    HRESULT hd = s.dev->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), ev, &n, 0);
                    if (SUCCEEDED(hd)) {
                        fromEvents = true;
                        for (DWORD e = 0; e < n; e++) {
                            if (!(ev[e].dwData & 0x80)) continue;
                            for (int ki = 0; ki < (int)s.spec->knobs.size() && ki < 16; ki++) {
                                const Knob& kn = s.spec->knobs[ki];
                                if (ev[e].dwOfs == ButtonOfs(kn.right)) net[ki]++; else if (ev[e].dwOfs == ButtonOfs(kn.left)) net[ki]--;
                            }
                        }
                    }
                }
                for (int ki = 0; ki < (int)s.spec->knobs.size() && ki < (int)s.knobs.size() && ki < 16; ki++) {
                    const Knob& kn = s.spec->knobs[ki]; Secondary::KnobState& ks = s.knobs[ki];
                    if (!fromEvents) {
                        auto clicked = [&](int b) { return b >= 0 && b < 128 && (st.rgbButtons[b] & 0x80) != 0 && (s.prev.rgbButtons[b] & 0x80) == 0; };
                        net[ki] = (clicked(kn.right) ? 1 : 0) - (clicked(kn.left) ? 1 : 0);
                    }
                    if (!net[ki]) continue;
                    ks.dir = net[ki] > 0 ? 1 : -1; ks.until = now + (DWORD)kn.pulseMs;
                    int span = std::max(1, 50 / std::max(1, kn.stepPct));   // clicks from the centre to either end in hold mode
                    ks.clicks = std::max(-span, std::min(span, ks.clicks + net[ki]));
                }
            }
            if (s.haveState && m_bufSize) {
                for (auto& i : m_inj) {
                    if (i.sec != si || i.gameOfs == 0xFFFFFFFF) continue;
                    DWORD a, b;
                    if (i.knob >= 0) { Secondary::KnobState& ks = s.knobs[i.knob]; b = KnobOut(s, i); a = ks.haveOut ? ks.lastOut : b; ks.lastOut = b; ks.haveOut = true; }
                    else { a = OutValue(i, s, s.prev, true); b = OutValue(i, s, s.state); }
                    if (a == b) continue;
                    DIDEVICEOBJECTDATA d{}; d.dwOfs = i.gameOfs; d.dwData = b; d.dwTimeStamp = now; d.dwSequence = m_seq++; d.uAppData = i.appData;
                    m_events.push_back(d);
                    if (m_events.size() > m_bufSize) { m_events.pop_front(); m_overflow = true; }
                }
            }
            s.haveState = true;
        }
    }
    void Overlay(void* buf, DWORD cb) {
        for (auto& i : m_inj) {
            if (i.gameOfs == 0xFFFFFFFF) continue;
            const Secondary& s = m_secs[i.sec];
            if (!s.haveState) continue;
            BYTE* p = (BYTE*)buf + i.gameOfs;
            if (i.kind == Kind::Button) { if (i.gameOfs + 1 > cb) continue; BYTE v = (BYTE)SrcValue(s, i); *p = i.overrides ? (*p | v) : v; }
            else {
                if (i.gameOfs + 4 > cb) continue;
                DWORD v = SrcValue(s, i);
                if (i.kind == Kind::Pov && i.overrides && PovCentered(v)) continue;   // a shared hat only speaks while it is pushed
                memcpy(p, &v, 4);
            }
        }
    }
    void FillObjInst(typename T::ObjInst* oi, const Injected& i) {
        DWORD size = oi->dwSize; memset(oi, 0, size); oi->dwSize = size;
        oi->guidType = i.kind == Kind::Axis ? *SlotGuid(i.slot) : i.kind == Kind::Pov ? GUID_POV : GUID_Button;
        oi->dwOfs = i.gameOfs; oi->dwType = i.id; oi->dwFlags = i.kind == Kind::Axis ? DIDOI_ASPECTPOSITION : 0;
        T::CopyName(oi->tszName, MAX_PATH, i.name.c_str());
        if (i.knob < 0 && size >= sizeof(typename T::ObjInst)) { const SrcObj& o = m_secs[i.sec].objs[i.obj]; oi->wUsagePage = o.usagePage; oi->wUsage = o.usage; }
    }
    // apply a property block to the merged device's own object
    HRESULT ForwardProp(bool set, const Secondary& s, const Injected& i, REFGUID rguid, const DIPROPHEADER* ph) {
        std::vector<BYTE> copy((const BYTE*)ph, (const BYTE*)ph + ph->dwSize);
        DIPROPHEADER* h = (DIPROPHEADER*)copy.data(); h->dwHow = DIPH_BYOFFSET; h->dwObj = s.objs[i.obj].srcOfs;
        return set ? s.dev->SetProperty(rguid, h) : s.dev->GetProperty(rguid, h);
    }
    // a knob axis has no device object behind it: range questions are answered from what the game set, the rest with zero
    HRESULT KnobProp(const Injected& i, REFGUID rguid, LPDIPROPHEADER ph) const {
        if (&rguid == &DIPROP_RANGE && ph->dwSize >= sizeof(DIPROPRANGE)) { DIPROPRANGE* r = (DIPROPRANGE*)ph; r->lMin = m_rangeMin[i.slot]; r->lMax = m_rangeMax[i.slot]; return DI_OK; }
        if (ph->dwSize >= sizeof(DIPROPDWORD)) ((DIPROPDWORD*)ph)->dwData = 0;
        return DI_OK;
    }
    static const char* PropName(REFGUID g) {
        if (&g == &DIPROP_BUFFERSIZE) return "BUFFERSIZE"; if (&g == &DIPROP_AXISMODE) return "AXISMODE"; if (&g == &DIPROP_RANGE) return "RANGE";
        if (&g == &DIPROP_DEADZONE) return "DEADZONE"; if (&g == &DIPROP_SATURATION) return "SATURATION"; if (&g == &DIPROP_FFGAIN) return "FFGAIN";
        if (&g == &DIPROP_AUTOCENTER) return "AUTOCENTER"; if (&g == &DIPROP_VIDPID) return "VIDPID"; if (&g == &DIPROP_GUIDANDPATH) return "GUIDANDPATH";
        if (&g == &DIPROP_PRODUCTNAME) return "PRODUCTNAME"; if (&g == &DIPROP_INSTANCENAME) return "INSTANCENAME"; if (&g == &DIPROP_CALIBRATIONMODE) return "CALIBRATIONMODE";
        if (&g == &DIPROP_JOYSTICKID) return "JOYSTICKID"; if (&g == &DIPROP_FFLOAD) return "FFLOAD"; if (&g == &DIPROP_APPDATA) return "APPDATA"; return "other";
    }

public:
    // ---------- IUnknown ----------
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == T::IidDev()) { *ppv = this; AddRef(); return S_OK; }
        LOG(1, "device QueryInterface(%s) forwarded to the real primary", GuidStr(riid).c_str());
        return m_prim->QueryInterface(riid, ppv);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override { ULONG r = InterlockedDecrement(&m_ref); if (r == 0) { LOG(1, "merged device released"); delete this; } return r; }

    // ---------- IDirectInputDevice8 ----------
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS caps) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        HRESULT hr = m_prim->GetCapabilities(caps);
        if (SUCCEEDED(hr) && caps) {
            for (auto& i : m_inj) if (!i.overrides && i.gameOfs != 0xFFFFFFFF) (i.kind == Kind::Axis ? caps->dwAxes : i.kind == Kind::Button ? caps->dwButtons : caps->dwPOVs)++;
            LOG(1, "GetCapabilities: axes=%u buttons=%u povs=%u flags=%08x", caps->dwAxes, caps->dwButtons, caps->dwPOVs, caps->dwFlags);
        }
        return hr;
    }
    struct ObjCtx { typename T::EnumObjCB cb; LPVOID ref; bool stop; };
    static BOOL CALLBACK ObjTramp(const typename T::ObjInst* oi, LPVOID p) { ObjCtx* c = (ObjCtx*)p; BOOL r = c->cb(oi, c->ref); if (r == DIENUM_STOP) c->stop = true; return r; }
    HRESULT STDMETHODCALLTYPE EnumObjects(typename T::EnumObjCB cb, LPVOID ref, DWORD flags) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        ObjCtx c{ cb, ref, false };
        HRESULT hr = m_prim->EnumObjects(ObjTramp, &c, flags);
        LOG(2, "EnumObjects(flags=%08x) -> 0x%08x", flags, hr);
        if (FAILED(hr) || c.stop) return hr;
        for (auto& i : m_inj) {
            if (i.overrides || i.gameOfs == 0xFFFFFFFF) continue;
            if (flags != DIDFT_ALL) {
                DWORD t = DIDFT_GETTYPE(flags);
                if (t && !(t & KindTypeBits(i.kind))) continue;
                if (flags & (DIDFT_FFACTUATOR | DIDFT_FFEFFECTTRIGGER)) continue;
                DWORD inst = DIDFT_GETINSTANCE(flags);
                if (inst != 0 && inst != 0xFFFF && inst != DIDFT_GETINSTANCE(i.id)) continue;   // 0 = no instance filter
            }
            typename T::ObjInst oi{}; oi.dwSize = sizeof(oi); FillObjInst(&oi, i);
            if (cb(&oi, ref) == DIENUM_STOP) break;
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID rguid, LPDIPROPHEADER ph) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        if (ph && ph->dwHow != DIPH_DEVICE) {
            Injected* i = Find(ph->dwHow, ph->dwObj);
            if (i && i->knob >= 0 && !i->overrides) return KnobProp(*i, rguid, ph);
            if (i && i->knob < 0 && !i->overrides) return ForwardProp(false, m_secs[i->sec], *i, rguid, ph);
        }
        HRESULT hr = m_prim->GetProperty(rguid, ph);
        LOG(2, "GetProperty(%s, how=%u obj=%u) -> 0x%08x", PropName(rguid), ph ? ph->dwHow : 0, ph ? ph->dwObj : 0, hr);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID rguid, LPCDIPROPHEADER ph) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        HRESULT hr = m_prim->SetProperty(rguid, ph);
        char extra[64] = "";
        if (ph) {
            if (&rguid == &DIPROP_RANGE && ph->dwSize >= sizeof(DIPROPRANGE)) {
                const DIPROPRANGE* r = (const DIPROPRANGE*)ph;
                sprintf_s(extra, " range %ld..%ld", r->lMin, r->lMax);
                for (auto& e : m_entries)
                    if (e.kind == Kind::Axis && (ph->dwHow == DIPH_DEVICE || (ph->dwHow == DIPH_BYOFFSET && e.ofs == ph->dwObj) || (ph->dwHow == DIPH_BYID && e.occupied && e.primId == ph->dwObj))) { m_rangeMin[e.slot] = r->lMin; m_rangeMax[e.slot] = r->lMax; }
            }
            if (ph->dwHow == DIPH_DEVICE) {
                if (&rguid == &DIPROP_BUFFERSIZE && ph->dwSize >= sizeof(DIPROPDWORD)) { m_bufSize = ((const DIPROPDWORD*)ph)->dwData; m_events.clear(); }
                if (&rguid == &DIPROP_RANGE || &rguid == &DIPROP_DEADZONE || &rguid == &DIPROP_SATURATION || &rguid == &DIPROP_AXISMODE || &rguid == &DIPROP_CALIBRATIONMODE)
                    for (auto& s : m_secs) if (s.dev) s.dev->SetProperty(rguid, ph);
            } else {
                Injected* i = Find(ph->dwHow, ph->dwObj);
                if (i && i->knob >= 0) {
                    if (&rguid == &DIPROP_RANGE && ph->dwSize >= sizeof(DIPROPRANGE)) { const DIPROPRANGE* r = (const DIPROPRANGE*)ph; m_rangeMin[i->slot] = r->lMin; m_rangeMax[i->slot] = r->lMax; }
                    if (!i->overrides) hr = DI_OK;
                } else if (i) {
                    HRESULT h2 = ForwardProp(true, m_secs[i->sec], *i, rguid, ph); if (!i->overrides) hr = h2;
                    if (&rguid == &DIPROP_APPDATA && ph->dwSize >= sizeof(DIPROPPOINTER)) i->appData = ((const DIPROPPOINTER*)ph)->uData;
                }
            }
        }
        LOG(1, "SetProperty(%s, how=%u obj=%u%s) -> 0x%08x", PropName(rguid), ph ? ph->dwHow : 0, ph ? ph->dwObj : 0, extra, hr);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE Acquire() override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        HRESULT hr = m_prim->Acquire();
        LOG(2, "Acquire -> 0x%08x", hr);
        for (auto& s : m_secs) if (s.dev && !s.acquired) { s.acquired = SUCCEEDED(s.dev->Acquire()); if (!s.acquired) s.nextAcquireTry = GetTickCount() + 1000; }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE Unacquire() override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        for (auto& s : m_secs) if (s.dev) { s.dev->Unacquire(); s.acquired = false; }
        HRESULT hr = m_prim->Unacquire();
        LOG(2, "Unacquire -> 0x%08x", hr);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD cb, LPVOID data) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        HRESULT hr = m_prim->GetDeviceState(cb, data);
        if (m_stateCalls++ < 3 || LogLevel() >= 2) LOG(1, "GetDeviceState(%u) -> 0x%08x", cb, hr);
        if (FAILED(hr) || !data) return hr;
        if (m_primNeeded) m_primBuf.assign((const BYTE*)data, (const BYTE*)data + cb);   // a layer's modifier may sit on the wheelbase
        Refresh(); Overlay(data, cb);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD cbObj, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD flags) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        if (!pdwInOut) return DIERR_INVALIDPARAM;
        DWORD capacity = *pdwInOut;
        HRESULT hr = m_prim->GetDeviceData(cbObj, rgdod, pdwInOut, flags);
        if (m_dataCalls++ < 3 || LogLevel() >= 2) LOG(1, "GetDeviceData(cb=%u cap=%u flags=%x) -> 0x%08x n=%u", cbObj, capacity, flags, hr, *pdwInOut);
        if (FAILED(hr)) return hr;
        DWORD primN = *pdwInOut;
        Refresh();
        if (!rgdod) {
            if (capacity == INFINITE && !(flags & DIGDD_PEEK)) { *pdwInOut = primN + (DWORD)m_events.size(); m_events.clear(); }
            else *pdwInOut = primN + (DWORD)m_events.size();
        } else if (cbObj >= 16) {
            DWORD room = capacity > primN ? capacity - primN : 0;
            DWORD n = (DWORD)std::min<size_t>(m_events.size(), room);
            for (DWORD k = 0; k < n; k++) {
                BYTE* p = (BYTE*)rgdod + (size_t)(primN + k) * cbObj; const DIDEVICEOBJECTDATA& e = m_events[k];
                memcpy(p, &e.dwOfs, 4); memcpy(p + 4, &e.dwData, 4); memcpy(p + 8, &e.dwTimeStamp, 4); memcpy(p + 12, &e.dwSequence, 4);
                if (cbObj >= 16 + sizeof(UINT_PTR)) memcpy(p + 16, &e.uAppData, sizeof(UINT_PTR));
            }
            if (!(flags & DIGDD_PEEK)) m_events.erase(m_events.begin(), m_events.begin() + n);
            *pdwInOut = primN + n;
        }
        if (m_overflow) { m_overflow = false; if (hr == DI_OK) hr = DI_BUFFEROVERFLOW; }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT f) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        HRESULT hr = m_prim->SetDataFormat(f);
        LOG(1, "SetDataFormat(size=%u objSize=%u flags=%x dataSize=%u objs=%u) -> 0x%08x", f ? f->dwSize : 0, f ? f->dwObjSize : 0, f ? f->dwFlags : 0, f ? f->dwDataSize : 0, f ? f->dwNumObjs : 0, hr);
        if (SUCCEEDED(hr) && f && f->dwObjSize == sizeof(DIOBJECTDATAFORMAT)) { CopyFormat(f); m_haveFmt = true; ComputeMapping(); }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE h) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        for (auto& s : m_secs) if (s.dev) s.dev->SetEventNotification(h);
        HRESULT hr = m_prim->SetEventNotification(h); LOG(1, "SetEventNotification(%p) -> 0x%08x", h, hr); return hr;
    }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND hwnd, DWORD flags) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        for (auto& s : m_secs) if (s.dev) { s.dev->Unacquire(); s.acquired = false; s.dev->SetCooperativeLevel(hwnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND); }
        DWORD asked = flags;
        if (m_foreign && (flags & DISCL_EXCLUSIVE)) flags = (flags & ~DISCL_EXCLUSIVE) | DISCL_NONEXCLUSIVE;
        HRESULT hr = m_prim->SetCooperativeLevel(hwnd, flags);
        LOG(1, "SetCooperativeLevel(hwnd=%p flags=%x%s) -> 0x%08x", hwnd, asked, flags != asked ? ", foreign client: exclusive downgraded to shared" : "", hr); return hr;
    }
    HRESULT STDMETHODCALLTYPE GetObjectInfo(typename T::ObjInst* oi, DWORD obj, DWORD how) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        if (oi) { Injected* i = Find(how, obj); if (i && !i->overrides) { FillObjInst(oi, *i); return DI_OK; } }
        return m_prim->GetObjectInfo(oi, obj, how);
    }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(typename T::DevInst* di) override { return m_prim->GetDeviceInfo(di); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND h, DWORD f) override { return m_prim->RunControlPanel(h, f); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE h, DWORD v, REFGUID g) override { return m_prim->Initialize(h, v, g); }
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID g, LPCDIEFFECT e, LPDIRECTINPUTEFFECT* out, LPUNKNOWN outer) override {
        HRESULT hr = m_prim->CreateEffect(g, e, out, outer);
        if (m_effects++ < 8 || LogLevel() >= 2) LOG(1, "CreateEffect(%s) -> 0x%08x", GuidStr(g).c_str(), hr);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE EnumEffects(typename T::EnumEffCB cb, LPVOID ref, DWORD t) override { return m_prim->EnumEffects(cb, ref, t); }
    HRESULT STDMETHODCALLTYPE GetEffectInfo(typename T::EffInfo* i, REFGUID g) override { return m_prim->GetEffectInfo(i, g); }
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD s) override { return m_prim->GetForceFeedbackState(s); }
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD c) override { HRESULT hr = m_prim->SendForceFeedbackCommand(c); LOG(1, "SendForceFeedbackCommand(%u) -> 0x%08x", c, hr); return hr; }
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK cb, LPVOID ref, DWORD f) override { return m_prim->EnumCreatedEffectObjects(cb, ref, f); }
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE e) override { return m_prim->Escape(e); }
    HRESULT STDMETHODCALLTYPE Poll() override { std::lock_guard<std::recursive_mutex> lk(m_mx); HRESULT hr = m_prim->Poll(); Refresh(); return hr; }
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD a, LPCDIDEVICEOBJECTDATA b, LPDWORD c, DWORD d) override { return m_prim->SendDeviceData(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(const typename T::Char* f, LPDIENUMEFFECTSINFILECALLBACK cb, LPVOID r, DWORD fl) override { return m_prim->EnumEffectsInFile(f, cb, r, fl); }
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(const typename T::Char* f, DWORD n, LPDIFILEEFFECT e, DWORD fl) override { return m_prim->WriteEffectToFile(f, n, e, fl); }
    HRESULT STDMETHODCALLTYPE BuildActionMap(typename T::ActionFmt* a, const typename T::Char* u, DWORD f) override { return m_prim->BuildActionMap(a, u, f); }
    HRESULT STDMETHODCALLTYPE SetActionMap(typename T::ActionFmt* a, const typename T::Char* u, DWORD f) override { return m_prim->SetActionMap(a, u, f); }
    HRESULT STDMETHODCALLTYPE GetImageInfo(typename T::ImgHdr* h) override { return m_prim->GetImageInfo(h); }
};

// ---------- the IDirectInput8 wrapper ----------
template<bool W>
class DIWrap final : public TR<W>::IDI {
    using T = TR<W>;
    ULONG m_ref = 1;
    typename T::IDI* m_real;
    bool m_foreign = false;
    struct Known { GUID inst; int role; };        // role: 0 other, 1 primary, 2 merged
    std::vector<Known> m_known;
    std::recursive_mutex m_mx;

    struct Pass { std::vector<int> seen; Pass() : seen(1 + Cfg().merges.size(), 0) {} };   // match counters for one enumeration pass
    static int Classify(const GUID& product, const GUID& instance, Pass& pass) {
        if (IdMatches(Cfg().prim, product, instance, pass.seen[0])) return 1;
        for (size_t k = 0; k < Cfg().merges.size(); k++) if (Cfg().merges[k].enabled && IdMatches(Cfg().merges[k].id, product, instance, pass.seen[1 + k])) return 2;
        return 0;
    }
    void Remember(const GUID& inst, int role) { for (auto& k : m_known) if (k.inst == inst) { k.role = role; return; } m_known.push_back(Known{ inst, role }); }
    int RoleOf(const GUID& inst) { for (auto& k : m_known) if (k.inst == inst) return k.role; return -1; }

    struct DevCtx { DIWrap* self; typename T::EnumDevCB cb; LPVOID ref; Pass pass; };
    static BOOL CALLBACK DevTramp(const typename T::DevInst* d, LPVOID p) {
        DevCtx* c = (DevCtx*)p;
        int role = Classify(d->guidProduct, d->guidInstance, c->pass);
        c->self->Remember(d->guidInstance, role);
        LOG(1, "  device \"%s\" type=%08x %04X:%04X %s %s", T::Str(d->tszProductName).c_str(), d->dwDevType, LOWORD(d->guidProduct.Data1), HIWORD(d->guidProduct.Data1), GuidStr(d->guidInstance).c_str(),
            role == 1 ? "= PRIMARY (merged wheel)" : role == 2 ? (Cfg().hideMerged ? "= merged, hidden from the game" : "= merged, still visible") : "");
        if (role == 2 && Cfg().hideMerged) return DIENUM_CONTINUE;
        if (!c->cb) return DIENUM_CONTINUE;
        return c->cb(d, c->ref);
    }
    struct ScanCtx { DIWrap* self; Pass pass; };
    static BOOL CALLBACK ScanTramp(const typename T::DevInst* d, LPVOID p) { ScanCtx* c = (ScanCtx*)p; c->self->Remember(d->guidInstance, Classify(d->guidProduct, d->guidInstance, c->pass)); return DIENUM_CONTINUE; }
    void Scan() { ScanCtx c{ this, Pass() }; m_real->EnumDevices(DI8DEVCLASS_GAMECTRL, ScanTramp, &c, DIEDFL_ATTACHEDONLY); }

public:
    DIWrap(typename T::IDI* real, bool foreign) : m_real(real), m_foreign(foreign) { Scan(); }
    ~DIWrap() { if (m_real) m_real->Release(); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == T::IidDI()) { *ppv = this; AddRef(); return S_OK; }
        LOG(1, "IDirectInput8 QueryInterface(%s) forwarded", GuidStr(riid).c_str());
        return m_real->QueryInterface(riid, ppv);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override { ULONG r = InterlockedDecrement(&m_ref); if (r == 0) delete this; return r; }

    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID rguid, typename T::IDev** ppDev, LPUNKNOWN outer) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        int role = RoleOf(rguid);
        if (role < 0) { Scan(); role = RoleOf(rguid); }
        if (role == 2 && Cfg().hideMerged) { LOG(1, "CreateDevice(%s): merged device requested directly, refused (hidden)", GuidStr(rguid).c_str()); return DIERR_DEVICENOTREG; }
        HRESULT hr = m_real->CreateDevice(rguid, ppDev, outer);
        LOG(1, "CreateDevice(%s) -> 0x%08x%s", GuidStr(rguid).c_str(), hr, role == 1 ? " [primary: wrapping]" : "");
        if (FAILED(hr) || role != 1 || !ppDev || !*ppDev) return hr;
        typename T::DevInst inst{}; inst.dwSize = sizeof(inst); (*ppDev)->GetDeviceInfo(&inst);
        try { *ppDev = new MergedDevice<W>(*ppDev, inst, m_foreign); }
        catch (const std::exception& e) { LOG(1, "merged device construction failed: %s; the wheelbase is passed through unmerged", e.what()); }
        catch (...) { LOG(1, "merged device construction failed; the wheelbase is passed through unmerged"); }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD devType, typename T::EnumDevCB cb, LPVOID ref, DWORD flags) override {
        std::lock_guard<std::recursive_mutex> lk(m_mx);
        LOG(1, "EnumDevices(class=%u flags=%x)", devType, flags);
        if (devType == DI8DEVCLASS_KEYBOARD || devType == DI8DEVCLASS_POINTER) return m_real->EnumDevices(devType, cb, ref, flags);
        DevCtx c{ this, cb, ref, Pass() };
        HRESULT hr = m_real->EnumDevices(devType, DevTramp, &c, flags);
        LOG(2, "EnumDevices -> 0x%08x", hr);
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID g) override { return m_real->GetDeviceStatus(g); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND h, DWORD f) override { return m_real->RunControlPanel(h, f); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE h, DWORD v) override { return m_real->Initialize(h, v); }
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID g, const typename T::Char* n, LPGUID out) override { return m_real->FindDevice(g, n, out); }
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(const typename T::Char* u, typename T::ActionFmt* a, typename T::EnumSemCB cb, LPVOID r, DWORD f) override { return m_real->EnumDevicesBySemantics(u, a, cb, r, f); }
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK cb, typename T::CfgParams* p, DWORD f, LPVOID r) override { return m_real->ConfigureDevices(cb, p, f, r); }
};

HRESULT CreateWrappedDI(REFIID riid, void* real, void** out, bool foreign) {
    if (riid == IID_IDirectInput8W) { *out = new DIWrap<true>((IDirectInput8W*)real, foreign); return S_OK; }
    if (riid == IID_IDirectInput8A) { *out = new DIWrap<false>((IDirectInput8A*)real, foreign); return S_OK; }
    return E_NOINTERFACE;
}
