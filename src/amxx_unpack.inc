// amxx_unpack.inc: AMXX Uncompress and Lysis Decompile of the AMXX-Editor package.
// Uncompress: a compiled plugin (.amxx) is unpacked into  .raw (the plugin with expanded code), .amxxmemory (every string of the data
// section, editable) and .amxxdump (the disassembled code); editing the strings and compiling again (F7 in those files) packs a new .amxx.
// Lysis Decompile: the plugin is decompiled by the Lysis web service of the package. Included by main.cpp after hud_editor.inc.

/* ---------------------------------------------------------------- zlib: inflate, and a "stored" deflate (valid for any zlib reader) */
struct InfS { const uint8_t* in; size_t inlen, incnt = 0; int bitbuf = 0, bitcnt = 0; vector<uint8_t>* out; bool err = false; };
struct InfHuff { short count[16]; short symbol[288]; };
static int infBits(InfS& s, int need) {
  long val = s.bitbuf;
  while (s.bitcnt < need) {
    if (s.incnt == s.inlen) { s.err = true; return 0; }
    val |= (long)s.in[s.incnt++] << s.bitcnt;
    s.bitcnt += 8;
  }
  s.bitbuf = (int)(val >> need);
  s.bitcnt -= need;
  return (int)(val & ((1L << need) - 1));
}
static int infConstruct(InfHuff& h, const short* length, int n) {
  for (int len = 0; len <= 15; len++) h.count[len] = 0;
  for (int sym = 0; sym < n; sym++) h.count[length[sym]]++;
  if (h.count[0] == n) return 0;
  int left = 1;
  for (int len = 1; len <= 15; len++) { left <<= 1; left -= h.count[len]; if (left < 0) return left; }
  short offs[16];
  offs[1] = 0;
  for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h.count[len];
  for (int sym = 0; sym < n; sym++) if (length[sym] != 0) h.symbol[offs[length[sym]]++] = (short)sym;
  return left;
}
static int infDecode(InfS& s, const InfHuff& h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len <= 15; len++) {
    code |= infBits(s, 1);
    if (s.err) return -1;
    int count = h.count[len];
    if (code - count < first) return h.symbol[index + (code - first)];
    index += count; first += count; first <<= 1; code <<= 1;
  }
  return -10;
}
static bool infCodes(InfS& s, const InfHuff& lencode, const InfHuff& distcode) {
  static const short lens[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const short dists[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
  static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  while (true) {
    int sym = infDecode(s, lencode);
    if (sym < 0) return false;
    if (sym < 256) s.out->push_back((uint8_t)sym);
    else if (sym == 256) return true;
    else {
      sym -= 257;
      if (sym >= 29) return false;
      int len = lens[sym] + infBits(s, lext[sym]);
      int ds = infDecode(s, distcode);
      if (ds < 0 || ds >= 30) return false;
      size_t dist = dists[ds] + infBits(s, dext[ds]);
      if (s.err || dist > s.out->size()) return false;
      size_t from = s.out->size() - dist;
      for (int i = 0; i < len; i++) s.out->push_back((*s.out)[from + i]);
    }
  }
}
static bool infRaw(const uint8_t* in, size_t n, vector<uint8_t>& out) {
  InfS s;
  s.in = in; s.inlen = n; s.out = &out;
  int last;
  do {
    last = infBits(s, 1);
    int type = infBits(s, 2);
    if (s.err) return false;
    if (type == 0) {
      s.bitbuf = 0; s.bitcnt = 0;
      if (s.incnt + 4 > s.inlen) return false;
      unsigned len = s.in[s.incnt] | (s.in[s.incnt + 1] << 8);
      unsigned nlen = s.in[s.incnt + 2] | (s.in[s.incnt + 3] << 8);
      s.incnt += 4;
      if (len != (~nlen & 0xffff) || s.incnt + len > s.inlen) return false;
      out.insert(out.end(), s.in + s.incnt, s.in + s.incnt + len);
      s.incnt += len;
    } else if (type == 1 || type == 2) {
      InfHuff lencode, distcode;
      short lengths[320];
      if (type == 1) {
        int sym = 0;
        for (; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < 288; sym++) lengths[sym] = 8;
        infConstruct(lencode, lengths, 288);
        for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
        infConstruct(distcode, lengths, 30);
      } else {
        static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        int nlen = infBits(s, 5) + 257, ndist = infBits(s, 5) + 1, ncode = infBits(s, 4) + 4;
        if (s.err || nlen > 286 || ndist > 30) return false;
        int index = 0;
        for (; index < ncode; index++) lengths[order[index]] = (short)infBits(s, 3);
        for (; index < 19; index++) lengths[order[index]] = 0;
        if (infConstruct(lencode, lengths, 19) != 0) return false;
        index = 0;
        while (index < nlen + ndist) {
          int sym = infDecode(s, lencode);
          if (sym < 0) return false;
          if (sym < 16) lengths[index++] = (short)sym;
          else {
            int len = 0, rep;
            if (sym == 16) { if (index == 0) return false; len = lengths[index - 1]; rep = 3 + infBits(s, 2); }
            else if (sym == 17) rep = 3 + infBits(s, 3);
            else rep = 11 + infBits(s, 7);
            if (index + rep > nlen + ndist) return false;
            while (rep--) lengths[index++] = (short)len;
          }
        }
        if (lengths[256] == 0) return false;
        int e = infConstruct(lencode, lengths, nlen);
        if (e < 0 || (e > 0 && nlen - lencode.count[0] != 1)) return false;
        e = infConstruct(distcode, lengths + nlen, ndist);
        if (e < 0 || (e > 0 && ndist - distcode.count[0] != 1)) return false;
      }
      if (!infCodes(s, lencode, distcode)) return false;
    } else return false;
  } while (!last);
  return true;
}
static bool zlibInflate(const uint8_t* in, size_t n, vector<uint8_t>& out) {
  if (n < 6 || (in[0] & 15) != 8) return false;
  return infRaw(in + 2, n - 2, out);
}
static vector<uint8_t> zlibStore(const vector<uint8_t>& d) {
  vector<uint8_t> o = {0x78, 0x01};
  size_t p = 0;
  do {
    size_t len = std::min<size_t>(65535, d.size() - p);
    bool last = p + len >= d.size();
    o.push_back(last ? 1 : 0);
    o.push_back((uint8_t)(len & 255)); o.push_back((uint8_t)(len >> 8));
    o.push_back((uint8_t)(~len & 255)); o.push_back((uint8_t)((~len >> 8) & 255));
    o.insert(o.end(), d.begin() + p, d.begin() + p + len);
    p += len;
  } while (p < d.size());
  uint32_t a = 1, b = 0;
  for (uint8_t c : d) { a = (a + c) % 65521; b = (b + a) % 65521; }
  uint32_t ad = (b << 16) | a;
  for (int sh = 24; sh >= 0; sh -= 8) o.push_back((uint8_t)(ad >> sh));
  return o;
}

/* ---------------------------------------------------------------- the plugin file */
struct AmxxPlug {
  uint32_t magic = 0, disksize = 0, imagesize = 0, memsize = 0, offs = 0;
  uint16_t version = 0;
  uint8_t sections = 0, cellsize = 0;
  vector<uint8_t> data;
  // AMX header
  uint32_t size = 0, cod = 0, dat = 0, hea = 0, stp = 0, cip = 0, publics = 0, natives = 0, libraries = 0, pubvars = 0, tags = 0, nametable = 0;
  uint16_t amagic = 0, flags = 0, defsize = 0;
  uint8_t fver = 0, aver = 0;
  bool dbg = false, compact = false;
  uint32_t dsize = 0;
  uint16_t dmagic = 0;
};
static uint32_t rd32(const vector<uint8_t>& d, size_t o) { return o + 4 <= d.size() ? (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8) | ((uint32_t)d[o + 2] << 16) | ((uint32_t)d[o + 3] << 24) : 0; }
static uint16_t rd16(const vector<uint8_t>& d, size_t o) { return o + 2 <= d.size() ? (uint16_t)(d[o] | (d[o + 1] << 8)) : 0; }
static void wr32(vector<uint8_t>& d, size_t o, uint32_t v) { if (o + 4 <= d.size()) for (int i = 0; i < 4; i++) d[o + i] = (uint8_t)(v >> (8 * i)); }
static void wr16(vector<uint8_t>& d, size_t o, uint16_t v) { if (o + 2 <= d.size()) { d[o] = (uint8_t)v; d[o + 1] = (uint8_t)(v >> 8); } }

static bool amxpReadHeader(AmxxPlug& p, const vector<uint8_t>& f) {
  if (f.size() < 24) return false;
  p.magic = rd32(f, 0);
  if (p.magic != 0x414d5858) return false;
  p.version = rd16(f, 4); p.sections = f[6]; p.cellsize = f[7];
  p.disksize = rd32(f, 8); p.imagesize = rd32(f, 12); p.memsize = rd32(f, 16); p.offs = rd32(f, 20);
  return true;
}
static vector<uint8_t> amxpPackHeader(const AmxxPlug& p) {
  vector<uint8_t> h(24, 0);
  wr32(h, 0, p.magic); wr16(h, 4, p.version); h[6] = p.sections; h[7] = p.cellsize;
  wr32(h, 8, p.disksize); wr32(h, 12, p.imagesize); wr32(h, 16, p.memsize); wr32(h, 20, p.offs);
  return h;
}
static bool amxpAmxHeader(AmxxPlug& p) {
  const vector<uint8_t>& d = p.data;
  if (d.size() < 56) return false;
  p.size = rd32(d, 0); p.amagic = rd16(d, 4); p.fver = d[6]; p.aver = d[7]; p.flags = rd16(d, 8); p.defsize = rd16(d, 10);
  p.cod = rd32(d, 12); p.dat = rd32(d, 16); p.hea = rd32(d, 20); p.stp = rd32(d, 24); p.cip = rd32(d, 28);
  p.publics = rd32(d, 32); p.natives = rd32(d, 36); p.libraries = rd32(d, 40); p.pubvars = rd32(d, 44); p.tags = rd32(d, 48); p.nametable = rd32(d, 52);
  if (p.amagic != 0xf1e0) return false;
  p.dbg = false;
  if (p.flags & 0x02) {
    if (p.size + 22 > d.size()) return false;
    p.dsize = rd32(d, p.size);
    p.dmagic = rd16(d, p.size + 4);
    p.dbg = true;
    if (p.dmagic != 0xf1ef) return false;
  }
  return true;
}
static void amxpWriteAmxHeader(AmxxPlug& p) {
  vector<uint8_t>& d = p.data;
  wr32(d, 0, p.size); wr16(d, 4, p.amagic); d[6] = p.fver; d[7] = p.aver; wr16(d, 8, p.flags); wr16(d, 10, p.defsize);
  wr32(d, 12, p.cod); wr32(d, 16, p.dat); wr32(d, 20, p.hea); wr32(d, 24, p.stp); wr32(d, 28, p.cip);
  wr32(d, 32, p.publics); wr32(d, 36, p.natives); wr32(d, 40, p.libraries); wr32(d, 44, p.pubvars); wr32(d, 48, p.tags); wr32(d, 52, p.nametable);
}
// the code of a "compact" plugin is stored with a variable length encoding; this puts every cell back to 4 bytes
static bool amxpExpand(vector<uint8_t>& code, size_t codesize, size_t memsize) {
  struct Sp { size_t loc; uint32_t val; } spare[64];
  int sh = 0, st = 0, sc = 0;
  if (memsize % 4) return false;
  while (codesize > 0) {
    uint32_t c = 0;
    int shift = 0;
    codesize--;
    c |= (uint32_t)(code[codesize] & 0x7f) << shift;
    shift += 7;
    while (codesize > 0 && (code[codesize - 1] & 0x80)) {
      if (shift >= 32) return false;
      codesize--;
      c |= (uint32_t)(code[codesize] & 0x7f) << shift;
      shift += 7;
    }
    if (code[codesize] & 0x40) while (shift < 32) { c |= (0xffu << shift); shift += 8; }
    while (sc && spare[sh].loc > codesize) { wr32(code, spare[sh].loc, spare[sh].val); sh = (sh + 1) % 64; sc--; }
    if (memsize < 4) return false;
    memsize -= 4;
    if (memsize > codesize || (memsize == codesize && memsize == 0)) wr32(code, memsize, c);
    else { if (sc >= 64) return false; spare[st] = {memsize, c}; st = (st + 1) % 64; sc++; }
  }
  return memsize == 0;
}

/* ---------------------------------------------------------------- strings of the data section */
static bool amxValidChar(uint32_t v, int prev = -1) {
  if (v > 255) return false;
  if ((v >= 32 && v < 127) || v == 9 || v == 10 || v == 13 || v == 11 || v == 12 || (v >= 1 && v <= 4)) return true;
  if (prev < 0) return v >= 194 && v <= 244;
  if (prev >= 194 && prev <= 244) return v >= 128 && v <= 191;
  return false;
}
static bool amxIsString(const AmxxPlug& p, size_t addr) {
  if (addr + 8 > p.data.size()) return false;
  uint32_t dat = rd32(p.data, addr);
  if (!amxValidChar(dat)) return false;
  uint32_t nx = rd32(p.data, addr + 4);
  return amxValidChar(nx, (int)dat) || nx == 0;
}
static wstring amxUtf8(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
  return w;
}
static wstring amxGetString(const AmxxPlug& p, size_t off, size_t& written) {
  std::string b;
  written = 0;
  while (off + written + 4 <= p.data.size()) {
    uint32_t c = rd32(p.data, off + written);
    if (c == 0) break;
    written += 4;
    b += (char)(c & 255);
  }
  return amxUtf8(b);
}
static wstring amxFormat(const wstring& s) {
  wstring o;
  for (wchar_t c : s) {
    if (c == L'\n') o += L"^n"; else if (c == L'\t') o += L"^t"; else if (c == L'"') o += L"^\""; else if (c == L'^') o += L"^^";
    else if (c > 0 && c < 32) { wchar_t b[8]; swprintf_s(b, L"^x%02x", (unsigned)c); o += b; }
    else o += c;
  }
  return o;
}
static wstring amxRaw(const wstring& s) {
  wstring o;
  bool ctrl = false;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == L'^') { if (!ctrl) ctrl = true; else { o += L'^'; ctrl = false; } }
    else if (ctrl) {
      ctrl = false;
      if (s[i] == L't') o += L'\t'; else if (s[i] == L'n') o += L'\n'; else if (s[i] == L'"') o += L'"';
      else if (s[i] == L'x') { o += (wchar_t)wcstol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
      else o += s[i];
    } else o += s[i];
  }
  return o;
}
static int amxpMemoryFile(const AmxxPlug& p, wstring& out) {
  out = L"; Guide: https://forums.alliedmods.net/showthread.php?t=250748\n\n";
  size_t cur = p.dat, end = p.hea;
  int count = 0;
  while (cur < end) {
    if (amxIsString(p, cur)) {
      size_t w;
      wstring s = amxGetString(p, cur, w);
      wchar_t b[32];
      swprintf_s(b, L";data:0x%08X=\"", (unsigned)(cur - p.dat));
      out += b + amxFormat(s) + L"\"\n";
      cur += std::max<size_t>(w, 4);
      count++;
    } else cur += 4;
  }
  return count;
}
static std::string amxToUtf8(const wstring& w) { return toUtf8(w); }
// the edited .amxxmemory file is applied to the data (strings are rewritten in place; [hex ...] writes cells)
static int amxpLoadMemory(AmxxPlug& p, const wstring& text, wstring& log) {
  int count = 0;
  size_t dataBase = p.dat, dataEnd = p.hea, codeBase = p.cod, codeEnd = p.dat;
  auto bad = [&](const wstring& l) { log += L"ERROR Memory File: Parse error, invalid format at line: [" + l + L"]\n"; };
  for (const wstring& raw : splitNl(text)) {
    wstring line = trimW(raw);
    if (line.empty() || line[0] == L';') continue;
    int section;
    if (line.compare(0, 5, L"data:") == 0) section = 1; else if (line.compare(0, 5, L"code:") == 0) section = 2; else { bad(line); continue; }
    if (line.size() < 18) { bad(line); continue; }
    size_t addr = wcstoul(line.substr(7, 8).c_str(), nullptr, 16);
    wchar_t kind = line[16];
    if (kind != L'"' && kind != L'[') { bad(line); continue; }
    wstring mem = line.substr(17);
    auto setCells = [&](size_t off, const wstring& hex) {
      int n = 0;
      std::wistringstream is(hex);
      wstring tok;
      while (is >> tok) { wr32(p.data, off + n * 4, (uint32_t)wcstoul(tok.c_str(), nullptr, 16)); n++; }
      return n;
    };
    if (section == 1) {
      addr += dataBase;
      if (addr > dataEnd) { log += L"ERROR: Bad Addr\n"; continue; }
      if (kind == L'"') {
        bool forced = false;
        if (!mem.empty() && mem.back() == L'f') { mem.pop_back(); forced = true; }
        if (mem.empty() || mem.back() != L'"') { bad(line); continue; }
        wstring ns = amxRaw(mem.substr(0, mem.size() - 1));
        std::string u = amxToUtf8(ns);
        if (addr + u.size() > p.data.size()) { log += L"ERROR: Address out of bounds\n"; continue; }
        size_t oldSize;
        wstring old = amxGetString(p, addr, oldSize);
        size_t off = addr, w = 0;
        while ((forced || (off + 4 <= p.data.size() && p.data[off] != 0)) && w < u.size() && off + 4 <= p.data.size()) { p.data[off] = (uint8_t)u[w]; off += 4; w++; }
        if (off + 1 <= p.data.size()) p.data[off] = 0;
        log += wstring(forced ? L"Replace forced: [" : L"Replace: [") + amxFormat(old) + L"] with [" + amxFormat(ns) + L"] (write " + std::to_wstring(w * 4) + L" bytes)\n";
      } else {
        if (mem.empty() || mem.back() != L']') { bad(line); continue; }
        int n = setCells(addr, mem.substr(0, mem.size() - 1));
        log += L"Write memory: DATA " + std::to_wstring(n) + L" cells\n";
      }
    } else {
      addr += codeBase;
      if (addr > codeEnd) { log += L"ERROR: Bad Addr\n"; continue; }
      if (kind != L'[' || mem.empty() || mem.back() != L']') { bad(line); continue; }
      int n = setCells(addr, mem.substr(0, mem.size() - 1));
      log += L"Write memory: CODE " + std::to_wstring(n) + L" cells\n";
    }
    count++;
  }
  return count;
}

/* ---------------------------------------------------------------- disassembler (the simple one of the package) */
static const char* AMX_OPS[0x8a] = {
    "NONE", "LOAD.pri", "LOAD.alt", "LOAD.S.pri", "LOAD.S.alt", "LREF.pri", "LREF.alt", "LREF.S.pri", "LREF.S.alt", "LOAD.I", "LODB.I", "CONST.pri", "CONST.alt",
    "ADDR.pri", "ADDR.alt", "STOR.pri", "STOR.alt", "STOR.S.pri", "STOR.S.alt", "SREF.pri", "SREF.alt", "SREF.S.pri", "SREF.S.alt", "STOR.I", "STRB.I", "LIDX",
    "LIDX.B", "IDXADDR", "IDXADDR.B", "ALIGN.pri", "ALIGN.alt", "LCTRL", "SCTRL", "MOVE.pri", "MOVE.alt", "XCHG", "PUSH.pri", "PUSH.alt", "PUSH.R", "PUSH.C", "PUSH",
    "PUSH.S", "POP.pri", "POP.alt", "STACK", "HEAP", "PROC", "RET", "RETN", "CALL", "CALL.pri", "JUMP", "JREL", "JZER", "JNZ", "JEQ", "JNEQ", "JLESS", "JLEQ", "JGRTR",
    "JGEQ", "JSLESS", "JSLEQ", "JSGRTR", "JSGEQ", "SHL", "SHR", "SSHR", "SHL.C.pri", "SHL.C.alt", "SHR.C.pri", "SHR.C.alt", "SMUL", "SDIV", "SDIV.alt", "UMUL", "UDIV",
    "UDIV.alt", "ADD", "SUB", "SUB.alt", "AND", "OR", "XOR", "NOT", "NEG", "INVERT", "ADD.C", "SMUL.C", "ZERO.pri", "ZERO.alt", "ZERO", "ZERO.S", "SIGN.pri", "SIGN.alt",
    "EQ", "NEQ", "LESS", "LEQ", "GRTR", "GEQ", "SLESS", "SLEQ", "SGRTR", "SGEQ", "EQ.C.pri", "EQ.C.alt", "INC.pri", "INC.alt", "INC", "INC.S", "INC.I", "DEC.pri",
    "DEC.alt", "DEC", "DEC.S", "DEC.I", "MOVS", "CMPS", "FILL", "HALT", "BOUNDS", "SYSREQ.pri", "SYSREQ.C", "FILE", "LINE", nullptr, "SRANGE", "JUMP.pri", "SWITCH",
    "CASETBL", "SWAP.pri", "SWAP.alt", "PUSH.ADR", "NOP", "SYSREQ.D", "SYMTAG", "BREAK"};
static bool amxOpHasParam(int op) {
  static const std::set<int> none = {0x09, 0x17, 0x19, 0x1b, 0x21, 0x22, 0x23, 0x24, 0x25, 0x2a, 0x2b, 0x2e, 0x2f, 0x30, 0x32, 0x41, 0x42, 0x43, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
                                     0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x59, 0x5a, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
                                     0x6b, 0x6c, 0x6f, 0x70, 0x71, 0x74, 0x7a, 0x80, 0x82, 0x83, 0x84, 0x86, 0x89};
  return !none.count(op);
}
static wstring amxpDisassemble(const AmxxPlug& p) {
  const vector<uint8_t>& d = p.data;
  struct Fn { uint32_t addr; std::string name; };
  auto readName = [&](size_t off) { std::string s; while (off < d.size() && d[off]) s += (char)d[off++]; return s; };
  auto functions = [&](uint32_t a, uint32_t b) {
    vector<Fn> v;
    if (!p.defsize || b < a) return v;
    for (uint32_t o = a; o + p.defsize <= b; o += p.defsize) {
      uint32_t addr = rd32(d, o);
      v.push_back({addr, p.defsize == 8 ? readName(rd32(d, o + 4)) : readName(o + 4)});
    }
    return v;
  };
  vector<Fn> natives = functions(p.natives, p.libraries), publics = functions(p.publics, p.natives);
  size_t cs = p.cod, ce = std::min<size_t>(p.dat, d.size());
  wstring out;
  auto W = [&](const std::string& s) { return amxUtf8(s); };
  for (auto& n : natives) out += L"native: " + W(n.name) + L"()\n";
  for (auto& pu : publics) { wchar_t b[24]; swprintf_s(b, L"0x%08X public ", pu.addr); out += b + W(pu.name) + L"()\n"; }
  out += L"\n";
  auto cellS = [&](size_t i) { return (int32_t)rd32(d, cs + i); };
  auto getString = [&](int32_t off) -> wstring {
    size_t addr = p.dat + (uint32_t)off;
    if (amxIsString(p, addr)) { size_t w; return L"\"" + amxFormat(amxGetString(p, addr, w)) + L"\""; }
    int32_t v = (int32_t)rd32(d, addr);
    return v ? std::to_wstring(v) : L"";
  };
  size_t index = 0, len = ce > cs ? ce - cs : 0;
  while (index < len) {
    uint32_t op = d[cs + index];
    index += 4;
    if (!op) continue;
    wchar_t head[40];
    swprintf_s(head, L"0x%X", (unsigned)(index - 4));
    wstring line;
    wstring operands;
    if (op < 0x8a && AMX_OPS[op]) {
      const char* name = AMX_OPS[op];
      if (amxOpHasParam((int)op)) {
        int32_t v = cellS(index);
        wchar_t hx[24];
        swprintf_s(hx, L"0x%X", (unsigned)v);
        wstring h = hx;
        while (h.size() < 11) h += L' ';
        if (op == 0x7b) operands = h + L" ; " + ((size_t)v < natives.size() ? W(natives[v].name) : L"<no found>") + L"()";
        else if (op == 0x27) operands = h + L" ; " + getString(v);
        else operands = h + L" ; ";
        index += 4;
      }
      if (op == 0x82) {  // CASETBL
        if (index + 8 <= len) {
          int32_t n = cellS(index), def = cellS(index + 4);
          index += 8;
          operands = L"cases:" + std::to_wstring(n) + L" default:" + std::to_wstring(def);
          for (int i = 0; i < n; i++) {
            if (index + 8 <= len) { operands += L" " + std::to_wstring(cellS(index)) + L":" + std::to_wstring(cellS(index + 4)); index += 8; }
            else { operands += L" INVALID_CASE"; break; }
          }
        } else operands = L"INVALID_CASETBL";
      }
      if (op == 0x2e) {
        out += L"\n";
        for (auto& pu : publics) if (pu.addr == index - 4) out += L"public " + W(pu.name) + L"():\n";
      }
      wstring hs = head;
      while (hs.size() < 9) hs += L' ';
      wstring nm = W(name);
      while (nm.size() < 21) nm = L' ' + nm;
      line = hs + L" " + nm + L"  " + operands;
    } else {
      wchar_t b[40];
      swprintf_s(b, L"UNKNOWN_OPCODE 0x%02X", op);
      line = b;
    }
    out += line + L"\n";
  }
  return out;
}

/* ---------------------------------------------------------------- the commands */
static bool amxpReadFile(const wstring& path, vector<uint8_t>& out) {
  vector<char> b;
  if (!readAll(path, b)) return false;
  out.assign(b.begin(), b.end());
  return true;
}
static bool amxpWriteFile(const wstring& path, const void* data, size_t n) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD w = 0;
  bool ok = WriteFile(h, data, (DWORD)n, &w, nullptr) && w == n;
  CloseHandle(h);
  return ok;
}
static bool amxpWriteText(const wstring& path, const wstring& text) {
  std::string u = toUtf8(text);
  return amxpWriteFile(path, u.data(), u.size());
}
static wstring amxpInfo(const AmxxPlug& p) {
  auto n = [](unsigned long long v) { return std::to_wstring(v); };
  auto hx = [](unsigned v) { wchar_t b[16]; swprintf_s(b, L"%x", v); return wstring(b); };
  wstring s = L"; AMXXPacket Header:\n; magic: 0x" + hx(p.magic) + L"  version: 0x" + hx(p.version) + L"  sections: " + n(p.sections) + L"  cellsize: " + n(p.cellsize) +
              L" bytes\n; disksize: " + n(p.disksize) + L"  imagesize: " + n(p.imagesize) + L"  memsize: " + n(p.memsize) + L"  offs: " + n(p.offs) + L"\n";
  s += L"; AMXCode Header:\n; code size: " + n(p.dat - p.cod) + L" bytes  data size: " + n(p.hea - p.dat) + L" bytes  heap size: " + n(p.stp - p.hea) + L" bytes\n";
  if (p.defsize) s += L"; public: " + n((p.natives - p.publics) / p.defsize) + L"  native: " + n((p.libraries - p.natives) / p.defsize) + L"\n";
  s += L"; Flags:";
  if (p.flags & 0x02) s += L" AMX_FLAG_DEBUG";
  if (p.compact) s += L" AMX_FLAG_COMPACT(removed)";
  if (p.flags & 0x08) s += L" AMX_FLAG_BYTEOPC";
  if (p.flags & 0x10) s += L" AMX_FLAG_NOCHECKS";
  s += L"\n\n";
  return s;
}
static wstring amxpExt(const wstring& path) {
  size_t k = path.find_last_of(L'.');
  return k == wstring::npos ? L"" : lowerPath(path.substr(k));
}
static wstring amxpBase(const wstring& path) {
  size_t k = path.find_last_of(L'.');
  return k == wstring::npos ? path : path.substr(0, k);
}
// .amxx -> .raw + .amxxmemory + .amxxdump
static bool amxxUncompressFile(const wstring& path, wstring& msg) {
  vector<uint8_t> f;
  AmxxPlug p;
  if (!amxpReadFile(path, f) || !amxpReadHeader(p, f)) { msg = L"Failed to load the header: not an .amxx plugin."; return false; }
  if ((size_t)p.offs + p.disksize > f.size()) { msg = L"The file is truncated."; return false; }
  vector<uint8_t> img;
  if (!zlibInflate(f.data() + p.offs, p.disksize, img)) { msg = L"Decompression failed."; return false; }
  if (img.size() != p.imagesize) { msg = L"Uncompressed size mismatch."; return false; }
  size_t finalsize = std::max<size_t>(p.memsize, p.imagesize);
  p.sections = 1;
  p.offs = 24;
  p.data = img;
  p.data.resize(finalsize, 0);
  if (!amxpAmxHeader(p)) { msg = L"Failed to read the AMX header of the plugin."; return false; }
  if (!(p.flags & 0x04) && p.hea != p.size) { msg = L"Invalid plugin format."; return false; }
  if (p.flags & 0x04) {
    p.flags &= ~0x04;
    p.compact = true;
    vector<uint8_t> code(p.data.begin() + p.cod, p.data.end());
    if (!amxpExpand(code, p.size - p.cod, p.hea - p.cod)) { msg = L"The code could not be expanded."; return false; }
    vector<uint8_t> debug;
    if (p.dbg) debug.assign(p.data.begin() + p.size, p.data.begin() + p.size + p.dsize);
    vector<uint8_t> nd(p.data.begin(), p.data.begin() + p.cod);
    nd.insert(nd.end(), code.begin(), code.begin() + (p.hea - p.cod));
    nd.insert(nd.end(), debug.begin(), debug.end());
    p.data = nd;
    p.imagesize = p.size = p.hea;
    if (p.dbg) p.imagesize += p.dsize;
  }
  p.data.resize(p.imagesize);
  amxpWriteAmxHeader(p);
  wstring base = amxpBase(path);
  {
    vector<uint8_t> raw = amxpPackHeader(p);
    raw.insert(raw.end(), p.data.begin(), p.data.end());
    if (!amxpWriteFile(base + L".raw", raw.data(), raw.size())) { msg = L"Cannot write the .raw file."; return false; }
  }
  wstring mem;
  int strings = amxpMemoryFile(p, mem);
  if (!amxpWriteText(base + L".amxxmemory", mem)) { msg = L"Cannot write the .amxxmemory file."; return false; }
  if (!amxpWriteText(base + L".amxxdump", amxpInfo(p) + amxpDisassemble(p))) { msg = L"Cannot write the .amxxdump file."; return false; }
  msg = L"Uncompressed " + baseName(path) + L": " + std::to_wstring(strings) + L" strings" + (p.compact ? L", code expanded" : L"");
  return true;
}
// .raw + the edited .amxxmemory -> a new .amxx (the strings of the memory file are written into the plugin)
static bool amxxRebuildFile(const wstring& anyOfThem, wstring& msg, wstring& log) {
  wstring base = amxpBase(anyOfThem);
  vector<uint8_t> f;
  AmxxPlug p;
  if (!amxpReadFile(base + L".raw", f) || !amxpReadHeader(p, f)) { msg = L"There is no " + baseName(base) + L".raw next to it (use AMXX Uncompress first)."; return false; }
  if (24 + (size_t)p.imagesize > f.size()) { msg = L"The .raw file is truncated."; return false; }
  p.data.assign(f.begin() + 24, f.begin() + 24 + p.imagesize);
  if (!amxpAmxHeader(p) || (p.flags & 0x04)) { msg = L"The .raw file is not valid."; return false; }
  int edits = 0;
  vector<char> mb;
  if (readAll(base + L".amxxmemory", mb)) edits = amxpLoadMemory(p, decodeBytes(std::string(mb.begin(), mb.end())), log);
  vector<uint8_t> z = zlibStore(p.data);
  p.disksize = (uint32_t)z.size();
  vector<uint8_t> outf = amxpPackHeader(p);
  outf.insert(outf.end(), z.begin(), z.end());
  if (!amxpWriteFile(base + L".amxx", outf.data(), outf.size())) { msg = L"Cannot write " + baseName(base) + L".amxx."; return false; }
  msg = L"Created " + baseName(base) + L".amxx" + (edits ? L" with " + std::to_wstring(edits) + L" memory edit" + (edits == 1 ? L"" : L"s") : L" (no memory edits)");
  return true;
}
static bool amxpIsUnpackedDoc(const Doc& d) {
  wstring e = amxpExt(d.path);
  return e == L".amxxmemory" || e == L".amxxdump" || e == L".raw";
}
static wstring g_testAmxPick;  // test driver: the file the open dialog "chooses"
static void amxxUncompress() {
  wchar_t buf[MAX_PATH * 2] = L"";
  OPENFILENAMEW o{};
  o.lStructSize = sizeof(o);
  o.hwndOwner = g_hwnd;
  o.lpstrFile = buf;
  o.nMaxFile = MAX_PATH * 2;
  o.lpstrFilter = L"AMX Mod X plugins (*.amxx)\0*.amxx\0All files\0*.*\0";
  o.lpstrTitle = L"AMXX Uncompress";
  o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!g_testAmxPick.empty()) wcsncpy_s(buf, g_testAmxPick.c_str(), MAX_PATH * 2 - 1);
  else if (!GetOpenFileNameW(&o)) return;
  wstring path = buf, msg;
  if (amxpExt(path) != L".amxx") { flash(L"Choose a compiled plugin (.amxx)."); return; }
  if (!amxxUncompressFile(path, msg)) { appMsgBox(g_hwnd, msg.c_str(), L"AMXX Uncompress", MB_ICONERROR); return; }
  wstring base = amxpBase(path);
  if (g_layout == 0) exec(C_LAYOUT2);
  if (g_curGroup != 0) focusGroup(0);
  openPath(base + L".amxxdump");
  openPath(base + L".amxxmemory");
  if (g_layout != 0) exec(C_MOVETAB);
  flash(msg + L". Edit the strings in the .amxxmemory file, then press F7 there to pack a new .amxx.");
}
static void amxxRebuildFromDoc(Doc& d) {
  if (d.dirty) doSave(d);
  wstring msg, log;
  bool ok = amxxRebuildFile(d.path, msg, log);
  if (!ok) { appMsgBox(g_hwnd, msg.c_str(), L"AMXX Uncompress", MB_ICONERROR); return; }
  flash(msg);
  if (!log.empty()) {  // what was changed, or what could not be read
    Doc* nd = new Doc();
    nd->lines = splitNl(log);
    nd->name = L"[AMXX] " + baseName(amxpBase(d.path)) + L" memory edits";
    nd->invalidate(0);
    addDoc(nd);
  }
}

/* ---------------------------------------------------------------- Lysis decompile */
// The upload address of the AMXX-Editor package (headlinedev.xyz/lysis/upload.php) moved to a page that checks the browser with a
// proof of work before it accepts a file, so the plugin is decompiled on the website: it is opened in the browser.
static void amxxLysis() {
  ShellExecuteW(g_hwnd, L"open", L"https://lysis.headlinedev.xyz", nullptr, nullptr, SW_SHOWNORMAL);
  flash(L"Lysis opened in the browser: drop the .amxx plugin on the page to get the Pawn source.");
}
