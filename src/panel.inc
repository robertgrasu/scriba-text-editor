// panel.inc: the panels under the editor: Find in Files and Run Command (and the build results of AMX Mod X later).
// Included by main.cpp after the settings page.
#include <thread>
#include <atomic>
#include <mutex>

#define WM_FIF_DONE (WM_APP + 1)
#define WM_RUN_DATA (WM_APP + 2)
#define WM_RUN_EXIT (WM_APP + 3)

static bool wildMatch(const wchar_t* p, const wchar_t* s) {  // * and ? only, case insensitive
  while (*p) {
    if (*p == L'*') {
      while (*p == L'*') p++;
      if (!*p) return true;
      for (; *s; s++) if (wildMatch(p, s)) return true;
      return false;
    }
    if (!*s) return false;
    if (*p != L'?' && towlower(*p) != towlower(*s)) return false;
    p++;
    s++;
  }
  return !*s;
}
static bool anyWild(const vector<wstring>& pats, const wstring& name, const wstring& full) {
  for (auto& p0 : pats) {
    if (p0.empty()) continue;
    wstring p = p0;
    for (auto& c : p) if (c == L'\\') c = L'/';
    bool hasSlash = p.find(L'/') != wstring::npos;
    if (!hasSlash) { if (wildMatch(p.c_str(), name.c_str())) return true; }
    else {
      wstring f = full;
      for (auto& c : f) if (c == L'\\') c = L'/';
      if (wildMatch(p.c_str(), f.c_str()) || wildMatch((L"*/" + p).c_str(), f.c_str())) return true;
    }
  }
  return false;
}
static wstring baseName(const wstring& p) {
  size_t k = p.find_last_of(L"\\/");
  return k == wstring::npos ? p : p.substr(k + 1);
}

/* ---------------------------------------------------------------- find in files */
struct FifMatch { int line, col, len; wstring text; int start; };
struct FifFile { wstring file; vector<FifMatch> m; };
struct FifResult { vector<FifFile> files; int count = 0, ms = 0; bool truncated = false; wstring error; int gen = 0; };
struct FifOpts {
  wstring query, where;
  bool regex = false, cs = false, word = false;
  vector<wstring> roots, exFolders, exFiles, binary;
  int gen = 0;
};
static LineEdit g_fifQ, g_fifWhere, g_fifRep;
static bool g_fifRegex = false, g_fifCase = false, g_fifWord = false;
static bool g_fifOpen = false, g_runOpen = false, g_buildOpen = false;
static float g_fifH = 260, g_runH = 220, g_buildH = 200;
static int g_panelFocus = 0;  // 0 none, 1 find in files, 2 run command
static int g_fifField = 0;    // 0 find, 1 where, 2 replace
static wstring g_fifSum;
static FifResult g_fifRes;
static bool g_fifHave = false, g_fifBusy = false;
static float g_fifScroll = 0, g_buildScroll = 0;
static int g_fifCursor = -1;  // index among all matches
static std::atomic<int> g_fifGen{0};
static std::atomic<bool> g_fifCancel{false};
static std::mutex g_panelMu;

