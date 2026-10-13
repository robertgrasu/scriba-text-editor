// amxx.inc: AMX Mod X support: autocomplete and hover documentation from the plugin and its #include files, parameter hints,
// unknown-function underlines, go to definition, outline, and compiling with amxxpc (profiles, clickable results, error and
// warning marks in the editor). Also the hover tooltip used by every language. Included by main.cpp after git.inc.

static Pos hit(float mx, float my);  // the text position under a point (main.cpp)
#define WM_AMX_DOC (WM_APP + 6)
#define WM_AMX_BUILD (WM_APP + 7)

static bool isPawnDoc(const Doc& d) { return d.lang == L_PAWN; }


/* ---------------------------------------------------------------- tooltips (hover, parameter hints) */
struct TipLine { wstring text; int style; };  // 0 signature (mono), 1 documentation (dim), 2 source (dim, fnSmall), 3 plain
struct Tooltip { bool show = false; vector<TipLine> lines; int line = 0, c0 = 0; int dispLine = 0; };
static Tooltip g_tip;
static bool lspDiagTip(Doc& d, int line, int col, vector<TipLine>& out, int& c0);  // lsp.inc
static void lspHover(Doc& d, int line, int col, std::function<void(const wstring&)> done);
static DWORD g_hoverAt = 0;
static int g_hoverLine = -1, g_hoverCol = -1;  // the character under the mouse once it rests
static bool g_hoverArmed = false;
static void tipHide() { if (g_tip.show) { g_tip.show = false; redraw(); } g_hoverArmed = false; }
/* ---------------------------------------------------------------- popup and console styles of the AMXX-Editor */
struct AmxLook { const wchar_t* name; DWORD bg, fg, border, err, warn; };
static const AmxLook AMX_POPUPS[] = {
    {L"AMXXStudio", 0xffffff, 0x111111, 0xaaaaaa, 0, 0}, {L"Basic Black", 0x000000, 0xffffff, 0x444444, 0, 0},
    {L"Green Notepad++", 0xffffff, 0x111111, 0xa5cd60, 0, 0}, {L"Notepad++", 0xffffff, 0x000000, 0x444444, 0, 0}};
static const AmxLook AMX_CONSOLES[] = {
    {L"AMXXStudio", 0xffffff, 0x000000, 0xaaaaaa, 0xcc0000, 0xcc6600}, {L"DarkColorize", 0x111111, 0x3880d1, 0x333333, 0xdd0000, 0xcf8506},
    {L"MS-DOS", 0x000000, 0xdddddd, 0x444444, 0xff5555, 0xffff55}, {L"Matrix", 0x000000, 0x0da615, 0x0a5a10, 0x66ff66, 0x99dd33},
    {L"SmallFonts Terminal", 0x111111, 0xffe52c, 0x555500, 0xff6644, 0xffe52c}, {L"Ubuntu", 0x300a24, 0xdddddd, 0x5e2750, 0xff6b6b, 0xe9b96e}};
static int g_lookPrev[2] = {-2, -2};  // while a picker is open: -2 not previewing, -1 the colour scheme, else the index
static int amxLookIdx(int which) {  // which: 0 popup, 1 console; -1 = the colours of the colour scheme
  if (g_lookPrev[which] != -2) return g_lookPrev[which];
  wstring nm = sStr(which ? "amxx_console_style" : "amxx_popup_style");
  const AmxLook* arr = which ? AMX_CONSOLES : AMX_POPUPS;
  int n = which ? (int)(sizeof(AMX_CONSOLES) / sizeof(AmxLook)) : (int)(sizeof(AMX_POPUPS) / sizeof(AmxLook));
  for (int i = 0; i < n; i++) if (nm == arr[i].name) return i;
  return -1;
}
static AmxLook amxLook(int which) {
  int i = amxLookIdx(which);
  if (i >= 0) return (which ? AMX_CONSOLES : AMX_POPUPS)[i];
  const Theme& t = T();
  return which ? AmxLook{L"", t.status, t.fg, t.border, 0xec7b7b, 0xe5c07b} : AmxLook{L"", t.menu, t.fg, t.border, 0, 0};
}
static void drawTipBox(float anchorX, float anchorBottomY, float anchorTopY, const vector<TipLine>& lines, bool above) {
  const Theme& t = T();
  AmxLook pl = amxLook(0);
  float maxW = 520;
  IDWriteTextFormat* mono = monoFont(12.5f);
  IDWriteTextFormat* ui = uiFont(12.5f);
  IDWriteTextFormat* fnSmall = uiFont(11.5f);
  float w = 120, h = 8;
  struct M { float h; };
  vector<M> ms;
  for (auto& l : lines) {
    IDWriteTextFormat* f = l.style == 0 ? mono : l.style == 2 ? fnSmall : ui;
    float tw = textWidth(l.text, f);
    if (l.style == 0 || l.style == 3) w = std::max(w, std::min(maxW, tw + 24));
    else w = std::max(w, std::min(maxW, std::min(tw, maxW) + 24));
  }
  for (auto& l : lines) {
    IDWriteTextFormat* f = l.style == 0 ? mono : l.style == 2 ? fnSmall : ui;
    float lh = wrappedHeight(l.text, f, w - 20, l.style == 2 ? 15.f : 17.f);
    if (l.style == 1) lh = std::min(lh, 240.f);
    ms.push_back({lh});
    h += lh + (l.style == 0 ? 0 : 6);
  }
  h += 4;
  float x = std::max(4.f, std::min(anchorX, cw() - w - 6));
  float y = above ? anchorTopY - h - 4 : anchorBottomY + 4;
  if (y < edTop()) y = anchorBottomY + 4;
  fillR(D2D1::RectF(x + 2, y + 3, x + w + 2, y + h + 3), 0x000000, 0.28f, 6);
  fillR(rc(x, y, w, h), pl.bg, 1.f, 6);
  strokeR(rc(x, y, w, h), pl.border, 0.9f, 6);
  float yy = y + 6;
  for (size_t i = 0; i < lines.size(); i++) {
    const TipLine& l = lines[i];
    IDWriteTextFormat* f = l.style == 0 ? mono : l.style == 2 ? fnSmall : ui;
    if (l.style == 1) { g_rt->PushAxisAlignedClip(D2D1::RectF(x + 10, yy, x + w - 10, yy + 240), D2D1_ANTIALIAS_MODE_ALIASED); }
    drawWrapped(l.text, f, x + 10, yy + (l.style == 0 ? 0 : 6), w - 20, pl.fg, l.style == 0 || l.style == 3 ? 1.f : l.style == 1 ? 0.85f : 0.5f, l.style == 2 ? 15.f : 17.f);
    if (l.style == 1) g_rt->PopAxisAlignedClip();
    yy += ms[i].h + (l.style == 0 ? 0 : 6);
  }
}
static void drawTooltip(Doc& d) {
  if (!g_tip.show || g_tip.line >= (int)d.lines.size() || g_palOpen || g_comp.open) return;
  ensureRows(d);
  int row = rowOfPos(d, {g_tip.line, g_tip.c0});
  float x = textX() - d.scrollX + rowVcol(d, {g_tip.line, g_tip.c0}) * (d.vis ? 1.f : g_charW);
  float top = edTop() + rowTop(d, row) - d.scrollY, bottom = top + rowH(d, row);
  if (top < edTop() - 2 || bottom > edTop() + edH() + 2) return;
  drawTipBox(x, bottom, top, g_tip.lines, true);
}

