// amxx_tools.inc: the other commands of the AMXX-Editor package: new plug-in / include from the templates, Functions List,
// Includes Tree, Search All (in every #include), the AMXX colour styles and the build number that grows on every compile.
// Included by main.cpp after amxx.inc.

static const wchar_t* AMX_TPL_PLUGIN =
    L"/* Scriba AMX Mod X */\n\n#include <amxmodx>\n// #include <reapi>\n// #include <amxmisc>\n// #include <cstrike>\n// #include <engine>\n"
    L"// #include <fakemeta>\n// #include <hamsandwich>\n// #include <fun>\n// #include <xs>\n// #include <sqlx>\n\n\n"
    L"#define PLUGIN  \"New Plug-In\"\n#define VERSION \"1.0.0-1\"\n#define AUTHOR  \"Author\"\n\n"
    L"public plugin_init()\n{\n\tregister_plugin(PLUGIN, VERSION, AUTHOR)\n\t\n\t// Add your code here...\n}\n";
static const wchar_t* AMX_TPL_INCLUDE =
    L"/* Scriba AMX Mod X */\n\n#if defined _example_included\n\t#endinput\n#endif\n#define _example_included\n\n"
    L"/* Load custom module ?\n\n#pragma reqlib LIBRARY_NAME\n#if !defined AMXMODX_NOAUTOLOAD\n\t#pragma loadlib LIBRARY_NAME\n#endif\n\n*/\n\n\n\n// Add your code here...\n";

static void amxNewDoc(const wchar_t* text) {
  Doc* d = new Doc();
  d->lines = splitNl(text);
  d->tabW = 4;
  d->spaces = false;
  d->lang = L_PAWN;
  d->langName = L"Pawn (AMX Mod X)";
  d->invalidate(0);
  for (size_t i = 0; i < d->lines.size(); i++)
    if (d->lines[i].find(L"Add your code here") != wstring::npos) { d->cur = d->anc = {(int)i, (int)d->lines[i].size()}; break; }
  addDoc(d);
}

static void amxNewPlugin() { amxNewDoc(AMX_TPL_PLUGIN); }
static void amxNewInclude() { amxNewDoc(AMX_TPL_INCLUDE); }

static bool amxNeedPawn(Doc& d) {
  if (isPawnDoc(d)) return true;
  flash(L"Open an AMX Mod X plugin (.sma) first.");
  return false;
}
static bool amxInDirs(const vector<wstring>& dirs, const wstring& file) {
  wstring dir = dirNameOf(file);
  for (auto& x : dirs) if (_wcsicmp(x.c_str(), dir.c_str()) == 0) return true;
  return false;
}
static void amxOpenSym(const wstring& file, int line1, int col, int len) {
  Doc& d = D();
  if (file.empty() || (!d.path.empty() && _wcsicmp(file.c_str(), d.path.c_str()) == 0)) gotoLine(std::max(0, line1 - 1));
  else openAt(file, line1, col, len);
}

// every function the plugin and the includes it uses define, except the ones of the compiler's own include folder
static void amxFuncList() {
  Doc& d = D();
  if (!amxNeedPawn(d)) return;
  if (g_amx.uid != d.uid) { flash(L"The plugin is being analysed, try again in a moment."); return; }
  vector<wstring> dirs = amxIncludeDirs(d);
  vector<PItem> all;
  for (auto& kv : g_amx.map) {
    const PawnSym& s = kv.second;
    if (s.kind != L"public" && s.kind != L"stock" && s.kind != L"function") continue;
    if (!s.file.empty() && amxInDirs(dirs, s.file)) continue;
    PItem it;
    it.label = s.name;
    it.right = (s.file.empty() ? d.name : baseName(s.file)) + L" : " + std::to_wstring(s.line);
    wstring file = s.file;
    int line = s.line;
    it.run = [file, line] { amxOpenSym(file, line, 0, 0); };
    all.push_back(it);
  }
  std::sort(all.begin(), all.end(), [](const PItem& a, const PItem& b) { return _wcsicmp(a.label.c_str(), b.label.c_str()) < 0; });
  openPick(L"Functions", listProvider(all));
}