static std::string readFileRaw(const wstring& path, size_t maxBytes, bool& tooBig) {
  tooBig = false;
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return "";
  LARGE_INTEGER sz;
  GetFileSizeEx(h, &sz);
  if ((size_t)sz.QuadPart > maxBytes) { CloseHandle(h); tooBig = true; return ""; }
  std::string b((size_t)sz.QuadPart, 0);
  DWORD got = 0;
  if (sz.QuadPart) ReadFile(h, b.data(), (DWORD)b.size(), &got, nullptr);
  CloseHandle(h);
  b.resize(got);
  return b;
}
// bytes of a file as text (UTF-8, UTF-16 with BOM or Windows 1252)
static wstring decodeBytes(const std::string& b) {
  if (b.size() >= 2 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xFE) return wstring((const wchar_t*)(b.data() + 2), (b.size() - 2) / 2);
  if (b.size() >= 2 && (unsigned char)b[0] == 0xFE && (unsigned char)b[1] == 0xFF) {
    wstring w((b.size() - 2) / 2, 0);
    for (size_t i = 0; i < w.size(); i++) w[i] = (wchar_t)(((unsigned char)b[2 + i * 2] << 8) | (unsigned char)b[3 + i * 2]);
    return w;
  }
  size_t off = (b.size() >= 3 && (unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF) ? 3 : 0;
  bool utf8 = b.size() == off || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, b.data() + off, (int)(b.size() - off), nullptr, 0) > 0;
  UINT cp = utf8 ? CP_UTF8 : 1252;
  int n = MultiByteToWideChar(cp, 0, b.data() + off, (int)(b.size() - off), nullptr, 0);
  wstring w(n, 0);
  if (n) MultiByteToWideChar(cp, 0, b.data() + off, (int)(b.size() - off), w.data(), n);
  return w;
}
static wstring regexEscape(const wstring& s) {
  wstring r;
  for (wchar_t c : s) { if (wcschr(L".*+?^${}()|[]\\", c)) r += L'\\'; r += c; }
  return r;
}
static std::wregex buildRegex(const FifOpts& o) {
  wstring pat = o.regex ? o.query : regexEscape(o.query);
  if (o.word) pat = L"\\b(?:" + pat + L")\\b";
  return std::wregex(pat, o.cs ? std::regex::ECMAScript : (std::regex::ECMAScript | std::regex::icase));
}
static void fifWalk(const wstring& root, const FifOpts& o, const std::function<bool(const wstring&)>& onFile) {
  vector<wstring> stack{root};
  while (!stack.empty() && !g_fifCancel) {
    wstring dir = stack.back();
    stack.pop_back();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) continue;
    vector<std::pair<wstring, bool>> ents;
    do {
      wstring nm = fd.cFileName;
      if (nm == L"." || nm == L"..") continue;
      ents.push_back({nm, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0});
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(ents.begin(), ents.end(), [](auto& a, auto& b) { return _wcsicmp(a.first.c_str(), b.first.c_str()) < 0; });
    vector<wstring> subdirs;
    for (auto& e : ents) {
      wstring full = dir + L"\\" + e.first;
      if (e.second) { if (!anyWild(o.exFolders, e.first, full)) subdirs.push_back(full); }
      else if (!anyWild(o.exFiles, e.first, full)) { if (!onFile(full)) return; }
    }
    for (int i = (int)subdirs.size() - 1; i >= 0; i--) stack.push_back(subdirs[i]);
  }
}
static FifResult fifSearch(const FifOpts& o) {
  FifResult res;
  res.gen = o.gen;
  DWORD t0 = GetTickCount();
  std::wregex re;
  bool useRe = o.regex;
  try { if (useRe) re = buildRegex(o); }
  catch (std::exception& e) { res.error = fromUtf8(e.what()); return res; }
  // where: folders, -excluded globs, plain globs
  vector<wstring> roots, inc, exc;
  {
    size_t p = 0;
    while (p <= o.where.size()) {
      size_t q = o.where.find(L',', p);
      if (q == wstring::npos) q = o.where.size();
      wstring tok = trimW(o.where.substr(p, q - p));
      p = q + 1;
      if (tok.empty()) continue;
      if (tok == L"<open folders>") { for (auto& r : o.roots) roots.push_back(r); continue; }
      if (tok[0] == L'-') { exc.push_back(tok.substr(1)); continue; }
      DWORD at = GetFileAttributesW(tok.c_str());
      if (tok.size() > 2 && tok[1] == L':' && at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) { roots.push_back(tok); continue; }
      inc.push_back(tok);
    }
    if (roots.empty()) roots = o.roots;
  }
  wstring lq = lowerStr(o.query);
  const int maxResults = 5000;
  for (auto& root : roots) {
    fifWalk(root, o, [&](const wstring& f) {
      wstring name = baseName(f);
      if (anyWild(o.binary, name, f)) return true;
      if (!inc.empty() && !anyWild(inc, name, f)) return true;
      if (!exc.empty() && anyWild(exc, name, f)) return true;
      bool big;
      std::string raw = readFileRaw(f, 4 * 1024 * 1024, big);
      if (big || raw.empty()) return true;
      bool bom16 = raw.size() >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE;
      if (!bom16 && memchr(raw.data(), 0, std::min<size_t>(raw.size(), 8000))) return true;
      wstring text = decodeBytes(raw);
      FifFile ff;
      ff.file = f;
      size_t p = 0;
      int lineNo = 0;
      while (p <= text.size() && !g_fifCancel) {
        size_t q = text.find(L'\n', p);
        if (q == wstring::npos) q = text.size();
        wstring line = text.substr(p, q - p);
        p = q + 1;
        lineNo++;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        auto add = [&](int idx, int len) {
          FifMatch m;
          m.line = lineNo;
          m.col = idx;
          m.len = len;
          int off = 0;
          wstring shown = line;
          if (shown.size() > 400) { off = std::max(0, idx - 100); shown = (off ? L"\u2026" : L"") + shown.substr(off, 300); }
          m.text = shown;
          m.start = idx - off + (off ? 1 : 0);
          ff.m.push_back(m);
          res.count++;
        };
        if (useRe) {
          try {
            auto begin = std::wsregex_iterator(line.begin(), line.end(), re), end = std::wsregex_iterator();
            for (auto it = begin; it != end; ++it) {
              if (it->length() == 0) continue;
              add((int)it->position(), (int)it->length());
              if (res.count >= maxResults) break;
            }
          } catch (...) {}
        } else if (!lq.empty()) {
          wstring ll = o.cs ? line : lowerStr(line);
          const wstring& needle = o.cs ? o.query : lq;
          size_t at = 0;
          while ((at = ll.find(needle, at)) != wstring::npos) {
            bool ok = true;
            if (o.word) {
              if (at > 0 && isIdCh(line[at - 1])) ok = false;
              if (at + needle.size() < line.size() && isIdCh(line[at + needle.size()])) ok = false;
            }
            if (ok) { add((int)at, (int)needle.size()); if (res.count >= maxResults) break; }
            at += std::max<size_t>(1, needle.size());
          }
        }
        if (res.count >= maxResults) { res.truncated = true; break; }
      }
      if (!ff.m.empty()) res.files.push_back(ff);
      return !res.truncated && !g_fifCancel;
    });
    if (res.truncated) break;
  }
  res.ms = (int)(GetTickCount() - t0);
  return res;
}
static void fifRun() {
  if (g_fifQ.t.empty()) return;
  if (!g_root && trimW(g_fifWhere.t).empty()) { g_fifSum = L"Open a folder first, or enter a folder path in \u201CWhere\u201D."; redraw(); return; }
  g_fifCancel = true;
  FifOpts o;
  o.query = g_fifQ.t;
  o.where = g_fifWhere.t;
  o.regex = g_fifRegex;
  o.cs = g_fifCase;
  o.word = g_fifWord;
  for (auto* rn : g_roots) o.roots.push_back(rn->path);
  o.exFolders = g_folderEx;
  o.exFiles = g_fileEx;
  o.binary = g_binaryPat;
  o.gen = ++g_fifGen;
  g_fifSum = L"Searching\u2026";
  g_fifHave = false;
  g_fifBusy = true;
  g_fifScroll = 0;
  g_fifCursor = -1;
  redraw();
  HWND hw = g_hwnd;
  std::thread([o, hw]() {
    // wait for a previous search to notice the cancellation
    Sleep(20);
    g_fifCancel = false;
    auto* r = new FifResult(fifSearch(o));
    PostMessageW(hw, WM_FIF_DONE, 0, (LPARAM)r);
  }).detach();
}
static void fifDone(FifResult* r) {
  if (r->gen != g_fifGen) { delete r; return; }
  g_fifRes = *r;
  delete r;
  g_fifBusy = false;
  g_fifHave = true;
  if (!g_fifRes.error.empty()) { g_fifSum = L"Invalid pattern: " + g_fifRes.error; g_fifHave = false; }
  else {
    int files = (int)g_fifRes.files.size();
    g_fifSum = std::to_wstring(g_fifRes.count) + (g_fifRes.truncated ? L"+" : L"") + L" match" + (g_fifRes.count == 1 ? L"" : L"es") + L" in " + std::to_wstring(files) +
               L" file" + (files == 1 ? L"" : L"s") + L" (" + std::to_wstring(g_fifRes.ms) + L" ms)";
  }
  redraw();
}
static void openAt(const wstring& path, int line1, int col, int len) {
  openPath(path);
  Doc& d = D();
  int l = std::max(0, std::min((int)d.lines.size() - 1, line1 - 1));
  d.cur = {l, std::min(col + len, (int)d.lines[l].size())};
  d.anc = {l, std::min(col, (int)d.lines[l].size())};
  revealCenter();
  moved(true);
}
static int fifMatchTotal() { int n = 0; for (auto& f : g_fifRes.files) n += (int)f.m.size(); return n; }
static void fifOpenCursor() {
  int k = g_fifCursor;
  for (auto& f : g_fifRes.files) {
    if (k < (int)f.m.size()) { openAt(f.file, f.m[k].line, f.m[k].col, f.m[k].len); return; }
    k -= (int)f.m.size();
  }
}
static bool fifNext(int dir) {
  int n = fifMatchTotal();
  if (!n) return false;
  g_fifCursor = g_fifCursor < 0 ? (dir > 0 ? 0 : n - 1) : (g_fifCursor + dir + n) % n;
  fifOpenCursor();
  // keep the row in view
  int k = g_fifCursor, rowIdx = 0;
  for (auto& f : g_fifRes.files) { rowIdx++; if (k < (int)f.m.size()) { rowIdx += k; break; } k -= (int)f.m.size(); rowIdx += (int)f.m.size(); }
  float y = rowIdx * 18.f;
  float viewH = g_fifH - 77;
  if (y < g_fifScroll) g_fifScroll = y;
  else if (y + 18 > g_fifScroll + viewH) g_fifScroll = y + 18 - viewH;
  redraw();
  return true;
}
static void reloadDocFromDisk(Doc& d) {
  Pos c = d.cur;
  float sy = d.scrollY;
  if (!loadFile(d.path, d)) return;
  d.cur = d.anc = {std::min(c.l, (int)d.lines.size() - 1), 0};
  d.cur.c = d.anc.c = std::min(c.c, (int)d.lines[d.cur.l].size());
  d.scrollY = sy;
  d.invalidate(0);
}
static void fifReplaceAll() {
  if (!g_fifHave || g_fifRes.files.empty()) { fifRun(); return; }
  vector<wstring> skipped, files;
  int n = 0;
  for (auto& f : g_fifRes.files) {
    bool dirty = false;
    for (auto* d : g_docs) if (!d->path.empty() && _wcsicmp(d->path.c_str(), f.file.c_str()) == 0 && d->dirty) dirty = true;
    if (dirty) skipped.push_back(f.file); else { files.push_back(f.file); n += (int)f.m.size(); }
  }
  wstring msg = L"Replace " + std::to_wstring(n) + L" occurrence" + (n == 1 ? L"" : L"s") + L" of \u201C" + g_fifQ.t + L"\u201D with \u201C" + g_fifRep.t + L"\u201D in " + std::to_wstring(files.size()) + L" file" + (files.size() == 1 ? L"" : L"s") + L"?";
  if (!skipped.empty()) msg += L"\n\n" + std::to_wstring(skipped.size()) + L" file(s) with unsaved changes will be skipped.";
  if (appMsgBox(g_hwnd, msg.c_str(), L"Replace in Files", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
  FifOpts o;
  o.query = g_fifQ.t;
  o.regex = g_fifRegex;
  o.cs = g_fifCase;
  o.word = g_fifWord;
  std::wregex re;
  try { re = buildRegex(o); }
  catch (std::exception& e) { g_fifSum = L"Invalid pattern: " + fromUtf8(e.what()); redraw(); return; }
  int nfiles = 0, repl = 0, errors = 0;
  for (auto& f : files) {
    Doc tmp;
    if (!loadFile(f, tmp)) { errors++; continue; }
    int cnt = 0;
    for (auto& ln : tmp.lines) {
      wstring out;
      auto begin = std::wsregex_iterator(ln.begin(), ln.end(), re), end = std::wsregex_iterator();
      size_t last = 0;
      for (auto it = begin; it != end; ++it) {
        if (it->length() == 0) continue;
        out += ln.substr(last, it->position() - last);
        out += g_fifRegex ? it->format(g_fifRep.t, std::regex_constants::format_default) : g_fifRep.t;
        last = it->position() + it->length();
        cnt++;
      }
      if (last) { out += ln.substr(last); ln = out; }
    }
    if (cnt) { if (saveFile(tmp, f)) { nfiles++; repl += cnt; } else errors++; }
  }
  g_fifSum = L"Replaced " + std::to_wstring(repl) + L" occurrence" + (repl == 1 ? L"" : L"s") + L" in " + std::to_wstring(nfiles) + L" file" + (nfiles == 1 ? L"." : L"s.") + (errors ? L" " + std::to_wstring(errors) + L" error(s)." : L"");
  g_fifHave = false;
  g_fifRes = FifResult();
  for (auto* d : g_docs) {
    if (d->path.empty() || d->dirty) continue;
    for (auto& f : files) if (_wcsicmp(d->path.c_str(), f.c_str()) == 0) { reloadDocFromDisk(*d); break; }
  }
  redraw();
}

/* ---------------------------------------------------------------- run command */
struct RunLink { int a, b; wstring file; int line, col; };
struct RunLine { wstring text; int kind = 0; vector<RunLink> links; };  // kind: 0 normal, 1 command, 2 error, 3 ok, 4 end
static LineEdit g_runIn;
static vector<RunLine> g_runOut;
static wstring g_runPartial;  // the unfinished last line
static int g_runPartialKind = 0;
static float g_runScroll = 0;
static bool g_runStick = true;
static wstring g_runCwd;
static vector<wstring> g_runHistory;
static int g_runHpos = -1;
static HANDLE g_runProc = nullptr, g_runJob = nullptr;
static std::atomic<int> g_runId{0};
static bool g_runBusy = false;
static size_t g_runChars = 0;

static wstring stripAnsi(const wstring& s) {
  wstring o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == 0x1b && i + 1 < s.size() && s[i + 1] == L'[') {
      size_t j = i + 2;
      while (j < s.size() && ((s[j] >= L'0' && s[j] <= L'?') || s[j] == L' ' || (s[j] >= L'!' && s[j] <= L'/'))) j++;
      i = j;  // the final byte of the sequence
      continue;
    }
    o += s[i];
  }
  return o;
}
// file.ext:12:5 links inside an output line
static void findLinks(RunLine& rl) {
  const wstring& s = rl.text;
  size_t i = 0;
  while (i < s.size()) {
    // look for ":digits" preceded by a file name with an extension
    size_t c = s.find(L':', i);
    if (c == wstring::npos || c + 1 >= s.size()) break;
    if (!iswdigit(s[c + 1])) { i = c + 1; continue; }
    size_t b = c;
    while (b > 0 && !iswspace(s[b - 1]) && !wcschr(L"\"'()<>|", s[b - 1])) b--;
    // a drive letter keeps its colon: C:\dir\file.c:12
    if (b + 1 < s.size() && c == b + 1 && b < s.size()) { size_t c2 = s.find(L':', c + 1); if (c2 != wstring::npos && iswdigit(s[c2 + 1 < s.size() ? c2 + 1 : c2])) { c = c2; } else { i = c + 1; continue; } }
    wstring file = s.substr(b, c - b);
    size_t dot = file.find_last_of(L'.');
    if (file.empty() || dot == wstring::npos || file.size() - dot > 7 || file.size() - dot < 2) { i = c + 1; continue; }
    size_t e = c + 1;
    int line = 0, col = 0;
    while (e < s.size() && iswdigit(s[e])) { line = line * 10 + (s[e] - L'0'); e++; }
    if (e < s.size() && s[e] == L':' && e + 1 < s.size() && iswdigit(s[e + 1])) { e++; while (e < s.size() && iswdigit(s[e])) { col = col * 10 + (s[e] - L'0'); e++; } }
    rl.links.push_back({(int)b, (int)e, file, line, col});
    i = e;
  }
}
static void runAppend(const wstring& chunk0, int kind) {
  wstring chunk = stripAnsi(chunk0);
  // a lone CR (progress output) acts as a new line
  wstring t;
  for (size_t i = 0; i < chunk.size(); i++) {
    if (chunk[i] == L'\r') { if (i + 1 < chunk.size() && chunk[i + 1] == L'\n') continue; t += L'\n'; }
    else t += chunk[i];
  }
  g_runChars += t.size();
  size_t p = 0;
  while (p <= t.size()) {
    size_t q = t.find(L'\n', p);
    if (q == wstring::npos) { g_runPartial += t.substr(p); g_runPartialKind = kind; break; }
    RunLine rl;
    rl.text = g_runPartial + t.substr(p, q - p);
    rl.kind = kind ? kind : g_runPartialKind;
    g_runPartial.clear();
    findLinks(rl);
    g_runOut.push_back(rl);
    p = q + 1;
  }
  while (g_runChars > 400000 && !g_runOut.empty()) { g_runChars -= std::min(g_runChars, g_runOut.front().text.size() + 1); g_runOut.erase(g_runOut.begin()); }
  if (g_runStick) g_runScroll = 1e9f;
  redraw();
}
static wstring runDefaultCwd() {
  if (!g_docs.empty() && !D().path.empty()) { wstring p = D().path; size_t k = p.find_last_of(L"\\/"); if (k != wstring::npos) return p.substr(0, k); }
  if (g_root) return g_root->path;
  wchar_t home[MAX_PATH];
  if (GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH)) return home;
  return L"C:\\";
}
struct RunChunk { int id; bool err; wstring text; };
static void runPipeThread(HANDLE h, int id, bool err, HWND hw) {
  char buf[8192];
  std::string pending;
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(h, buf, sizeof(buf), &got, nullptr) || got == 0) break;
    pending.append(buf, got);
    // keep an incomplete UTF-8 sequence for the next read
    size_t cut = pending.size();
    while (cut > 0 && ((unsigned char)pending[cut - 1] & 0xC0) == 0x80) cut--;
    if (cut > 0 && ((unsigned char)pending[cut - 1] & 0xC0) == 0xC0) {
      unsigned char lead = (unsigned char)pending[cut - 1];
      size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
      if (pending.size() - (cut - 1) < need) cut--; else cut = pending.size();
    } else cut = pending.size();
    std::string use = pending.substr(0, cut);
    pending.erase(0, cut);
    if (use.empty()) continue;
    // console programs write the OEM code page; most tools write UTF-8: take UTF-8 when valid
    bool utf8 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, use.data(), (int)use.size(), nullptr, 0) > 0;
    UINT cp = utf8 ? CP_UTF8 : GetOEMCP();
    int n = MultiByteToWideChar(cp, 0, use.data(), (int)use.size(), nullptr, 0);
    wstring w(n, 0);
    if (n) MultiByteToWideChar(cp, 0, use.data(), (int)use.size(), w.data(), n);
    PostMessageW(hw, WM_RUN_DATA, 0, (LPARAM) new RunChunk{id, err, w});
  }
}
static void runStop() {
  if (g_runBusy && g_runJob) TerminateJobObject(g_runJob, 1);
}
static void runStart(const wstring& cmd0) {
  wstring cmd = trimW(cmd0);
  if (cmd.empty() || g_runBusy) return;
  g_runCwd = runDefaultCwd();
  if (g_runHistory.empty() || g_runHistory.back() != cmd) g_runHistory.push_back(cmd);
  g_runHpos = -1;
  g_runIn.set(cmd);
  runAppend(L"> " + cmd + L"\n", 1);
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE outR, outW, errR, errW;
  if (!CreatePipe(&outR, &outW, &sa, 0) || !CreatePipe(&errR, &errW, &sa, 0)) { runAppend(L"Could not start the command.\n", 2); return; }
  SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(errR, HANDLE_FLAG_INHERIT, 0);
  HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = outW;
  si.hStdError = errW;
  si.hStdInput = nul;
  PROCESS_INFORMATION pi{};
  wstring line = L"cmd.exe /d /s /c \"" + cmd + L"\"";
  vector<wchar_t> buf(line.begin(), line.end());
  buf.push_back(0);
  BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, g_runCwd.c_str(), &si, &pi);
  CloseHandle(outW);
  CloseHandle(errW);
  if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
  if (!ok) { runAppend(L"Could not start the command.\n", 2); CloseHandle(outR); CloseHandle(errR); return; }
  g_runJob = CreateJobObjectW(nullptr, nullptr);
  if (g_runJob) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
    li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(g_runJob, JobObjectExtendedLimitInformation, &li, sizeof(li));
    AssignProcessToJobObject(g_runJob, pi.hProcess);
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  g_runProc = pi.hProcess;
  g_runBusy = true;
  int id = ++g_runId;
  HWND hw = g_hwnd;
  HANDLE proc = pi.hProcess;
  std::thread([=]() {
    std::thread t1(runPipeThread, outR, id, false, hw), t2(runPipeThread, errR, id, true, hw);
    t1.join();
    t2.join();
    CloseHandle(outR);
    CloseHandle(errR);
    WaitForSingleObject(proc, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(proc, &code);
    PostMessageW(hw, WM_RUN_EXIT, (WPARAM)id, (LPARAM)code);
  }).detach();
  redraw();
}
static void runData(RunChunk* c) {
  if (c->id == g_runId) runAppend(c->text, c->err ? 2 : 0);
  delete c;
}
static void runExit(int id, DWORD code) {
  if (id != g_runId) return;
  bool killed = code == 1 && g_runJob != nullptr && false;
  (void)killed;
  if (!g_runPartial.empty()) runAppend(L"\n", g_runPartialKind);
  runAppend(L"\n[exit code " + std::to_wstring((int)code) + L"]\n", code == 0 ? 3 : 4);
  if (g_runProc) { CloseHandle(g_runProc); g_runProc = nullptr; }
  if (g_runJob) { CloseHandle(g_runJob); g_runJob = nullptr; }
  g_runBusy = false;
  redraw();
}