/* ---------------------------------------------------------------- profiles and the compiler */
struct AmxEffective { wstring name, compiler, include, output; int debug = 2; json profiles = json::object(); };
static AmxEffective amxEffective() {
  AmxEffective e;
  json profiles = g_settings["amxx_profiles"];
  if (!profiles.is_object()) profiles = json::object();
  e.profiles = profiles;
  std::string name = g_settings["amxx_profile"].is_string() ? g_settings["amxx_profile"].get<std::string>() : "";
  json p = json::object();
  if (!name.empty() && profiles.contains(name) && profiles[name].is_object()) { p = profiles[name]; e.name = fromUtf8(name); }
  auto str = [&](const char* k, const char* setting) -> wstring {
    if (p.contains(k) && p[k].is_string()) return fromUtf8(p[k].get<std::string>());
    return sStr(setting);
  };
  e.compiler = str("compiler", "amxx_compiler_path");
  e.include = str("include", "amxx_include_dir");
  e.output = str("output", "amxx_output_dir");
  e.debug = p.contains("debug") && p["debug"].is_number() ? (int)p["debug"].get<double>() : sInt("amxx_debug_level");
  return e;
}
struct AmxDetected { wstring compiler, include; };
static bool amxDetect(const wstring& file, AmxDetected& out) {
  wstring dir = dirNameOf(file);
  for (int i = 0; !dir.empty() && i < 8; i++) {
    for (const wchar_t* rel : {L"", L"scripting", L"addons\\amxmodx\\scripting", L"amxmodx\\scripting"}) {
      wstring base = rel[0] ? dir + L"\\" + rel : dir;
      wstring exe = base + L"\\amxxpc.exe";
      if (GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES) {
        out.compiler = exe;
        wstring inc = base + L"\\include";
        out.include = GetFileAttributesW(inc.c_str()) != INVALID_FILE_ATTRIBUTES ? inc : L"";
        return true;
      }
    }
    wstring up = dirNameOf(dir);
    if (up == dir || up.empty()) break;
    dir = up;
  }
  return false;
}
static vector<wstring> amxIncludeDirs(const Doc& d) {
  AmxEffective e = amxEffective();
  vector<wstring> out;
  if (!e.include.empty()) out.push_back(e.include);
  if (!e.compiler.empty()) out.push_back(dirNameOf(e.compiler) + L"\\include");
  AmxDetected det;
  if (!d.path.empty() && amxDetect(d.path, det) && !det.include.empty()) out.push_back(det.include);
  vector<wstring> uniq;
  for (auto& s : out) { bool dup = false; for (auto& u : uniq) if (_wcsicmp(u.c_str(), s.c_str()) == 0) dup = true; if (!dup) uniq.push_back(s); }
  return uniq;
}

/* ---------------------------------------------------------------- analysis of the focused Pawn document */
struct AmxSnap { unsigned long long uid = 0; unsigned ver = ~0u; std::map<wstring, PawnSym> map; vector<PawnSym> local; vector<wstring> unresolved; };
static AmxSnap g_amx;
struct AmxJob { unsigned long long uid; unsigned ver; wstring text, path; vector<wstring> dirs; bool check; };
struct AmxResult { unsigned long long uid; unsigned ver; PawnVisible vis; vector<Doc::Unk> unknown; };
static std::atomic<bool> g_amxBusy{false};
static std::mutex g_amxMu;
static const std::unordered_set<wstring> NOT_CALLS = {
    L"if", L"else", L"for", L"while", L"do", L"switch", L"case", L"default", L"return", L"break", L"continue", L"sizeof", L"tagof", L"charsmax", L"new",
    L"static", L"const", L"public", L"stock", L"native", L"forward", L"enum", L"goto", L"sleep", L"exit", L"assert", L"state", L"operator", L"defined"};