// the #include files of a plugin, each once, in the order of the tree
static void amxTreeWalk(const vector<PawnInclude>& incs, const wstring& dir, int depth, std::set<wstring>& seen, vector<std::pair<int, wstring>>& out) {
  for (auto& inc : incs) {
    wstring p = pawnResolve(inc, dir);
    if (p.empty()) { out.push_back({depth, L"?" + inc.name}); continue; }
    wstring key = lowerPath(p);
    if (seen.count(key)) continue;
    seen.insert(key);
    out.push_back({depth, p});
    if (depth < 8) if (const PawnParsed* pp = pawnLoad(p)) amxTreeWalk(pp->includes, dirNameOf(p), depth + 1, seen, out);
  }
}
static vector<std::pair<int, wstring>> amxIncludeTree(Doc& d) {
  vector<std::pair<int, wstring>> out;
  wstring text;
  for (size_t i = 0; i < d.lines.size(); i++) { if (i) text += L"\n"; text += d.lines[i]; }
  std::lock_guard<std::mutex> lk(g_amxMu);
  g_pawnDirs = amxIncludeDirs(d);
  std::set<wstring> seen;
  if (!d.path.empty()) seen.insert(lowerPath(d.path));
  amxTreeWalk(parsePawn(text).includes, d.path.empty() ? L"" : dirNameOf(d.path), 0, seen, out);
  return out;
}
static void amxTree() {
  Doc& d = D();
  if (!amxNeedPawn(d)) return;
  vector<PItem> all;
  PItem root;
  root.label = d.name;
  root.right = L"this plugin";
  root.run = [] {};
  all.push_back(root);
  for (auto& e : amxIncludeTree(d)) {
    PItem it;
    bool missing = !e.second.empty() && e.second[0] == L'?';
    wstring name = missing ? e.second.substr(1) : baseName(e.second);
    it.label = wstring(e.first * 4, L' ') + L"\x2514 " + name;
    it.right = missing ? L"not found" : dirNameOf(e.second);
    wstring file = e.second;
    it.run = [file, missing] { if (!missing) openPath(file); };
    all.push_back(it);
  }
  openPick(L"Includes tree", listProvider(all));
}

static wstring amxReEscape(const wstring& s) {
  wstring r;
  for (wchar_t c : s) { if (wcschr(L"\\^$.|?*+()[]{}", c)) r += L'\\'; r += c; }
  return r;
}
// Search All: R::regex, E::exact word, I::exact word ignoring case, anything else is found inside words (ignoring case)
static void amxSearchAll() {
  Doc& d = D();
  if (!amxNeedPawn(d)) return;
  openPrompt(L"Search in all #includes   (R:: regex   E:: exact word   I:: word, ignore case)", L"", [](const wstring& q) {
    Doc& d = D();
    wstring pat = q;
    size_t bodyLen = q.size();
    if (q.size() >= 3 && q.compare(1, 2, L"::") == 0 && wcschr(L"REI", q[0])) bodyLen -= 3;
    if (bodyLen < 2) { flash(L"Type at least 2 characters."); return; }
    auto flags = std::regex_constants::ECMAScript;
    if (pat.compare(0, 3, L"R::") == 0) pat = pat.substr(3);
    else if (pat.compare(0, 3, L"E::") == 0) pat = L"\\b" + amxReEscape(pat.substr(3)) + L"\\b";
    else if (pat.compare(0, 3, L"I::") == 0) { pat = L"\\b" + amxReEscape(pat.substr(3)) + L"\\b"; flags |= std::regex_constants::icase; }
    else { pat = amxReEscape(pat); flags |= std::regex_constants::icase; }
    std::wregex re;
    try { re = std::wregex(pat, flags); } catch (...) { flash(L"Invalid pattern."); return; }
    vector<std::pair<wstring, vector<wstring>>> files;
    {
      vector<wstring> own;
      for (auto& l : d.lines) own.push_back(l);
      files.push_back({d.path, own});
    }
    for (auto& e : amxIncludeTree(d)) {
      if (e.second.empty() || e.second[0] == L'?') continue;
      bool big;
      std::string raw = readFileRaw(e.second, 4 * 1024 * 1024, big);
      if (big) continue;
      files.push_back({e.second, splitNl(decodeBytes(raw))});
    }
    vector<PItem> all;
    for (auto& f : files) {
      for (size_t i = 0; i < f.second.size() && all.size() < 500; i++) {
        const wstring& ln = f.second[i];
        std::wsmatch m;
        if (!std::regex_search(ln, m, re)) continue;
        PItem it;
        wstring t = trimW(ln);
        if (t.size() > 140) t = t.substr(0, 140) + L"\x2026";
        it.label = t;
        it.right = (f.first.empty() ? d.name : baseName(f.first)) + L" : " + std::to_wstring(i + 1);
        wstring file = f.first;
        int line = (int)i + 1, col = (int)m.position(0), len = (int)m.length(0);
        it.run = [file, line, col, len] { if (file.empty() || (!D().path.empty() && _wcsicmp(file.c_str(), D().path.c_str()) == 0)) { Doc& dd = D(); int l = std::max(0, std::min((int)dd.lines.size() - 1, line - 1)); dd.cur = {l, std::min(col + len, (int)dd.lines[l].size())}; dd.anc = {l, std::min(col, (int)dd.lines[l].size())}; revealCenter(); moved(true); } else openAt(file, line, col, len); };
        all.push_back(it);
      }
    }
    if (all.empty()) { flash(L"Nothing found for “" + q + L"”."); return; }
    openPick(L"Search results (" + std::to_wstring(all.size()) + (all.size() >= 500 ? L"+" : L"") + L")", listProvider(all));
  });
}