/* ---------------------------------------------------------------- layout of the panels */
static float panelsHeight() { return (g_fifOpen ? g_fifH : 0.f) + (g_buildOpen ? g_buildH : 0.f) + (g_runOpen ? g_runH : 0.f); }
static float panelsLeft() { return g_sbVisible ? g_sbW : 0.f; }
// rectangles from the top: find in files, build results, run
static D2D1_RECT_F panelRect(int which) {  // 1 fif, 2 build, 3 run
  float bottom = ch() - STATUS_H, left = panelsLeft(), right = ccLeft();
  float yRun = bottom - (g_runOpen ? g_runH : 0.f);
  float yBuild = yRun - (g_buildOpen ? g_buildH : 0.f);
  float yFif = yBuild - (g_fifOpen ? g_fifH : 0.f);
  if (which == 3) return D2D1::RectF(left, yRun, right, bottom);
  if (which == 2) return D2D1::RectF(left, yBuild, right, yRun);
  return D2D1::RectF(left, yFif, right, yBuild);
}
static IDWriteTextFormat* monoFont(float size) {
  static std::map<int, IDWriteTextFormat*> cache;
  static unsigned epoch = ~0u;
  if (epoch != g_fontEpoch) { for (auto& kv : cache) if (kv.second) kv.second->Release(); cache.clear(); epoch = g_fontEpoch; }
  int key = (int)std::lround(size * 10);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  IDWriteTextFormat* f = nullptr;
  g_dw->CreateTextFormat(g_fontFace.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &f);
  if (f) f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  cache[key] = f;
  return f;
}
struct PHit { D2D1_RECT_F r; int kind; int idx; };
enum { P_FIF_Q = 1, P_FIF_WHERE, P_FIF_REP, P_FIF_RE, P_FIF_CASE, P_FIF_WORD, P_FIF_FIND, P_FIF_CLOSE, P_FIF_REPLACE, P_FIF_ROW, P_FIF_RES,
       P_RUN_IN, P_RUN_RUN, P_RUN_STOP, P_RUN_CLEAR, P_RUN_CLOSE, P_RUN_OUT, P_RESIZE_FIF, P_RESIZE_RUN, P_RESIZE_BUILD,
       P_BUILD_CLOSE, P_BUILD_AGAIN, P_BUILD_RAW, P_BUILD_PICK, P_BUILD_ROW, P_BUILD_LIST };