static void amxWorker(AmxJob job, HWND hw) {
  auto* r = new AmxResult();
  r->uid = job.uid;
  r->ver = job.ver;
  {
    std::lock_guard<std::mutex> lk(g_amxMu);
    g_pawnDirs = job.dirs;
    r->vis = pawnCollect(job.text, job.path);
    if (job.check && r->vis.unresolved.empty() && !r->vis.included.empty()) {
      std::set<wstring> known;
      for (auto& s : r->vis.included) known.insert(s.name);
      for (auto& s : r->vis.local) { known.insert(s.name); for (auto& p : s.params) known.insert(p); for (auto& l : s.locals) known.insert(l.name); }
      std::set<std::pair<int, int>> seen;
      for (auto& c : pawnFindCalls(job.text)) {
        if (NOT_CALLS.count(c.name) || known.count(c.name)) continue;
        if (!seen.insert({c.line, c.col}).second) continue;
        r->unknown.push_back({c.line, c.col, (int)c.name.size()});
        if (r->unknown.size() > 300) break;
      }
    }
  }
  PostMessageW(hw, WM_AMX_DOC, 0, (LPARAM)r);
}
static void amxxAnalyze(Doc& d) {
  if (g_amxBusy.exchange(true)) return;
  AmxJob job;
  job.uid = d.uid;
  job.ver = d.ver;
  job.path = d.path;
  job.check = sBool("amxx_check_invalid");
  job.dirs = amxIncludeDirs(d);
  for (size_t i = 0; i < d.lines.size(); i++) { if (i) job.text += L"\n"; job.text += d.lines[i]; }
  HWND hw = g_hwnd;
  std::thread([job, hw]() mutable { amxWorker(std::move(job), hw); }).detach();
}
static void amxxDone(AmxResult* r) {
  g_amxBusy = false;
  for (auto* d : g_docs) {
    if (d->uid != r->uid) continue;
    if (d->ver == r->ver) d->unknown = r->unknown;
    if (d == &D() || g_amx.uid == d->uid || g_amx.uid == 0) {
      g_amx.uid = d->uid;
      g_amx.ver = r->ver;
      g_amx.map.clear();
      for (auto& s : r->vis.included) g_amx.map[s.name] = s;
      for (auto& s : r->vis.local) g_amx.map[s.name] = s;
      g_amx.local = r->vis.local;
      g_amx.unresolved = r->vis.unresolved;
      g_constSet.clear();  // constants (#define, const, enum values) are coloured by the Pawn syntax colouring
      g_constUid = d->uid;
      for (auto* lst : {&r->vis.included, &r->vis.local}) for (auto& sy : *lst) if (sy.kind == L"define" || sy.kind == L"const" || sy.kind == L"enumvalue") g_constSet.insert(sy.name);
    }
    break;
  }
  delete r;
  redraw();
}
// timer: analyse the visible Pawn files a moment after their text changed
static void amxxTick() {
  DWORD now = GetTickCount();
  for (int g = 0; g < paneCount(); g++) {
    int idx = g == g_curGroup ? g_active : g_groupActive[g];
    if (idx < 0 || idx >= (int)g_docs.size()) continue;
    Doc& d = *g_docs[idx];
    if (!isPawnDoc(d)) { if (g == g_curGroup && g_amx.uid && g_amx.uid != d.uid) { g_amx = AmxSnap(); } continue; }
    if (d.ver != d.amxVerSeen) { d.amxVerSeen = d.ver; d.amxDue = now + (d.amxDone ? 350 : 80); }
    if (d.amxDue && now >= d.amxDue && !g_amxBusy) { d.amxDue = 0; d.amxDone = true; amxxAnalyze(d); }
    if (g == g_curGroup && g_amx.uid != d.uid && d.amxDone && !g_amxBusy && !d.amxDue) { d.amxDue = now; d.amxVerSeen = ~0u; }
  }
}
static const PawnSym* amxLookup(const wstring& name) {
  auto it = g_amx.map.find(name);
  return it == g_amx.map.end() ? nullptr : &it->second;
}
static vector<TipLine> amxTip(const PawnSym& s) {
  vector<TipLine> out;
  out.push_back({s.sig, 0});
  if (!s.doc.empty()) out.push_back({s.doc, 1});
  out.push_back({(s.file.empty() ? L"this file" : baseName(s.file) + L":" + std::to_wstring(s.line)), 2});
  return out;
}

