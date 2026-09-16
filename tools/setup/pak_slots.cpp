// pak_slots.cpp: adds wheel binding slots to SnowRunner's initial.pak, from the wizard's menu or the command line.
//
//   dimerge-setup pak                        menu: pick the install, see which sets it carries, add or restore
//   dimerge-setup pakfile <in> <out> [sets]  patch a copy of a pak (for a release build from the vanilla install)
//   dimerge-setup pakexport <in> <folder> [sets]
//                                            write the patched cache block and strings files as loose files (the
//                                            drop-in package for archive tools), nothing else
//
// A port of tools\pakpatch\wheel_slots.py: the same sets, the same bytes in the cache block. The pak is a zip; the
// cache block and the strings files are inflated, patched and deflated again, every other entry is copied as it is.
// The text objects sit in a container with a name table, an offset per object and a size per object, so an
// insertion moves every later object and the two tables are rewritten to match.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

std::vector<std::wstring> SteamGameFolders();   // dimerge-setup.cpp

namespace {

// ---------- files and little-endian fields
bool ReadAll(const std::wstring& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    f.seekg(0, std::ios::end); std::streamoff n = f.tellg(); f.seekg(0);
    out.resize((size_t)n);
    return n == 0 || (bool)f.read((char*)out.data(), n);
}
bool WriteAll(const std::wstring& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary); if (!f) return false;
    return (bool)f.write((const char*)data.data(), (std::streamsize)data.size());
}
bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint64_t U64(const uint8_t* p) { return (uint64_t)U32(p) | ((uint64_t)U32(p + 4) << 32); }
void P16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }
void P32(std::vector<uint8_t>& o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back((uint8_t)(v >> (8 * i))); }
void W32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
void W64(uint8_t* p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
std::string Narrow(const std::wstring& w) { std::string s; for (wchar_t c : w) s.push_back((char)c); return s; }
std::wstring Widen(const std::string& s) { std::wstring w; for (unsigned char c : s) w.push_back((wchar_t)c); return w; }

uint32_t Crc32(const uint8_t* p, size_t n) {
    static uint32_t table[256]; static bool init = false;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

// ---------- inflate (RFC 1951), after Mark Adler's puff
struct BitIn {
    const uint8_t* p; size_t n; size_t pos = 0; uint32_t buf = 0; int cnt = 0; bool bad = false;
    int Bits(int k) {
        while (cnt < k) { if (pos >= n) { bad = true; return 0; } buf |= (uint32_t)p[pos++] << cnt; cnt += 8; }
        int v = (int)(buf & ((1u << k) - 1)); buf >>= k; cnt -= k; return v;
    }
    void Align() { buf = 0; cnt = 0; }   // Bits never keeps a whole byte back, so this drops only the rest of the current one
};
struct Huff { int count[16]; int symbol[320]; };
int BuildHuff(Huff& h, const int* lengths, int n) {
    for (int i = 0; i < 16; i++) h.count[i] = 0;
    for (int i = 0; i < n; i++) h.count[lengths[i]]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len < 16; len++) { left <<= 1; left -= h.count[len]; if (left < 0) return -1; }
    int offs[16]; offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h.count[len];
    for (int i = 0; i < n; i++) if (lengths[i]) h.symbol[offs[lengths[i]]++] = i;
    return left;
}
int Decode(BitIn& in, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= in.Bits(1);
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -1;
}
const int kLenBase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
const int kLenExtra[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
const int kDistBase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
const int kDistExtra[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
bool InflateCodes(BitIn& in, std::vector<uint8_t>& out, const Huff& lit, const Huff& dist) {
    for (;;) {
        int sym = Decode(in, lit);
        if (sym < 0 || in.bad) return false;
        if (sym < 256) { out.push_back((uint8_t)sym); continue; }
        if (sym == 256) return true;
        sym -= 257; if (sym >= 29) return false;
        int len = kLenBase[sym] + in.Bits(kLenExtra[sym]);
        int ds = Decode(in, dist);
        if (ds < 0 || ds >= 30 || in.bad) return false;
        size_t d = (size_t)kDistBase[ds] + (size_t)in.Bits(kDistExtra[ds]);
        if (d > out.size()) return false;
        size_t from = out.size() - d;
        for (int i = 0; i < len; i++) out.push_back(out[from + i]);
    }
}
bool Inflate(const uint8_t* p, size_t n, std::vector<uint8_t>& out, size_t expect) {
    BitIn in{ p, n }; out.clear(); out.reserve(expect);
    int last;
    do {
        last = in.Bits(1); int type = in.Bits(2);
        if (type == 0) {
            in.Align();
            if (in.pos + 4 > n) return false;
            unsigned len = U16(p + in.pos), nlen = U16(p + in.pos + 2); in.pos += 4;
            if ((len ^ 0xFFFFu) != nlen || in.pos + len > n) return false;
            out.insert(out.end(), p + in.pos, p + in.pos + len); in.pos += len;
        } else if (type == 1) {
            static Huff lit, dist; static bool built = false;
            if (!built) {
                int l[288]; int i = 0;
                for (; i < 144; i++) l[i] = 8; for (; i < 256; i++) l[i] = 9; for (; i < 280; i++) l[i] = 7; for (; i < 288; i++) l[i] = 8;
                BuildHuff(lit, l, 288);
                int d[30]; for (i = 0; i < 30; i++) d[i] = 5;
                BuildHuff(dist, d, 30); built = true;
            }
            if (!InflateCodes(in, out, lit, dist)) return false;
        } else if (type == 2) {
            int nlen = in.Bits(5) + 257, ndist = in.Bits(5) + 1, ncode = in.Bits(4) + 4;
            if (nlen > 286 || ndist > 30) return false;
            static const int order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
            int lengths[320] = {};
            for (int i = 0; i < ncode; i++) lengths[order[i]] = in.Bits(3);
            Huff lencode; if (BuildHuff(lencode, lengths, 19) != 0) return false;
            int idx = 0;
            while (idx < nlen + ndist) {
                int sym = Decode(in, lencode); if (sym < 0 || in.bad) return false;
                if (sym < 16) { lengths[idx++] = sym; continue; }
                int len = 0, rep;
                if (sym == 16) { if (idx == 0) return false; len = lengths[idx - 1]; rep = 3 + in.Bits(2); }
                else if (sym == 17) rep = 3 + in.Bits(3);
                else rep = 11 + in.Bits(7);
                if (idx + rep > nlen + ndist) return false;
                while (rep--) lengths[idx++] = len;
            }
            if (lengths[256] == 0) return false;
            Huff lit, dist;
            int e1 = BuildHuff(lit, lengths, nlen); if (e1 < 0 || (e1 > 0 && nlen - lit.count[0] != 1)) return false;
            int e2 = BuildHuff(dist, lengths + nlen, ndist); if (e2 < 0 || (e2 > 0 && ndist - dist.count[0] != 1)) return false;
            if (!InflateCodes(in, out, lit, dist)) return false;
        } else return false;
        if (in.bad) return false;
    } while (!last);
    return true;
}

// ---------- deflate: one block of fixed Huffman codes over a greedy LZ77 match finder
struct BitOut {
    std::vector<uint8_t> out; uint32_t buf = 0; int cnt = 0;
    void Put(uint32_t v, int k) { buf |= v << cnt; cnt += k; while (cnt >= 8) { out.push_back((uint8_t)buf); buf >>= 8; cnt -= 8; } }
    void PutCode(uint32_t code, int len) { uint32_t r = 0; for (int i = 0; i < len; i++) { r = (r << 1) | (code & 1); code >>= 1; } Put(r, len); }   // Huffman codes go out most significant bit first
    void Flush() { if (cnt > 0) { out.push_back((uint8_t)buf); buf = 0; cnt = 0; } }
};
void PutSymbol(BitOut& o, int sym) {
    if (sym < 144) o.PutCode(0x30 + sym, 8);
    else if (sym < 256) o.PutCode(0x190 + sym - 144, 9);
    else if (sym < 280) o.PutCode(sym - 256, 7);
    else o.PutCode(0xC0 + sym - 280, 8);
}
void Deflate(const uint8_t* p, size_t n, std::vector<uint8_t>& out) {
    BitOut o; o.out.reserve(n / 3 + 64);
    o.Put(1, 1); o.Put(1, 2);
    const size_t W = 32768, HSIZE = 1 << 15;
    std::vector<int32_t> head(HSIZE, -1), prev(W, -1);
    auto hash = [&](size_t i) { return (size_t)((((uint32_t)p[i] << 10) ^ ((uint32_t)p[i + 1] << 5) ^ p[i + 2]) & (HSIZE - 1)); };
    auto insert = [&](size_t i) { if (i + 2 < n) { size_t h = hash(i); prev[i & (W - 1)] = head[h]; head[h] = (int32_t)i; } };
    size_t i = 0;
    while (i < n) {
        int best = 0; size_t bestDist = 0;
        if (i + 2 < n) {
            size_t maxLen = std::min<size_t>(258, n - i);
            int32_t cand = head[hash(i)]; int chain = 48;
            while (cand >= 0 && chain-- > 0 && i - (size_t)cand <= W) {
                const uint8_t* a = p + cand; const uint8_t* b = p + i;
                if (a[best] == b[best]) {
                    size_t len = 0;
                    while (len < maxLen && a[len] == b[len]) len++;
                    if ((int)len > best) { best = (int)len; bestDist = i - (size_t)cand; if (len == maxLen) break; }
                }
                int32_t next = prev[cand & (W - 1)];
                if (next >= cand) break;   // the ring slot was reused by a newer position, the chain ends here
                cand = next;
            }
        }
        if (best >= 3) {
            int c = 28; while (best < kLenBase[c]) c--;
            PutSymbol(o, 257 + c); if (kLenExtra[c]) o.Put((uint32_t)(best - kLenBase[c]), kLenExtra[c]);
            int dc = 29; while ((int)bestDist < kDistBase[dc]) dc--;
            o.PutCode((uint32_t)dc, 5); if (kDistExtra[dc]) o.Put((uint32_t)(bestDist - (size_t)kDistBase[dc]), kDistExtra[dc]);
            for (int k = 0; k < best; k++) insert(i + k);
            i += best;
        } else {
            PutSymbol(o, p[i]); insert(i); i++;
        }
    }
    PutSymbol(o, 256);
    o.Flush();
    out.swap(o.out);
}

// ---------- the zip container
struct ZEntry {
    std::string name, extra, comment;
    uint16_t verMade = 0, verNeed = 20, flags = 0, method = 0, time = 0, date = 0, intAttr = 0;
    uint32_t crc = 0, csize = 0, usize = 0, extAttr = 0, localOfs = 0;
};
struct Zip {
    std::vector<uint8_t> data; std::vector<ZEntry> entries; std::string comment;
    bool Load(const std::vector<uint8_t>& bytes, std::string& err) {
        data = bytes; entries.clear();
        size_t n = data.size(); if (n < 22) { err = "not a zip"; return false; }
        size_t eocd = std::string::npos;
        for (size_t i = n - 22; i + 1 > 0 && n - i <= 65557; i--) if (U32(&data[i]) == 0x06054b50) { eocd = i; break; }
        if (eocd == std::string::npos) { err = "no end of central directory"; return false; }
        const uint8_t* e = &data[eocd];
        uint32_t count = U16(e + 10), cdOfs = U32(e + 16); uint16_t clen = U16(e + 20);
        if (count == 0xFFFF || cdOfs == 0xFFFFFFFFu) { err = "zip64 paks are not handled"; return false; }
        comment.assign((const char*)e + 22, std::min<size_t>(clen, n - eocd - 22));
        size_t p = cdOfs;
        for (uint32_t k = 0; k < count; k++) {
            if (p + 46 > n || U32(&data[p]) != 0x02014b50) { err = "central directory damaged"; return false; }
            const uint8_t* c = &data[p]; ZEntry z;
            z.verMade = U16(c + 4); z.verNeed = U16(c + 6); z.flags = U16(c + 8); z.method = U16(c + 10); z.time = U16(c + 12); z.date = U16(c + 14);
            z.crc = U32(c + 16); z.csize = U32(c + 20); z.usize = U32(c + 24);
            uint16_t nl = U16(c + 28), xl = U16(c + 30), cl = U16(c + 32);
            z.intAttr = U16(c + 36); z.extAttr = U32(c + 38); z.localOfs = U32(c + 42);
            if (p + 46 + nl + xl + cl > n) { err = "central directory damaged"; return false; }
            z.name.assign((const char*)c + 46, nl); z.extra.assign((const char*)c + 46 + nl, xl); z.comment.assign((const char*)c + 46 + nl + xl, cl);
            if (z.csize == 0xFFFFFFFFu || z.usize == 0xFFFFFFFFu || z.localOfs == 0xFFFFFFFFu) { err = "zip64 paks are not handled"; return false; }
            entries.push_back(z); p += 46 + nl + xl + cl;
        }
        return true;
    }
    // where an entry's compressed bytes start
    size_t DataStart(const ZEntry& z) const {
        if (z.localOfs + 30 > data.size()) return std::string::npos;
        const uint8_t* l = &data[z.localOfs];
        if (U32(l) != 0x04034b50) return std::string::npos;
        return z.localOfs + 30 + U16(l + 26) + U16(l + 28);
    }
    bool Read(const ZEntry& z, std::vector<uint8_t>& out, std::string& err) const {
        size_t s = DataStart(z);
        if (s == std::string::npos || s + z.csize > data.size()) { err = "local header damaged for " + z.name; return false; }
        if (z.method == 0) { out.assign(data.begin() + s, data.begin() + s + z.csize); return true; }
        if (z.method != 8) { err = "unknown compression in " + z.name; return false; }
        if (!Inflate(&data[s], z.csize, out, z.usize) || out.size() != z.usize) { err = "inflate failed for " + z.name; return false; }
        return true;
    }
};
struct Replacement { std::string name; std::vector<uint8_t> data; };
// The same entries in the same order; replaced ones get fresh deflate data, the rest their original bytes. Every local
// header is written clean (no data descriptors), the central directory keeps its attributes and comments.
bool Rebuild(const Zip& z, const std::vector<Replacement>& reps, std::vector<uint8_t>& out, std::string& err) {
    out.clear(); out.reserve(z.data.size() + (4u << 20));
    std::vector<ZEntry> cd = z.entries;
    for (auto& e : cd) {
        const Replacement* r = nullptr; for (auto& x : reps) if (x.name == e.name) r = &x;
        std::vector<uint8_t> packed; const uint8_t* src; size_t srcLen;
        if (r) {
            Deflate(r->data.data(), r->data.size(), packed);
            e.method = 8; e.crc = Crc32(r->data.data(), r->data.size()); e.usize = (uint32_t)r->data.size(); e.csize = (uint32_t)packed.size();
            src = packed.data(); srcLen = packed.size();
        } else {
            size_t s = z.DataStart(e); if (s == std::string::npos) { err = "local header damaged for " + e.name; return false; }
            src = &z.data[s]; srcLen = e.csize;
        }
        e.flags &= (uint16_t)~0x0008;   // sizes go into the header, never into a descriptor after the data
        e.localOfs = (uint32_t)out.size();
        P32(out, 0x04034b50); P16(out, e.verNeed); P16(out, e.flags); P16(out, e.method); P16(out, e.time); P16(out, e.date);
        P32(out, e.crc); P32(out, e.csize); P32(out, e.usize); P16(out, (uint16_t)e.name.size()); P16(out, 0);
        out.insert(out.end(), e.name.begin(), e.name.end());
        out.insert(out.end(), src, src + srcLen);
    }
    uint32_t cdOfs = (uint32_t)out.size();
    for (auto& e : cd) {
        P32(out, 0x02014b50); P16(out, e.verMade); P16(out, e.verNeed); P16(out, e.flags); P16(out, e.method); P16(out, e.time); P16(out, e.date);
        P32(out, e.crc); P32(out, e.csize); P32(out, e.usize);
        P16(out, (uint16_t)e.name.size()); P16(out, (uint16_t)e.extra.size()); P16(out, (uint16_t)e.comment.size()); P16(out, 0); P16(out, e.intAttr); P32(out, e.extAttr); P32(out, e.localOfs);
        out.insert(out.end(), e.name.begin(), e.name.end()); out.insert(out.end(), e.extra.begin(), e.extra.end()); out.insert(out.end(), e.comment.begin(), e.comment.end());
    }
    uint32_t cdSize = (uint32_t)out.size() - cdOfs;
    P32(out, 0x06054b50); P16(out, 0); P16(out, 0); P16(out, (uint16_t)cd.size()); P16(out, (uint16_t)cd.size()); P32(out, cdSize); P32(out, cdOfs); P16(out, (uint16_t)z.comment.size());
    out.insert(out.end(), z.comment.begin(), z.comment.end());
    return true;
}
// Reads the new pak back: same names in the same order, untouched entries with their old checksums, replaced ones
// inflating to exactly the bytes that were meant.
bool Verify(const std::vector<uint8_t>& bytes, const Zip& orig, const std::vector<Replacement>& reps, std::string& err) {
    Zip z; if (!z.Load(bytes, err)) return false;
    if (z.entries.size() != orig.entries.size()) { err = "entry count changed"; return false; }
    for (size_t i = 0; i < z.entries.size(); i++) {
        const ZEntry& a = orig.entries[i]; const ZEntry& b = z.entries[i];
        if (a.name != b.name) { err = "entry order changed at " + a.name; return false; }
        const Replacement* r = nullptr; for (auto& x : reps) if (x.name == a.name) r = &x;
        if (!r) { if (a.crc != b.crc || a.csize != b.csize || a.usize != b.usize) { err = "content changed for " + a.name; return false; } continue; }
        std::vector<uint8_t> back;
        if (!z.Read(b, back, err)) return false;
        if (back != r->data) { err = "round trip differs for " + a.name; return false; }
    }
    return true;
}

// ---------- the slot sets
struct SlotDef { const char* name; const char* key; const char* text; const char* target; const char* dir; const char* ctx; };   // text null: the key exists in the game; dir null: forward vector, "ACTION": plain action
struct SetDef { const char* name; const char* about; const char* after; std::vector<SlotDef> slots; std::vector<const char*> unhide; bool engine; };
const std::vector<SetDef>& Sets() {
    static const std::vector<SetDef> s = {
        { "crane", "moving the crane, crane mode, attaching cargo and the anchor", "CraneArrowLower", {
            { "CraneMoveForward", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_FORWARD", "Move Crane Forward", "Crane.moveXZplane", nullptr, "CRANE" },
            { "CraneMoveBackward", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_BACKWARD", "Move Crane Backward", "Crane.moveXZplane", "BACKWARD", "CRANE" },
            { "CraneMoveLeft", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_LEFT", "Move Crane Left", "Crane.moveXZplane", "LEFT", "CRANE" },
            { "CraneMoveRight", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MOVE_RIGHT", "Move Crane Right", "Crane.moveXZplane", "RIGHT", "CRANE" },
            { "CraneTurnOn", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_MODE", "Enter Crane Mode", "Crane.TurnOn", "ACTION", "GAME" },
            { "CraneAttachCargo", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_ATTACH", "Attach or Detach Cargo", "Crane.AttachCargo", "ACTION", "CRANE" },
            { "CraneAnchor", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CRANE_ANCHOR", "Crane Anchor", "Crane.EnableAnchor", "ACTION", "CRANE" } },
          { "UI_CRANE_ANCHOR" }, false },
        { "engine", "the engine start/stop slot the stock data carries but never shows", nullptr, {}, {}, true },
        { "hud", "the HUD toggle (keyboard H)", "ToggleGameCamera", {
            { "HudVisibility", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_CUSTOM_ADDON_ACTION_HUD_VISIBILITY", nullptr, "Exploration.ToggleHudVisibility", "ACTION", "GAME" } },
          {}, false },
    };
    return s;
}
const std::string CRLF = "\r\n";
std::string Lines(const std::vector<std::string>& rows) { std::string s; for (auto& r : rows) s += r + CRLF; return s; }
std::string SlotBlock(const SlotDef& sl) {
    std::vector<std::string> rows;
    std::string name = sl.name;
    if (sl.dir && std::string(sl.dir) == "ACTION") {
        rows = { "   " + name + "   =   {", "      uiName   =   \"" + std::string(sl.key) + "\"", "      targetInputsToRemap   =   [", "         {", "            inputLink   =   \"" + std::string(sl.target) + "\"", "         }", "      ]" };
    } else {
        rows = { "   " + name + "   =   {", "      uiName   =   \"" + std::string(sl.key) + "\"", "      targetInputsToRemapInVector   =   [", "         {", "            inputLink   =   {", "               inputLink   =   \"" + std::string(sl.target) + "\"", "               __type   =   \"InputLinkMr\"", "            }" };
        if (sl.dir) rows.push_back("            direction   =   \"" + std::string(sl.dir) + "\"");
        rows.push_back("         }"); rows.push_back("      ]");
    }
    rows.push_back("      gameContexts   =   ["); rows.push_back("         \"" + std::string(sl.ctx) + "\""); rows.push_back("      ]");
    rows.push_back("      __type   =   \"SteeringWheelInputDescription\""); rows.push_back("   }");
    return Lines(rows);
}
std::string MenuRow(const std::string& name) {
    return Lines({ "            " + name + "   =   {", "               name   =   \"SteeringWheelInput" + name + "\"", "               __type   =   \"UiSettingScreenElementInteractive\"", "            }" });
}
std::string Controller(const std::string& name, const std::string& key) {
    return Lines({ "   SteeringWheelInput" + name + "   =   {", "      header   =   \"" + key + "\"", "      inputDescriptionLink   =   \"" + name + "\"", "      __type   =   \"UiOneSettingSteeringWheelInputShortcutController\"", "   }" });
}
std::string SlotHead(const std::string& name) { return "   " + name + "   =   {"; }
std::string RowHead(const std::string& name) { return "            " + name + "   =   {" + CRLF + "               name   =   \"SteeringWheelInput" + name + "\""; }
std::string CtrlHead(const std::string& name) { return "   SteeringWheelInput" + name + "   =   {"; }

// ---------- the cache block's index: a name table, then per object a text-relative offset and a size
struct Index { std::vector<std::string> names; size_t t1 = 0, t2 = 0, text0 = 0; std::vector<uint64_t> offs; std::vector<uint32_t> sizes; };
bool ParseIndex(const std::string& d, Index& ix, std::string& err) {
    const uint8_t* b = (const uint8_t*)d.data();
    size_t p = 0x4e; ix.names.clear(); ix.offs.clear(); ix.sizes.clear();
    while (p + 4 <= d.size()) {
        uint32_t n = U32(b + p);
        if (n == 0 || n > 300 || p + 4 + n > d.size() || d[p + 4] != '<') break;
        ix.names.push_back(d.substr(p + 4, n)); p += 4 + n;
    }
    size_t N = ix.names.size();
    if (N == 0 || p >= d.size() || d[p] != 1) { err = "unexpected byte after the name table"; return false; }
    ix.t1 = p + 1; if (ix.t1 + 8 * N > d.size()) { err = "index truncated"; return false; }
    for (size_t i = 0; i < N; i++) ix.offs.push_back(U64(b + ix.t1 + 8 * i));
    ix.t2 = ix.t1 + 8 * N; if (ix.t2 >= d.size() || d[ix.t2] != 1) { err = "unexpected byte after the offset table"; return false; }
    ix.t2++; if (ix.t2 + 4 * N > d.size()) { err = "index truncated"; return false; }
    for (size_t i = 0; i < N; i++) ix.sizes.push_back(U32(b + ix.t2 + 4 * i));
    size_t t3 = ix.t2 + 4 * N; if (t3 >= d.size() || d[t3] != 1) { err = "unexpected byte after the size table"; return false; }
    t3++; ix.text0 = t3 + 4 * N;
    for (size_t i = 0; i + 1 < N; i++) if (ix.offs[i] + ix.sizes[i] != ix.offs[i + 1]) { err = "index does not describe the text area, format changed"; return false; }
    if (ix.text0 > d.size() || ix.offs[N - 1] + ix.sizes[N - 1] != d.size() - ix.text0) { err = "index does not describe the text area, format changed"; return false; }
    return true;
}
struct TextEntry {
    const std::string* d = nullptr; size_t k = 0, start = 0, end = 0; bool ok = false;
    TextEntry() {}
    TextEntry(const std::string& data, const Index& ix, const std::string& suffix) : d(&data) {
        int found = 0;
        for (size_t i = 0; i < ix.names.size(); i++) if (ix.names[i].size() >= suffix.size() && ix.names[i].compare(ix.names[i].size() - suffix.size(), suffix.size(), suffix) == 0) { k = i; found++; }
        ok = found == 1;
        if (ok) { start = ix.text0 + (size_t)ix.offs[k]; end = start + ix.sizes[k]; }
    }
    bool Has(const std::string& head) const { return Find(CRLF + head + CRLF, start) != std::string::npos; }
    size_t Find(const std::string& s, size_t from) const { size_t r = d->find(s, from); return (r == std::string::npos || r + s.size() > end) ? std::string::npos : r; }
    // position after the closing line of the block that starts with the given header line; npos when absent
    size_t BlockEnd(const std::string& head, int indent, std::string& err) const {
        std::string h = CRLF + head + CRLF;
        size_t a = Find(h, start);
        if (a == std::string::npos) return std::string::npos;
        if (Find(h, a + 1) != std::string::npos) { err = "block header not unique: " + head.substr(0, 60); return std::string::npos; }
        std::string close = CRLF + std::string((size_t)indent, ' ') + "}" + CRLF;
        size_t e = Find(close, a);
        if (e == std::string::npos) { err = "block never closes: " + head.substr(0, 60); return std::string::npos; }
        return e + close.size();
    }
};
struct Op { size_t pos; size_t old; std::string bytes; };

// Adds the chosen sets to the cache block. Returns false with err on a damaged pak; 'changed' says whether anything
// was missing.
bool PatchCache(const std::string& d, const std::vector<const SetDef*>& sets, std::string& out, bool& changed, std::string& log, std::string& err) {
    Index ix; if (!ParseIndex(d, ix, err)) return false;
    TextEntry mapper(d, ix, "steering_wheel_input_mapper.sso"), settings(d, ix, "ui_settings_controller.sso");
    if (!mapper.ok || !settings.ok) { err = "the wheel mapper or the settings controller is not in the cache block once"; return false; }
    std::vector<Op> ops;
    for (const SetDef* st : sets) {
        if (!st->slots.empty()) {
            std::vector<const SlotDef*> missing;
            for (auto& sl : st->slots) if (!mapper.Has(SlotHead(sl.name))) missing.push_back(&sl);
            if (!missing.empty()) {
                size_t pos = mapper.BlockEnd(SlotHead(st->after), 3, err); if (!err.empty()) return false;
                size_t row = settings.BlockEnd(RowHead(st->after), 12, err); if (!err.empty()) return false;
                size_t ctrl = settings.BlockEnd(CtrlHead(st->after), 3, err); if (!err.empty()) return false;
                if (pos == std::string::npos || row == std::string::npos || ctrl == std::string::npos) { err = std::string("set ") + st->name + ": the stock slot " + st->after + " it follows is missing"; return false; }
                std::string a, b, c;
                for (auto* sl : missing) { a += SlotBlock(*sl); b += MenuRow(sl->name); c += Controller(sl->name, sl->key); }
                ops.push_back({ pos, 0, a }); ops.push_back({ row, 0, b }); ops.push_back({ ctrl, 0, c });
            }
            for (auto& sl : st->slots) {
                bool isMissing = false; for (auto* m : missing) if (m == &sl) isMissing = true;
                if (!isMissing && mapper.Has(SlotHead(sl.name)) && !settings.Has(CtrlHead(sl.name))) { err = std::string("slot ") + sl.name + " exists but its menu entries do not; restore the backup and run again"; return false; }
            }
        }
        if (st->engine) {
            // the stock StartEngine slot has no target, no row in the wheel settings screen and no controller
            std::string oldEngine = Lines({ "   StartEngine   =   {", "      uiName   =   \"UI_SETTINGS_CONTROL_LAYOUT_ACTION_ENGINE\"", "      gameContexts   =   [", "         \"GAME\"", "      ]", "      __type   =   \"SteeringWheelInputDescription\"", "   }" });
            std::string newEngine = Lines({ "   StartEngine   =   {", "      uiName   =   \"UI_SETTINGS_CONTROL_LAYOUT_ACTION_ENGINE\"", "      targetInputsToRemap   =   [", "         {", "            inputLink   =   \"Truck.EngineSolo\"", "         }", "      ]", "      gameContexts   =   [", "         \"GAME\"", "      ]", "      __type   =   \"SteeringWheelInputDescription\"", "   }" });
            size_t a = mapper.Find(oldEngine, mapper.start);
            if (a != std::string::npos) ops.push_back({ a, oldEngine.size(), newEngine });
            if (!settings.Has(CtrlHead("StartEngine"))) {
                size_t row = settings.BlockEnd(RowHead("Headlights"), 12, err); if (!err.empty()) return false;
                size_t ctrl = settings.BlockEnd(CtrlHead("Handbrake"), 3, err); if (!err.empty()) return false;
                if (row == std::string::npos || ctrl == std::string::npos) { err = "engine: the Headlights row or the Handbrake controller is missing"; return false; }
                ops.push_back({ row, 0, MenuRow("StartEngine") }); ops.push_back({ ctrl, 0, Controller("StartEngine", "UI_SETTINGS_CONTROL_LAYOUT_ACTION_ENGINE") });
            }
        }
        for (const char* key : st->unhide) {
            std::string needle = "text   =   \"" + std::string(key) + "\"";
            size_t i = d.find(needle);
            while (i != std::string::npos) {
                size_t stop = d.find("__type   =   \"NavLegendHintController\"", i); if (stop == std::string::npos) stop = d.size();
                size_t j = d.find("excludedDevices   =   [", i); if (j != std::string::npos && j >= stop) j = std::string::npos;
                if (j != std::string::npos) {
                    std::string sw = "\"steering_wheel\"," + CRLF;
                    size_t line = d.find(sw, j); if (line != std::string::npos && line >= stop) line = std::string::npos;
                    if (line != std::string::npos) { size_t ls = d.rfind(CRLF, line) + 2; ops.push_back({ ls, line + sw.size() - ls, "" }); }
                }
                i = d.find(needle, i + 1);
            }
        }
    }
    changed = !ops.empty();
    if (!changed) { out = d; return true; }
    std::vector<uint64_t> offs = ix.offs; std::vector<uint32_t> sizes = ix.sizes;
    size_t N = ix.names.size();
    for (auto& op : ops) {
        long long delta = (long long)op.bytes.size() - (long long)op.old;
        size_t k = 0; for (size_t i = 0; i < N; i++) if (offs[i] <= (uint64_t)(op.pos - ix.text0)) k = i;
        sizes[k] = (uint32_t)((long long)sizes[k] + delta);
        for (size_t i = k + 1; i < N; i++) offs[i] = (uint64_t)((long long)offs[i] + delta);
        char line[200]; sprintf_s(line, "  %s %zu bytes in [%zu] %s\n", op.old == 0 ? "insert" : "replace", op.old == 0 ? op.bytes.size() : op.old, k, ix.names[k].c_str()); log += line;
    }
    std::vector<Op> sorted = ops;
    std::sort(sorted.begin(), sorted.end(), [](const Op& a, const Op& b) { return a.pos > b.pos; });
    out = d;
    for (auto& op : sorted) out.replace(op.pos, op.old, op.bytes);
    for (size_t i = 0; i < N; i++) { W64((uint8_t*)&out[ix.t1 + 8 * i], offs[i]); W32((uint8_t*)&out[ix.t2 + 4 * i], sizes[i]); }
    Index check; if (!ParseIndex(out, check, err)) { err = "the rewritten index does not check out: " + err; return false; }
    return true;
}
// Appends an English line for every new name to a UTF-16LE strings file. Returns whether anything was added.
bool PatchStrings(std::vector<uint8_t>& data, const std::vector<const SetDef*>& sets) {
    auto u16 = [](const std::string& s) { std::vector<uint8_t> v; for (unsigned char c : s) { v.push_back(c); v.push_back(0); } return v; };
    bool changed = false;
    for (const SetDef* st : sets) for (auto& sl : st->slots) {
        if (!sl.text) continue;
        std::vector<uint8_t> needle = u16(std::string(sl.key) + "\t");
        if (std::search(data.begin(), data.end(), needle.begin(), needle.end()) != data.end()) continue;
        std::vector<uint8_t> add = u16("\r\n" + std::string(sl.key) + "\t\t\t\t\"" + sl.text + "\"");
        data.insert(data.end(), add.begin(), add.end()); changed = true;
    }
    return changed;
}
const char* CACHE = "initial.cache_block";
bool IsStrings(const std::string& name) { return name.rfind("[strings]", 0) == 0 && name.size() > 4 && name.compare(name.size() - 4, 4, ".str") == 0; }

// Loads a pak and works out the replacements the chosen sets need. 'reps' empty = nothing missing.
bool Prepare(const std::vector<uint8_t>& bytes, const std::vector<const SetDef*>& sets, Zip& z, std::vector<Replacement>& reps, std::string& log, std::string& err) {
    if (!z.Load(bytes, err)) return false;
    const ZEntry* cache = nullptr; for (auto& e : z.entries) if (e.name == CACHE) cache = &e;
    if (!cache) { err = "no initial.cache_block in the pak"; return false; }
    std::vector<uint8_t> raw; if (!z.Read(*cache, raw, err)) return false;
    std::string d(raw.begin(), raw.end()), out; bool changed = false;
    if (!PatchCache(d, sets, out, changed, log, err)) return false;
    if (changed) reps.push_back({ CACHE, std::vector<uint8_t>(out.begin(), out.end()) });
    for (auto& e : z.entries) {
        if (!IsStrings(e.name)) continue;
        std::vector<uint8_t> s; if (!z.Read(e, s, err)) return false;
        if (PatchStrings(s, sets)) reps.push_back({ e.name, s });
    }
    return true;
}
std::vector<const SetDef*> PickSets(const std::string& list) {
    std::vector<const SetDef*> out;
    if (list.empty()) { for (auto& s : Sets()) out.push_back(&s); return out; }
    size_t p = 0;
    while (p <= list.size()) {
        size_t q = list.find(',', p); if (q == std::string::npos) q = list.size();
        std::string item = list.substr(p, q - p);
        for (auto& s : Sets()) if (item == s.name) out.push_back(&s);
        p = q + 1;
    }
    return out;
}
bool Locked(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return true;
    CloseHandle(h); return false;
}
// Patches the pak in place: backup once, write next to it, read the result back, then swap it in.
int Apply(const std::wstring& pak, const std::vector<const SetDef*>& sets) {
    std::vector<uint8_t> bytes; if (!ReadAll(pak, bytes)) { wprintf(L"cannot read %s\n", pak.c_str()); return 1; }
    Zip z; std::vector<Replacement> reps; std::string log, err;
    if (!Prepare(bytes, sets, z, reps, log, err)) { printf("%s\n", err.c_str()); return 1; }
    printf("%s", log.c_str());
    if (reps.empty()) { printf("nothing to do, the pak already carries everything\n"); return 0; }
    if (Locked(pak)) { printf("the pak is locked, close the game first\n"); return 1; }
    std::wstring backup = pak + L".orig";
    if (!Exists(backup)) { if (!CopyFileW(pak.c_str(), backup.c_str(), TRUE)) { printf("could not write the backup\n"); return 1; } wprintf(L"backup written: %s\n", backup.c_str()); }
    std::vector<uint8_t> out;
    DWORD t = GetTickCount();
    if (!Rebuild(z, reps, out, err) || !Verify(out, z, reps, err)) { printf("%s\n", err.c_str()); return 1; }
    std::wstring tmp = pak + L".dimerge-new";
    if (!WriteAll(tmp, out) || !MoveFileExW(tmp.c_str(), pak.c_str(), MOVEFILE_REPLACE_EXISTING)) { printf("could not write the pak\n"); DeleteFileW(tmp.c_str()); return 1; }
    wprintf(L"pak rewritten in %.1f s (%zu entries checked): %s\n", (GetTickCount() - t) / 1000.0, z.entries.size(), pak.c_str());
    return 0;
}
int Restore(const std::wstring& pak) {
    std::wstring backup = pak + L".orig";
    if (!Exists(backup)) { wprintf(L"no backup at %s\n", backup.c_str()); return 1; }
    if (Locked(pak)) { printf("the pak is locked, close the game first\n"); return 1; }
    if (!CopyFileW(backup.c_str(), pak.c_str(), FALSE)) { printf("could not copy the backup back\n"); return 1; }
    wprintf(L"restored %s from %s\n", pak.c_str(), backup.c_str());
    return 0;
}
// which sets a pak already carries
std::string Carried(const std::vector<uint8_t>& bytes, std::string& err) {
    Zip z; if (!z.Load(bytes, err)) return "";
    const ZEntry* cache = nullptr; for (auto& e : z.entries) if (e.name == CACHE) cache = &e;
    if (!cache) { err = "no initial.cache_block in the pak"; return ""; }
    std::vector<uint8_t> raw; if (!z.Read(*cache, raw, err)) return "";
    std::string d(raw.begin(), raw.end()); Index ix; if (!ParseIndex(d, ix, err)) return "";
    TextEntry mapper(d, ix, "steering_wheel_input_mapper.sso"), settings(d, ix, "ui_settings_controller.sso");
    if (!mapper.ok || !settings.ok) { err = "the wheel mapper or the settings controller is not in the cache block once"; return ""; }
    std::string s;
    for (auto& st : Sets()) {
        bool have = st.engine ? settings.Has(CtrlHead("StartEngine")) : mapper.Has(SlotHead(st.slots.front().name));
        s += std::string("  ") + st.name + (have ? ": present" : ": missing") + "\n";
    }
    return s;
}
std::wstring PakOf(const std::wstring& game) { return game + L"\\preload\\paks\\client\\initial.pak"; }
std::wstring AskLine(const wchar_t* prompt, const wchar_t* def) {
    wprintf(L"%s", prompt); if (def && *def) wprintf(L" [%s]", def); wprintf(L": ");
    wchar_t buf[512] = {}; if (!fgetws(buf, 512, stdin)) return def ? def : L"";
    std::wstring s = buf; while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' ')) s.pop_back();
    return s.empty() && def ? def : s;
}

} // namespace

int PakSlotsMenu() {
    wprintf(L"\nWheel binding slots for initial.pak\n===================================\n");
    wprintf(L"A wheel the game does not know gets the Custom preset, which can bind only a fixed list of slots. These sets add\nthe missing ones; the rows appear in the steering wheel tab. A game update replaces the pak: run this again then.\n");
    for (auto& st : Sets()) printf("  %-7s %s\n", st.name, st.about);
    std::vector<std::wstring> games = SteamGameFolders();
    if (games.empty()) { wprintf(L"No SnowRunner install found through Steam. Use: dimerge-setup pakfile <initial.pak> <output.pak>\n"); return 1; }
    wprintf(L"\nGame folders found:\n"); for (size_t i = 0; i < games.size(); i++) wprintf(L"  %zu  %s\n", i + 1, games[i].c_str());
    int pick = _wtoi(AskLine(L"Patch the pak of", L"1").c_str()); if (pick < 1 || pick > (int)games.size()) pick = 1;
    std::wstring pak = PakOf(games[pick - 1]);
    std::vector<uint8_t> bytes; if (!ReadAll(pak, bytes)) { wprintf(L"cannot read %s\n", pak.c_str()); return 1; }
    std::string err, have = Carried(bytes, err);
    if (!err.empty()) { printf("%s\n", err.c_str()); return 1; }
    printf("\nThis pak carries:\n%s", have.c_str());
    if (Exists(pak + L".orig")) wprintf(L"A copy of the untouched pak exists next to it (initial.pak.orig).\n");
    for (;;) {
        std::wstring c = AskLine(L"\n[a] add every missing set  [c] crane only  [e] engine only  [h] hud only  [r] restore the untouched pak  [q] back", L"a");
        if (c == L"q" || c == L"Q") return 0;
        if (c == L"r" || c == L"R") return Restore(pak);
        std::string list = c == L"c" || c == L"C" ? "crane" : c == L"e" || c == L"E" ? "engine" : c == L"h" || c == L"H" ? "hud" : "";
        if (c == L"a" || c == L"A" || !list.empty()) return Apply(pak, PickSets(list));
    }
}

// pak, pakfile <in> <out> [sets] and pakexport <in> <folder> [sets]
int PakCommand(int argc, wchar_t** argv) {
    std::wstring cmd = argv[1];
    if (cmd == L"pak") return PakSlotsMenu();
    if (argc < 4) { wprintf(L"usage: dimerge-setup %s <initial.pak> <%s> [crane,engine,hud]\n", cmd.c_str(), cmd == L"pakfile" ? L"output.pak" : L"folder"); return 1; }
    std::vector<const SetDef*> sets = PickSets(argc > 4 ? Narrow(argv[4]) : "");
    std::vector<uint8_t> bytes; if (!ReadAll(argv[2], bytes)) { wprintf(L"cannot read %s\n", argv[2]); return 1; }
    Zip z; std::vector<Replacement> reps; std::string log, err;
    if (!Prepare(bytes, sets, z, reps, log, err)) { printf("%s\n", err.c_str()); return 1; }
    printf("%s", log.c_str());
    if (cmd == L"pakfile") {
        std::vector<uint8_t> out;
        if (reps.empty()) { printf("nothing missing; the output is the input\n"); out = bytes; }
        else if (!Rebuild(z, reps, out, err) || !Verify(out, z, reps, err)) { printf("%s\n", err.c_str()); return 1; }
        if (!WriteAll(argv[3], out)) { wprintf(L"cannot write %s\n", argv[3]); return 1; }
        wprintf(L"written: %s (%zu bytes, %zu entries replaced)\n", argv[3], out.size(), reps.size());
        return 0;
    }
    // loose files in pak layout: the cache block at the root, the strings files in a [strings] folder
    std::wstring folder = argv[3]; CreateDirectoryW(folder.c_str(), nullptr); CreateDirectoryW((folder + L"\\[strings]").c_str(), nullptr);
    int written = 0;
    for (auto& e : z.entries) {
        if (e.name != CACHE && !IsStrings(e.name)) continue;
        const Replacement* r = nullptr; for (auto& x : reps) if (x.name == e.name) r = &x;
        std::vector<uint8_t> data; if (r) data = r->data; else if (!z.Read(e, data, err)) { printf("%s\n", err.c_str()); return 1; }
        std::wstring name = Widen(e.name); size_t bs = name.rfind(L'\\');
        std::wstring path = e.name == CACHE ? folder + L"\\" + name : folder + L"\\[strings]\\" + (bs == std::wstring::npos ? name : name.substr(bs + 1));
        if (!WriteAll(path, data)) { wprintf(L"cannot write %s\n", path.c_str()); return 1; }
        written++;
    }
    wprintf(L"%d files written to %s\n", written, folder.c_str());
    return 0;
}