// the colour styles of the AMXX-Editor (the last colour schemes)
static void amxPickStyle() {
  int total = (int)(sizeof(THEMES) / sizeof(THEMES[0])), first = total - (int)(sizeof(AMX_PALS) / sizeof(AMX_PALS[0])), orig = g_theme;
  vector<PItem> all;
  for (int i = first; i < total; i++) {
    PItem it;
    it.label = THEMES[i].name;
    it.right = i == orig ? L"current" : (THEMES[i].dark ? L"dark" : L"light");
    it.run = [i] { chooseScheme(i); };
    all.push_back(it);
  }
  openPick(L"AMX Mod X color style", listProvider(all), [orig] { if (g_theme != orig) applyScheme(orig); },
           [first, total](const PItem& it) { for (int i = first; i < total; i++) if (it.label == THEMES[i].name) applyScheme(i); });
}

// "1.0.0-1" becomes "1.0.0-2" in the #define VERSION line (or new const VERSION[]) before every compile
static void amxxBumpBuild(Doc& d) {
  if (!sBool("amxx_build_version")) return;
  static const std::wregex def(L"^\\s*#define\\s+(?:PLUGIN_)?VERSION\\s+\".+\"", std::regex_constants::icase);
  static const std::wregex cst(L"new\\s+const\\s+(?:PLUGIN_)?VERSION\\s*\\[\\s*\\]\\s*=\\s*\".+\"", std::regex_constants::icase);
  static const std::wregex ver(L"(.*\"v?\\d{1,2}\\.\\d{1,2}\\.(?:\\d{1,2}-)?)(\\d+)(b(?:eta)?)?\"");
  for (int pass = 0; pass < 2; pass++) {
    for (size_t l = 0; l < d.lines.size(); l++) {
      std::wsmatch m;
      if (!std::regex_search(d.lines[l], m, pass == 0 ? def : cst)) continue;
      int start = (int)m.position(0);
      while (start < (int)d.lines[l].size() && iswspace(d.lines[l][start])) start++;
      wstring region = d.lines[l].substr(start);
      std::wsmatch v;
      if (!std::regex_search(region, v, ver, std::regex_constants::match_continuous)) return;
      wstring nl = v[1].str() + std::to_wstring(_wtoi(v[2].str().c_str()) + 1) + v[3].str() + L"\"";
      d.snapshot(0);
      d.replaceRange({(int)l, start}, {(int)l, start + (int)v.length(0)}, nl);
      return;
    }
  }
}

// popup / console styles: a list with a live preview
static void amxPickLook(int which) {
  const AmxLook* arr = which ? AMX_CONSOLES : AMX_POPUPS;
  int n = which ? (int)(sizeof(AMX_CONSOLES) / sizeof(AmxLook)) : (int)(sizeof(AMX_POPUPS) / sizeof(AmxLook));
  const char* key = which ? "amxx_console_style" : "amxx_popup_style";
  int cur = amxLookIdx(which);
  vector<PItem> all;
  for (int i = -1; i < n; i++) {
    PItem it;
    it.label = i < 0 ? L"Default" : arr[i].name;
    it.right = i == cur ? L"current" : (i < 0 ? L"colours of the colour scheme" : L"");
    wstring nm = it.label;
    it.run = [key, nm] { g_lookPrev[0] = g_lookPrev[1] = -2; setSetting(key, toUtf8(nm)); redraw(); };
    all.push_back(it);
  }
  openPick(which ? L"Compile results style" : L"Popup style", listProvider(all), [] { g_lookPrev[0] = g_lookPrev[1] = -2; redraw(); },
           [which, arr, n](const PItem& it) { g_lookPrev[which] = -1; for (int i = 0; i < n; i++) if (it.label == arr[i].name) g_lookPrev[which] = i; redraw(); });
}