/* ---------------------------------------------------------------- completion, hover, parameter hints */
static const wchar_t* BUILTIN_TAGS[] = {L"Float", L"bool", L"any", L"Handle", L"Array", L"Trie", L"String", L"Fwd", L"Menu", L"Hud"};
static void amxxCompletion(Doc& d, const wstring& pre, vector<CompItem>& out) {
  if (!isPawnDoc(d) || g_amx.uid != d.uid) return;
  const wstring& s = d.lines[d.cur.l];
  // not inside a comment or a string
  wstring before = s.substr(0, d.cur.c);
  if (before.find(L"//") != wstring::npos) return;
  if (std::count(before.begin(), before.end(), L'"') % 2 == 1) return;
  std::set<wstring> seen;
  auto push = [&](const PawnSym& sy, int boost, const wchar_t* kindOverride = nullptr) {
    if (!seen.insert(sy.name).second) return;
    CompItem it;
    it.label = sy.name;
    it.detail = kindOverride ? kindOverride : (sy.kind == L"enumvalue" ? (sy.parent.empty() ? L"enum" : sy.parent) : sy.kind);
    it.sig = sy.sig;
    it.doc = sy.doc;
    it.src = sy.file.empty() ? L"this file" : baseName(sy.file) + L":" + std::to_wstring(sy.line);
    it.boost = boost;
    out.push_back(it);
  };
  int ln = d.cur.l + 1;
  for (auto& fn : g_amx.local) {
    if (fn.kind == L"native" || fn.kind == L"forward") continue;
    if (ln >= fn.line && ln <= fn.endLine && fn.endLine >= fn.line) {
      for (auto& p : fn.params) { PawnSym ps; ps.name = p; ps.kind = L"param"; ps.sig = L"parameter of " + fn.name + L"()"; ps.line = fn.line; push(ps, 30); }
      for (auto& l : fn.locals) if (l.line <= ln) { PawnSym ls; ls.name = l.name; ls.kind = L"local"; ls.sig = l.sig; ls.line = l.line; push(ls, 25); }
    }
  }
  for (auto& sy : g_amx.local) push(sy, 10);
  for (auto& kv : g_amx.map) if (!seen.count(kv.first)) push(kv.second, 0);
  std::set<wstring> tags(std::begin(BUILTIN_TAGS), std::end(BUILTIN_TAGS));
  for (auto& kv : g_amx.map) if (kv.second.kind == L"enum") tags.insert(kv.first);
  for (auto& tg : tags) if (!seen.count(tg + L":")) { CompItem it; it.label = tg + L":"; it.detail = L"tag"; it.boost = -2; out.push_back(it); }
}
// the nearest enclosing call "name(" before the cursor and the index of the argument being typed
struct AmxCall { const PawnSym* sym; int arg; };
static bool amxCallAt(Doc& d, AmxCall& out) {
  // text before the cursor, at most 2000 characters back
  wstring text;
  {
    int l0 = d.cur.l;
    size_t total = d.cur.c;
    wstring tail = d.lines[l0].substr(0, d.cur.c);
    text = tail;
    for (int l = l0 - 1; l >= 0 && total < 2000; l--) { text = d.lines[l] + L"\n" + text; total += d.lines[l].size() + 1; }
  }
  struct Fr { wstring name; int commas; };
  vector<Fr> stack;
  for (size_t i = 0; i < text.size(); i++) {
    wchar_t c = text[i];
    if (c == L'/' && i + 1 < text.size() && text[i + 1] == L'/') { size_t j = text.find(L'\n', i); if (j == wstring::npos) return false; i = j; continue; }
    if (c == L'/' && i + 1 < text.size() && text[i + 1] == L'*') { size_t j = text.find(L"*/", i + 2); if (j == wstring::npos) return false; i = j + 1; continue; }
    if (c == L'"' || c == L'\'') {
      size_t j = i + 1;
      while (j < text.size() && text[j] != c && text[j] != L'\n') { if (text[j] == L'^' || text[j] == L'\\') j++; j++; }
      if (j >= text.size()) return false;  // the cursor is inside a string
      i = j;
      continue;
    }
    if (c == L'(') {
      size_t b = i;
      while (b > 0 && iswspace(text[b - 1])) b--;
      size_t a = b;
      while (a > 0 && (isIdCh(text[a - 1]) || text[a - 1] == L'@')) a--;
      stack.push_back({text.substr(a, b - a), 0});
    } else if (c == L')') { if (!stack.empty()) stack.pop_back(); }
    else if (c == L',' && !stack.empty()) stack.back().commas++;
    else if (c == L';' || c == L'{' || c == L'}') stack.clear();
  }
  for (int k = (int)stack.size() - 1; k >= 0; k--) {
    const PawnSym* sym = stack[k].name.empty() ? nullptr : amxLookup(stack[k].name);
    if (sym && sym->sig.find(L'(') != wstring::npos) { out = {sym, stack[k].commas}; return true; }
  }
  return false;
}
static vector<wstring> splitParamsSig(const wstring& sig) {
  size_t a = sig.find(L'('), b = sig.rfind(L')');
  vector<wstring> out;
  if (a == wstring::npos || b == wstring::npos || b < a) return out;
  wstring inner = sig.substr(a + 1, b - a - 1), cur;
  int d = 0;
  for (wchar_t ch : inner) {
    if (wcschr(L"([{", ch)) d++;
    else if (wcschr(L")]}", ch)) d--;
    if (ch == L',' && d == 0) { out.push_back(trimW(cur)); cur.clear(); } else cur += ch;
  }
  if (!trimW(cur).empty()) out.push_back(trimW(cur));
  return out;
}
// the hint above the cursor: the signature with the current parameter in bold
static void drawParamHint(Doc& d) {
  if (!isPawnDoc(d) || g_amx.uid != d.uid || g_comp.open || g_palOpen || !d.extra.empty()) return;
  AmxCall call;
  if (!amxCallAt(d, call)) return;
  const Theme& t = T();
  vector<wstring> params = splitParamsSig(call.sym->sig);
  wstring head = call.sym->sig.substr(0, call.sym->sig.find(L'('));
  int idx = std::min(call.arg, std::max(0, (int)params.size() - 1));
  IDWriteTextFormat* mono = monoFont(12.5f);
  // measure
  float w = textWidth(head + L"(", mono);
  for (size_t i = 0; i < params.size(); i++) w += textWidth(params[i], mono) + (i + 1 < params.size() ? textWidth(L", ", mono) : 0);
  w += textWidth(L")", mono) + 16;
  w = std::min(w, 620.f);
  ensureRows(d);
  int row = rowOfPos(d, d.cur);
  float cx = textX() - d.scrollX + rowVcol(d, d.cur) * (d.vis ? 1.f : g_charW);
  float top = edTop() + rowTop(d, row) - d.scrollY;
  float x = std::max(4.f, std::min(cx, cw() - w - 6)), y = top - 28;
  if (y < edTop()) y = top + rowH(d, row) + 4;
  fillR(D2D1::RectF(x + 2, y + 3, x + w + 2, y + 25), 0x000000, 0.25f, 6);
  fillR(rc(x, y, w, 22), t.menu, 1.f, 6);
  strokeR(rc(x, y, w, 22), t.border, 0.9f, 6);
  float px = x + 8;
  auto put = [&](const wstring& s, DWORD col) { drawText(s.c_str(), (int)s.size(), px, y + 3, col, mono, w, 18); px += textWidth(s, mono); };
  put(head + L"(", t.fg);
  for (size_t i = 0; i < params.size(); i++) {
    put(params[i], (int)i == idx ? t.accent : t.fg);
    if (i + 1 < params.size()) put(L", ", t.fg);
  }
  put(L")", t.fg);
}
// the word at a position
static bool wordAt(Doc& d, int line, int col, wstring& word, int& c0, int& c1) {
  if (line < 0 || line >= (int)d.lines.size()) return false;
  const wstring& s = d.lines[line];
  if (col < 0 || col > (int)s.size()) return false;
  int a = col, b = col;
  while (a > 0 && (isIdCh(s[a - 1]) || s[a - 1] == L'@')) a--;
  while (b < (int)s.size() && (isIdCh(s[b]) || s[b] == L'@')) b++;
  if (a == b) return false;
  word = s.substr(a, b - a);
  c0 = a;
  c1 = b;
  return true;
}
// what the mouse resting on a word shows: the message of a compile error or an unknown function, the documentation of a symbol
static bool hoverContent(Doc& d, int line, int col, vector<TipLine>& out, int& c0) {
  if (lspDiagTip(d, line, col, out, c0)) return true;
  for (auto& mk : d.amxMarks) if (mk.line == line + 1) { out.push_back({mk.msg, 3}); c0 = std::min(col, (int)d.lines[line].size()); return true; }
  for (auto& u : d.unknown) if (u.line == line && col >= u.c0 && col < u.c0 + u.len) {
    out.push_back({L"Unknown function: not declared in this file or its #include files", 3});
    c0 = u.c0;
    return true;
  }
  wstring w;
  int a, b;
  if (!wordAt(d, line, col, w, a, b) || iswdigit(w[0]) || w.size() < 2) return false;
  if (isPawnDoc(d) && g_amx.uid == d.uid) {
    if (const PawnSym* sym = amxLookup(w)) { out = amxTip(*sym); c0 = a; return true; }
  }
  return false;
}
static void hoverReset() { g_hoverLine = g_hoverCol = -1; g_hoverArmed = false; }
static void hoverMove(float x, float y, bool overText) {
  if (!overText || g_docs.empty() || g_palOpen || g_openMenu >= 0 || g_suOpen) { tipHide(); hoverReset(); return; }
  Doc& d = D();
  Pos p = hit(x, y);
  // the mouse must stay on the same character
  if (p.l == g_hoverLine && p.c == g_hoverCol) return;
  if (g_tip.show) {
    wstring w;
    int a, b;
    // still on the word of the tooltip: keep it
    if (p.l == g_tip.line && wordAt(d, p.l, p.c, w, a, b) && a == g_tip.c0) { g_hoverLine = p.l; g_hoverCol = p.c; return; }
    g_tip.show = false;
    redraw();
  }
  g_hoverLine = p.l;
  g_hoverCol = p.c;
  g_hoverAt = GetTickCount();
  g_hoverArmed = true;
}
static void hoverTick() {
  if (!g_hoverArmed || g_docs.empty() || GetTickCount() - g_hoverAt < 350) return;
  g_hoverArmed = false;
  Doc& d = D();
  vector<TipLine> lines;
  int c0 = 0;
  if (g_hoverLine >= 0 && hoverContent(d, g_hoverLine, g_hoverCol, lines, c0)) {
    g_tip.show = true;
    g_tip.lines = lines;
    g_tip.line = g_hoverLine;
    g_tip.c0 = c0;
    redraw();
  }
  // the language server may know more: its documentation is added when it answers
  if (g_hoverLine >= 0) {
    int ln = g_hoverLine, col = g_hoverCol;
    unsigned long long uid = d.uid;
    lspHover(d, ln, col, [ln, col, uid](const wstring& text) {
      if (g_docs.empty() || D().uid != uid || g_hoverLine != ln || g_hoverCol != col) return;
      wstring w;
      int a, b;
      if (!wordAt(D(), ln, col, w, a, b)) return;
      if (!g_tip.show || g_tip.line != ln || g_tip.c0 != a) { g_tip.lines.clear(); g_tip.line = ln; g_tip.c0 = a; }
      g_tip.lines.push_back({text, 3});
      g_tip.show = true;
      redraw();
    });
  }
}