static void drawBuild();  // the AMX Mod X build results (amxx.inc)
static bool buildClick(const PHit& h);
static vector<PHit> g_pHits;
static void panelToggle(const D2D1_RECT_F& r, const wchar_t* label, bool on, bool hover) {
  const Theme& t = T();
  if (on || hover) fillR(r, t.btn, 1.f, 3);
  if (on) strokeR(r, t.accent, 1.f, 3);
  IDWriteTextFormat* f = uiFont(12.f, true, true);
  float w = textWidth(label, f);
  drawText(label, (int)wcslen(label), r.left + (r.right - r.left - w) / 2, r.top + 4, on ? t.fg : t.statusText, f, w + 4, 20);
}
static void panelButton(const D2D1_RECT_F& r, const wchar_t* label, bool hover, bool enabled = true) {
  const Theme& t = T();
  fillR(r, hover && enabled ? t.btnHi : t.btn, enabled ? 1.f : 0.5f, 3);
  strokeR(r, t.border, 1.f, 3);
  float w = textWidth(label, g_ui);
  drawText(label, (int)wcslen(label), r.left + (r.right - r.left - w) / 2, r.top + 3, enabled ? t.fg : t.statusText, g_ui, w + 6, 20);
}
static void drawPanelClose(const D2D1_RECT_F& r) {
  g_br->SetColor(Rgb(inRect(r, g_mx, g_my) ? T().fg : T().statusText));
  g_rt->DrawLine(D2D1::Point2F(r.left + 8, r.top + 8), D2D1::Point2F(r.right - 8, r.bottom - 8), g_br, 1.5f);
  g_rt->DrawLine(D2D1::Point2F(r.right - 8, r.top + 8), D2D1::Point2F(r.left + 8, r.bottom - 8), g_br, 1.5f);
}
static void drawFif() {
  const Theme& t = T();
  D2D1_RECT_F pr = panelRect(1);
  float x = pr.left, y = pr.top, w = pr.right - pr.left, h = pr.bottom - pr.top;
  fillRect(x, y, w, h, t.status);
  fillRect(x, y, w, 1, t.border);
  g_pHits.push_back({rc(x, y - 2, w, 5), P_RESIZE_FIF, 0});
  float m = 10, ry1 = y + 8, ry2 = y + 41;
  auto label = [&](const wchar_t* s, float lx, float ly) { drawText(s, (int)wcslen(s), lx, ly + 4, t.statusText, g_ui, 70, 20); };
  // row 1
  label(L"Find:", x + m, ry1);
  float closeW = 28, btnFindW = textWidth(L"Find", g_ui) + 24, togW = 28;
  float whereLabW = textWidth(L"Where:", g_ui);
  float fixed = m + 54 + 6 + 6 + togW * 3 + 8 + whereLabW + 6 + 6 + btnFindW + 6 + closeW + 6 + m;
  float inW = std::max(60.f, (w - fixed) / 2);
  float cx = x + m + 54 + 6;
  D2D1_RECT_F qr = rc(cx, ry1, inW, 27);
  drawInput(g_fifQ, qr, g_panelFocus == 1 && g_fifField == 0, L"Find in files");
  g_pHits.push_back({qr, P_FIF_Q, 0});
  cx += inW + 6;
  const wchar_t* tl[3] = {L".*", L"Aa", L"ab"};
  bool tv[3] = {g_fifRegex, g_fifCase, g_fifWord};
  int tk[3] = {P_FIF_RE, P_FIF_CASE, P_FIF_WORD};
  for (int i = 0; i < 3; i++) {
    D2D1_RECT_F r = rc(cx, ry1 + 1, togW, 25);
    panelToggle(r, tl[i], tv[i], inRect(r, g_mx, g_my));
    if (i == 2) fillRect(r.left + 9, r.top + 20, 10, 1, tv[i] ? t.fg : t.statusText);
    g_pHits.push_back({r, tk[i], 0});
    cx += togW;
  }
  cx += 8;
  label(L"Where:", cx, ry1);
  cx += whereLabW + 6;
  D2D1_RECT_F wr = rc(cx, ry1, inW, 27);
  drawInput(g_fifWhere, wr, g_panelFocus == 1 && g_fifField == 1, L"<open folders>");
  g_pHits.push_back({wr, P_FIF_WHERE, 0});
  cx += inW + 6;
  D2D1_RECT_F fr = rc(cx, ry1 + 1, btnFindW, 25);
  panelButton(fr, L"Find", inRect(fr, g_mx, g_my));
  g_pHits.push_back({fr, P_FIF_FIND, 0});
  D2D1_RECT_F xr = rc(x + w - m - closeW, ry1, closeW, 27);
  drawPanelClose(xr);
  g_pHits.push_back({xr, P_FIF_CLOSE, 0});
  // row 2
  label(L"Replace:", x + m, ry2);
  float repBtnW = textWidth(L"Replace All", g_ui) + 24;
  float sumW = std::max(0.f, textWidth(g_fifSum, g_ui));
  float repInW = std::max(60.f, w - (m + 54 + 6) - 6 - repBtnW - 8 - sumW - m - 6);
  D2D1_RECT_F rr = rc(x + m + 54 + 6, ry2, repInW, 27);
  drawInput(g_fifRep, rr, g_panelFocus == 1 && g_fifField == 2, L"Replace with");
  g_pHits.push_back({rr, P_FIF_REP, 0});
  D2D1_RECT_F rb = rc(rr.right + 6, ry2 + 1, repBtnW, 25);
  panelButton(rb, L"Replace All", inRect(rb, g_mx, g_my));
  g_pHits.push_back({rb, P_FIF_REPLACE, 0});
  drawText(g_fifSum.c_str(), (int)g_fifSum.size(), rb.right + 8, ry2 + 4, t.statusText, g_ui, sumW + 10, 20);
  fillRect(x, y + 76, w, 1, t.border);
  // results
  float top = y + 77, viewH = h - 77;
  D2D1_RECT_F res = rc(x, top, w, viewH);
  g_pHits.push_back({res, P_FIF_RES, 0});
  IDWriteTextFormat* mono = monoFont(13.f);
  g_rt->PushAxisAlignedClip(D2D1::RectF(x, top, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
  if (g_fifHave && g_fifRes.files.empty()) drawText(L"No matches found.", 17, x + 14, top + 10, t.fg, g_ui, 300, 20);
  float total = 0;
  int matchIdx = 0;
  float rowY = top - g_fifScroll;
  for (size_t fi = 0; fi < g_fifRes.files.size(); fi++) {
    auto& f = g_fifRes.files[fi];
    // file header
    float hh = 18;
    if (rowY + hh > top && rowY < y + h) {
      fillRect(x, rowY, w, hh, t.fg, 0.1f);
      wstring nm = baseName(f.file), rel;
      rel = f.file;
      for (auto* rn : g_roots)
        if (f.file.size() > rn->path.size() && _wcsicmp(f.file.substr(0, rn->path.size()).c_str(), rn->path.c_str()) == 0) { rel = f.file.substr(rn->path.size() + 1); break; }
      if (rel == nm) rel.clear();
      drawText(nm.c_str(), (int)nm.size(), x + 10, rowY, t.fg, g_uiBold, 400, 18);
      float nw = textWidth(nm, g_uiBold);
      wstring dir = rel;
      size_t k = dir.find_last_of(L"\\/");
      dir = k == wstring::npos ? L"" : dir.substr(0, k);
      drawText(dir.c_str(), (int)dir.size(), x + 10 + nw + 8, rowY + 1, mixc(t.status, t.fg, 0.5f), g_ui, w - nw - 100, 18);
      wstring cnt = std::to_wstring(f.m.size());
      drawText(cnt.c_str(), (int)cnt.size(), x + w - 24 - textWidth(cnt, g_ui), rowY + 1, mixc(t.status, t.fg, 0.55f), g_ui, 40, 18);
    }
    rowY += hh;
    total += hh;
    for (size_t mi = 0; mi < f.m.size(); mi++, matchIdx++) {
      const FifMatch& mt = f.m[mi];
      if (rowY + 18 > top && rowY < y + h) {
        bool cur = matchIdx == g_fifCursor;
        bool hv = inRect(rc(x, rowY, w, 18), g_mx, g_my) && g_my > top;
        if (cur || hv) fillRect(x, rowY, w, 18, t.panelHi, 1.f);
        wstring ln = std::to_wstring(mt.line);
        drawText(ln.c_str(), (int)ln.size(), x + 10 + 44 - textWidth(ln, mono), rowY + 1, mixc(t.status, t.fg, 0.5f), mono, 60, 18);
        float tx = x + 10 + 44 + 12;
        float cw2 = textWidth(L"M", mono);
        int a = std::max(0, mt.start), b = std::min((int)mt.text.size(), mt.start + mt.len);
        // highlight behind the match
        fillR(rc(tx + a * cw2 - 1, rowY + 1, (b - a) * cw2 + 2, 16), t.accent, 0.22f, 2);
        drawText(mt.text.c_str(), (int)mt.text.size(), tx, rowY + 1, t.fg, mono, w - tx - 10, 18);
        g_pHits.push_back({rc(x, rowY, w, 18), P_FIF_ROW, matchIdx});
      }
      rowY += 18;
      total += 18;
    }
  }
  g_rt->PopAxisAlignedClip();
  float maxS = std::max(0.f, total - viewH);
  g_fifScroll = std::max(0.f, std::min(g_fifScroll, maxS));
  if (total > viewH) {
    float th = std::max(24.f, viewH * viewH / total), ty = top + (viewH - th) * (g_fifScroll / std::max(1.f, maxS));
    fillR(rc(x + w - 9, ty, 6, th), t.scroll, 1.f, 3);
  }
}
static void drawRun() {
  const Theme& t = T();
  D2D1_RECT_F pr = panelRect(3);
  float x = pr.left, y = pr.top, w = pr.right - pr.left, h = pr.bottom - pr.top;
  fillRect(x, y, w, h, t.status);
  fillRect(x, y, w, 1, t.border);
  g_pHits.push_back({rc(x, y - 2, w, 5), P_RESIZE_RUN, 0});
  float m = 10, ry = y + 6;
  drawText(L"Run", 3, x + m, ry + 5, t.fg, g_uiBold, 40, 20);
  float cx = x + m + 40;
  float bw1 = textWidth(L"Run", g_ui) + 22, bw2 = textWidth(L"Stop", g_ui) + 22, bw3 = textWidth(L"Clear", g_ui) + 22, bw4 = 28;
  float inW = std::max(80.f, w - (cx - x) - 8 - (bw1 + bw2 + bw3 + bw4 + 24) - m);
  D2D1_RECT_F ir = rc(cx, ry, inW, 28);
  drawInput(g_runIn, ir, g_panelFocus == 2, L"Command, e.g.  git status   or   npm test");
  g_pHits.push_back({ir, P_RUN_IN, 0});
  cx = ir.right + 8;
  D2D1_RECT_F b1 = rc(cx, ry + 1, bw1, 26);
  panelButton(b1, L"Run", inRect(b1, g_mx, g_my), !g_runBusy);
  if (!g_runBusy) g_pHits.push_back({b1, P_RUN_RUN, 0});
  cx += bw1 + 8;
  D2D1_RECT_F b2 = rc(cx, ry + 1, bw2, 26);
  panelButton(b2, L"Stop", inRect(b2, g_mx, g_my), g_runBusy);
  if (g_runBusy) g_pHits.push_back({b2, P_RUN_STOP, 0});
  cx += bw2 + 8;
  D2D1_RECT_F b3 = rc(cx, ry + 1, bw3, 26);
  panelButton(b3, L"Clear", inRect(b3, g_mx, g_my));
  g_pHits.push_back({b3, P_RUN_CLEAR, 0});
  cx += bw3 + 8;
  D2D1_RECT_F b4 = rc(cx, ry, bw4, 28);
  drawPanelClose(b4);
  g_pHits.push_back({b4, P_RUN_CLOSE, 0});
  fillRect(x, y + 40, w, 1, t.border);
  wstring cwd = g_runCwd.empty() ? runDefaultCwd() : g_runCwd;
  drawText(cwd.c_str(), (int)cwd.size(), x + m, y + 42, mixc(t.status, t.fg, 0.55f), uiFont(11.5f), w - 20, 16);
  // output
  float top = y + 60, viewH = h - 60;
  IDWriteTextFormat* mono = monoFont(12.5f);
  float lineH = 18, cw2 = textWidth(L"M", mono);
  int cols = std::max(10, (int)((w - 2 * m - 12) / cw2));
  // wrap the lines to the width
  struct Vis { int line; int a, b; };
  vector<Vis> rows;
  for (size_t i = 0; i < g_runOut.size() + (g_runPartial.empty() ? 0 : 1); i++) {
    const wstring& s = i < g_runOut.size() ? g_runOut[i].text : g_runPartial;
    int n = (int)s.size();
    if (n == 0) { rows.push_back({(int)i, 0, 0}); continue; }
    for (int a = 0; a < n; a += cols) rows.push_back({(int)i, a, std::min(n, a + cols)});
  }
  float total = rows.size() * lineH + 10;
  float maxS = std::max(0.f, total - viewH);
  if (g_runScroll > maxS) g_runScroll = maxS;
  g_runScroll = std::max(0.f, g_runScroll);
  g_pHits.push_back({rc(x, top, w, viewH), P_RUN_OUT, 0});
  g_rt->PushAxisAlignedClip(D2D1::RectF(x, top, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
  int first = std::max(0, (int)(g_runScroll / lineH));
  for (int i = first; i < (int)rows.size(); i++) {
    float ly = top + 4 + i * lineH - g_runScroll;
    if (ly > y + h) break;
    const Vis& v = rows[i];
    bool partial = v.line >= (int)g_runOut.size();
    const wstring& s = partial ? g_runPartial : g_runOut[v.line].text;
    int kind = partial ? g_runPartialKind : g_runOut[v.line].kind;
    DWORD col = kind == 1 ? t.accent : kind == 2 ? 0xe0707a : kind == 3 ? 0x7bc67e : kind == 4 ? 0xe5c07b : t.fg;
    if (v.b > v.a) drawText(s.c_str() + v.a, v.b - v.a, x + m, ly, col, mono, w, lineH);
    if (!partial) {
      for (auto& lk : g_runOut[v.line].links) {
        int a = std::max(lk.a, v.a), b = std::min(lk.b, v.b);
        if (b <= a) continue;
        float lx = x + m + (a - v.a) * cw2;
        fillRect(lx, ly + 15, (b - a) * cw2, 1, t.accent);
        drawText(s.c_str() + a, b - a, lx, ly, t.accent, mono, (b - a) * cw2 + 4, lineH);
        g_pHits.push_back({rc(lx, ly, (b - a) * cw2, lineH), P_RUN_OUT, 1000 + v.line * 16 + (int)(&lk - &g_runOut[v.line].links[0])});
      }
    }
  }
  g_rt->PopAxisAlignedClip();
  if (total > viewH) {
    float th = std::max(24.f, viewH * viewH / total), ty = top + (viewH - th) * (g_runScroll / std::max(1.f, maxS));
    fillR(rc(x + w - 9, ty, 6, th), t.scroll, 1.f, 3);
  }
}
static void drawPanels() {
  g_pHits.clear();
  if (g_fifOpen) drawFif();
  if (g_buildOpen) drawBuild();
  if (g_runOpen) drawRun();
}

/* ---------------------------------------------------------------- panel input */
static void fifOpenPanel(const wstring& text) {
  g_fifOpen = true;
  g_panelFocus = 1;
  g_fifField = 0;
  if (!text.empty()) g_fifQ.set(text);
  else g_fifQ.a = 0, g_fifQ.c = (int)g_fifQ.t.size();
  computeLayout();
  ensureVisible();
  caretReset();
  if (!text.empty() && g_root) fifRun();
  redraw();
}
static void fifClosePanel() {
  g_fifOpen = false;
  g_fifCancel = true;
  if (g_panelFocus == 1) g_panelFocus = 0;
  computeLayout();
  ensureVisible();
  redraw();
}
static void runOpenPanel(const wstring& prefill = L"") {
  g_runOpen = true;
  g_panelFocus = 2;
  if (!prefill.empty()) g_runIn.set(prefill);
  else g_runIn.a = 0, g_runIn.c = (int)g_runIn.t.size();
  if (g_runCwd.empty()) g_runCwd = runDefaultCwd();
  computeLayout();
  ensureVisible();
  caretReset();
  redraw();
}
static void runClosePanel() {
  runStop();
  g_runOpen = false;
  if (g_panelFocus == 2) g_panelFocus = 0;
  computeLayout();
  ensureVisible();
  redraw();
}
static int g_pDrag = 0;
static bool panelsMouse(float x, float y, int what, int delta) {  // what: 0 down, 1 move, 2 up, 3 wheel
  if (what == 2) { bool was = g_pDrag != 0; g_pDrag = 0; return was; }
  if (what == 1 && g_pDrag) {
    float bottom = ch() - STATUS_H;
    if (g_pDrag == P_RESIZE_RUN) g_runH = std::max(90.f, std::min(ch() * 0.7f, bottom - y));
    else if (g_pDrag == P_RESIZE_BUILD) g_buildH = std::max(90.f, std::min(ch() * 0.7f, bottom - (g_runOpen ? g_runH : 0.f) - y));
    else if (g_pDrag == P_RESIZE_FIF) {
      float yBelow = bottom - (g_runOpen ? g_runH : 0.f) - (g_buildOpen ? g_buildH : 0.f);
      g_fifH = std::max(120.f, std::min(ch() * 0.7f, yBelow - y));
    }
    computeLayout();
    redraw();
    return true;
  }
  if (!g_fifOpen && !g_runOpen && !g_buildOpen) return false;
  if (what == 3) {
    for (int i = (int)g_pHits.size() - 1; i >= 0; i--) {
      if (!inRect(g_pHits[i].r, x, y)) continue;
      if (g_pHits[i].kind == P_FIF_RES) { g_fifScroll = std::max(0.f, g_fifScroll - delta / 120.f * 54); redraw(); return true; }
      if (g_pHits[i].kind == P_RUN_OUT) { g_runScroll = std::max(0.f, g_runScroll - delta / 120.f * 54); g_runStick = false; redraw(); return true; }
      if (g_pHits[i].kind == P_BUILD_LIST) { g_buildScroll = std::max(0.f, g_buildScroll - delta / 120.f * 54); redraw(); return true; }
    }
    return false;
  }
  const PHit* hit = nullptr;
  for (int i = (int)g_pHits.size() - 1; i >= 0; i--) if (inRect(g_pHits[i].r, x, y)) { hit = &g_pHits[i]; break; }
  if (what == 1) return false;
  if (!hit) return false;
  auto setCaretFromX = [&](LineEdit& e, const D2D1_RECT_F& r) {
    float rel = x - (r.left + 9);
    int best = (int)e.t.size();
    for (int i = 0; i <= (int)e.t.size(); i++) if (textWidth(e.t.substr(0, i), g_ui) >= rel) { best = i; if (i > 0 && rel - textWidth(e.t.substr(0, i - 1), g_ui) < textWidth(e.t.substr(0, i), g_ui) - rel) best = i - 1; break; }
    e.c = e.a = best;
    caretReset();
  };
  switch (hit->kind) {
    case P_RESIZE_FIF: case P_RESIZE_RUN: case P_RESIZE_BUILD: g_pDrag = hit->kind; return true;
    case P_BUILD_CLOSE: case P_BUILD_AGAIN: case P_BUILD_RAW: case P_BUILD_PICK: case P_BUILD_ROW: buildClick(*hit); break;
    case P_FIF_Q: g_panelFocus = 1; g_fifField = 0; setCaretFromX(g_fifQ, hit->r); break;
    case P_FIF_WHERE: g_panelFocus = 1; g_fifField = 1; setCaretFromX(g_fifWhere, hit->r); break;
    case P_FIF_REP: g_panelFocus = 1; g_fifField = 2; setCaretFromX(g_fifRep, hit->r); break;
    case P_FIF_RE: g_fifRegex = !g_fifRegex; break;
    case P_FIF_CASE: g_fifCase = !g_fifCase; break;
    case P_FIF_WORD: g_fifWord = !g_fifWord; break;
    case P_FIF_FIND: fifRun(); break;
    case P_FIF_REPLACE: fifReplaceAll(); break;
    case P_FIF_CLOSE: fifClosePanel(); break;
    case P_FIF_ROW: g_fifCursor = hit->idx; fifOpenCursor(); break;
    case P_FIF_RES: break;
    case P_RUN_IN: g_panelFocus = 2; setCaretFromX(g_runIn, hit->r); break;
    case P_RUN_RUN: runStart(g_runIn.t); break;
    case P_RUN_STOP: runStop(); break;
    case P_RUN_CLEAR: g_runOut.clear(); g_runPartial.clear(); g_runChars = 0; g_runScroll = 0; break;
    case P_RUN_CLOSE: runClosePanel(); break;
    case P_RUN_OUT:
      if (hit->idx >= 1000) {
        int line = (hit->idx - 1000) / 16, k = (hit->idx - 1000) % 16;
        if (line < (int)g_runOut.size() && k < (int)g_runOut[line].links.size()) {
          auto lk = g_runOut[line].links[k];
          wstring f = lk.file;
          bool abs = f.size() > 1 && (f[1] == L':' || f[0] == L'\\' || f[0] == L'/');
          if (!abs) { for (auto& c : f) if (c == L'/') c = L'\\'; f = g_runCwd + L"\\" + f; }
          openAt(f, lk.line, std::max(0, lk.col - 1), 0);
        }
      }
      break;
  }
  redraw();
  return true;
}
static bool panelsKey(WPARAM wp, bool ctrl, bool shift, bool alt) {
  if (g_panelFocus == 1 && g_fifOpen) {
    LineEdit& e = g_fifField == 0 ? g_fifQ : g_fifField == 1 ? g_fifWhere : g_fifRep;
    if (wp == VK_ESCAPE) { fifClosePanel(); return true; }
    if (wp == VK_RETURN) { if (g_fifField == 2) fifReplaceAll(); else fifRun(); return true; }
    if (wp == VK_TAB) { g_fifField = (g_fifField + (shift ? 2 : 1)) % 3; caretReset(); redraw(); return true; }
    if (alt && !ctrl && (wp == 'R' || wp == 'C' || wp == 'W')) { if (wp == 'R') g_fifRegex = !g_fifRegex; else if (wp == 'C') g_fifCase = !g_fifCase; else g_fifWord = !g_fifWord; redraw(); return true; }
    if (wp == VK_UP || wp == VK_DOWN || wp == VK_PRIOR || wp == VK_NEXT) { fifNext(wp == VK_DOWN || wp == VK_NEXT ? 1 : -1); return true; }
    if (!(ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V')) {
      editKey(e, wp, ctrl, shift);
      caretReset();
      redraw();
      return true;
    }
    return false;
  }
  if (g_panelFocus == 2 && g_runOpen) {
    if (wp == VK_ESCAPE) { runClosePanel(); return true; }
    if (wp == VK_RETURN) { runStart(g_runIn.t); return true; }
    if (wp == 'C' && ctrl && g_runBusy && g_runIn.a == g_runIn.c) { runStop(); return true; }
    if (wp == VK_UP && !g_runHistory.empty()) { g_runHpos = g_runHpos < 0 ? (int)g_runHistory.size() - 1 : std::max(0, g_runHpos - 1); g_runIn.set(g_runHistory[g_runHpos]); redraw(); return true; }
    if (wp == VK_DOWN && !g_runHistory.empty()) {
      g_runHpos = g_runHpos < 0 ? -1 : g_runHpos + 1;
      if (g_runHpos >= (int)g_runHistory.size()) g_runHpos = -1;
      g_runIn.set(g_runHpos >= 0 ? g_runHistory[g_runHpos] : L"");
      redraw();
      return true;
    }
    if (!(ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V')) {
      if (!g_runBusy) editKey(g_runIn, wp, ctrl, shift);
      caretReset();
      redraw();
      return true;
    }
    return false;
  }
  return false;
}
static bool panelsChar(wchar_t c) {
  if (g_panelFocus == 1 && g_fifOpen) {
    LineEdit& e = g_fifField == 0 ? g_fifQ : g_fifField == 1 ? g_fifWhere : g_fifRep;
    e.insert(wstring(1, c));
    caretReset();
    redraw();
    return true;
  }
  if (g_panelFocus == 2 && g_runOpen) {
    if (!g_runBusy) g_runIn.insert(wstring(1, c));
    caretReset();
    redraw();
    return true;
  }
  return false;
}