/* ---------------------------------------------------------------- go to definition (Pawn) */
static bool amxxGotoDefinition() {
  Doc& d = D();
  if (!isPawnDoc(d)) return false;
  wstring w;
  int a, b;
  if (!wordAt(d, d.cur.l, d.cur.c, w, a, b)) { flash(L"Put the cursor on a function or constant name."); return true; }
  const PawnSym* sym = g_amx.uid == d.uid ? amxLookup(w) : nullptr;
  if (!sym) { flash(L"No definition found for “" + w + L"”."); return true; }
  if (sym->file.empty() || (!d.path.empty() && _wcsicmp(sym->file.c_str(), d.path.c_str()) == 0)) {
    d.cur = d.anc = {std::max(0, std::min(sym->line - 1, (int)d.lines.size() - 1)), 0};
    revealCenter();
    moved(false);
  } else openAt(sym->file, sym->line, 0, 0);
  return true;
}
// the symbols of the outline (functions, enums, ...) of a Pawn document
static void amxOutline(Doc& d, vector<Symbol>& out) {
  out.clear();
  wstring text;
  for (size_t i = 0; i < d.lines.size(); i++) { if (i) text += L"\n"; text += d.lines[i]; }
  static const std::set<wstring> keep = {L"public", L"stock", L"function", L"native", L"forward", L"macro", L"enum"};
  for (auto& s : parsePawn(text).symbols) if (keep.count(s.kind)) out.push_back({s.line - 1, s.name, s.kind, 0});
}

/* ---------------------------------------------------------------- compile */
struct AmxItem { wstring file; int line; int type; wstring code, msg; };  // type 1 error, 2 warning
struct AmxBuild {
  bool have = false, busy = false, ok = false, raw = false, noCompiler = false;
  wstring title, state, profile, outFile, output, compiler;
  vector<AmxItem> items;
  int cur = -1;
  unsigned long long uid = 0;
  wstring file;
} g_build;
struct AmxBuildJob { wstring compiler, file, outDir; vector<wstring> includes; int debug; };
struct AmxBuildResult { bool error = false; wstring errorText, output, outFile; int code = 0; ULONGLONG size = 0; DWORD ms = 0; wstring file; };
static void amxBuildWorker(AmxBuildJob job, HWND hw) {
  auto* r = new AmxBuildResult();
  r->file = job.file;
  DWORD t0 = GetTickCount();
  wstring base = baseName(job.file);
  size_t dot = base.find_last_of(L'.');
  if (dot != wstring::npos) base = base.substr(0, dot);
  wstring outDir = job.outDir.empty() ? dirNameOf(job.file) : job.outDir;
  { size_t pos = 0; while ((pos = outDir.find(L'\\', pos + 1)) != wstring::npos) CreateDirectoryW(outDir.substr(0, pos).c_str(), nullptr); CreateDirectoryW(outDir.c_str(), nullptr); }
  r->outFile = outDir + L"\\" + base + L".amxx";
  wstring cmd = L"\"" + job.compiler + L"\" \"" + job.file + L"\"";
  for (auto& i : job.includes) if (!i.empty()) cmd += L" \"-i" + i + L"\"";
  cmd += L" \"-o" + r->outFile + L"\" -d" + std::to_wstring(job.debug);
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE rd, wr;
  if (!CreatePipe(&rd, &wr, &sa, 0)) { r->error = true; r->errorText = L"Could not start the compiler."; PostMessageW(hw, WM_AMX_BUILD, 0, (LPARAM)r); return; }
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = wr;
  si.hStdError = wr;
  si.hStdInput = INVALID_HANDLE_VALUE;
  PROCESS_INFORMATION pi{};
  vector<wchar_t> buf(cmd.begin(), cmd.end());
  buf.push_back(0);
  wstring cwd = dirNameOf(job.file);
  BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi);
  CloseHandle(wr);
  if (!ok) { CloseHandle(rd); r->error = true; r->errorText = L"Could not start the compiler: " + job.compiler; PostMessageW(hw, WM_AMX_BUILD, 0, (LPARAM)r); return; }
  CloseHandle(pi.hThread);
  std::string out;
  std::atomic<bool> done{false};
  std::thread dog([&]() { for (int i = 0; i < 1200 && !done; i++) Sleep(50); if (!done) TerminateProcess(pi.hProcess, 1); });
  char b[8192];
  for (;;) { DWORD got = 0; if (!ReadFile(rd, b, sizeof(b), &got, nullptr) || got == 0) break; out.append(b, got); }
  WaitForSingleObject(pi.hProcess, 5000);
  done = true;
  dog.join();
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  CloseHandle(rd);
  int n = MultiByteToWideChar(1252, 0, out.data(), (int)out.size(), nullptr, 0);
  wstring w(n, 0);
  if (n) MultiByteToWideChar(1252, 0, out.data(), (int)out.size(), w.data(), n);
  r->output = w;
  r->code = (int)code;
  r->ms = GetTickCount() - t0;
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (GetFileAttributesExW(r->outFile.c_str(), GetFileExInfoStandard, &fa)) {
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULONGLONG nt = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime, wt = ((ULONGLONG)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime;
    if (nt - wt < (ULONGLONG)(r->ms + 2000) * 10000) r->size = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
  }
  PostMessageW(hw, WM_AMX_BUILD, 0, (LPARAM)r);
}
static vector<AmxItem> amxParseOutput(const wstring& out, const wstring& dir) {
  vector<AmxItem> items;
  static const std::wregex re(L"^(.+?)\\((\\d+)(?:--\\d+)?\\)\\s*:\\s*(fatal error|error|warning)\\s+(\\d+)\\s*:\\s*(.*)$", std::regex::icase);
  for (auto& raw : splitNl(out)) {
    wstring ln = trimW(raw);
    std::wsmatch m;
    if (!std::regex_match(ln, m, re)) continue;
    wstring f = m[1].str();
    bool abs = (f.size() > 1 && f[1] == L':') || (!f.empty() && (f[0] == L'\\' || f[0] == L'/'));
    if (!abs) f = dir + L"\\" + f;
    AmxItem it;
    it.file = f;
    it.line = _wtoi(m[2].str().c_str());
    it.type = lowerStr(m[3].str()).find(L"warning") != wstring::npos ? 2 : 1;
    it.code = m[4].str();
    it.msg = m[5].str();
    items.push_back(it);
  }
  return items;
}
static void amxApplyMarks(const vector<AmxItem>& items) {
  for (auto* d : g_docs) {
    d->amxMarks.clear();
    if (d->path.empty()) continue;
    std::set<std::pair<int, int>> seen;
    for (auto& it : items) {
      if (_wcsicmp(it.file.c_str(), d->path.c_str()) != 0) continue;
      if (!seen.insert({it.line, it.type}).second) continue;
      d->amxMarks.push_back({it.line, it.type, (it.type == 1 ? L"error " : L"warning ") + it.code + L": " + it.msg});
    }
  }
  redraw();
}
static void amxShowBuild() { g_buildOpen = true; computeLayout(); ensureVisible(); redraw(); }
static void amxxHide() { g_buildOpen = false; computeLayout(); ensureVisible(); redraw(); }
static wstring amxChooseCompilerDialog(wstring& include) {
  wchar_t buf[MAX_PATH * 2] = {};
  OPENFILENAMEW o{sizeof(o)};
  o.hwndOwner = g_hwnd;
  o.lpstrFilter = L"amxxpc\0amxxpc.exe;*.exe\0All files\0*.*\0";
  o.lpstrFile = buf;
  o.nMaxFile = MAX_PATH * 2;
  o.lpstrTitle = L"Choose the AMX Mod X compiler (amxxpc)";
  o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (!GetOpenFileNameW(&o)) return L"";
  wstring inc = dirNameOf(buf) + L"\\include";
  include = GetFileAttributesW(inc.c_str()) != INVALID_FILE_ATTRIBUTES ? inc : L"";
  return buf;
}
static void amxxCompile();
static void amxxBumpBuild(Doc& d);  // amxx_tools.inc
static bool amxpIsUnpackedDoc(const Doc& d);  // amxx_unpack.inc
static void amxxRebuildFromDoc(Doc& d);
static void amxxChooseCompiler() {
  wstring inc;
  wstring exe = amxChooseCompilerDialog(inc);
  if (exe.empty()) return;
  AmxEffective e = amxEffective();
  if (!e.name.empty()) {
    json profiles = e.profiles;
    std::string nm = toUtf8(e.name);
    profiles[nm]["compiler"] = toUtf8(exe);
    if (!inc.empty() && !profiles[nm].contains("include")) profiles[nm]["include"] = toUtf8(inc);
    setSetting("amxx_profiles", profiles);
  } else {
    setSetting("amxx_compiler_path", toUtf8(exe));
    if (!inc.empty() && sStr("amxx_include_dir").empty()) setSetting("amxx_include_dir", toUtf8(inc));
  }
  g_pawnFiles.clear();
  g_pawnExists.clear();
  flash(L"Compiler set: " + exe);
}
static void amxxPickProfile() {
  AmxEffective e = amxEffective();
  vector<wstring> names;
  for (auto it = e.profiles.begin(); it != e.profiles.end(); ++it) names.push_back(fromUtf8(it.key()));
  openPick(L"Build profile", [e, names](const wstring& q) {
    vector<PItem> items;
    PItem none;
    none.label = L"(Preferences: no profile)";
    none.right = e.name.empty() ? L"current" : L"";
    none.run = [] { setSetting("amxx_profile", ""); };
    items.push_back(none);
    for (auto& n : names) {
      PItem it;
      it.label = n;
      std::string key = toUtf8(n);
      it.right = n == e.name ? L"current" : (e.profiles[key].is_object() && e.profiles[key].contains("compiler") && e.profiles[key]["compiler"].is_string() ? fromUtf8(e.profiles[key]["compiler"].get<std::string>()) : L"");
      it.run = [key] { setSetting("amxx_profile", key); };
      items.push_back(it);
    }
    wstring typed = trimW(q);
    vector<PItem> out;
    for (auto& it : items) if (typed.empty() || fuzzy(it.label, typed) >= 0) out.push_back(it);
    if (!typed.empty() && std::find(names.begin(), names.end(), typed) == names.end()) {
      PItem it;
      it.label = L"New profile “" + typed + L"” from the current settings";
      it.right = L"Enter to create";
      it.run = [e, typed] {
        json profiles = e.profiles;
        profiles[toUtf8(typed)] = {{"compiler", toUtf8(e.compiler)}, {"include", toUtf8(e.include)}, {"output", toUtf8(e.output)}, {"debug", e.debug}};
        setSetting("amxx_profiles", profiles);
        setSetting("amxx_profile", toUtf8(typed));
        flash(L"Profile “" + typed + L"” created. Edit it in settings.json (amxx_profiles) or choose a compiler for it.");
      };
      out.insert(out.begin(), it);
    }
    return out;
  });
}
static void amxxCompile() {
  Doc& d = D();
  if (amxpIsUnpackedDoc(d)) { amxxRebuildFromDoc(d); return; }  // a .amxxmemory / .amxxdump / .raw of AMXX Uncompress: pack the .amxx again
  if (!isPawnDoc(d)) { flash(L"Open a .sma plugin to compile it."); return; }
  if (d.path.empty()) { flash(L"Save the plugin first (Ctrl+S): the compiler needs a file."); return; }
  amxxBumpBuild(d);
  if (d.dirty) doSave(d);
  AmxEffective e = amxEffective();
  wstring compiler = e.compiler, include = e.include;
  if (compiler.empty() || GetFileAttributesW(compiler.c_str()) == INVALID_FILE_ATTRIBUTES) {
    AmxDetected det;
    if (amxDetect(d.path, det)) { compiler = det.compiler; if (include.empty()) include = det.include; } else compiler.clear();
  }
  g_build = AmxBuild();
  g_build.have = true;
  g_build.profile = e.name;
  g_build.file = d.path;
  if (compiler.empty()) {
    g_build.noCompiler = true;
    g_build.state = L"No compiler found";
    amxShowBuild();
    return;
  }
  if (include.empty()) include = dirNameOf(compiler) + L"\\include";
  amxApplyMarks({});
  g_build.busy = true;
  g_build.state = L"Compiling " + baseName(d.path) + L"…";
  g_build.compiler = compiler;
  amxShowBuild();
  AmxBuildJob job{compiler, d.path, e.output, {include}, e.debug};
  HWND hw = g_hwnd;
  std::thread([job, hw]() mutable { amxBuildWorker(std::move(job), hw); }).detach();
}
static void amxBuildDone(AmxBuildResult* r) {
  g_build.busy = false;
  if (r->error) { g_build.state = r->errorText; g_build.ok = false; delete r; redraw(); return; }
  g_build.items = amxParseOutput(r->output, dirNameOf(r->file));
  g_build.output = r->output + L"\n(" + g_build.compiler + L")";
  g_build.outFile = r->outFile;
  int errors = 0, warnings = 0;
  for (auto& it : g_build.items) (it.type == 1 ? errors : warnings)++;
  g_build.ok = r->code == 0 && r->size > 0 && !errors;
  wchar_t ms[32];
  swprintf_s(ms, L"%.1f", r->ms / 1000.0);
  wchar_t kb[32];
  swprintf_s(kb, L"%.1f", r->size / 1024.0);
  if (g_build.ok) g_build.state = L"Compiled " + baseName(r->outFile) + L" (" + kb + L" KB) in " + ms + L" s" + (warnings ? L" - " + std::to_wstring(warnings) + L" warning" + (warnings == 1 ? L"" : L"s") : L"");
  else g_build.state = L"Failed - " + std::to_wstring(errors) + L" error" + (errors == 1 ? L"" : L"s") + (warnings ? L", " + std::to_wstring(warnings) + L" warning" + (warnings == 1 ? L"" : L"s") : L"");
  amxApplyMarks(g_build.items);
  flash(g_build.ok ? L"Compiled " + baseName(r->outFile) : L"Compilation failed - see the list below the editor");
  if (!g_build.ok) {
    for (auto& it : g_build.items) if (it.type == 1) { openAt(it.file, it.line, 0, 0); break; }
  }
  delete r;
  redraw();
}

/* ---------------------------------------------------------------- the build results panel */
static void drawBuild() {
  const Theme& t = T();
  AmxLook cl = amxLook(1);
  D2D1_RECT_F pr = panelRect(2);
  float x = pr.left, y = pr.top, w = pr.right - pr.left, h = pr.bottom - pr.top;
  fillRect(x, y, w, h, cl.bg);
  fillRect(x, y, w, 1, cl.border);
  g_pHits.push_back({rc(x, y - 2, w, 5), P_RESIZE_BUILD, 0});
  float m = 10;
  wstring title = L"AMX Mod X" + (g_build.profile.empty() ? wstring() : L" · " + g_build.profile);
  drawText(title.c_str(), (int)title.size(), x + m, y + 8, cl.fg, g_uiBold, 300, 20);
  float tw = textWidth(title, g_uiBold);
  DWORD sc = g_build.busy ? cl.fg : g_build.ok ? 0x7bc67e : cl.err;
  drawText(g_build.state.c_str(), (int)g_build.state.size(), x + m + tw + 12, y + 8, sc, g_ui, w - tw - 360, 20);
  float cx = x + w - m;
  auto btn = [&](const wchar_t* label, int kind) {
    float bw = textWidth(label, g_ui) + 22;
    D2D1_RECT_F r = rc(cx - bw, y + 5, bw, 26);
    panelButton(r, label, inRect(r, g_mx, g_my));
    g_pHits.push_back({r, kind, 0});
    cx -= bw + 8;
  };
  { D2D1_RECT_F xr = rc(cx - 28, y + 4, 28, 28); drawPanelClose(xr); g_pHits.push_back({xr, P_BUILD_CLOSE, 0}); cx -= 36; }
  if (g_build.noCompiler) btn(L"Choose amxxpc.exe…", P_BUILD_PICK);
  else { btn(L"Output", P_BUILD_RAW); btn(L"Compile", P_BUILD_AGAIN); }
  fillRect(x, y + 36, w, 1, cl.border);
  float top = y + 37, viewH = h - 37;
  g_pHits.push_back({rc(x, top, w, viewH), P_BUILD_LIST, 0});
  g_rt->PushAxisAlignedClip(D2D1::RectF(x, top, x + w, y + h), D2D1_ANTIALIAS_MODE_ALIASED);
  IDWriteTextFormat* mono = monoFont(13.f);
  float total = 0;
  if (g_build.noCompiler) {
    wstring msg = L"Scriba looked in the profile, in Preferences, and next to the plugin (and in its “scripting” folders). Choose amxxpc.exe once; its “include” folder is used automatically.";
    drawWrapped(msg, g_ui, x + 14, top + 12, w - 28, cl.fg, 0.75f, 18.f);
  } else if (g_build.raw) {
    float hh = wrappedHeight(g_build.output, uiFont(12.5f, false, true), w - 24, 17.f);
    total = hh + 16;
    drawWrapped(g_build.output, uiFont(12.5f, false, true), x + m, top + 8 - g_buildScroll, w - 24, cl.fg, 1.f, 17.f);
  } else if (g_build.items.empty() && !g_build.busy) {
    const wchar_t* msg = g_build.ok ? L"No errors or warnings." : L"The compiler printed no recognisable error lines. Press “Output” to read it.";
    drawText(msg, (int)wcslen(msg), x + 14, top + 12, cl.fg, g_ui, w, 20);
  } else {
    float ry = top - g_buildScroll;
    // the file column is as wide as the longest "file:line" (at most a third of the panel), the message starts after it
    float locW = 90;
    for (auto& it : g_build.items) locW = std::max(locW, textWidth(baseName(it.file) + L":" + std::to_wstring(it.line), mono));
    locW = std::min(locW + 6, std::max(120.f, w * 0.34f));
    float lx = x + m + 28 + locW + 14;
    for (size_t i = 0; i < g_build.items.size(); i++) {
      const AmxItem& it = g_build.items[i];
      if (ry + 20 > top && ry < y + h) {
        bool cur = (int)i == g_build.cur, hv = inRect(rc(x, ry, w, 20), g_mx, g_my) && g_my > top;
        if (cur || hv) fillRect(x, ry, w, 20, mixc(cl.bg, cl.fg, 0.12f));
        DWORD ic = it.type == 1 ? cl.err : cl.warn;
        drawText(it.type == 1 ? L"✕" : L"▲", 1, x + m + 2, ry + 1, ic, mono, 20, 20);
        wstring loc = baseName(it.file) + L":" + std::to_wstring(it.line);
        g_rt->PushAxisAlignedClip(rc(x + m + 28, ry, locW, 20), D2D1_ANTIALIAS_MODE_ALIASED);
        drawText(loc.c_str(), (int)loc.size(), x + m + 28, ry + 1, mixc(cl.bg, cl.fg, 0.75f), mono, 2000, 20);
        g_rt->PopAxisAlignedClip();
        g_rt->PushAxisAlignedClip(rc(lx, ry, std::max(20.f, x + w - 66 - lx), 20), D2D1_ANTIALIAS_MODE_ALIASED);
        drawText(it.msg.c_str(), (int)it.msg.size(), lx, ry + 1, cl.fg, mono, 4000, 20);
        g_rt->PopAxisAlignedClip();
        drawText(it.code.c_str(), (int)it.code.size(), x + w - 60, ry + 1, mixc(cl.bg, cl.fg, 0.45f), mono, 50, 20);
        g_pHits.push_back({rc(x, ry, w, 20), P_BUILD_ROW, (int)i});
      }
      ry += 20;
    }
    total = g_build.items.size() * 20.f + 4;
  }
  g_rt->PopAxisAlignedClip();
  float maxS = std::max(0.f, total - viewH);
  g_buildMax = maxS;
  g_buildScroll = std::max(0.f, std::min(g_buildScroll, maxS));
  panelScrollbar(x, top, w, viewH, total, &g_buildScroll);
}
static bool buildClick(const PHit& h) {
  switch (h.kind) {
    case P_BUILD_CLOSE: amxxHide(); return true;
    case P_BUILD_AGAIN: amxxCompile(); return true;
    case P_BUILD_RAW: g_build.raw = !g_build.raw; g_buildScroll = 0; return true;
    case P_BUILD_PICK: amxxChooseCompiler(); amxxCompile(); return true;
    case P_BUILD_ROW:
      g_build.cur = h.idx;
      if (h.idx < (int)g_build.items.size()) openAt(g_build.items[h.idx].file, g_build.items[h.idx].line, 0, 0);
      return true;
  }
  return false;
}
