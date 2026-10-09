// Scriba (native C++ edition): Win32 + Direct2D/DirectWrite, no browser engine, no third-party UI toolkit.
#include <windows.h>
#include <tree_sitter/api.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <regex>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <cwctype>
#include <string>
#include <unordered_set>
#include <vector>
#include <deque>

using std::wstring;
using std::vector;

/* ---------------------------------------------------------------- themes */
struct Theme {
  const wchar_t* name;
  bool dark;
  DWORD window, bg, gutterFg, gutterActive, fg, chrome, chromeFg, chromeHover, menu, menuHi, menuSep, tabbar, tabText, tabActive, status,
      statusText, caret, sel, panel, panelHi, input, border, accent, sidebar, sidebarFg, sidebarHi, sidebarHead, scroll, scrollHover, ws,
      bookmark, btn, btnHi, matchOutline, tabIcon;
  DWORD syn[13];  // def keyword string number comment fn type tag heading attr op link invalid
};
#include "themes_data.inc"
static int g_theme = 0;
// the AMXX-Editor colour schemes (the last entries of THEMES) also colour every Pawn syntax kind (13 + AK_*) and give it a font style
struct AmxPal { DWORD col[33]; unsigned char sty[33]; DWORD invBg; };
static const AmxPal AMX_PALS[] = {
#include "amx_pals.inc"
};
static const AmxPal* amxPal();
static Theme g_curTheme = THEMES[0];  // the active scheme, with the accent colour setting applied
static const Theme& T() { return g_curTheme; }
static const AmxPal* amxPal() {
  int first = (int)(sizeof(THEMES) / sizeof(THEMES[0])) - (int)(sizeof(AMX_PALS) / sizeof(AMX_PALS[0]));
  return g_theme >= first ? &AMX_PALS[g_theme - first] : nullptr;
}
// colour / font style of a syntax span kind; the Pawn kinds fall back to the ordinary kinds when the scheme is not an AMXX one
static DWORD synCol(int k) {
  if (k < 13) return g_curTheme.syn[k];
  if (const AmxPal* p = amxPal()) return p->col[k];
  static const unsigned char fb[20] = {1, 2, 3, 9, 1, 10, 0, 0, 0, 0, 1, 6, 7, 3, 3, 5, 5, 3, 3, 12};
  return g_curTheme.syn[fb[k - 13]];
}
static int synStyle(int k, bool pawn) {  // 1 bold, 2 italic
  const AmxPal* p = pawn ? amxPal() : nullptr;
  return p && (k >= 13 || (k >= 2 && k <= 4)) ? p->sty[k] : 0;
}

/* ---------------------------------------------------------------- syntax */
enum Lang { L_PLAIN, L_CLIKE, L_PY, L_MD, L_HTML, L_SH, L_DIFF, L_PAWN };
static bool isClike(Lang l) { return l == L_CLIKE || l == L_PAWN; }  // brace languages (Pawn included)
struct Span { int a, b, kind; };
static const std::unordered_set<wstring> KW_PAWN = {
    L"if", L"else", L"for", L"while", L"do", L"switch", L"case", L"default", L"return", L"break", L"continue", L"sizeof", L"tagof",
    L"charsmax", L"new", L"static", L"const", L"public", L"stock", L"native", L"forward", L"enum", L"goto", L"sleep", L"exit", L"assert",
    L"state", L"operator", L"defined", L"char"};
static const std::unordered_set<wstring> CONST_PAWN = {
    L"true", L"false", L"EOS", L"cellmin", L"cellmax", L"cellbits", L"__DATE__", L"__TIME__", L"__LINE__", L"__BINARY__", L"__FILE__", L"INVALID_HANDLE"};

static const std::unordered_set<wstring> KW_C = {
    L"if", L"else", L"for", L"while", L"do", L"switch", L"case", L"default", L"break", L"continue", L"return", L"goto",
    L"function", L"var", L"let", L"const", L"new", L"delete", L"class", L"struct", L"enum", L"union", L"typedef", L"namespace",
    L"using", L"template", L"typename", L"public", L"private", L"protected", L"static", L"stock", L"native", L"forward",
    L"extends", L"implements", L"import", L"export", L"from", L"package", L"try", L"catch", L"finally", L"throw", L"throws",
    L"this", L"super", L"void", L"true", L"false", L"null", L"nullptr", L"undefined", L"async", L"await", L"yield", L"in",
    L"of", L"instanceof", L"typeof", L"inline", L"virtual", L"override", L"final", L"volatile", L"extern", L"register",
    L"fn", L"impl", L"trait", L"mod", L"pub", L"use", L"match", L"mut", L"self", L"loop", L"unsafe", L"as", L"where", L"defer",
    L"go", L"chan", L"func", L"interface", L"type", L"range", L"sizeof", L"operator", L"explicit", L"friend", L"auto",
    L"and", L"or", L"not", L"foreach", L"echo", L"elseif", L"endif", L"include", L"require"};
static const std::unordered_set<wstring> TY_C = {
    L"int", L"char", L"float", L"double", L"long", L"short", L"unsigned", L"signed", L"bool", L"boolean", L"string", L"size_t",
    L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t", L"int8_t", L"int16_t", L"int32_t", L"int64_t", L"byte", L"wchar_t",
    L"i32", L"u32", L"i64", L"u64", L"f32", L"f64", L"usize", L"str", L"String", L"Float", L"Array", L"Object", L"Map", L"Set"};
static const std::unordered_set<wstring> KW_PY = {
    L"def", L"class", L"if", L"elif", L"else", L"for", L"while", L"return", L"import", L"from", L"as", L"try", L"except",
    L"finally", L"raise", L"with", L"lambda", L"pass", L"break", L"continue", L"yield", L"global", L"nonlocal", L"in", L"is",
    L"not", L"and", L"or", L"None", L"True", L"False", L"async", L"await", L"del", L"assert", L"self"};
static const std::unordered_set<wstring> KW_SH = {
    L"if", L"then", L"else", L"elif", L"fi", L"for", L"while", L"do", L"done", L"case", L"esac", L"function", L"return",
    L"in", L"echo", L"export", L"local", L"set", L"exit", L"cd", L"rem", L"call", L"goto", L"setlocal", L"endlocal"};

static Lang langForPath(const wstring& p, const wchar_t** nameOut) {
  size_t d = p.find_last_of(L'.');
  wstring e = d == wstring::npos ? L"" : p.substr(d + 1);
  for (auto& c : e) c = (wchar_t)towlower(c);
  auto in = [&](std::initializer_list<const wchar_t*> l) { for (auto s : l) if (e == s) return true; return false; };
  if (in({L"c", L"h", L"cpp", L"cc", L"cxx", L"hpp", L"hh", L"cs", L"java", L"js", L"jsx", L"mjs", L"ts", L"tsx", L"go", L"rs",
          L"php", L"swift", L"kt", L"kts", L"lua", L"json", L"jsonc", L"css", L"scss", L"sma", L"inc", L"sp", L"pwn", L"pawn", L"glsl", L"m"})) {
    *nameOut = in({L"sma", L"inc", L"pwn", L"pawn"}) ? L"Pawn (AMX Mod X)" : in({L"json", L"jsonc"}) ? L"JSON" : in({L"js", L"jsx", L"mjs"}) ? L"JavaScript" : in({L"ts", L"tsx"}) ? L"TypeScript" : in({L"css", L"scss"}) ? L"CSS" : in({L"cs"}) ? L"C#" : in({L"java"}) ? L"Java" : in({L"go"}) ? L"Go" : in({L"rs"}) ? L"Rust" : in({L"php"}) ? L"PHP" : in({L"c", L"h"}) ? L"C" : L"C++";
    return in({L"sma", L"inc", L"sp", L"pwn", L"pawn"}) ? L_PAWN : L_CLIKE;
  }
  if (in({L"py", L"pyw", L"rb", L"yml", L"yaml", L"toml", L"ini", L"cfg", L"conf"})) { *nameOut = in({L"py", L"pyw"}) ? L"Python" : in({L"rb"}) ? L"Ruby" : in({L"yml", L"yaml"}) ? L"YAML" : L"Config"; return L_PY; }
  if (in({L"sh", L"bash", L"zsh", L"bat", L"cmd", L"ps1"})) { *nameOut = in({L"bat", L"cmd"}) ? L"Batch" : in({L"ps1"}) ? L"PowerShell" : L"Shell"; return L_SH; }
  if (in({L"md", L"markdown", L"txt"}) && e != L"txt") { *nameOut = L"Markdown"; return L_MD; }
  if (in({L"diff", L"patch"})) { *nameOut = L"Diff"; return L_DIFF; }
  if (in({L"html", L"htm", L"xml", L"svg", L"xhtml"})) { *nameOut = in({L"xml"}) ? L"XML" : L"HTML"; return L_HTML; }
  *nameOut = L"Plain Text";
  return L_PLAIN;
}

static bool isIdStart(wchar_t c) { return iswalpha(c) || c == L'_' || c == L'@' || c == L'$'; }
static bool isIdCh(wchar_t c) { return iswalnum(c) || c == L'_'; }
#include "pawn_tok.inc"

// tokenises one line; `state` carries block comments / fences / triple quotes to the next line
static void tokenize(const wstring& s, int& state, Lang lang, vector<Span>& out) {
  out.clear();
  const int n = (int)s.size();
  int i = 0;
  auto push = [&](int a, int b, int k) { if (b > a) out.push_back({a, b, k}); };
  auto firstNonSpace = [&]() { int k = 0; while (k < n && (s[k] == L' ' || s[k] == L'\t')) k++; return k; };

  if (lang == L_MD) {
    int f = firstNonSpace();
    if (s.compare(f, 3, L"```") == 0) { state = state == 3 ? 0 : 3; push(0, n, 2); return; }
    if (state == 3) { push(0, n, 2); return; }
    if (f < n && s[f] == L'#') { push(0, n, 8); return; }
    if (f < n && s[f] == L'>') { push(0, n, 4); return; }
    while (i < n) {
      if (s[i] == L'`') { int j = (int)s.find(L'`', i + 1); if (j != (int)wstring::npos) { push(i, j + 1, 2); i = j + 1; continue; } }
      if (s[i] == L'*' || s[i] == L'_') { wchar_t c = s[i]; int j = (int)s.find(c, i + 1); if (j != (int)wstring::npos && j > i + 1) { push(i, j + 1, 1); i = j + 1; continue; } }
      if (s[i] == L'[') { int j = (int)s.find(L']', i); if (j != (int)wstring::npos) { push(i, j + 1, 5); i = j + 1; continue; } }
      i++;
    }
    return;
  }
  if (lang == L_DIFF) {  // + green, - red, @@ purple, the file headers like comments
    if (s.empty()) return;
    int kind = 0;
    if (s.compare(0, 3, L"+++") == 0 || s.compare(0, 3, L"---") == 0 || s.compare(0, 4, L"diff") == 0 || s.compare(0, 5, L"index") == 0) kind = 4;
    else if (s[0] == L'+') kind = 2;
    else if (s[0] == L'-') kind = 7;
    else if (s.compare(0, 2, L"@@") == 0) kind = 1;
    if (kind) push(0, n, kind);
    return;
  }
  if (lang == L_PLAIN) return;
  if (lang == L_PAWN) { tokenizePawn(s, state, out); return; }

  while (i < n) {
    if (state == 1) {  // block comment (C) or <!-- -->
      const wchar_t* end = lang == L_HTML ? L"-->" : L"*/";
      size_t el = lang == L_HTML ? 3 : 2;
      size_t j = s.find(end, i);
      if (j == wstring::npos) { push(i, n, 4); return; }
      push(i, (int)(j + el), 4);
      i = (int)(j + el);
      state = 0;
      continue;
    }
    if (state == 2 || state == 4) {  // python triple quote
      const wchar_t* end = state == 2 ? L"\"\"\"" : L"'''";
      size_t j = s.find(end, i);
      if (j == wstring::npos) { push(i, n, 2); return; }
      push(i, (int)j + 3, 2);
      i = (int)j + 3;
      state = 0;
      continue;
    }
    wchar_t c = s[i];
    if (c == L' ' || c == L'\t') { i++; continue; }

    if (lang == L_HTML) {
      if (s.compare(i, 4, L"<!--") == 0) { state = 1; push(i, i + 4, 4); i += 4; continue; }
      if (c == L'<') {
        int j = i + 1;
        if (j < n && (s[j] == L'/' || s[j] == L'?' || s[j] == L'!')) j++;
        while (j < n && (isIdCh(s[j]) || s[j] == L'-' || s[j] == L':')) j++;
        push(i, j, 7);
        i = j;
        while (i < n && s[i] != L'>') {
          if (s[i] == L'"' || s[i] == L'\'') { int e = (int)s.find(s[i], i + 1); e = e == (int)wstring::npos ? n : e + 1; push(i, e, 2); i = e; }
          else if (isIdStart(s[i])) { int e = i; while (e < n && (isIdCh(s[e]) || s[e] == L'-' || s[e] == L':')) e++; push(i, e, 6); i = e; }
          else i++;
        }
        if (i < n) { push(i, i + 1, 7); i++; }
        continue;
      }
      i++;
      continue;
    }

    bool hashComment = lang == L_PY || lang == L_SH;
    if (isClike(lang)) {
      if (c == L'/' && i + 1 < n && s[i + 1] == L'/') { push(i, n, 4); return; }
      if (c == L'/' && i + 1 < n && s[i + 1] == L'*') { state = 1; push(i, i + 2, 4); i += 2; continue; }
      if (c == L'#' && i == firstNonSpace()) {
        if (lang != L_PAWN) { push(i, n, 1); return; }
        // Pawn: the directive word is a keyword, <name> after #include is a string, the rest is code
        int j = i + 1;
        while (j < n && s[j] == L' ') j++;
        while (j < n && iswalpha(s[j])) j++;
        push(i, j, 1);
        wstring word = s.substr(i, j - i);
        if (word.find(L"include") != wstring::npos) {
          int p = j;
          while (p < n && s[p] == L' ') p++;
          if (p < n && s[p] == L'<') { int e = (int)s.find(L'>', p); e = e == (int)wstring::npos ? n : e + 1; push(p, e, 2); j = e; }
        }
        i = j;
        continue;
      }
    } else if (hashComment) {
      if (c == L'#') { push(i, n, 4); return; }
      if (lang == L_SH && (s.compare(i, 3, L"rem") == 0 || s.compare(i, 3, L"REM") == 0) && i == firstNonSpace()) { push(i, n, 4); return; }
    }
    if (lang == L_PY && (s.compare(i, 3, L"\"\"\"") == 0 || s.compare(i, 3, L"'''") == 0)) {
      state = s[i] == L'"' ? 2 : 4;
      push(i, i + 3, 2);
      i += 3;
      continue;
    }
    if (c == L'"' || c == L'\'' || (c == L'`' && isClike(lang))) {
      int j = i + 1;
      while (j < n && s[j] != c) { if (s[j] == L'\\' || (lang == L_PAWN && s[j] == L'^')) j++; j++; }
      if (j < n) j++;
      push(i, std::min(j, n), 2);
      i = std::min(j, n);
      continue;
    }
    if (iswdigit(c) || (c == L'.' && i + 1 < n && iswdigit(s[i + 1]))) {
      int j = i + 1;
      while (j < n && (iswalnum(s[j]) || s[j] == L'.' || s[j] == L'_')) j++;
      push(i, j, 3);
      i = j;
      continue;
    }
    if (isIdStart(c)) {
      int j = i + 1;
      while (j < n && isIdCh(s[j])) j++;
      wstring w = s.substr(i, j - i);
      int k = 0;
      if (lang == L_PAWN) {
        if (CONST_PAWN.count(w)) { push(i, j, 3); i = j; continue; }
        // tags (Float: bool: any:) but not "case X:" and not "name::"
        if (j < n && s[j] == L':' && !(j + 1 < n && s[j + 1] == L':') && w != L"default") {
          int q = i - 1;
          while (q >= 0 && s[q] == L' ') q--;
          bool afterCase = q >= 3 && s.compare(q - 3, 4, L"case") == 0 && (q < 4 || !isIdCh(s[q - 4]));
          if (!afterCase && !KW_PAWN.count(w)) { push(i, j + 1, 6); i = j + 1; continue; }
        }
      }
      const auto& kws = lang == L_PY ? KW_PY : lang == L_SH ? KW_SH : lang == L_PAWN ? KW_PAWN : KW_C;
      if (kws.count(w)) k = 1;
      else if (lang == L_CLIKE && TY_C.count(w)) k = 6;
      else {
        int p = j;
        while (p < n && s[p] == L' ') p++;
        if (p < n && s[p] == L'(') k = 5;
        else if (lang == L_CLIKE && iswupper(w[0]) && w.size() > 1 && iswlower(w[1])) k = 6;
      }
      push(i, j, k);
      i = j;
      continue;
    }
    i++;
  }
}

/* ---------------------------------------------------------------- document */
struct Pos { int l = 0, c = 0; };
static bool operator<(Pos a, Pos b) { return a.l < b.l || (a.l == b.l && a.c < b.c); }
static bool operator==(Pos a, Pos b) { return a.l == b.l && a.c == b.c; }

// Visual Markdown: per-line styling and concealed markup
enum { VF_BOLD = 1, VF_ITALIC = 2, VF_STRIKE = 4, VF_CODE = 8, VF_LINK = 16, VF_HEAD = 32, VF_QUOTE = 64, VF_DIM = 128, VF_ACCENT = 256 };
struct VStyle {
  vector<unsigned char> hid;
  vector<unsigned short> fl;
  vector<wchar_t> rep;
  vector<float> adv;  // advance of every character in pixels (proportional text of the visual mode)
  float scale = 1.f;
  int block = 0;  // 1 quote, 2 code, 3 rule, 4 heading, 5 list
  bool eb = false, ei = false, es = false;  // bold / italic / strike still open at the end of the line (they go on in the next line of the paragraph)
};
static unsigned g_fontEpoch = 0;  // bumped when fonts change so cached visual line styles are rebuilt
static bool g_vSyms = false, g_vLines = true;  // Visual mode: show the Markdown symbols, show line numbers
static bool g_noSnap = false;  // set while one edit is applied to several carets (single undo step)
static int countBr(const wstring& s, const vector<Span>& sp, int depth) {
  vector<char> covered(s.size(), 0);
  for (auto& x : sp) { if (x.kind >= AK_BRACKET && x.kind <= AK_BRACE) continue; for (int i = x.a; i < x.b && i < (int)s.size(); i++) covered[i] = 1; }
  for (size_t i = 0; i < s.size(); i++) {
    if (covered[i]) continue;
    wchar_t c = s[i];
    if (c == L'(' || c == L'[' || c == L'{') depth++;
    else if (c == L')' || c == L']' || c == L'}') depth--;
  }
  return depth;
}
struct Snapshot { vector<wstring> lines; Pos cur, anc; };

static int g_defTab = 4;
static bool g_defSpaces = false;
static bool g_detectIndent = true;

struct GitHunk { int kind; int from, to; vector<wstring> old; };  // kind 1 added, 2 modified, 3 deleted; lines are 1-based, `to` exclusive
static unsigned long long g_docUid = 0;
struct CcBlock;
struct WBlock { int l0 = 0, l1 = 0, kind = 0; float h = 0, iw = 0, ih = 0, boxW = 0, optW = 0; int align = 0; bool optPct = false; wstring path, alt, srcRaw; std::shared_ptr<CcBlock> blk; };  // a picture or table drawn instead of its lines (widgets.inc)
struct Doc;
// the tree-sitter tree of a document; a copy of a document starts without one
struct TsTreeBox {
  TSTree* t = nullptr;
  TsTreeBox() = default;
  TsTreeBox(const TsTreeBox&) {}
  TsTreeBox& operator=(const TsTreeBox&) { return *this; }
  ~TsTreeBox() { if (t) ts_tree_delete(t); }
};
static bool tsSpansOf(Doc& d, int line, vector<Span>& out);                 // ts.inc
static void tsEditPre(Doc& d, Pos a, Pos b, const wstring& text);
static int tsForPath(const wstring& p);
static int tsFindName(const wchar_t* name);
static const wchar_t* tsNameOf(int lang);
struct Doc {
  int tsLang = -1;  // the tree-sitter grammar of the document (-1: coloured by the simple tokenizer)
  bool tsOff = false, tsKeep = false;
  TsTreeBox tsTree;
  unsigned tsVerParsed = ~0u, tsSyncVer = ~0u;
  vector<vector<Span>> tsSpans;
  vector<unsigned char> tsOk;
  unsigned long long uid = ++g_docUid;  // identifies the document for background jobs
  struct Unk { int line, c0, len; };       // a call of a function nobody declared (AMX Mod X)
  struct AmxMk { int line; int type; wstring msg; };  // a compile error (1) or warning (2) of the last build
  vector<Unk> unknown;
  wstring lspSrv;          // key of the language server this document is open in
  std::string lspUri;
  int lspVersion = 0;
  unsigned lspSentVer = ~0u;
  DWORD lspDue = 0;
  vector<AmxMk> amxMarks;
  unsigned amxVerSeen = ~0u;
  DWORD amxDue = 0;
  bool amxDone = false;
  vector<GitHunk> gitHunks;
  DWORD gitDue = 0, gitAt = 0;
  bool gitBusy = false;
  unsigned gitVerSeen = ~0u, gitVerSeenTimer = ~0u;
  ULONGLONG askedTime = 0;  // the disk time we already asked about (external change)
  unsigned autoVer = ~0u;
  DWORD changedAt = 0;      // when the text last changed (auto save)
  wstring gitBranch;
  int gitChanges = 0;
  wstring path;
  wstring name = L"untitled";
  vector<wstring> lines{L""};
  Pos cur, anc;
  int wantV = 0;
  bool dirty = false, crlf = true, bom = false;
  int enc = 0;  // 0 UTF-8, 1 UTF-16 LE, 2 UTF-16 BE, 3 Windows-1252
  int tabW = g_defTab;
  bool spaces = g_defSpaces;  // indent with spaces instead of tab characters
  std::set<int> marks;  // bookmarked lines
  std::set<int> folded;  // first lines of collapsed blocks
  vector<char> gitMark;  // per line: 0 none, 1 added, 2 modified, 3 deleted below (filled by the Git code)
  ULONGLONG diskTime = 0;  // last write time of the file when it was loaded or saved
  bool readOnly = false;
  vector<int> wAt;      // per line: index in wb of the widget that covers it, or -1
  vector<WBlock> wb;    // pictures and tables of a note in Visual mode
  bool mdPrev = false;       // Markdown preview beside the editor
  float pvScroll = 0, pvEdScroll = -1;
  std::function<void(Pos, Pos, Pos)> onEdit;  // called after replaceRange(a, b, ...) with the end of the new text (snippets follow the edits)
  vector<char> hid;  // lines hidden by a fold
  bool isHidden(int l) const { return l >= 0 && l < (int)hid.size() && hid[l]; }
  size_t lineCount = 1;  // number of lines at the last invalidate(), to move bookmarks and folds with the text
  float scrollY = 0, scrollX = 0;
  Lang lang = L_PLAIN;
  const wchar_t* langName = L"Plain Text";
  vector<Snapshot> undo, redo;
  DWORD lastEditTick = 0;
  int lastKind = 0;
  vector<unsigned char> st;  // lexer state at the start of each line
  int stValid = 0;
  struct Sel { Pos c, a; };
  vector<Sel> extra;  // additional carets (multi-cursor)
  vector<int> rowStart;  // first visual row of each logical line (word wrap)
  vector<vector<int>> brk;  // wrap break columns per line
  unsigned rowVer = ~0u;
  int rowCols = -1;
  bool rowWrap = false;
  vector<short> bd;  // bracket depth at the start of each line
  bool vis = false;  // Visual Markdown mode
  bool isNote = false;
  int group = 0;  // split view: which group (pane) shows this tab
  wstring noteId;
  int rowAct = -2;
  vector<float> rowPx;  // top of each visual row in pixels (only when vis, headings are taller)
  mutable std::unordered_map<int, VStyle> vc;
  mutable unsigned vcVer = ~0u;
  mutable unsigned vcEpoch = ~0u;
  const VStyle& vstyle(int l) const;
  bool activeLine(int l) const {
    Pos a = selA(), b = selB();
    if (l >= a.l && l <= b.l) return true;
    for (auto& e : extra) { int lo = std::min(e.c.l, e.a.l), hi = std::max(e.c.l, e.a.l); if (l >= lo && l <= hi) return true; }
    return false;
  }
  bool concealed(int l) const { (void)l; return vis && !g_vSyms; }  // the markup stays hidden, also on the line of the caret (Settings: visual_show_syntax shows it)
  unsigned ver = 0;  // bumped on every change so caches (search matches) know when to refresh

  bool hasSel() const { return !(cur == anc); }
  Pos selA() const { return cur < anc ? cur : anc; }
  Pos selB() const { return cur < anc ? anc : cur; }

  void invalidate(int fromLine) {
    bool keepTree = tsKeep && tsTree.t && tsSyncVer == ver;  // replaceRange told the tree about this change
    tsKeep = false;
    ver++;
    if (keepTree) tsSyncVer = ver;
    else if (tsTree.t) { ts_tree_delete(tsTree.t); tsTree.t = nullptr; }
    stValid = std::min(stValid, std::max(0, fromLine));
    long delta = (long)lines.size() - (long)lineCount;
    lineCount = lines.size();
    if (delta != 0 && (!marks.empty() || !folded.empty())) {
      auto shift = [&](std::set<int>& st) {
        std::set<int> r;
        for (int m : st) r.insert(m > fromLine ? std::max(fromLine, m + (int)delta) : m);
        st.swap(r);
      };
      shift(marks);
      shift(folded);
    }
  }
  void ensureStates(int upTo) {
    if (st.size() < lines.size() + 1) st.resize(lines.size() + 1, 0);
    if (bd.size() < lines.size() + 1) bd.resize(lines.size() + 1, 0);
    if (stValid == 0) { st[0] = 0; bd[0] = 0; stValid = 1; }
    vector<Span> tmp;
    upTo = std::min(upTo, (int)lines.size() - 1);
    while (stValid <= upTo) {
      int s = st[stValid - 1];
      tokenize(lines[stValid - 1], s, lang, tmp);
      st[stValid] = (unsigned char)s;
      bd[stValid] = (short)countBr(lines[stValid - 1], tmp, bd[stValid - 1]);
      stValid++;
    }
  }
  void tokensOf(int line, vector<Span>& out) {
    ensureStates(line);
    int s = st[line];
    g_pawnConsts = lang == L_PAWN && g_constUid == uid ? &g_constSet : nullptr;
    if (!tsSpansOf(*this, line, out)) tokenize(lines[line], s, lang, out);
    if (lang == L_PAWN && !amxPal()) for (auto& sp : out) if (sp.kind >= AK_BRACKET && sp.kind <= AK_COMMA) sp.kind = 0;  // ordinary schemes: plain punctuation (rainbow brackets)
    // fill the gaps (punctuation, plain text) with default-colour spans so every character is drawn
    vector<Span> full;
    int at = 0, n = (int)lines[line].size();
    for (auto& sp : out) {
      if (sp.a > at) full.push_back({at, sp.a, 0});
      full.push_back(sp);
      at = sp.b;
    }
    if (at < n) full.push_back({at, n, 0});
    out.swap(full);
  }

  void setPath(const wstring& p) {
    path = p;
    size_t k = p.find_last_of(L"\\/");
    name = k == wstring::npos ? p : p.substr(k + 1);
    lang = langForPath(p, &langName);
    tsLang = tsForPath(p);
    tsOff = false;
    if (tsLang >= 0) langName = tsNameOf(tsLang);
    invalidate(0);
  }
  int vcol(int line, int col) const {
    const wstring& s = lines[line];
    int v = 0;
    if (vis) {
      // Visual Markdown draws proportional text: positions are pixels
      const VStyle& vs = vstyle(line);
      const bool conc = concealed(line);
      float x = 0;
      for (int i = 0; i < col && i < (int)s.size(); i++) {
        if (conc && vs.hid[i]) continue;
        x += vs.adv[i];
      }
      return (int)std::lround(x);
    }
    for (int i = 0; i < col && i < (int)s.size(); i++) v = s[i] == L'\t' ? (v / tabW + 1) * tabW : v + 1;
    return v;
  }
  int colFromV(int line, int v) const {
    const wstring& s = lines[line];
    int cv = 0;
    if (vis) {
      const VStyle& vs = vstyle(line);
      const bool conc = concealed(line);
      float x = 0;
      for (int i = 0; i < (int)s.size(); i++) {
        if (conc && vs.hid[i]) continue;
        float w = vs.adv[i];
        if (v < x + w) return (v < x + w / 2) ? i : i + 1;
        x += w;
      }
      return (int)s.size();
    }
    for (int i = 0; i < (int)s.size(); i++) {
      int nv = s[i] == L'\t' ? (cv / tabW + 1) * tabW : cv + 1;
      if (v < (cv + nv + 1) / 2 && v < nv) return i;
      if (v < nv) return i + 1;
      cv = nv;
    }
    return (int)s.size();
  }

  void snapshot(int kind) {
    if (g_noSnap) return;
    DWORD now = GetTickCount();
    bool merge = kind != 0 && kind == lastKind && now - lastEditTick < 900 && !undo.empty();
    lastEditTick = now;
    lastKind = kind;
    if (!merge) {
      if (undo.size() > 200) undo.erase(undo.begin());
      undo.push_back({lines, cur, anc});
    }
    redo.clear();
  }
  void replaceRange(Pos a, Pos b, const wstring& text) {
    if (tsTree.t) tsEditPre(*this, a, b, text);
    vector<wstring> ins;
    size_t p = 0;
    while (true) {
      size_t q = text.find(L'\n', p);
      if (q == wstring::npos) { ins.push_back(text.substr(p)); break; }
      ins.push_back(text.substr(p, q - p));
      p = q + 1;
    }
    wstring head = lines[a.l].substr(0, a.c), tail = lines[b.l].substr(b.c);
    Pos end;
    if (ins.size() == 1) {
      end = {a.l, (int)(head.size() + ins[0].size())};
      ins[0] = head + ins[0] + tail;
    } else {
      end = {a.l + (int)ins.size() - 1, (int)ins.back().size()};
      ins.front() = head + ins.front();
      ins.back() += tail;
    }
    lines.erase(lines.begin() + a.l, lines.begin() + b.l + 1);
    lines.insert(lines.begin() + a.l, ins.begin(), ins.end());
    cur = anc = end;
    dirty = true;
    invalidate(a.l);
    if (onEdit) onEdit(a, b, end);
  }
  wstring textOf(Pos a, Pos b) const {
    if (a.l == b.l) return lines[a.l].substr(a.c, b.c - a.c);
    wstring r = lines[a.l].substr(a.c) + L"\n";
    for (int i = a.l + 1; i < b.l; i++) r += lines[i] + L"\n";
    return r + lines[b.l].substr(0, b.c);
  }
  wstring selectedText() const { return hasSel() ? textOf(selA(), selB()) : L""; }
  void insertText(const wstring& t, int kind) {
    snapshot(kind);
    Pos a = selA(), b = selB();
    replaceRange(a, b, t);
  }
  void doUndo() {
    if (undo.empty()) return;
    redo.push_back({lines, cur, anc});
    Snapshot s = undo.back();
    undo.pop_back();
    lines = s.lines; cur = s.cur; anc = s.anc;
    dirty = true; lastKind = 0;
    extra.clear();
    invalidate(0);
  }
  void doRedo() {
    if (redo.empty()) return;
    undo.push_back({lines, cur, anc});
    Snapshot s = redo.back();
    redo.pop_back();
    lines = s.lines; cur = s.cur; anc = s.anc;
    dirty = true; lastKind = 0;
    extra.clear();
    invalidate(0);
  }
};

static void computeAdvances(Doc& d, int l, VStyle& o);  // defined with the fonts below
// is there a closing marker in the lines that follow, inside the same paragraph?
static bool mdLater(const Doc& d, int l, const wchar_t* pat) {
  for (int k = l + 1; k < (int)d.lines.size() && k < l + 30; k++) {
    const wstring& t = d.lines[k];
    bool blank = true;
    for (wchar_t c : t) if (c != L' ' && c != L'\t') { blank = false; break; }
    if (blank) return false;
    if (t.find(pat) != wstring::npos) return true;
  }
  return false;
}
static void mdLineStyle(Doc& d, int l, VStyle& o) {
  const wstring& s = d.lines[l];
  int n = (int)s.size();
  o.hid.assign(n, 0);
  o.fl.assign(n, 0);
  o.rep.assign(n, 0);
  o.scale = 1.f;
  o.block = 0;
  d.ensureStates(l);
  bool inFence = d.st[l] == 3;
  int ind = 0;
  while (ind < n && (s[ind] == L' ' || s[ind] == L'\t')) ind++;
  if (s.compare(ind, 3, L"```") == 0) { o.block = 2; o.scale = 0.6f; for (int i = 0; i < n; i++) { o.fl[i] = VF_DIM | VF_CODE; o.hid[i] = 1; } computeAdvances(d, l, o); return; }
  if (inFence) { o.block = 2; o.scale = 0.92f; for (int i = 0; i < n; i++) o.fl[i] = VF_CODE; computeAdvances(d, l, o); return; }
  int start = ind;
  unsigned base = 0;
  int accEnd = 0;  // end of an ordered-list number (drawn in the accent colour)
  if (ind < n && s[ind] == L'#') {
    int k = 0;
    while (ind + k < n && s[ind + k] == L'#') k++;
    if (k <= 6 && (ind + k == n || s[ind + k] == L' ')) {
      o.block = 4;
      static const float sc[7] = {1.f, 1.8f, 1.5f, 1.3f, 1.15f, 1.05f, 1.f};
      o.scale = sc[k];
      for (int i = 0; i < std::min(n, ind + k + 1); i++) o.hid[i] = 1;
      start = std::min(n, ind + k + 1);
      base = VF_HEAD | VF_BOLD;
    }
  } else if (ind < n && s[ind] == L'>') {
    o.block = 1;
    int k = ind + 1;
    if (k < n && s[k] == L' ') k++;
    for (int i = ind; i < k; i++) o.hid[i] = 1;
    start = k;
    base = VF_QUOTE;
  } else {
    wstring t = s.substr(ind);
    while (!t.empty() && t.back() == L' ') t.pop_back();
    if (t.size() >= 3 && (t[0] == L'-' || t[0] == L'*' || t[0] == L'_')) {
      bool same = true;
      for (wchar_t c : t) if (c != t[0] && c != L' ') same = false;
      if (same) { o.block = 3; for (int i = 0; i < n; i++) o.hid[i] = 1; computeAdvances(d, l, o); return; }
    }
    if (ind + 1 < n && (s[ind] == L'*' || s[ind] == L'-' || s[ind] == L'+') && s[ind + 1] == L' ') {
      if (ind + 4 < n && s[ind + 2] == L'[' && s[ind + 4] == L']' && (s[ind + 3] == L' ' || s[ind + 3] == L'x' || s[ind + 3] == L'X')) {
        o.block = 5;  // task item
        o.hid[ind] = 1;
        o.hid[ind + 1] = 1;
        o.rep[ind + 2] = s[ind + 3] == L' ' ? 0x2610 : 0x2611;
        o.hid[ind + 3] = 1;
        o.hid[ind + 4] = 1;
        start = std::min(n, ind + 5);
        if (start < n && s[start] == L' ') start++;
        if (s[ind + 3] != L' ') base = VF_DIM | VF_STRIKE;
      } else if (s[ind] == L'*') {
        o.block = 5;  // star bullet; a dash stays a dash
        o.rep[ind] = 0x2022;
        start = ind + 2;
      }
    } else {
      int k = ind;
      while (k < n && iswdigit(s[k])) k++;
      if (k > ind && k + 1 < n && (s[k] == L'.' || s[k] == L')') && s[k + 1] == L' ') { o.block = 5; accEnd = k + 1; }
    }
  }
  // inline markup
  bool bold = false, ital = false, strike = false;
  if (o.block == 0 && l > 0) {  // a paragraph goes on: what was left open in the line above is still on
    bool prevBlank = true;
    for (wchar_t c : d.lines[l - 1]) if (c != L' ' && c != L'\t') { prevBlank = false; break; }
    if (!prevBlank) {
      const VStyle& pv = d.vstyle(l - 1);
      bold = pv.eb;
      ital = pv.ei;
      strike = pv.es;
    }
  }
  for (int i = start; i < n;) {
    wchar_t c = s[i];
    unsigned f = base | (bold ? VF_BOLD : 0) | (ital ? VF_ITALIC : 0) | (strike ? VF_STRIKE : 0);
    if (c == L'`') {
      size_t j = s.find(L'`', i + 1);
      if (j != wstring::npos) {
        o.hid[i] = 1;
        o.hid[j] = 1;
        for (int k = i + 1; k < (int)j; k++) o.fl[k] = f | VF_CODE;
        i = (int)j + 1;
        continue;
      }
    }
    if ((c == L'*' || c == L'_' || c == L'~') && i + 1 < n && s[i + 1] == c) {
      bool on = ((c == L'*' || c == L'_') && bold) || (c == L'~' && strike);
      wchar_t pair[3] = {c, c, 0};
      if (on || s.find(pair, i + 2) != wstring::npos || mdLater(d, l, pair)) {
        if (c == L'~') strike = !strike; else bold = !bold;
        o.hid[i] = o.hid[i + 1] = 1;
        i += 2;
        continue;
      }
    }
    if (c == L'*' || (c == L'_' && (i == 0 || !iswalnum(s[i - 1])))) {
      wchar_t one[2] = {c, 0};
      bool opening = !ital && i + 1 < n && s[i + 1] != L' ' && (s.find(c, i + 1) != wstring::npos || mdLater(d, l, one));
      if (ital || opening) { ital = !ital; o.hid[i] = 1; i++; continue; }
    }
    bool img = c == L'!' && i + 1 < n && s[i + 1] == L'[';
    if (c == L'[' || img) {
      int b0 = img ? i + 1 : i;
      size_t cb = s.find(L']', b0 + 1);
      if (cb != wstring::npos && cb + 1 < (size_t)n && s[cb + 1] == L'(') {
        size_t cp = s.find(L')', cb + 2);
        if (cp != wstring::npos) {
          for (int k = i; k <= b0; k++) o.hid[k] = 1;
          for (int k = b0 + 1; k < (int)cb; k++) o.fl[k] = f | VF_LINK | (img ? VF_ITALIC : 0);
          for (int k = (int)cb; k <= (int)cp; k++) o.hid[k] = 1;
          i = (int)cp + 1;
          continue;
        }
      }
    }
    if (c == L'h' && (s.compare(i, 7, L"http://") == 0 || s.compare(i, 8, L"https://") == 0)) {
      int j = i;
      while (j < n && s[j] != L' ') j++;
      for (int k = i; k < j; k++) o.fl[k] = f | VF_LINK;
      i = j;
      continue;
    }
    o.fl[i] = f;
    i++;
  }
  for (int i = 0; i < start && i < n; i++) o.fl[i] |= base;
  for (int i = ind; i < accEnd && i < n; i++) o.fl[i] |= VF_ACCENT;
  if (o.block == 0) { o.eb = bold; o.ei = ital; o.es = strike; }
  computeAdvances(d, l, o);
}
const VStyle& Doc::vstyle(int l) const {
  if (vcVer != ver || vcEpoch != g_fontEpoch) { vc.clear(); vcVer = ver; vcEpoch = g_fontEpoch; }
  auto it = vc.find(l);
  if (it != vc.end()) return it->second;
  VStyle& v = vc[l];
  mdLineStyle(const_cast<Doc&>(*this), l, v);
  return v;
}

#include "doc_helpers.inc"
#include "ts.inc"

/* ---------------------------------------------------------------- file io */
static bool validUtf8(const unsigned char* d, size_t n) {
  return n == 0 || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)d, (int)n, nullptr, 0) > 0;
}
static ULONGLONG fileTime(const wstring& path);
// looks at the first lines: tabs win when most indented lines start with one, otherwise the common step of the spaces
static void guessIndent(Doc& d) {
  int tabs = 0, spaces = 0, step[9] = {};
  int prev = 0, seen = 0;
  for (size_t i = 0; i < d.lines.size() && seen < 2000; i++, seen++) {
    const wstring& s = d.lines[i];
    if (s.empty()) continue;
    if (s[0] == L'\t') { tabs++; continue; }
    int k = 0;
    while (k < (int)s.size() && s[k] == L' ') k++;
    if (k == (int)s.size()) continue;
    if (k >= 2) {
      spaces++;
      int diff = std::abs(k - prev);
      if (diff >= 2 && diff <= 8) step[diff]++;
    }
    prev = k;
  }
  if (tabs + spaces < 3) return;
  if (tabs > spaces) { d.spaces = false; return; }
  d.spaces = true;
  int best = 4, bc = 0;
  for (int s2 = 2; s2 <= 8; s2++) if (step[s2] > bc) { bc = step[s2]; best = s2; }
  d.tabW = best;
}
static bool loadFile(const wstring& path, Doc& d) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER sz;
  GetFileSizeEx(h, &sz);
  vector<unsigned char> buf((size_t)sz.QuadPart);
  DWORD got = 0;
  if (sz.QuadPart) ReadFile(h, buf.data(), (DWORD)buf.size(), &got, nullptr);
  CloseHandle(h);
  buf.resize(got);
  size_t off = 0;
  UINT cp = CP_UTF8;
  d.bom = false;
  d.enc = 0;
  wstring w;
  if (buf.size() >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) { off = 3; d.bom = true; }
  if (buf.size() >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
    w.assign((const wchar_t*)(buf.data() + 2), (buf.size() - 2) / 2);
    d.enc = 1;
    d.bom = true;
  } else if (buf.size() >= 2 && buf[0] == 0xFE && buf[1] == 0xFF) {
    w.resize((buf.size() - 2) / 2);
    for (size_t i = 0; i < w.size(); i++) w[i] = (wchar_t)((buf[2 + i * 2] << 8) | buf[3 + i * 2]);
    d.enc = 2;
    d.bom = true;
  } else {
    if (!validUtf8(buf.data() + off, buf.size() - off)) { cp = 1252; d.enc = 3; }
    int n = MultiByteToWideChar(cp, 0, (const char*)buf.data() + off, (int)(buf.size() - off), nullptr, 0);
    w.resize(n);
    if (n) MultiByteToWideChar(cp, 0, (const char*)buf.data() + off, (int)(buf.size() - off), w.data(), n);
  }
  d.lines.clear();
  size_t crlf = 0, lf = 0, p = 0;
  while (true) {
    size_t q = w.find_first_of(L"\r\n", p);
    if (q == wstring::npos) { d.lines.push_back(w.substr(p)); break; }
    d.lines.push_back(w.substr(p, q - p));
    if (w[q] == L'\r' && q + 1 < w.size() && w[q + 1] == L'\n') { crlf++; p = q + 2; }
    else { if (w[q] == L'\n') lf++; p = q + 1; }
  }
  d.crlf = crlf > lf;
  d.cur = d.anc = {0, 0};
  d.dirty = false;
  d.undo.clear(); d.redo.clear();
  d.setPath(path);
  d.diskTime = fileTime(path);
  if (g_detectIndent) guessIndent(d);
  return true;
}
static const wchar_t* ENC_NAMES[4] = {L"UTF-8", L"UTF-16 LE", L"UTF-16 BE", L"Western (Windows 1252)"};
static vector<char> encodeDoc(const Doc& d) {
  wstring w;
  for (size_t i = 0; i < d.lines.size(); i++) {
    w += d.lines[i];
    if (i + 1 < d.lines.size()) w += d.crlf ? L"\r\n" : L"\n";
  }
  vector<char> out;
  if (d.enc == 1 || d.enc == 2) {
    if (d.bom) { if (d.enc == 1) out.insert(out.end(), {'\xFF', '\xFE'}); else out.insert(out.end(), {'\xFE', '\xFF'}); }
    for (wchar_t c : w) {
      if (d.enc == 1) { out.push_back((char)(c & 255)); out.push_back((char)(c >> 8)); }
      else { out.push_back((char)(c >> 8)); out.push_back((char)(c & 255)); }
    }
    return out;
  }
  UINT cp = d.enc == 3 ? 1252 : CP_UTF8;
  int n = WideCharToMultiByte(cp, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  if (d.bom && d.enc == 0) out.insert(out.end(), {'\xEF', '\xBB', '\xBF'});
  size_t at = out.size();
  out.resize(at + n);
  if (n) WideCharToMultiByte(cp, 0, w.c_str(), (int)w.size(), out.data() + at, n, nullptr, nullptr);
  return out;
}
static ULONGLONG fileTime(const wstring& path) {
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa)) return 0;
  return ((ULONGLONG)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime;
}
static bool saveFile(Doc& d, const wstring& path) {
  vector<char> out = encodeDoc(d);
  wstring tmp = path + L".scribatmp";
  HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD wr = 0;
  bool ok = WriteFile(h, out.data(), (DWORD)out.size(), &wr, nullptr) && wr == out.size();
  CloseHandle(h);
  if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(tmp.c_str()); return false; }
  d.setPath(path);
  d.dirty = false;
  d.diskTime = fileTime(path);
  return true;
}

/* ---------------------------------------------------------------- app */
#include "commands.inc"
static bool cmdAvailable(int cmd);  // false for commands that are not implemented in this edition (shown dimmed)
static bool cmdChecked(int cmd);    // toggles that are on get a check mark in the menu

static HWND g_hwnd;
static ID2D1Factory* g_d2d;
static IDWriteFactory* g_dw;
static ID2D1HwndRenderTarget* g_hrt;  // the window's own render target
static ID2D1RenderTarget* g_rt;       // what everything draws on (the window's, or an off-screen bitmap for test screenshots)
static ID2D1SolidColorBrush* g_br;
static IDWriteTextFormat *g_mono, *g_monoB, *g_monoI, *g_monoBI, *g_ui, *g_uiSmall, *g_uiBold;
static float g_charW = 8.5f, g_lineH = 22.f, g_fontSize = 15.f;
static vector<Doc*> g_docs;
static int g_active = 0;
static bool g_caretOn = true;
static int g_openMenu = -1, g_hoverMenu = -1, g_hoverItem = -1, g_hoverTab = -1;
static bool g_dragSel = false, g_dragMap = false;
static DWORD g_lastDbl = 0;
static float g_dpiScale = 1.f;  // device pixels -> DIPs
static bool g_sbVisible = false, g_mapVisible = true;
static int g_sbView = 0;  // sidebar: 0 files, 1 notes
static void refreshNoteTitle(Doc& d);
static void drawNotesView(float top, float w, float h);
static float g_sbW = 250;
static wstring g_statusMsg;  // transient message in the status bar (like the toast of the Electron edition)
static DWORD g_statusMsgAt = 0;
static bool g_recording = false;  // macro recording
static wstring g_gitBranch;  // shown in the status bar when the project is a Git repository
static int g_gitChanges = 0;
static std::map<wstring, char> g_gitStatusMap;  // lower-case absolute path -> M A D R ? (side bar colours)
static std::set<wstring> g_gitDirSet;           // folders that hold changed files
static wstring lowerPath(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static wstring g_lspStatus;
static void flash(const wstring& m);
static int appMsgBox(HWND, const wchar_t* text, const wchar_t* title, UINT flags);  // modal.inc
static int sInt(const char* k);
// values of settings.json that the drawing and editing code reads (see applySettingsNow)
static wstring g_fontFace = L"Consolas";
static bool g_gutterOn = true, g_lineNums = true, g_hlLine = false, g_scrollPastEnd = true, g_autoClose = true, g_matchBr = true;
static bool g_trimOnSave = false, g_eofNewline = false, g_spellOn = true, g_notesVisual = true;
static int g_wsMode = 0;  // draw_white_space: 0 none, 1 selection, 2 all
static vector<int> g_rulerCols;
static int g_visualMaxW = 0, g_autoSave = 0, g_autoSaveDelay = 2;
static vector<wstring> g_folderEx, g_fileEx, g_binaryPat;
static bool g_testing = false;  // SCRIBA_TEST: the scripted test driver runs the window
static int g_instance = 0;      // 0 for the first window (process); the others keep their own session files
static void recentAdd(const char* kind, const wstring& path);
struct Doc;
static void pushClosedDoc(Doc& d);
static void lspDocClosedImpl(Doc& d);
static void lspDocSavedImpl(Doc& d);
static int g_sessionFont = 0;     // font size of the last session when it was changed with Ctrl+= (0 = use settings.json)
static bool g_sessionUi = false;  // the session had side bar / minimap state

// MENU_H is the top of the content area: a 30px title bar plus the 22px menu row
static float TITLE_H = 30, MENU_H = 52, TAB_H = 34, STATUS_H = 24;  // changed by View > Toggle Status Bar and Distraction Free
static int g_uiStyle = 0;      // 0 Classic, 1 Refined, 2 Modern, 3 Zen
static float g_crumbH = 0;     // the breadcrumb row under the tabs (Refined and Modern)
static bool g_zenMenu = false;  // Zen: the menu bar is shown in the title row after the menu button was pressed
static bool inMenuRow(float y) { return g_uiStyle == 3 ? (g_zenMenu && y < TITLE_H) : (y >= TITLE_H && y < MENU_H); }
static D2D1_RECT_F zenMenuBtn() { return D2D1::RectF(36, 5, 66, 29); }
static DWORD styChrome() { const Theme& t = T(); return g_uiStyle == 0 ? t.chrome : g_uiStyle == 3 ? t.bg : t.window; }
static DWORD styTabBar() { return g_uiStyle == 0 ? T().tabbar : styChrome(); }
static DWORD styStatus() { return g_uiStyle == 0 ? T().status : styChrome(); }
static DWORD stySidebar() { return g_uiStyle == 3 ? T().bg : T().sidebar; }
static const float MAP_W = 112, MAP_LINE = 2.f, CAP_W = 46;
static float g_mx = -1, g_my = -1;  // mouse position in DIPs, for hover effects
static int g_hoverCaption = 0;  // 1 minimise, 2 maximise, 3 close
static IDWriteTextFormat* g_capFmt_tmp = nullptr;

static Doc& D() { return *g_docs[g_active]; }

static D2D1_COLOR_F Rgb(DWORD c, float a = 1.f) { return D2D1::ColorF(c, a); }
static void fillRect(float x, float y, float w, float h, DWORD c, float a = 1.f) {
  g_br->SetColor(Rgb(c, a));
  g_rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), g_br);
}
static void drawText(const wchar_t* s, int n, float x, float y, DWORD c, IDWriteTextFormat* f, float w = 4000, float h = 30) {
  g_br->SetColor(Rgb(c));
  g_rt->DrawText(s, (UINT32)n, f, D2D1::RectF(x, y, x + w, y + h), g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
static float textWidth(const wstring& s, IDWriteTextFormat* f) {
  IDWriteTextLayout* l = nullptr;
  g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, 4000, 30, &l);
  if (!l) return 0;
  DWRITE_TEXT_METRICS m{};
  l->GetMetrics(&m);
  l->Release();
  return m.widthIncludingTrailingWhitespace;
}

static std::map<int, IDWriteTextFormat*> g_fmtCache;
static IDWriteTextFormat* fmtFor(int style, float scale) {
  int key = style * 10000 + (int)std::lround(scale * 100);
  auto it = g_fmtCache.find(key);
  if (it != g_fmtCache.end()) return it->second;
  IDWriteTextFormat* f = nullptr;
  g_dw->CreateTextFormat(g_fontFace.c_str(), nullptr, (style & 1) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                         (style & 2) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                         g_fontSize * scale, L"en-us", &f);
  if (f) f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  g_fmtCache[key] = f;
  return f;
}
// proportional UI font of the visual mode (style bit 4 = monospace, used for code)
static std::map<int, IDWriteTextFormat*> g_propCache;
static IDWriteTextFormat* propFor(int style, float scale) {
  if (style & 4) return fmtFor(style & 3, scale * 0.92f);
  int key = style * 10000 + (int)std::lround(scale * 100);
  auto it = g_propCache.find(key);
  if (it != g_propCache.end()) return it->second;
  IDWriteTextFormat* f = nullptr;
  g_dw->CreateTextFormat(L"Segoe UI", nullptr, (style & 1) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                         (style & 2) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                         g_fontSize * 1.03f * scale, L"en-us", &f);
  if (f) f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  g_propCache[key] = f;
  return f;
}
static int flagStyle(unsigned f) {
  return ((f & VF_BOLD) ? 1 : 0) | ((f & (VF_ITALIC | VF_QUOTE)) ? 2 : 0) | ((f & VF_CODE) ? 4 : 0);
}
static void computeAdvances(Doc& d, int l, VStyle& o) {
  const wstring& s = d.lines[l];
  const int n = (int)s.size();
  o.adv.assign(n, 0.f);
  int i = 0;
  while (i < n) {
    if (s[i] == L'\t') {
      IDWriteTextLayout* sp = nullptr;
      g_dw->CreateTextLayout(L"    ", 4, propFor(0, o.scale), 4000, 40, &sp);
      DWRITE_TEXT_METRICS tm{};
      if (sp) { sp->GetMetrics(&tm); sp->Release(); }
      o.adv[i] = tm.widthIncludingTrailingWhitespace;
      i++;
      continue;
    }
    unsigned f = o.fl[i];
    bool hd = o.hid[i] != 0;
    wchar_t rp = o.rep[i];
    if (rp == 0x2610 || rp == 0x2611 || rp == 0x2022) {  // a drawn check box (1.05em + margins) or bullet (1.15em)
      o.adv[i] = g_fontSize * 1.07f * o.scale * (rp == 0x2022 ? 1.15f : 1.55f);
      i++;
      continue;
    }
    int j = i + 1;
    if (!rp) while (j < n && s[j] != L'\t' && o.fl[j] == f && (o.hid[j] != 0) == hd && !o.rep[j]) j++;
    wstring run = rp ? wstring(1, rp) : s.substr(i, j - i);
    IDWriteTextLayout* lay = nullptr;
    g_dw->CreateTextLayout(run.c_str(), (UINT32)run.size(), propFor(flagStyle(f), o.scale), 4000, 60, &lay);
    if (lay) {
      UINT32 cnt = 0;
      lay->GetClusterMetrics(nullptr, 0, &cnt);
      vector<DWRITE_CLUSTER_METRICS> cm(cnt);
      if (cnt) lay->GetClusterMetrics(cm.data(), cnt, &cnt);
      int pos = i;
      for (UINT32 c = 0; c < cnt; c++) {
        int len = std::max<int>(1, cm[c].length);
        for (int k = 0; k < len && pos < j; k++, pos++) o.adv[pos] = cm[c].width / len;
      }
      for (; pos < j; pos++) o.adv[pos] = 0;
      lay->Release();
    } else {
      for (int k = i; k < j; k++) o.adv[k] = 8.f;
    }
    i = j;
  }
}
static void makeFonts() {
  for (auto& kv : g_fmtCache) if (kv.second) kv.second->Release();
  g_fmtCache.clear();
  for (auto& kv : g_propCache) if (kv.second) kv.second->Release();
  g_propCache.clear();
  g_fontEpoch++;
  for (IDWriteTextFormat** f : {&g_mono, &g_monoB, &g_monoI, &g_monoBI}) if (*f) { (*f)->Release(); *f = nullptr; }
  for (int v = 0; v < 4; v++) {
    IDWriteTextFormat** f = v == 0 ? &g_mono : v == 1 ? &g_monoB : v == 2 ? &g_monoI : &g_monoBI;
    g_dw->CreateTextFormat(g_fontFace.c_str(), nullptr, v & 1 ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, v & 2 ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, g_fontSize, L"en-us", f);
    if (*f) (*f)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  }
  g_charW = textWidth(L"MMMMMMMMMM", g_mono) / 10.f;
  g_lineH = std::round(g_fontSize * 1.27f);
}

static float cw() { return g_rt ? g_rt->GetSize().width : 800; }
static float ch() { return g_rt ? g_rt->GetSize().height : 600; }
// split view: every group (pane) has its own rectangle; the geometry helpers below always describe the pane in g_vp
struct VP { float left = 0, right = 800, top = 28, bottom = 600; };
static VP g_vp, g_paneRect[2];
static bool g_ccOpen = false;  // the Claude Code dock on the right
static bool g_ccFocusInput = false;  // the keyboard goes to the chat text box
static float g_ccW = 400;
static float ccLeft();         // left edge of the dock (cw() when it is closed), claude_ui.inc
static bool g_findOpen = false, g_replOpen = false;  // the Find / Replace panel under the focused pane
static VP g_findStrip;
static float findH() { return g_findOpen ? (g_replOpen ? 75.f : 42.f) : 0.f; }
static int g_layout = 0;       // 0 single, 1 two columns, 2 two rows
static int g_curGroup = 0;     // the focused group
static int g_groupActive[2] = {0, 0};  // active document of each group (the focused one follows g_active)
static float g_splitRatio = 0.5f;
static bool g_dragSplit = false;
static bool g_chordK = false;  // Ctrl+K was pressed, waiting for the second key
static int paneCount() { return g_layout == 0 ? 1 : 2; }
static float L0() { return g_vp.left; }
static const float FMT_H = 36;
static bool g_fmtBar = true;
static bool fmtVisible() { return g_fmtBar && !g_docs.empty() && g_active < (int)g_docs.size() && g_docs[g_active]->lang == L_MD; }
static float edTop() { return g_vp.top + TAB_H + g_crumbH + (fmtVisible() ? FMT_H : 0.f); }
static float edH() { return g_vp.bottom - edTop(); }
static float gutterW() {
  if (!g_gutterOn) return 8;
  if (!g_docs.empty() && D().vis && !g_vLines) return 24;
  if (!g_lineNums && !(!g_docs.empty() && D().vis)) return 34;
  int digits = 2;
  for (size_t n = D().lines.size(); n >= 100; n /= 10) digits++;
  return std::ceil(digits * g_charW) + 34;  // measured on docs/screenshots/editor.png of the Electron edition: text starts 57 px from the pane edge
}
static float textX() { return L0() + gutterW() + 6; }
static D2D1_RECT_F g_mdStrip{0, 0, 0, 0};
static float g_mdCut = 0;  // width of the Markdown preview next to the focused editor
static float mdCut() { return g_mdCut > 0 && g_vp.left == g_paneRect[std::min(g_curGroup, 1)].left && g_vp.right == g_paneRect[std::min(g_curGroup, 1)].right ? g_mdCut : 0.f; }
static float mapX() { float c = mdCut(); return g_mapVisible && c == 0 && g_uiStyle != 3 ? g_vp.right - MAP_W : g_vp.right - c; }
static bool mapShown() { return g_mapVisible && mdCut() == 0 && g_uiStyle != 3; }
// pane rectangles follow the window, the sidebar and the layout
static float panelsHeight();  // Find in Files / Run Command / build results under the editor (panel.inc)
static void computeLayout() {
  float left = g_sbVisible ? g_sbW : 0.f, right = ccLeft(), top = MENU_H, bottom = ch() - STATUS_H - panelsHeight();
  if (g_layout == 0) {
    g_paneRect[0] = {left, right, top, bottom};
  } else if (g_layout == 1) {
    float mid = std::floor(left + (right - left) * g_splitRatio);
    g_paneRect[0] = {left, mid, top, bottom};
    g_paneRect[1] = {mid + 1, right, top, bottom};
  } else {
    float mid = std::floor(top + (bottom - top) * g_splitRatio);
    g_paneRect[0] = {left, right, top, mid};
    g_paneRect[1] = {left, right, mid + 1, bottom};
  }
  int fg = std::min(g_curGroup, paneCount() - 1);
  g_findStrip = {0, 0, 0, 0};
  if (g_findOpen) {
    float fh = findH();
    g_paneRect[fg].bottom -= fh;
    g_findStrip = {g_paneRect[fg].left, g_paneRect[fg].right, g_paneRect[fg].bottom, g_paneRect[fg].bottom + fh};
  }
  g_vp = g_paneRect[fg];
  g_mdCut = 0;
  g_mdStrip = D2D1::RectF(0, 0, 0, 0);
  if (!g_docs.empty() && g_active >= 0 && g_active < (int)g_docs.size() && g_docs[g_active]->mdPrev && g_docs[g_active]->lang == L_MD) {
    float w = std::max(260.f, std::min((g_vp.right - g_vp.left) * 0.44f, (g_vp.right - g_vp.left) * 0.7f));
    g_mdCut = w;
    g_mdStrip = D2D1::RectF(g_vp.right - w, edTop(), g_vp.right, g_vp.bottom);
  }
}
static int paneAt(float x, float y) {
  for (int i = 0; i < paneCount(); i++) {
    const VP& r = g_paneRect[i];
    if (x >= r.left && x < r.right + 1 && y >= r.top && y < r.bottom + 1) return i;
  }
  return -1;
}
static float edW() { return mapX() - textX(); }

static void caretReset() { g_caretOn = true; SetTimer(g_hwnd, 1, 530, nullptr); }
static void redraw() { InvalidateRect(g_hwnd, nullptr, FALSE); }

static void updateTitle() {
  Doc& d = D();
  if (d.isNote) refreshNoteTitle(d);
  wstring t = d.name + (d.dirty && !d.isNote ? L" ●" : L"") + L" - Scriba";
  SetWindowTextW(g_hwnd, t.c_str());
}
static bool g_wrap = true, g_guides = true, g_brackets = true;
static void computeWidgets(Doc& d);
static float widgetHeightOf(const Doc& d, int l);
static const WBlock* widgetStart(const Doc& d, int l);
static void drawWidget(Doc& d, const WBlock& w, float x, float y, float clipTop, float clipBottom);
static void ensureRows(Doc& d) {
  int cols = g_wrap ? std::max(10, (int)std::floor((edW() - 24) / g_charW)) : 0;
  if (d.rowVer == d.ver && d.rowCols == cols && d.rowWrap == g_wrap && d.rowStart.size() == d.lines.size() + 1 &&
      (d.rowPx.empty() != d.vis) && (!d.vis || d.rowAct == d.cur.l)) return;
  d.rowVer = d.ver;
  d.rowAct = d.cur.l;
  d.rowCols = cols;
  d.rowWrap = g_wrap;
  size_t n = d.lines.size();
  d.rowStart.assign(n + 1, 0);
  d.brk.assign(n, vector<int>());
  recomputeHidden(d);
  computeWidgets(d);
  int row = 0;
  for (size_t l = 0; l < n; l++) {
    d.rowStart[l] = row;
    if (d.isHidden((int)l)) continue;  // inside a fold: no rows
    if (l < d.wAt.size() && d.wAt[l] >= 0) { row++; continue; }  // a picture or table: one tall row
    row++;
    if (!g_wrap) continue;
    const wstring& s = d.lines[l];
    // columns in the code editor, pixels in the proportional visual mode
    int lcols = d.vis ? std::max(60, (int)(edW() - 28)) : cols;
    if (!d.vis && (int)s.size() <= lcols / 2) continue;
    if (d.vis && s.empty()) continue;
    bool hideMarks = d.vis && !g_vSyms;
    int v = 0, rs = 0, lastSp = -1;
    for (int i = 0; i < (int)s.size(); i++) {
      if (hideMarks && d.vstyle((int)l).hid[i]) continue;
      int w = d.vis ? (int)std::lround(d.vstyle((int)l).adv[i]) : (s[i] == L'\t' ? (v / d.tabW + 1) * d.tabW - v : 1);
      if (v + w > lcols && i > rs) {
        int b = lastSp > rs ? lastSp + 1 : i;
        d.brk[l].push_back(b);
        row++;
        rs = b;
        i = b - 1;
        v = 0;
        lastSp = -1;
        continue;
      }
      v += w;
      if (s[i] == L' ') lastSp = i;
    }
  }
  d.rowStart[n] = row;
  d.rowPx.clear();
  if (d.vis) {
    d.rowPx.assign(row + 1, 0.f);
    float y = 0;
    for (size_t l = 0; l < n; l++) {
      // row heights follow the font size (zoom), headings are taller
      float hh = (d.lines[l].empty() ? 20.f : 25.f) * (g_fontSize / 15.f) * d.vstyle((int)l).scale;
      if (float wh = widgetHeightOf(d, (int)l)) hh = wh;
      int cnt = d.rowStart[l + 1] - d.rowStart[l];
      for (int k = 0; k < cnt; k++) { d.rowPx[d.rowStart[l] + k] = y; y += hh; }
    }
    d.rowPx[row] = y;
  }
}
static float lineScale(Doc& d, int l) { return d.vis ? d.vstyle(l).scale : 1.f; }
static float rowTop(Doc& d, int row) {
  ensureRows(d);
  if (!d.vis || d.rowPx.empty()) return row * g_lineH;
  return d.rowPx[std::max(0, std::min((int)d.rowPx.size() - 1, row))];
}
static float rowH(Doc& d, int row) {
  ensureRows(d);
  if (!d.vis || d.rowPx.empty()) return g_lineH;
  row = std::max(0, std::min((int)d.rowPx.size() - 2, row));
  return d.rowPx[row + 1] - d.rowPx[row];
}
static int rowAtY(Doc& d, float y) {
  ensureRows(d);
  if (!d.vis || d.rowPx.empty()) return (int)std::floor(y / g_lineH);
  auto it = std::upper_bound(d.rowPx.begin(), d.rowPx.end() - 1, y);
  return std::max(0, (int)(it - d.rowPx.begin()) - 1);
}
static int totalRows(Doc& d) { ensureRows(d); return d.rowStart.back(); }
static int lineOfRow(Doc& d, int row) {
  ensureRows(d);
  auto it = std::upper_bound(d.rowStart.begin(), d.rowStart.end() - 1, row);
  return std::max(0, (int)(it - d.rowStart.begin()) - 1);
}
static void segOf(Doc& d, int l, int k, int& a, int& b) {
  auto& br = d.brk[l];
  a = k == 0 ? 0 : br[k - 1];
  b = k < (int)br.size() ? br[k] : (int)d.lines[l].size();
}
static int rowOfPos(Doc& d, Pos p) {
  ensureRows(d);
  auto& br = d.brk[p.l];
  return d.rowStart[p.l] + (int)(std::upper_bound(br.begin(), br.end(), p.c) - br.begin());
}
static int rowVcol(Doc& d, Pos p) {
  int row = rowOfPos(d, p);  // first: it builds the rows, only then rowStart can be read
  int k = row - d.rowStart[p.l];
  int a, b;
  segOf(d, p.l, k, a, b);
  return d.vcol(p.l, p.c) - d.vcol(p.l, a);
}
static Pos posFromRowCols(Doc& d, int row, float cols) {
  int total = totalRows(d);
  row = std::max(0, std::min(total - 1, row));
  int l = lineOfRow(d, row), k = row - d.rowStart[l];
  int a, b;
  segOf(d, l, k, a, b);
  int v = std::max(0, (int)std::floor(cols + 0.5f)) + d.vcol(l, a);
  int c = std::max(a, std::min(b, d.colFromV(l, v)));
  if (k < (int)d.brk[l].size() && c >= b && b > a) c = b - 1;
  return {l, c};
}
static Pos posFromRowX(Doc& d, int row, float x) {
  int total = totalRows(d);
  row = std::max(0, std::min(total - 1, row));
  return posFromRowCols(d, row, d.vis ? x : x / (g_charW * lineScale(d, lineOfRow(d, row))));
}
static float maxScrollY() {
  Doc& d = D();
  if (!g_scrollPastEnd) return std::max(0.f, rowTop(d, totalRows(d)) - edH());
  return std::max(0.f, rowTop(d, totalRows(d) - 1));
}
static void ensureVisible() {
  Doc& d = D();
  d.cur.l = std::max(0, std::min(d.cur.l, (int)d.lines.size() - 1));
  unfoldAround(d, d.cur.l);  // a caret inside a fold opens it
  int crow = rowOfPos(d, d.cur);
  float y = rowTop(d, crow), rhh = rowH(d, crow), h = edH();
  if (y < d.scrollY) d.scrollY = y;
  else if (y + rhh > d.scrollY + h) d.scrollY = y + rhh - h;
  float x = d.vcol(d.cur.l, d.cur.c) * (d.vis ? 1.f : g_charW), w = edW() - 20;
  if (g_wrap) d.scrollX = 0;
  else if (x < d.scrollX) d.scrollX = std::max(0.f, x - 40);
  else if (x + 8 > d.scrollX + w) d.scrollX = x + 8 - w + 40;
  d.scrollY = std::max(0.f, std::min(d.scrollY, maxScrollY()));
}
static void moved(bool extend, bool keepWant = false) {
  Doc& d = D();
  if (!extend) d.anc = d.cur;
  if (!keepWant) d.wantV = rowVcol(d, d.cur);
  d.lastKind = 0;
  ensureVisible();
  caretReset();
  updateTitle();
  redraw();
}
static void edited() {
  Doc& d = D();
  d.wantV = rowVcol(d, d.cur);
  ensureVisible();
  caretReset();
  updateTitle();
  redraw();
}

/* --- tabs --- */
static int docsInGroup(int g) {
  int n = 0;
  for (auto* d : g_docs) if (d->group == g) n++;
  return n;
}
static int pickInGroup(int g, int near) {
  int n = (int)g_docs.size();
  for (int k = 0; k < n; k++) {
    int a = near - 1 - k, b = near + k;
    if (a >= 0 && a < n && g_docs[a]->group == g) return a;
    if (b >= 0 && b < n && g_docs[b]->group == g) return b;
  }
  return -1;
}
// keeps groups consistent after tabs were closed or moved: an empty group collapses the split
static void normalizeGroups(int hint) {
  if (g_layout != 0 && (docsInGroup(0) == 0 || docsInGroup(1) == 0)) {
    for (auto* d : g_docs) d->group = 0;
    g_layout = 0;
    g_curGroup = 0;
  }
  if (g_layout == 0) {
    for (auto* d : g_docs) d->group = 0;
    g_curGroup = 0;
  }
  if (g_active < 0 || g_active >= (int)g_docs.size() || g_docs[g_active]->group != g_curGroup) {
    int k = pickInGroup(g_curGroup, hint);
    if (k < 0) k = 0;
    g_active = k;
    g_curGroup = g_docs[k]->group;
  }
  for (int g = 0; g < paneCount(); g++) {
    if (g == g_curGroup) { g_groupActive[g] = g_active; continue; }
    int idx = g_groupActive[g];
    if (idx < 0 || idx >= (int)g_docs.size() || g_docs[idx]->group != g) g_groupActive[g] = pickInGroup(g, hint);
  }
  computeLayout();
}
static void activateDoc(int idx) {
  g_groupActive[g_curGroup] = g_active;
  g_active = idx;
  g_curGroup = g_docs[idx]->group;
  g_groupActive[g_curGroup] = idx;
  computeLayout();
  updateTitle();
  redraw();
}
static void focusGroup(int g) {
  if (g < 0 || g >= paneCount() || g == g_curGroup) return;
  g_groupActive[g_curGroup] = g_active;
  g_curGroup = g;
  int idx = g_groupActive[g];
  if (idx < 0 || idx >= (int)g_docs.size() || g_docs[idx]->group != g) {
    idx = pickInGroup(g, 0);
    if (idx < 0) { Doc* nd = new Doc(); nd->group = g; g_docs.push_back(nd); idx = (int)g_docs.size() - 1; }
  }
  g_active = idx;
  computeLayout();
  updateTitle();
  caretReset();
  redraw();
}
static void setLayout(int mode) {
  if (mode == g_layout) return;
  g_groupActive[g_curGroup] = g_active;
  if (mode == 0) {
    for (auto* d : g_docs) d->group = 0;
    g_layout = 0;
    g_curGroup = 0;
  } else if (g_layout == 0) {
    if (g_docs.size() > 1) {
      g_docs[g_active]->group = 1;
      g_groupActive[1] = g_active;
      g_groupActive[0] = pickInGroup(0, g_active);
    } else {
      Doc* nd = new Doc();
      nd->group = 1;
      g_docs.push_back(nd);
      g_groupActive[0] = g_active;
      g_active = (int)g_docs.size() - 1;
      g_groupActive[1] = g_active;
    }
    g_layout = mode;
    g_curGroup = 1;
  } else {
    g_layout = mode;
  }
  normalizeGroups(g_active);
  updateTitle();
  redraw();
}
static void moveTabToOtherGroup() {
  if (g_layout == 0) { setLayout(1); return; }
  int dst = 1 - g_curGroup;
  g_docs[g_active]->group = dst;
  g_curGroup = dst;
  g_groupActive[dst] = g_active;
  normalizeGroups(g_active);
  updateTitle();
  redraw();
}
static void addDoc(Doc* d) {
  d->group = g_layout == 0 ? 0 : g_curGroup;
  g_docs.push_back(d);
  g_groupActive[g_curGroup] = g_active;
  g_active = (int)g_docs.size() - 1;
  g_groupActive[d->group] = g_active;
  g_curGroup = d->group;
  computeLayout();
  updateTitle();
  redraw();
}
static void openPath(const wstring& p) {
  for (size_t i = 0; i < g_docs.size(); i++)
    if (!g_docs[i]->path.empty() && _wcsicmp(g_docs[i]->path.c_str(), p.c_str()) == 0) { activateDoc((int)i); return; }
  Doc* d = new Doc();
  if (!loadFile(p, *d)) { delete d; appMsgBox(g_hwnd, (L"Cannot open " + p).c_str(), L"Scriba", MB_ICONERROR); return; }
  if (g_docs.size() == 1 && g_docs[0]->path.empty() && !g_docs[0]->dirty && g_docs[0]->lines.size() == 1 && g_docs[0]->lines[0].empty()) {
    delete g_docs[0];
    g_docs.clear();
  }
  addDoc(d);
  recentAdd("files", p);
}
// trailing white space and the final newline, as the settings ask, just before the text is written
static void prepareForSave(Doc& d) {
  bool changed = false;
  if (g_trimOnSave)
    for (auto& ln : d.lines)
      while (!ln.empty() && (ln.back() == L' ' || ln.back() == L'\t')) { ln.pop_back(); changed = true; }
  if (g_eofNewline && !d.lines.back().empty()) { d.lines.push_back(L""); changed = true; }
  if (changed) {
    d.invalidate(0);
    d.cur.l = std::min(d.cur.l, (int)d.lines.size() - 1);
    d.cur.c = std::min(d.cur.c, (int)d.lines[d.cur.l].size());
    d.anc = d.cur;
    d.extra.clear();
  }
}
static void afterSave(Doc& d) {
  d.gitVerSeenTimer = ~0u;  // the Git marks are recomputed
  d.gitAt = 0;
  lspDocSavedImpl(d);
}
static bool doSaveAs(Doc& d) {
  wchar_t buf[MAX_PATH * 2] = {};
  wcsncpy_s(buf, d.name.c_str(), MAX_PATH);
  OPENFILENAMEW o{sizeof(o)};
  o.hwndOwner = g_hwnd;
  o.lpstrFile = buf;
  o.nMaxFile = MAX_PATH * 2;
  o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  if (!GetSaveFileNameW(&o)) return false;
  prepareForSave(d);
  bool ok = saveFile(d, buf);
  if (ok) afterSave(d);
  return ok;
}
static void flushNote(Doc& d);
static void refreshNoteTitle(Doc& d);
static wstring dataDir();
static bool licLocked();  // license.inc
static bool doSave(Doc& d) {
  if (licLocked()) return false;
  if (d.isNote) { d.dirty = true; flushNote(d); return !d.dirty; }
  if (d.path.empty()) return doSaveAs(d);
  prepareForSave(d);
  if (!saveFile(d, d.path)) { appMsgBox(g_hwnd, L"Could not save the file.", L"Scriba", MB_ICONERROR); return false; }
  afterSave(d);
  return true;
}
static bool closeDoc(int i) {
  Doc* d = g_docs[i];
  if (d->isNote) {
    // notes autosave; an empty note nobody typed into is removed
    flushNote(*d);
    bool blank = true;
    for (auto& ln : d->lines) for (wchar_t c : ln) if (!iswspace(c)) blank = false;
    if (blank) DeleteFileW((dataDir() + L"\\Notes\\" + d->noteId).c_str());
  } else if (d->dirty) {
    int r = appMsgBox(g_hwnd, (L"Save changes to " + d->name + L"?").c_str(), L"Scriba", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return false;
    if (r == IDYES && !doSave(*d)) return false;
  }
  int grp = d->group;
  pushClosedDoc(*d);
  lspDocClosedImpl(*d);
  delete d;
  g_docs.erase(g_docs.begin() + i);
  for (int g = 0; g < 2; g++) {
    if (g_groupActive[g] > i) g_groupActive[g]--;
    else if (g_groupActive[g] == i) g_groupActive[g] = -1;
  }
  if (g_active > i) g_active--;
  else if (g_active == i) g_active = -1;
  if (g_docs.empty()) {
    g_docs.push_back(new Doc());
    g_layout = 0;
    g_curGroup = 0;
    g_active = 0;
  } else if (g_active == -1) {
    // the focused tab went away: its neighbour in the same group takes over
    g_curGroup = grp;
  }
  normalizeGroups(i);
  updateTitle();
  redraw();
  return true;
}

/* --- clipboard --- */
static void clipSet(const wstring& t) {
  if (!OpenClipboard(g_hwnd)) return;
  wstring w;
  for (wchar_t c : t) { if (c == L'\n') w += L'\r'; w += c; }
  HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * 2);
  if (g) {
    memcpy(GlobalLock(g), w.c_str(), (w.size() + 1) * 2);
    GlobalUnlock(g);
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, g);
  }
  CloseClipboard();
}
static wstring clipGet() {
  wstring r;
  if (!OpenClipboard(g_hwnd)) return r;
  if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
    if (auto p = (const wchar_t*)GlobalLock(h)) { r = p; GlobalUnlock(h); }
  }
  CloseClipboard();
  wstring o;
  for (size_t i = 0; i < r.size(); i++) {
    if (r[i] == L'\r') { if (i + 1 < r.size() && r[i + 1] == L'\n') continue; o += L'\n'; }
    else o += r[i];
  }
  return o;
}

/* --- editing commands --- */
static bool isWordCh(wchar_t c) { return isIdCh(c); }
static Pos wordLeft(const Doc& d, Pos p) {
  if (p.c == 0) return p.l > 0 ? Pos{p.l - 1, (int)d.lines[p.l - 1].size()} : p;
  const wstring& s = d.lines[p.l];
  int c = p.c;
  while (c > 0 && s[c - 1] == L' ') c--;
  if (c > 0 && isWordCh(s[c - 1])) while (c > 0 && isWordCh(s[c - 1])) c--;
  else if (c > 0) c--;
  return {p.l, c};
}
static Pos wordRight(const Doc& d, Pos p) {
  const wstring& s = d.lines[p.l];
  int n = (int)s.size();
  if (p.c >= n) return p.l + 1 < (int)d.lines.size() ? Pos{p.l + 1, 0} : p;
  int c = p.c;
  if (isWordCh(s[c])) while (c < n && isWordCh(s[c])) c++;
  else c++;
  while (c < n && s[c] == L' ') c++;
  return {p.l, c};
}
static wchar_t closerOf(wchar_t c) {
  switch (c) {
    case L'(': return L')';
    case L'[': return L']';
    case L'{': return L'}';
    case L'"': return L'"';
    case L'\'': return L'\'';
    case L'`': return L'`';
    default: return 0;
  }
}
static wstring indentUnit(const Doc& d);
static void typeChar(wchar_t ch0) {
  Doc& d = D();
  wstring t(1, ch0);
  if (g_autoClose && !d.vis && !d.readOnly) {
    const wstring& s = d.lines[d.selA().l];
    int c = d.selA().c;
    wchar_t next = c < (int)s.size() && !d.hasSel() ? s[c] : 0;
    wchar_t prev = c > 0 && !d.hasSel() ? s[c - 1] : 0;
    wchar_t close = closerOf(ch0);
    bool quote = ch0 == L'"' || ch0 == L'\'' || ch0 == L'`';
    // typing the closing character in front of an identical one just steps over it
    if (!d.hasSel() && next == ch0 && (ch0 == L')' || ch0 == L']' || ch0 == L'}' || quote)) {
      d.cur.c++;
      d.anc = d.cur;
      edited();
      return;
    }
    if (close) {
      if (d.hasSel() && d.selA().l == d.selB().l) {  // wrap the selection
        wstring sel = d.selectedText();
        d.snapshot(0);
        Pos a = d.selA();
        d.replaceRange(a, d.selB(), wstring(1, ch0) + sel + wstring(1, close));
        d.anc = {a.l, a.c + 1};
        d.cur = {a.l, a.c + 1 + (int)sel.size()};
        edited();
        return;
      }
      bool nextOk = next == 0 || iswspace(next) || next == L')' || next == L']' || next == L'}' || next == L',' || next == L';' || next == L':';
      bool prevOk = !quote || !(isIdCh(prev) || prev == ch0);
      if (!d.hasSel() && nextOk && prevOk) {
        d.snapshot(1);
        d.replaceRange(d.selA(), d.selB(), wstring(1, ch0) + wstring(1, close));
        d.cur.c--;
        d.anc = d.cur;
        edited();
        return;
      }
    }
  }
  d.insertText(t, 1);
  edited();
}
// Markdown: Enter at the end of a list item, to-do or quote line starts the next item; on an empty item it ends the list
static bool mdContinueList(Doc& d) {
  if (d.lang != L_MD || d.hasSel()) return false;
  int l = d.cur.l;
  const wstring s = d.lines[l];
  int n = (int)s.size(), i = 0;
  while (i < n && (s[i] == L' ' || s[i] == L'\t')) i++;
  wstring next;
  int pre = 0;
  if (i + 1 < n && (s[i] == L'-' || s[i] == L'*' || s[i] == L'+') && s[i + 1] == L' ') {
    pre = i + 2;
    next = wstring(1, s[i]) + L" ";
    if (pre + 2 < n + 0 && s[pre] == L'[' && (s[pre + 1] == L' ' || s[pre + 1] == L'x' || s[pre + 1] == L'X') && s[pre + 2] == L']') {
      pre += 3;
      if (pre < n && s[pre] == L' ') pre++;
      next += L"[ ] ";
    }
  } else if (i < n && s[i] == L'>') {
    pre = i + 1;
    if (pre < n && s[pre] == L' ') pre++;
    next = L"> ";
  } else {
    int k = i;
    while (k < n && iswdigit(s[k])) k++;
    if (k > i && k + 1 < n && (s[k] == L'.' || s[k] == L')') && s[k + 1] == L' ') {
      pre = k + 2;
      next = std::to_wstring(_wtoi(s.substr(i, k - i).c_str()) + 1) + wstring(1, s[k]) + L" ";
    }
  }
  if (!pre || d.cur.c < pre) return false;
  d.snapshot(0);
  bool emptyItem = trimW(s.substr(pre)).empty();
  if (emptyItem && d.cur.c >= n) {  // Enter on an empty item: the marker goes away
    d.replaceRange({l, 0}, {l, n}, L"");
    d.cur = d.anc = {l, 0};
    edited();
    return true;
  }
  d.insertText(L"\n" + s.substr(0, i) + next, 0);
  edited();
  return true;
}
static void doEnter() {
  Doc& d = D();
  if (mdContinueList(d)) return;
  const wstring& s = d.lines[d.selA().l];
  wstring ind;
  for (wchar_t c : s) { if (c == L' ' || c == L'\t') ind += c; else break; }
  int upTo = d.selA().c;
  if ((int)ind.size() > upTo) ind.resize(upTo);
  // between a pair of brackets: the closing one goes to its own line, the cursor to an indented empty line
  if (!d.hasSel() && upTo > 0 && upTo < (int)s.size() && ((s[upTo - 1] == L'{' && s[upTo] == L'}') || (s[upTo - 1] == L'(' && s[upTo] == L')') || (s[upTo - 1] == L'[' && s[upTo] == L']'))) {
    wstring inner = ind + indentUnit(d);
    d.insertText(L"\n" + inner + L"\n" + ind, 0);
    d.cur = d.anc = {d.cur.l - 1, (int)inner.size()};
    edited();
    return;
  }
  d.insertText(L"\n" + ind, 0);
  edited();
}
// Visual mode: "- [ ] text" is drawn as a box; a click on the box ticks or unticks it in the text
static bool taskBoxAt(const Doc& d, int l, int& ind) {
  const wstring& s = d.lines[l];
  ind = 0;
  while (ind < (int)s.size() && (s[ind] == L' ' || s[ind] == L'\t')) ind++;
  return ind + 4 < (int)s.size() && (s[ind] == L'-' || s[ind] == L'*' || s[ind] == L'+') && s[ind + 1] == L' ' && s[ind + 2] == L'[' &&
         (s[ind + 3] == L' ' || s[ind + 3] == L'x' || s[ind + 3] == L'X') && s[ind + 4] == L']';
}
static bool taskBoxClick(Doc& d, Pos p, float x) {
  int ind;
  if (p.l < 0 || p.l >= (int)d.lines.size() || !taskBoxAt(d, p.l, ind) || d.isHidden(p.l)) return false;
  float left = textX() - d.scrollX + d.vcol(p.l, ind + 2), right = textX() - d.scrollX + d.vcol(p.l, ind + 3);
  if (x < left - 3 || x > right + 5) return false;
  d.snapshot(0);
  d.replaceRange({p.l, ind + 3}, {p.l, ind + 4}, d.lines[p.l][ind + 3] == L' ' ? L"x" : L" ");
  d.cur = d.anc = {p.l, std::min((int)d.lines[p.l].size(), ind + 6)};
  edited();
  return true;
}
// ---- Visual mode editing helpers
// the part of a line that is list / quote / heading markup (bold and the like must not start inside it)
static int mdPrefixLen(const wstring& s) {
  int n = (int)s.size(), i = 0;
  while (i < n && (s[i] == L' ' || s[i] == L'\t')) i++;
  if (i < n && s[i] == L'#') {
    int k = i;
    while (k < n && s[k] == L'#') k++;
    return k < n && s[k] == L' ' ? k + 1 : 0;
  }
  int j = i;
  while (j < n && s[j] == L'>') { j++; if (j < n && s[j] == L' ') j++; }
  i = j;
  if (i + 1 < n && (s[i] == L'-' || s[i] == L'*' || s[i] == L'+') && s[i + 1] == L' ') {
    int k = i + 2;
    if (k + 2 < n && s[k] == L'[' && (s[k + 1] == L' ' || s[k + 1] == L'x' || s[k + 1] == L'X') && s[k + 2] == L']') { k += 3; if (k < n && s[k] == L' ') k++; }
    return k;
  }
  int k = i;
  while (k < n && iswdigit(s[k])) k++;
  if (k > i && k + 1 < n && (s[k] == L'.' || s[k] == L')') && s[k + 1] == L' ') return k + 2;
  return j;
}
// an emphasis pair with nothing inside ("****" after the text between was deleted) goes away
static void mdCleanEmpty(Doc& d, int l) {
  if (l < 0 || l >= (int)d.lines.size()) return;
  wstring& s = d.lines[l];
  if (trimW(s).empty() || s.find(L"```") != wstring::npos) return;
  for (const wchar_t* pat : {L"****", L"~~~~", L"____", L"``"}) {
    size_t len = wcslen(pat);
    size_t k = s.find(pat);
    if (k == wstring::npos || trimW(s) == pat) continue;
    bool onlyMarks = true;
    for (wchar_t c : s) if (c != pat[0] && c != L' ') onlyMarks = false;
    if (onlyMarks) continue;
    d.replaceRange({l, (int)k}, {l, (int)(k + len)}, L"");
    if (d.cur.l == l && d.cur.c > (int)k) d.cur.c = std::max((int)k, d.cur.c - (int)len);
    d.anc = d.cur;
    return;
  }
}
static void doBackspace(bool word) {
  Doc& d = D();
  if (d.hasSel()) { d.snapshot(0); d.replaceRange(d.selA(), d.selB(), L""); if (d.vis) mdCleanEmpty(d, d.cur.l); edited(); return; }
  Pos p = d.cur;
  if (p.c == 0 && p.l == 0) return;
  if (d.vis && !g_vSyms && !word && p.c > 0) {  // Visual mode: delete the character you can see, not a hidden * next to it
    const VStyle& vs0 = d.vstyle(p.l);
    int q = p.c - 1;
    while (q > 0 && vs0.hid[q]) q--;
    if (q < p.c - 1 && !vs0.hid[q]) {
      d.snapshot(2);
      d.replaceRange({p.l, q}, {p.l, q + 1}, L"");
      d.cur = d.anc = {p.l, p.c - 1};
      mdCleanEmpty(d, p.l);
      edited();
      return;
    }
  }
  if (d.vis && !g_vSyms && !word && p.c > 0) {  // Visual mode: the hidden start of a line (to-do box, "## ", "> ") goes away as one piece
    const wstring& ls = d.lines[p.l];
    int ind, k = -1;
    if (taskBoxAt(d, p.l, ind)) { if (p.c >= ind + 2 && p.c <= ind + 6) k = ind; }
    else if (int pl = mdPrefixLen(ls); pl > 0 && p.c == pl && ls.find_first_not_of(L" \t") != wstring::npos && ls[ls.find_first_not_of(L" \t")] != L'#' && ls[ls.find_first_not_of(L" \t")] != L'>') {
      k = (int)ls.find_first_not_of(L" \t");  // "* ", "- ", "1. " go as one piece
    } else {
      int q = 0;
      while (q < (int)ls.size() && (ls[q] == L' ' || ls[q] == L'\t')) q++;
      int h = q;
      while (h < (int)ls.size() && ls[h] == L'#') h++;
      if (h > q && h - q <= 6 && h < (int)ls.size() && ls[h] == L' ') { if (p.c == h + 1) k = q; }
      else if (q < (int)ls.size() && ls[q] == L'>') { if (p.c == q + 1 || p.c == q + 2) k = q; }
    }
    if (k >= 0 && k < p.c) {
      d.snapshot(2);
      d.replaceRange({p.l, k}, p, L"");
      edited();
      return;
    }
  }
  // between an auto-closed pair both characters go away
  if (g_autoClose && !d.vis && p.c > 0 && p.c < (int)d.lines[p.l].size() && !word) {
    wchar_t o = d.lines[p.l][p.c - 1], c = d.lines[p.l][p.c];
    if (closerOf(o) == c) { d.snapshot(2); d.replaceRange({p.l, p.c - 1}, {p.l, p.c + 1}, L""); edited(); return; }
  }
  Pos a;
  if (word) a = wordLeft(d, p);
  else if (p.c == 0) a = {p.l - 1, (int)d.lines[p.l - 1].size()};
  else {
    a = {p.l, p.c - 1};
    // delete a whole indent step of spaces
    const wstring& s = d.lines[p.l];
    if (s[p.c - 1] == L' ') {
      int k = p.c;
      while (k > 0 && s[k - 1] == L' ') k--;
      if (k == 0 && p.c % d.tabW == 0) a.c = p.c - d.tabW;
    }
  }
  d.snapshot(2);
  d.replaceRange(a, p, L"");
  if (d.vis) mdCleanEmpty(d, d.cur.l);
  edited();
}
static void doDelete(bool word) {
  Doc& d = D();
  if (d.hasSel()) { d.snapshot(0); d.replaceRange(d.selA(), d.selB(), L""); edited(); return; }
  Pos p = d.cur, b;
  if (p.l + 1 >= (int)d.lines.size() && p.c >= (int)d.lines[p.l].size()) return;
  if (d.vis && !g_vSyms && !word && p.c < (int)d.lines[p.l].size()) {  // Visual mode: skip the hidden markup after the caret
    const VStyle& vs0 = d.vstyle(p.l);
    int q = p.c;
    while (q < (int)d.lines[p.l].size() - 1 && vs0.hid[q]) q++;
    if (q > p.c && !vs0.hid[q]) {
      d.snapshot(2);
      d.replaceRange({p.l, q}, {p.l, q + 1}, L"");
      mdCleanEmpty(d, p.l);
      edited();
      return;
    }
  }
  if (word) b = wordRight(d, p);
  else if (p.c >= (int)d.lines[p.l].size()) b = {p.l + 1, 0};
  else b = {p.l, p.c + 1};
  d.snapshot(2);
  d.replaceRange(p, b, L"");
  if (d.vis) mdCleanEmpty(d, p.l);
  edited();
}
static wstring indentUnit(const Doc& d) { return d.spaces ? wstring(d.tabW, L' ') : wstring(L"\t"); }
static void doTab(bool back) {
  Doc& d = D();
  bool multi = d.hasSel() && d.selA().l != d.selB().l;
  if (!multi && !back) {
    if (d.spaces) {
      int col = d.vcol(d.selA().l, d.selA().c);
      d.insertText(wstring(d.tabW - col % d.tabW, L' '), 1);
    } else d.insertText(L"\t", 1);
    edited();
    return;
  }
  d.snapshot(0);
  Pos a = d.selA(), b = d.selB();
  int l0 = a.l, l1 = b.l;
  if (b.c == 0 && l1 > l0) l1--;
  for (int l = l0; l <= l1; l++) {
    wstring& s = d.lines[l];
    if (!back) s.insert(0, indentUnit(d));
    else if (!s.empty() && s[0] == L'\t') s.erase(0, 1);
    else { int k = 0; while (k < d.tabW && k < (int)s.size() && s[k] == L' ') k++; s.erase(0, k); }
  }
  d.dirty = true;
  d.invalidate(l0);
  d.cur = {d.cur.l, std::min(d.cur.c, (int)d.lines[d.cur.l].size())};
  d.anc = {d.anc.l, std::min(d.anc.c, (int)d.lines[d.anc.l].size())};
  edited();
}
static void dupLine() {
  Doc& d = D();
  d.snapshot(0);
  int l = d.cur.l;
  d.lines.insert(d.lines.begin() + l + 1, d.lines[l]);
  d.cur.l++;
  d.anc = d.cur;
  d.dirty = true;
  d.invalidate(l);
  edited();
}
static void delLine() {
  Doc& d = D();
  d.snapshot(0);
  int l = d.cur.l;
  if (d.lines.size() == 1) d.lines[0].clear();
  else d.lines.erase(d.lines.begin() + l);
  d.cur.l = std::min(l, (int)d.lines.size() - 1);
  d.cur.c = std::min(d.cur.c, (int)d.lines[d.cur.l].size());
  d.anc = d.cur;
  d.dirty = true;
  d.invalidate(l);
  edited();
}

/* ---------------------------------------------------------------- shared helpers for the overlays */
static void revealCenter();
static void multi(const std::function<void()>& fn, bool isEdit = true) {
  Doc& d = D();
  if (d.extra.empty()) { fn(); return; }
  struct S { Pos c, a; int idx; };
  vector<S> v;
  v.push_back({d.cur, d.anc, 0});
  for (size_t i = 0; i < d.extra.size(); i++) v.push_back({d.extra[i].c, d.extra[i].a, (int)i + 1});
  std::sort(v.begin(), v.end(), [](const S& x, const S& y) {
    Pos xs = x.c < x.a ? x.c : x.a, ys = y.c < y.a ? y.c : y.a;
    return ys < xs;
  });
  if (isEdit) { d.snapshot(0); g_noSnap = true; }
  for (auto& s : v) { d.cur = s.c; d.anc = s.a; fn(); s.c = d.cur; s.a = d.anc; }
  g_noSnap = false;
  d.extra.clear();
  Pos prim{-1, -1};
  for (auto& s : v) if (s.idx == 0) { d.cur = s.c; d.anc = s.a; prim = s.c; }
  for (auto& s : v) {
    if (s.idx == 0 || s.c == prim) continue;
    bool dup = false;
    for (auto& e : d.extra) if (e.c == s.c) dup = true;
    if (!dup) d.extra.push_back({s.c, s.a});
  }
}
static void moveKey(WPARAM wp, bool ctrl, bool shift) {
  Doc& d = D();
  int pageRows = std::max(1, (int)(edH() / g_lineH) - 1);
  auto vmove = [&](int dr) {
    int row = rowOfPos(d, d.cur), total = totalRows(d);
    int nr = std::max(0, std::min(total - 1, row + dr));
    if (nr == row && dr < 0) d.cur.c = 0;
    else if (nr == row && dr > 0) d.cur.c = (int)d.lines[d.cur.l].size();
    else d.cur = posFromRowCols(d, nr, (float)d.wantV);
    moved(shift, true);
  };
  switch (wp) {
    case VK_LEFT:
      if (d.hasSel() && !shift) { d.cur = d.selA(); moved(false); }
      else {
        Pos p = d.cur;
        if (ctrl) p = wordLeft(d, p);
        else if (p.c > 0) p.c--;
        else if (p.l > 0) { p.l--; p.c = (int)d.lines[p.l].size(); }
        d.cur = p;
        moved(shift);
      }
      break;
    case VK_RIGHT:
      if (d.hasSel() && !shift) { d.cur = d.selB(); moved(false); }
      else {
        Pos p = d.cur;
        if (ctrl) p = wordRight(d, p);
        else if (p.c < (int)d.lines[p.l].size()) p.c++;
        else if (p.l + 1 < (int)d.lines.size()) { p.l++; p.c = 0; }
        d.cur = p;
        moved(shift);
      }
      break;
    case VK_UP: vmove(-1); break;
    case VK_DOWN: vmove(1); break;
    case VK_PRIOR: vmove(-pageRows); break;
    case VK_NEXT: vmove(pageRows); break;
    case VK_HOME: {
      const wstring& s = d.lines[d.cur.l];
      int k = 0;
      while (k < (int)s.size() && (s[k] == L' ' || s[k] == L'\t')) k++;
      d.cur.c = d.cur.c == k ? 0 : k;
      moved(shift);
      break;
    }
    case VK_END: d.cur.c = (int)d.lines[d.cur.l].size(); moved(shift); break;
  }
}
static void addNextOccurrence() {
  Doc& d = D();
  if (!d.hasSel()) {
    const wstring& s = d.lines[d.cur.l];
    int a = d.cur.c, b = d.cur.c;
    while (a > 0 && isWordCh(s[a - 1])) a--;
    while (b < (int)s.size() && isWordCh(s[b])) b++;
    if (a == b) return;
    d.anc = {d.cur.l, a};
    d.cur = {d.cur.l, b};
    moved(true);
    return;
  }
  wstring q = d.selectedText();
  if (q.empty() || q.find(L'\n') != wstring::npos) return;
  Pos from = d.selB();
  for (auto& e : d.extra) { Pos eb = e.c < e.a ? e.a : e.c; if (from < eb) from = eb; }
  int n = (int)d.lines.size();
  auto taken = [&](Pos pa) {
    if (d.selA() == pa) return true;
    for (auto& e : d.extra) if ((e.c < e.a ? e.c : e.a) == pa) return true;
    return false;
  };
  for (int i = 0; i <= n; i++) {
    int l = (from.l + i) % n;
    size_t start = i == 0 ? (size_t)from.c : 0;
    size_t p = d.lines[l].find(q, start);
    while (p != wstring::npos && taken({l, (int)p})) p = d.lines[l].find(q, p + 1);
    if (p == wstring::npos) continue;
    d.extra.push_back({d.cur, d.anc});
    d.anc = {l, (int)p};
    d.cur = {l, (int)(p + q.size())};
    d.wantV = rowVcol(d, d.cur);
    revealCenter();
    caretReset();
    redraw();
    return;
  }
}
static void addCursorVert(int dir) {
  Doc& d = D();
  Pos edge = d.cur;
  for (auto& e : d.extra) { if (dir < 0 ? e.c < edge : edge < e.c) edge = e.c; }
  int nl = edge.l + dir;
  if (nl < 0 || nl >= (int)d.lines.size()) return;
  int c = d.colFromV(nl, d.vcol(edge.l, edge.c));
  d.extra.push_back({d.cur, d.anc});
  d.cur = d.anc = {nl, c};
  moved(false);
}
static void splitSelection() {
  Doc& d = D();
  if (!d.hasSel() || d.selA().l == d.selB().l) return;
  Pos a = d.selA(), b = d.selB();
  d.extra.clear();
  bool first = true;
  for (int l = a.l; l <= b.l; l++) {
    int c1 = l == b.l ? b.c : (int)d.lines[l].size();
    if (l == b.l && c1 == 0 && l != a.l) break;
    Pos p{l, c1};
    if (first) { d.cur = d.anc = p; first = false; }
    else d.extra.push_back({p, p});
  }
  moved(false);
}
static void exec(int cmd);
static DWORD mixc(DWORD a, DWORD b, float t) {
  auto ch = [&](int sh) { return (DWORD)std::lround(((a >> sh) & 255) * (1 - t) + ((b >> sh) & 255) * t) & 255; };
  return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}
static DWORD inputBg() { return mixc(T().bg, T().dark ? 0x000000 : 0xffffff, T().dark ? 0.3f : 0.55f); }
static DWORD panelBg() { return mixc(T().bg, T().dark ? 0x000000 : 0xffffff, T().dark ? 0.18f : 0.35f); }
static bool inRect(const D2D1_RECT_F& r, float x, float y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }
static D2D1_RECT_F rc(float x, float y, float w, float h) { return D2D1::RectF(x, y, x + w, y + h); }
static void fillR(const D2D1_RECT_F& r, DWORD c, float a = 1.f, float radius = 0) {
  g_br->SetColor(Rgb(c, a));
  if (radius > 0) g_rt->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), g_br);
  else g_rt->FillRectangle(r, g_br);
}
static void strokeR(const D2D1_RECT_F& r, DWORD c, float a = 1.f, float radius = 0) {
  g_br->SetColor(Rgb(c, a));
  if (radius > 0) g_rt->DrawRoundedRectangle(D2D1::RoundedRect(r, radius, radius), g_br, 1.f);
  else g_rt->DrawRectangle(r, g_br, 1.f);
}

// single-line text input (find, replace, palette)
struct LineEdit {
  wstring t;
  int c = 0, a = 0;  // caret and selection anchor
  void set(const wstring& s) { t = s; c = a = (int)s.size(); }
  void delSel() { if (a != c) { int x = std::min(a, c), y = std::max(a, c); t.erase(x, y - x); c = a = x; } }
  void insert(const wstring& s) { delSel(); t.insert(c, s); c += (int)s.size(); a = c; }
};
// returns true when the key changed the text
static bool editKey(LineEdit& e, WPARAM wp, bool ctrl, bool shift) {
  int n = (int)e.t.size();
  auto mv = [&](int p) { e.c = std::max(0, std::min(n, p)); if (!shift) e.a = e.c; };
  switch (wp) {
    case VK_LEFT: if (e.a != e.c && !shift) mv(std::min(e.a, e.c)); else mv(ctrl ? [&] { int p = e.c; while (p > 0 && e.t[p - 1] == L' ') p--; while (p > 0 && e.t[p - 1] != L' ') p--; return p; }() : e.c - 1); return false;
    case VK_RIGHT: if (e.a != e.c && !shift) mv(std::max(e.a, e.c)); else mv(ctrl ? [&] { int p = e.c; while (p < n && e.t[p] == L' ') p++; while (p < n && e.t[p] != L' ') p++; return p; }() : e.c + 1); return false;
    case VK_HOME: mv(0); return false;
    case VK_END: mv(n); return false;
    case VK_BACK:
      if (e.a != e.c) e.delSel();
      else if (e.c > 0) { int p = e.c; if (ctrl) { while (p > 0 && e.t[p - 1] == L' ') p--; while (p > 0 && e.t[p - 1] != L' ') p--; } else p--; e.t.erase(p, e.c - p); e.c = e.a = p; }
      else return false;
      return true;
    case VK_DELETE:
      if (e.a != e.c) e.delSel();
      else if (e.c < n) { e.t.erase(e.c, 1); }
      else return false;
      return true;
    case 'A': if (ctrl) { e.a = 0; e.c = n; } return false;
    case 'C': if (ctrl && e.a != e.c) clipSet(e.t.substr(std::min(e.a, e.c), std::abs(e.a - e.c))); return false;
    case 'X': if (ctrl && e.a != e.c) { clipSet(e.t.substr(std::min(e.a, e.c), std::abs(e.a - e.c))); e.delSel(); return true; } return false;
    case 'V':
      if (ctrl) {
        wstring p = clipGet(), o;
        for (wchar_t c : p) o += (c == L'\n' || c == L'\r') ? L' ' : c;
        if (!o.empty()) { e.insert(o); return true; }
      }
      return false;
  }
  return false;
}
static void drawInput(const LineEdit& e, const D2D1_RECT_F& r, bool focus, const wchar_t* placeholder, bool err = false) {
  const Theme& t = T();
  fillR(r, inputBg(), 1.f, 5);
  strokeR(r, err ? 0xec5f66 : (focus ? t.accent : t.border), focus || err ? 0.9f : 0.5f, 5);
  float x = r.left + 9, y = r.top + (r.bottom - r.top - 18) / 2;
  float maxW = r.right - r.left - 18;
  // keep the caret in view by scrolling the text left when it is long
  float cx = textWidth(e.t.substr(0, e.c), g_ui);
  float off = cx > maxW - 4 ? cx - (maxW - 4) : 0;
  g_rt->PushAxisAlignedClip(D2D1::RectF(r.left + 4, r.top + 2, r.right - 4, r.bottom - 2), D2D1_ANTIALIAS_MODE_ALIASED);
  if (e.t.empty()) drawText(placeholder, (int)wcslen(placeholder), x, y, t.gutterFg, g_ui, maxW, 20);
  else {
    if (e.a != e.c && focus) {
      float x0 = textWidth(e.t.substr(0, std::min(e.a, e.c)), g_ui), x1 = textWidth(e.t.substr(0, std::max(e.a, e.c)), g_ui);
      fillRect(x + x0 - off, y - 1, x1 - x0, 20, t.sel);
    }
    drawText(e.t.c_str(), (int)e.t.size(), x - off, y, t.fg, g_ui, 4000, 20);
  }
  if (focus && g_caretOn) fillRect(x + cx - off, y, 1.5f, 18, t.caret);
  g_rt->PopAxisAlignedClip();
}
static void revealCenter() {
  Doc& d = D();
  int crow = rowOfPos(d, d.cur);
  float y = rowTop(d, crow), h = edH();
  if (y < d.scrollY || y + rowH(d, crow) > d.scrollY + h) d.scrollY = std::max(0.f, std::min(maxScrollY(), y - h / 2));
  ensureVisible();
}

/* ---------------------------------------------------------------- find and replace */
struct Match { int l, a, b; };
static bool g_findFocus = false;
static bool g_fSelOn = false;  // "In sel.": search only inside the selection that was current when it was switched on
static Pos g_fSelA, g_fSelB, g_fAnchor;
static wstring g_fMsg;  // a hint shown instead of the match count
static bool g_fCase = false, g_fWord = false, g_fRegex = false, g_regexBad = false;
static int g_fField = 0;  // 0 find, 1 replace
static LineEdit g_fq, g_fr;
static vector<Match> g_matches;
static wstring g_mKey;

// with "In sel." only the matches inside the remembered selection stay
static void filterSelMatches() {
  if (!g_fSelOn) return;
  vector<Match> keep;
  for (auto& m : g_matches) {
    Pos a{m.l, m.a}, b{m.l, m.b};
    if (!(a < g_fSelA) && !(g_fSelB < b)) keep.push_back(m);
  }
  g_matches = keep;
}
static void computeMatches() {
  Doc& d = D();
  wchar_t key[160];
  swprintf_s(key, L"%p|%u|%d%d%d|%d|%d:%d.%d.%d.%d|", (void*)&d, d.ver, (int)g_fCase, (int)g_fWord, (int)g_fRegex, (int)g_findOpen, (int)g_fSelOn, g_fSelA.l,
             g_fSelA.c, g_fSelB.l, g_fSelB.c);
  wstring k = wstring(key) + g_fq.t;
  if (k == g_mKey) return;
  g_mKey = k;
  g_matches.clear();
  g_regexBad = false;
  if (!g_findOpen || g_fq.t.empty()) return;
  if (g_fRegex) {
    std::wregex re;
    try {
      re.assign(g_fq.t, g_fCase ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase);
    } catch (...) { g_regexBad = true; return; }
    for (int l = 0; l < (int)d.lines.size() && g_matches.size() < 20000; l++) {
      const wstring& s = d.lines[l];
      try {
        for (std::wsregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) {
          if (it->length(0) == 0) continue;
          int a = (int)it->position(0), b = a + (int)it->length(0);
          if (g_fWord && ((a > 0 && isIdCh(s[a - 1])) || (b < (int)s.size() && isIdCh(s[b])))) continue;
          g_matches.push_back({l, a, b});
        }
      } catch (...) {}
    }
    filterSelMatches();
    return;
  }
  wstring q = g_fq.t;
  if (!g_fCase) for (auto& c : q) c = (wchar_t)towlower(c);
  for (int l = 0; l < (int)d.lines.size() && g_matches.size() < 20000; l++) {
    wstring low;
    const wstring* sp = &d.lines[l];
    if (!g_fCase) { low = d.lines[l]; for (auto& c : low) c = (wchar_t)towlower(c); sp = &low; }
    const wstring& s = *sp;
    size_t p = 0;
    while ((p = s.find(q, p)) != wstring::npos) {
      int a = (int)p, b = (int)(p + q.size());
      p += std::max<size_t>(1, q.size());
      if (g_fWord && ((a > 0 && isIdCh(s[a - 1])) || (b < (int)s.size() && isIdCh(s[b])))) continue;
      g_matches.push_back({l, a, b});
    }
  }
  filterSelMatches();
}
static int curMatchIndex() {
  Doc& d = D();
  for (size_t i = 0; i < g_matches.size(); i++)
    if (g_matches[i].l == d.selA().l && g_matches[i].a == d.selA().c && g_matches[i].l == d.selB().l && g_matches[i].b == d.selB().c) return (int)i;
  return -1;
}
static void selectMatch(const Match& m) {
  Doc& d = D();
  d.anc = {m.l, m.a};
  d.cur = {m.l, m.b};
  d.lastKind = 0;
  d.wantV = rowVcol(d, d.cur);
  revealCenter();
  caretReset();
  redraw();
}
static void findStep(int dir, bool fromStart = false) {
  computeMatches();
  if (g_matches.empty()) { redraw(); return; }
  Doc& d = D();
  Pos from = dir > 0 ? d.selB() : d.selA();
  if (fromStart) from = g_fAnchor;  // incremental search: the first match at or after where the search began
  int pick = -1;
  if (dir > 0) {
    for (size_t i = 0; i < g_matches.size(); i++) {
      Pos p{g_matches[i].l, g_matches[i].a};
      if (!(p < from)) { pick = (int)i; break; }
    }
    if (pick < 0) pick = 0;
  } else {
    for (int i = (int)g_matches.size() - 1; i >= 0; i--) {
      Pos p{g_matches[i].l, g_matches[i].a};
      if (p < from) { pick = i; break; }
    }
    if (pick < 0) pick = (int)g_matches.size() - 1;
  }
  selectMatch(g_matches[pick]);
  if (!fromStart) g_fAnchor = {g_matches[pick].l, g_matches[pick].a};
}
static wstring expandReplacement(const Match& m) {
  if (!g_fRegex) return g_fr.t;
  Doc& d = D();
  try {
    std::wregex re(g_fq.t, g_fCase ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase);
    return std::regex_replace(d.lines[m.l].substr(m.a, m.b - m.a), re, g_fr.t, std::regex_constants::format_first_only);
  } catch (...) { return g_fr.t; }
}
static void replaceOne() {
  computeMatches();
  int i = curMatchIndex();
  if (i < 0) { findStep(1, true); return; }
  Doc& d = D();
  Match m = g_matches[i];
  wstring rep = expandReplacement(m);
  d.snapshot(0);
  d.replaceRange({m.l, m.a}, {m.l, m.b}, rep);
  if (g_fSelOn && m.l == g_fSelB.l) g_fSelB.c += (int)rep.size() - (m.b - m.a);
  edited();
  g_mKey.clear();
  findStep(1);
}
static void replaceAll() {
  computeMatches();
  if (g_matches.empty()) return;
  Doc& d = D();
  d.snapshot(0);
  vector<Match> ms = g_matches;
  for (int i = (int)ms.size() - 1; i >= 0; i--) {
    wstring rep = expandReplacement(ms[i]);
    d.replaceRange({ms[i].l, ms[i].a}, {ms[i].l, ms[i].b}, rep);
    if (g_fSelOn && ms[i].l == g_fSelB.l) g_fSelB.c += (int)rep.size() - (ms[i].b - ms[i].a);
  }
  d.cur = d.anc = {std::min(d.cur.l, (int)d.lines.size() - 1), 0};
  d.cur.c = d.anc.c = std::min(d.cur.c, (int)d.lines[d.cur.l].size());
  edited();
  g_mKey.clear();
}
static void openFind(bool replace) {
  Doc& d = D();
  if (!replace && d.hasSel() && d.selA().l == d.selB().l) g_fq.set(d.selectedText());
  if (!g_findOpen) g_fAnchor = d.selA();
  g_findOpen = true;
  g_replOpen = g_replOpen || replace;
  g_findFocus = true;
  g_fField = replace ? 1 : 0;
  LineEdit& e = replace ? g_fr : g_fq;
  e.a = 0;
  e.c = (int)e.t.size();
  g_mKey.clear();
  g_fMsg.clear();
  computeLayout();
  ensureVisible();
  caretReset();
  redraw();
}
static void closeFind() {
  g_findOpen = false;
  g_findFocus = false;
  g_fSelOn = false;
  g_mKey.clear();
  computeLayout();
  redraw();
}
#include "findbar.inc"


/* ---------------------------------------------------------------- project folder (sidebar) */
struct FNode {
  wstring name, path;
  bool dir = false, open = false, loaded = false;
  vector<FNode*> kids;
};
static FNode* g_root = nullptr;
static float g_sbScroll = 0, g_olScroll = 0;
static vector<Symbol>& symbolsOf(Doc& d);
static void gotoLine(int l);
static vector<wstring> g_projFiles;  // relative paths, for Goto Anything
static bool g_projScanned = false;

static bool anyWild(const vector<wstring>& pats, const wstring& name, const wstring& full);  // panel.inc
// the side bar hides the folders and files of the settings; the project scan (Goto Anything) also skips binary files
static bool skipName(const wstring& n, bool dir, bool scan = false) {
  if (dir) return anyWild(g_folderEx, n, n);
  if (anyWild(g_fileEx, n, n)) return true;
  return scan && anyWild(g_binaryPat, n, n);
}
static void loadKids(FNode* n) {
  if (n->loaded) return;
  n->loaded = true;
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((n->path + L"\\*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    wstring nm = fd.cFileName;
    if (nm == L"." || nm == L"..") continue;
    bool dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (skipName(nm, dir)) continue;
    FNode* k = new FNode();
    k->name = nm;
    k->path = n->path + L"\\" + nm;
    k->dir = dir;
    n->kids.push_back(k);
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  std::sort(n->kids.begin(), n->kids.end(), [](FNode* a, FNode* b) {
    if (a->dir != b->dir) return a->dir;
    return _wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
  });
}
static void freeNode(FNode* n) { for (auto* k : n->kids) freeNode(k); delete n; }
static vector<FNode*> g_roots;  // the folders of the project; g_root is the first one
static vector<wstring> g_projAbs;  // absolute path of each entry of g_projFiles
static void scanProject() {
  vector<std::pair<wstring, wstring>> all;  // relative, absolute
  for (auto* rn : g_roots) {
    const wstring& root = rn->path;
    vector<wstring> stack{root};
    while (!stack.empty() && all.size() < 60000) {
      wstring dir = stack.back();
      stack.pop_back();
      WIN32_FIND_DATAW fd;
      HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
      if (h == INVALID_HANDLE_VALUE) continue;
      do {
        wstring nm = fd.cFileName;
        if (nm == L"." || nm == L"..") continue;
        bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (skipName(nm, isDir, true)) continue;
        if (isDir) stack.push_back(dir + L"\\" + nm);
        else {
          wstring rel = (dir + L"\\" + nm).substr(root.size() + 1);
          if (g_roots.size() > 1) rel = rn->name + L"\\" + rel;  // several folders: tell them apart
          all.push_back({rel, dir + L"\\" + nm});
        }
      } while (FindNextFileW(h, &fd));
      FindClose(h);
    }
  }
  std::sort(all.begin(), all.end());
  g_projFiles.clear();
  g_projAbs.clear();
  for (auto& p : all) { g_projFiles.push_back(p.first); g_projAbs.push_back(p.second); }
  g_projScanned = true;
}
static void clearRoots() {
  for (auto* r : g_roots) freeNode(r);
  g_roots.clear();
  g_root = nullptr;
  g_projFiles.clear();
  g_projAbs.clear();
  g_projScanned = false;
  g_sbScroll = 0;
}
static void addRoot(const wstring& path) {
  if (path.empty()) return;
  for (auto* r : g_roots) if (_wcsicmp(r->path.c_str(), path.c_str()) == 0) return;
  FNode* n = new FNode();
  size_t k = path.find_last_of(L"\\/");
  n->name = k == wstring::npos || k + 1 >= path.size() ? path : path.substr(k + 1);
  n->path = path;
  n->dir = true;
  n->open = true;
  loadKids(n);
  g_roots.push_back(n);
  g_root = g_roots.front();
  g_projScanned = false;
  g_sbVisible = true;
}
static void setRoot(const wstring& path) {
  clearRoots();
  addRoot(path);
}
static wstring pickFolder() {
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg)))) return L"";
  DWORD o = 0;
  dlg->GetOptions(&o);
  dlg->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  wstring r;
  if (SUCCEEDED(dlg->Show(g_hwnd))) {
    IShellItem* it = nullptr;
    if (SUCCEEDED(dlg->GetResult(&it))) {
      PWSTR p = nullptr;
      if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { r = p; CoTaskMemFree(p); }
      it->Release();
    }
  }
  dlg->Release();
  return r;
}
struct SbRow { FNode* n; int depth; };
static void flatten(FNode* n, int depth, vector<SbRow>& out) {
  out.push_back({n, depth});
  if (n->dir && n->open) {
    loadKids(n);
    for (auto* k : n->kids) flatten(k, depth + 1, out);
  }
}
static const float SB_HEAD = 67, SB_ROW = 22, NOTE_TOP = 112;
static int g_noteMode = 0;  // notes list: 0 list with previews, 1 compact titles, 2 grouped by tag, 3 grouped by colour
static int g_noteSort = 0;  // 0 newest first, 1 oldest first, 2 title A-Z, 3 date created
static float g_nFiltH = 0;  // height of the tag/colour filter chips under the notes search box
// the "Open Folder..." button of the empty sidebar (y is the top, given the height of the message above it)
static float g_openBtnY = 0;
static D2D1_RECT_F openFolderBtnRect(float y) {
  g_openBtnY = y;
  float w = textWidth(L"Open Folder...", g_ui) + 32;
  return D2D1::RectF(14, y, 14 + w, y + 30);
}
static DWORD sbHi() { return mixc(T().sidebar, T().fg, 0.09f); }
static DWORD sbFg() { return mixc(T().sidebar, T().fg, 0.72f); }
// the Files | Notes | Outline pills at the top of the sidebar
static D2D1_RECT_F sbPill(int i) {
  float w = g_sbW - 20, pw = (w - 4) / 3;
  return rc(10 + i * (pw + 2), MENU_H + 8, pw, 26);
}
static std::string toUtf8(const wstring& w);
static wstring fromUtf8(const std::string& s);
#include "svg_path.inc"
#include "icons.inc"
static void drawFolderIcon(float x, float y, DWORD col, bool open) {
  g_br->SetColor(Rgb(col, 0.95f));
  g_rt->FillRoundedRectangle(D2D1::RoundedRect(rc(x, y + 3, 6, 4), 1.2f, 1.2f), g_br);
  g_rt->FillRoundedRectangle(D2D1::RoundedRect(rc(x, y + 5, 14, 9), 1.8f, 1.8f), g_br);
  (void)open;
}
static void drawFileIcon(float x, float y, DWORD col) {
  g_br->SetColor(Rgb(col, 0.9f));
  g_rt->DrawRoundedRectangle(D2D1::RoundedRect(rc(x + 1.5f, y + 1.5f, 10, 13), 1.5f, 1.5f), g_br, 1.2f);
  g_rt->DrawLine(D2D1::Point2F(x + 7.5f, y + 1.5f), D2D1::Point2F(x + 11.5f, y + 5.5f), g_br, 1.0f);
}
static int g_sbHover = -1;
static bool g_dragSb = false;
static float sbContentH() {
  vector<SbRow> rows;
  for (auto* rn : g_roots) flatten(rn, 0, rows);
  return rows.size() * SB_ROW;
}
static void drawSidebar() {
  if (!g_sbVisible) return;
  const Theme& t = T();
  float top = MENU_H, h = ch() - MENU_H - STATUS_H, w = g_sbW;
  fillRect(0, top, w, h, stySidebar());
  fillRect(w - 1, top, 1, h, t.border, 0.6f);
  {  // view switch: Files | Notes | Outline pills
    const wchar_t* names[3] = {L"Files", L"Notes", L"Outline"};
    for (int i = 0; i < 3; i++) {
      D2D1_RECT_F pr = sbPill(i);
      bool on = g_sbView == i;
      if (on) fillR(pr, sbHi(), 1.f, 7);
      float tw = textWidth(names[i], g_uiSmall);
      drawText(names[i], (int)wcslen(names[i]), pr.left + (pr.right - pr.left - tw) / 2, pr.top + 4, on ? t.fg : mixc(t.sidebar, t.fg, 0.5f), g_uiSmall, tw + 6, 20);
    }
  }
  if (g_sbView == 1) { drawNotesView(top, w, h); return; }
  if (g_sbView == 2) {
    drawText(L"OUTLINE", 7, 12, top + 44, mixc(t.sidebar, t.fg, 0.45f), g_uiSmall, 150, 18);
    auto& syms = symbolsOf(D());
    if (syms.empty()) {
      drawText(L"No headings or symbols in this file.", 36, 14, top + 76, sbFg(), g_uiSmall, w - 24, 20);
      return;
    }
    int active = -1;
    for (size_t i = 0; i < syms.size(); i++) if (syms[i].line <= D().cur.l) active = (int)i;
    float viewH = h - SB_HEAD;
    g_olScroll = std::max(0.f, std::min(g_olScroll, std::max(0.f, syms.size() * 24.f - viewH)));
    g_rt->PushAxisAlignedClip(D2D1::RectF(0, top + SB_HEAD, w - 1, top + h), D2D1_ANTIALIAS_MODE_ALIASED);
    for (size_t i = 0; i < syms.size(); i++) {
      float y = top + SB_HEAD + i * 24.f - g_olScroll;
      if (y + 24 < top + SB_HEAD || y > top + h) continue;
      const Symbol& s = syms[i];
      bool hov = g_my >= y && g_my < y + 24 && g_mx < w;
      if ((int)i == active) fillR(rc(6, y, w - 12, 22), sbHi(), 1.f, 6);
      else if (hov) fillR(rc(6, y, w - 12, 22), sbHi(), 0.6f, 6);
      int lvl = s.level > 0 ? std::min(5, s.kind == L"heading" ? s.level - 1 : s.level) : 0;
      DWORD col = (lvl == 0 || (int)i == active) ? t.fg : sbFg();
      wchar_t num[16];
      int nn = swprintf_s(num, L"%d", s.line + 1);
      float nw = textWidth(wstring(num, nn), g_uiSmall);
      drawText(s.name.c_str(), (int)s.name.size(), 14 + lvl * 14, y + 2, col, lvl == 0 ? g_uiBold : g_ui, w - 30 - nw - lvl * 14, 20);
      drawText(num, nn, w - 18 - nw, y + 3, mixc(t.sidebar, t.fg, 0.4f), g_uiSmall, nw + 4, 18);
    }
    g_rt->PopAxisAlignedClip();
    return;
  }
  drawText(L"FOLDERS", 7, 12, top + 44, mixc(t.sidebar, t.fg, 0.45f), g_uiSmall, 150, 18);
  if (!g_root) {
    // the message wraps inside the sidebar; the button sits under it, its label centred
    IDWriteTextLayout* lay = nullptr;
    g_dw->CreateTextLayout(L"You have not yet opened any folders.", 36, g_ui, std::max(60.f, w - 28), 200, &lay);
    float msgH = 0;
    if (lay) {
      lay->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
      DWRITE_TEXT_METRICS tm{};
      lay->GetMetrics(&tm);
      msgH = tm.height;
      g_br->SetColor(Rgb(sbFg()));
      g_rt->DrawTextLayout(D2D1::Point2F(14, top + 76), lay, g_br);
      lay->Release();
    }
    D2D1_RECT_F br = openFolderBtnRect(top + 76 + msgH + 14);
    bool hvb = inRect(br, g_mx, g_my);
    fillR(br, t.fg, hvb ? 0.08f : 0.0f, 6);
    strokeR(br, mixc(t.sidebar, t.fg, 0.28f), 1.f, 6);
    const wchar_t* ob = L"Open Folder...";
    float obw = textWidth(ob, g_ui);
    drawText(ob, (int)wcslen(ob), br.left + (br.right - br.left - obw) / 2, br.top + (br.bottom - br.top - 18) / 2, t.fg, g_ui, obw + 6, 20);
    return;
  }
  vector<SbRow> rows;
  for (auto* rn : g_roots) flatten(rn, 0, rows);
  g_rt->PushAxisAlignedClip(D2D1::RectF(0, top + SB_HEAD, w - 1, top + h), D2D1_ANTIALIAS_MODE_ALIASED);
  const wstring& active = D().path;
  for (size_t i = 0; i < rows.size(); i++) {
    float y = top + SB_HEAD + i * SB_ROW - g_sbScroll;
    if (y + SB_ROW < top + SB_HEAD || y > top + h) continue;
    FNode* n = rows[i].n;
    bool isActive = !n->dir && !active.empty() && _wcsicmp(active.c_str(), n->path.c_str()) == 0;
    if (isActive) fillR(rc(0, y, w - 1, SB_ROW), sbHi(), 1.f);
    else if ((int)i == g_sbHover) fillR(rc(0, y, w - 1, SB_ROW), sbHi(), 0.6f);
    float x = 10 + rows[i].depth * 14;
    if (n->dir) {
      g_br->SetColor(Rgb(mixc(t.sidebar, t.fg, 0.45f)));
      float cx = x + 5, cy = y + SB_ROW / 2;
      if (n->open) { g_rt->DrawLine(D2D1::Point2F(cx - 3.5f, cy - 1.5f), D2D1::Point2F(cx, cy + 2.5f), g_br, 1.3f); g_rt->DrawLine(D2D1::Point2F(cx + 3.5f, cy - 1.5f), D2D1::Point2F(cx, cy + 2.5f), g_br, 1.3f); }
      else { g_rt->DrawLine(D2D1::Point2F(cx - 1.5f, cy - 3.5f), D2D1::Point2F(cx + 2.5f, cy), g_br, 1.3f); g_rt->DrawLine(D2D1::Point2F(cx - 1.5f, cy + 3.5f), D2D1::Point2F(cx + 2.5f, cy), g_br, 1.3f); }
      drawNameIcon(n->name, true, n->open, x + 14, y + 2);
    } else {
      drawNameIcon(n->name, false, false, x + 14, y + 2);
    }
    DWORD col = sbFg();
    float tx = x + 34;
    if (isActive) col = t.fg;
    // Git: changed files are coloured, folders that hold changes get a dot
    char gcode = 0;
    bool gdir = false;
    if (!g_gitStatusMap.empty() || !g_gitDirSet.empty()) {
      wstring lp = lowerPath(n->path);
      auto git = g_gitStatusMap.find(lp);
      if (git != g_gitStatusMap.end()) gcode = git->second;
      else if (n->dir && g_gitDirSet.count(lp)) gdir = true;
    }
    if (gcode) col = gcode == 'M' ? 0xe2c08d : (gcode == 'A' || gcode == '?') ? 0x73c991 : gcode == 'D' ? 0xe0596a : 0x6fb7e8;
    drawText(n->name.c_str(), (int)n->name.size(), tx, y + 2, col, g_ui, w - tx - 8, 20);
    if (gcode == 'D') fillRect(tx, y + SB_ROW / 2, textWidth(n->name, g_ui), 1, col, 0.9f);
    if (gdir) { g_br->SetColor(Rgb(0xe2c08d, 0.7f)); g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(w - 16, y + SB_ROW / 2), 3.f, 3.f), g_br); }
  }
  g_rt->PopAxisAlignedClip();
  // scrollbar thumb
  float ch2 = rows.size() * SB_ROW, vh = h - SB_HEAD;
  if (ch2 > vh) {
    float th = std::max(30.f, vh * vh / ch2), ty = top + SB_HEAD + (vh - th) * (g_sbScroll / (ch2 - vh));
    fillR(rc(w - 7, ty, 4, th), t.gutterFg, 0.35f, 2);
  }
}
static bool sbExtraMouse(float x, float y);  // header switch and the notes list (defined with the notes code)
static bool sidebarMouse(float x, float y, int button) {
  if (!g_sbVisible || x >= g_sbW + 3 || y < MENU_H || y >= ch() - STATUS_H) return false;
  if (x >= g_sbW - 3) { g_dragSb = true; SetCapture(g_hwnd); return true; }
  if (sbExtraMouse(x, y)) return true;
  if (!g_root) {
    float bw = 150, bx = (g_sbW - bw) / 2, by = MENU_H + 70;
    if (x >= bx && x < bx + bw && y >= by && y < by + 32) exec(C_OPENFOLDER);
    return true;
  }
  if (y < MENU_H + SB_HEAD) return true;
  vector<SbRow> rows;
  for (auto* rn : g_roots) flatten(rn, 0, rows);
  int i = (int)std::floor((y - MENU_H - SB_HEAD + g_sbScroll) / SB_ROW);
  if (i < 0 || i >= (int)rows.size()) return true;
  FNode* n = rows[i].n;
  if (n->dir) { n->open = !n->open; if (n->open) loadKids(n); }
  else openPath(n->path);
  (void)button;
  redraw();
  return true;
}

/* ---------------------------------------------------------------- command palette / goto anything */
struct PItem { wstring label, right; int cmd = 0; int line = -1; wstring path; std::function<void()> run; };
static bool g_palOpen = false;
static std::function<vector<PItem>(const wstring&)> g_pickProvider;  // set while the overlay shows a custom list (pickers)
static wstring g_pickPlaceholder;
static std::function<void()> g_pickCancel;
static std::function<void(const PItem&)> g_pickHighlight;
static std::function<void(const wstring&)> g_promptCb;  // set while the overlay is used as a small text prompt
static wstring g_promptTitle;
static LineEdit g_pq;
static vector<PItem> g_pitems;
static int g_psel = 0, g_pscroll = 0;

static int fuzzy(const wstring& hay, const wstring& q) {
  if (q.empty()) return 0;
  size_t qi = 0;
  int score = 0, prev = -2;
  for (size_t i = 0; i < hay.size() && qi < q.size(); i++) {
    if (towlower(hay[i]) == towlower(q[qi])) {
      score += 10;
      if ((int)i == prev + 1) score += 15;
      if (i == 0 || wcschr(L"\\/ _-.", hay[i - 1])) score += 20;
      prev = (int)i;
      qi++;
    }
  }
  if (qi < q.size()) return -1;
  return score - (int)hay.size() / 6;
}
static void buildPalette() {
  g_pitems.clear();
  if (g_promptCb) return;
  const wstring& q = g_pq.t;
  if (g_pickProvider) {
    g_pitems = g_pickProvider(q);
    if (g_pitems.size() > 400) g_pitems.resize(400);
  } else if (!q.empty() && q[0] == L'@') {
    // symbols of the active file (functions, classes, headings)
    wstring qq = q.substr(1);
    while (!qq.empty() && qq[0] == L' ') qq.erase(0, 1);
    vector<std::pair<int, PItem>> sc;
    for (auto& s : symbolsOf(D())) {
      int sco = fuzzy(s.name, qq);
      if (sco < 0) continue;
      PItem it;
      it.label = s.name;
      it.right = s.kind + L"  " + std::to_wstring(s.line + 1);
      it.line = s.line;
      sc.push_back({qq.empty() ? -s.line : sco, it});
    }
    std::stable_sort(sc.begin(), sc.end(), [](auto& a, auto& b) { return a.first > b.first; });
    for (auto& p : sc) g_pitems.push_back(p.second);
  } else if (!q.empty() && q[0] == L':') {
    int n = _wtoi(q.c_str() + 1);
    PItem it;
    int total = (int)D().lines.size();
    n = std::max(1, std::min(total, n > 0 ? n : D().cur.l + 1));
    it.label = L"Go to line " + std::to_wstring(n) + L" of " + std::to_wstring(total);
    it.line = n - 1;
    g_pitems.push_back(it);
  } else if (!q.empty() && q[0] == L'>') {
    wstring qq = q.substr(1);
    while (!qq.empty() && qq[0] == L' ') qq.erase(0, 1);
    vector<std::pair<int, PItem>> sc;
    for (int i = 0; i < N_CMDS; i++) {
      const CmdDef& c = g_cmdDefs[i];
      if ((c.flags & 2) || !cmdAvailable(c.cmd)) continue;
      int s = fuzzy(c.title, qq);
      if (s < 0) continue;
      PItem it;
      it.label = c.title;
      it.right = effectiveKeys(c.cmd);
      it.cmd = c.cmd;
      sc.push_back({s, it});
    }
    for (int i = 0; i < (int)(sizeof(THEMES) / sizeof(THEMES[0])); i++) {
      wstring title = wstring(L"UI: Color Scheme: ") + THEMES[i].name;
      int s = fuzzy(title, qq);
      if (s < 0) continue;
      PItem it;
      it.label = title;
      it.cmd = C_SCHEME_BASE + i;
      sc.push_back({s, it});
    }
    for (int i = 0; i < (int)(sizeof(SYNTAXES) / sizeof(SYNTAXES[0])); i++) {
      wstring title = wstring(L"Set Syntax: ") + SYNTAXES[i].name;
      int s = fuzzy(title, qq);
      if (s < 0) continue;
      PItem it;
      it.label = title;
      it.cmd = C_SYNTAX_BASE + i;
      sc.push_back({s, it});
    }
    if (qq.empty()) { /* keep the registry order, like the Electron palette */ }
    else std::stable_sort(sc.begin(), sc.end(), [](auto& a, auto& b) { return a.first > b.first; });
    for (auto& p : sc) g_pitems.push_back(p.second);
  } else {
    vector<std::pair<int, PItem>> sc;
    // open files first
    for (auto* d : g_docs) {
      int s = fuzzy(d->name, q);
      if (s < 0) continue;
      PItem it;
      it.label = d->name;
      it.right = d->path.empty() ? L"unsaved" : L"open";
      it.path = d->path;
      it.cmd = -1;
      sc.push_back({s + 50, it});
    }
    if (g_root) {
      if (!g_projScanned) scanProject();
      for (size_t pi = 0; pi < g_projFiles.size(); pi++) {
        const wstring& rel = g_projFiles[pi];
        size_t k = rel.find_last_of(L'\\');
        wstring base = k == wstring::npos ? rel : rel.substr(k + 1);
        int s = fuzzy(rel, q);
        if (s < 0) continue;
        int sb = fuzzy(base, q);
        if (sb >= 0) s += 30 + sb;
        PItem it;
        it.label = base;
        it.right = k == wstring::npos ? L"" : rel.substr(0, k);
        it.path = g_projAbs[pi];
        it.cmd = -1;
        sc.push_back({s, it});
      }
    }
    std::stable_sort(sc.begin(), sc.end(), [](auto& a, auto& b) { return a.first > b.first; });
    for (auto& p : sc) {
      bool dup = false;
      for (auto& e : g_pitems) if (!p.second.path.empty() && _wcsicmp(e.path.c_str(), p.second.path.c_str()) == 0) { dup = true; break; }
      if (!dup) g_pitems.push_back(p.second);
      if (g_pitems.size() >= 200) break;
    }
  }
  g_psel = 0;
  g_pscroll = 0;
}
static void openPalette(const wstring& prefix) {
  g_promptCb = nullptr;
  g_pickProvider = nullptr;
  g_pickCancel = nullptr;
  g_pickHighlight = nullptr;
  g_palOpen = true;
  g_pq.set(prefix);
  buildPalette();
  caretReset();
  redraw();
}
// a list of choices in the palette overlay: `provider` filters the entries for the typed text; each entry has its own `run`
static void openPick(const wstring& placeholder, std::function<vector<PItem>(const wstring&)> provider, std::function<void()> onCancel = nullptr,
                     std::function<void(const PItem&)> onHighlight = nullptr) {
  g_promptCb = nullptr;
  g_pickProvider = provider;
  g_pickPlaceholder = placeholder;
  g_pickCancel = onCancel;
  g_pickHighlight = onHighlight;
  g_palOpen = true;
  g_pq.set(L"");
  buildPalette();
  caretReset();
  redraw();
}
// filters a fixed list with the fuzzy matcher
static std::function<vector<PItem>(const wstring&)> listProvider(vector<PItem> all) {
  return [all](const wstring& q) {
    vector<std::pair<int, PItem>> sc;
    for (auto& it : all) {
      int s = fuzzy(it.label, q);
      if (s < 0) continue;
      sc.push_back({s, it});
    }
    if (!q.empty()) std::stable_sort(sc.begin(), sc.end(), [](auto& a, auto& b) { return a.first > b.first; });
    vector<PItem> out;
    for (auto& p : sc) out.push_back(p.second);
    return out;
  };
}
static void closePalette() {
  bool wasPick = g_pickProvider != nullptr;
  auto cancel = g_pickCancel;
  g_palOpen = false;
  g_promptCb = nullptr;
  g_pickProvider = nullptr;
  g_pickCancel = nullptr;
  g_pickHighlight = nullptr;
  (void)wasPick;
  redraw();
  if (cancel) cancel();
}
static void palAccept() {
  if (g_promptCb) {
    auto cb = g_promptCb;
    wstring v = g_pq.t;
    g_pickCancel = nullptr;
    closePalette();
    cb(v);
    return;
  }
  if (g_pitems.empty()) { closePalette(); return; }
  PItem it = g_pitems[g_psel];
  g_pickCancel = nullptr;
  closePalette();
  if (it.run) { it.run(); return; }
  if (it.line >= 0) {
    Doc& d = D();
    d.cur = d.anc = {it.line, 0};
    d.wantV = 0;
    revealCenter();
    moved(false);
  } else if (it.cmd > 0) exec(it.cmd);
  else if (!it.path.empty()) openPath(it.path);
}
static const int PAL_W = 640, PAL_ROW = 32, PAL_MAX = 10;
static void drawPalette() {
  if (!g_palOpen) return;
  const Theme& t = T();
  fillRect(0, 0, cw(), ch(), 0x000000, 0.28f);
  float w = std::min((float)PAL_W, cw() - 40), x = (cw() - w) / 2, y = MENU_H + 18;
  int rows = std::min((int)g_pitems.size(), PAL_MAX);
  float h = 52 + (rows ? rows * PAL_ROW + 8 : (g_promptCb ? 0 : 40));
  fillR(D2D1::RectF(x + 3, y + 5, x + w + 3, y + h + 5), 0x000000, 0.3f, 12);
  fillR(rc(x, y, w, h), panelBg(), 1.f, 12);
  strokeR(rc(x, y, w, h), t.border, 0.9f, 12);
  const wchar_t* ph = g_promptCb ? g_promptTitle.c_str() : g_pickProvider ? g_pickPlaceholder.c_str() : L"Search files, or type > for commands, : for a line and @ for a symbol";
  drawInput(g_pq, rc(x + 10, y + 10, w - 20, 32), true, ph);
  if (g_pitems.empty() && !g_promptCb) {
    drawText(L"No results", 10, x + 20, y + 58, t.gutterFg, g_ui, w, 20);
  }
  for (int i = 0; i < rows; i++) {
    int idx = g_pscroll + i;
    if (idx >= (int)g_pitems.size()) break;
    float ry = y + 50 + i * PAL_ROW;
    if (idx == g_psel) fillR(rc(x + 6, ry, w - 12, PAL_ROW - 2), t.menuHi, 1.f, 7);
    const PItem& it = g_pitems[idx];
    float rw = it.right.empty() ? 0 : std::min(w * 0.45f, textWidth(it.right, g_uiSmall));
    bool fileRow = !it.path.empty() && it.cmd == -1;
    if (fileRow) drawNameIcon(it.label, false, false, x + 16, ry + 3, 18.f);
    float lx = fileRow ? 42.f : 18.f;
    drawText(it.label.c_str(), (int)it.label.size(), x + lx, ry + 5, t.chromeFg, g_ui, w - lx - 22 - rw, 22);
    if (!it.right.empty()) {
      // paths are shown from their end, shortcuts as they are
      drawText(it.right.c_str(), (int)it.right.size(), x + w - 18 - rw, ry + 7, t.gutterFg, g_uiSmall, rw + 4, 20);
    }
  }
}
static void palKey(WPARAM wp, bool ctrl, bool shift) {
  if (wp == VK_ESCAPE) { closePalette(); return; }
  if (wp == VK_RETURN) { palAccept(); return; }
  int n = (int)g_pitems.size();
  if (wp == VK_DOWN || wp == VK_UP) {
    if (n) g_psel = (g_psel + (wp == VK_DOWN ? 1 : n - 1)) % n;
    if (g_psel < g_pscroll) g_pscroll = g_psel;
    if (g_psel >= g_pscroll + PAL_MAX) g_pscroll = g_psel - PAL_MAX + 1;
    if (n && g_pickHighlight) g_pickHighlight(g_pitems[g_psel]);
    redraw();
    return;
  }
  if (wp == VK_NEXT || wp == VK_PRIOR) {
    if (n) g_psel = std::max(0, std::min(n - 1, g_psel + (wp == VK_NEXT ? PAL_MAX : -PAL_MAX)));
    g_pscroll = std::max(0, std::min(g_psel, std::max(0, n - PAL_MAX)));
    redraw();
    return;
  }
  if (ctrl && (wp == 'P' || wp == 'G')) { closePalette(); return; }
  if (ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V') return;
  if (editKey(g_pq, wp, ctrl, shift)) buildPalette();
  caretReset();
  redraw();
}
static void palChar(wchar_t c) {
  g_pq.insert(wstring(1, c));
  buildPalette();
  caretReset();
  redraw();
}
static bool palMouse(float x, float y) {
  if (!g_palOpen) return false;
  float w = std::min((float)PAL_W, cw() - 40), px = (cw() - w) / 2, py = MENU_H + 18;
  int rows = std::min((int)g_pitems.size(), PAL_MAX);
  float h = 52 + (rows ? rows * PAL_ROW + 8 : 40);
  if (x < px || x >= px + w || y < py || y >= py + h) { closePalette(); return true; }
  int i = (int)std::floor((y - py - 50) / PAL_ROW);
  if (i >= 0 && i < rows) { g_psel = g_pscroll + i; palAccept(); }
  return true;
}

/* ---------------------------------------------------------------- session (tabs, folder, view settings) */
static wstring dataDir() {
  wchar_t ov[MAX_PATH];
  if (GetEnvironmentVariableW(L"SCRIBA_DATA", ov, MAX_PATH)) { CreateDirectoryW(ov, nullptr); return ov; }
  PWSTR p = nullptr;
  wstring r;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) { r = p; CoTaskMemFree(p); }
  r += L"\\scriba-cpp";
  CreateDirectoryW(r.c_str(), nullptr);
  return r;
}
static bool writeDocTo(Doc& d, const wstring& path) {
  wstring w;
  for (size_t i = 0; i < d.lines.size(); i++) {
    w += d.lines[i];
    if (i + 1 < d.lines.size()) w += d.crlf ? L"\r\n" : L"\n";
  }
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  vector<char> out;
  if (d.bom) out.insert(out.end(), {'\xEF', '\xBB', '\xBF'});
  size_t at = out.size();
  out.resize(at + n);
  if (n) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data() + at, n, nullptr, nullptr);
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD wr = 0;
  bool ok = WriteFile(h, out.data(), (DWORD)out.size(), &wr, nullptr) && wr == out.size();
  CloseHandle(h);
  return ok;
}
static void saveSession() {
  wstring dir = dataDir(), hot = dir + (g_instance ? L"\\hot-" + std::to_wstring(g_instance) : wstring(L"\\hot"));
  CreateDirectoryW(hot.c_str(), nullptr);
  wstring out = L"v1\n";
  out += L"theme " + std::to_wstring(g_theme) + L"\n";
  if ((int)g_fontSize != sInt("font_size")) out += L"font " + std::to_wstring((int)g_fontSize) + L"\n";
  out += L"sidebar " + std::to_wstring(g_sbVisible ? 1 : 0) + L" " + std::to_wstring((int)g_sbW) + L"\n";
  out += L"minimap " + std::to_wstring(g_mapVisible ? 1 : 0) + L"\n";
  out += L"notes " + std::to_wstring(g_noteMode) + L" " + std::to_wstring(g_noteSort) + L"\n";
  for (auto* rn : g_roots) out += L"root " + rn->path + L"\n";
  out += L"active " + std::to_wstring(g_active) + L"\n";
  out += L"layout " + std::to_wstring(g_layout) + L" " + std::to_wstring((int)(g_splitRatio * 1000)) + L" " + std::to_wstring(g_curGroup) + L" " + std::to_wstring(g_groupActive[0]) + L" " + std::to_wstring(g_groupActive[1]) + L"\n";
  for (size_t i = 0; i < g_docs.size(); i++) {
    Doc& d = *g_docs[i];
    wstring hotfile;
    bool keep = !d.isNote && (d.dirty || (d.path.empty() && !(d.lines.size() == 1 && d.lines[0].empty())));
    if (keep) {
      hotfile = hot + L"\\" + std::to_wstring(i) + L".txt";
      writeDocTo(d, hotfile);
    }
    out += L"tab\t" + d.path + L"\t" + std::to_wstring(d.cur.l) + L"\t" + std::to_wstring(d.cur.c) + L"\t" + std::to_wstring((int)d.scrollY) + L"\t" + hotfile + L"\t" + d.noteId + L"\t" + (d.vis ? L"1" : L"0") + L"\t" + std::to_wstring(d.group) + L"\n";
  }
  // remove stale hot files
  for (size_t i = g_docs.size(); i < g_docs.size() + 40; i++) DeleteFileW((hot + L"\\" + std::to_wstring(i) + L".txt").c_str());
  int n = WideCharToMultiByte(CP_UTF8, 0, out.c_str(), (int)out.size(), nullptr, 0, nullptr, nullptr);
  vector<char> b(n);
  WideCharToMultiByte(CP_UTF8, 0, out.c_str(), (int)out.size(), b.data(), n, nullptr, nullptr);
  wstring f = dir + (g_instance ? L"\\session-" + std::to_wstring(g_instance) + L".txt" : wstring(L"\\session.txt")), tmp = f + L".tmp";
  HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD wr;
  WriteFile(h, b.data(), (DWORD)b.size(), &wr, nullptr);
  CloseHandle(h);
  MoveFileExW(tmp.c_str(), f.c_str(), MOVEFILE_REPLACE_EXISTING);
}
static bool restoreSession() {
  wstring f = dataDir() + (g_instance ? L"\\session-" + std::to_wstring(g_instance) + L".txt" : wstring(L"\\session.txt"));
  HANDLE h = CreateFileW(f.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD sz = GetFileSize(h, nullptr), got = 0;
  vector<char> b(sz);
  ReadFile(h, b.data(), sz, &got, nullptr);
  CloseHandle(h);
  int n = MultiByteToWideChar(CP_UTF8, 0, b.data(), (int)got, nullptr, 0);
  wstring s(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, b.data(), (int)got, s.data(), n);
  vector<wstring> lines;
  size_t p = 0;
  while (p < s.size()) {
    size_t q = s.find(L'\n', p);
    if (q == wstring::npos) q = s.size();
    lines.push_back(s.substr(p, q - p));
    p = q + 1;
  }
  if (lines.empty() || lines[0] != L"v1") return false;
  vector<wstring> rootList;
  int active = 0;
  vector<Doc*> docs;
  for (size_t i = 1; i < lines.size(); i++) {
    const wstring& l = lines[i];
    if (l.rfind(L"theme ", 0) == 0) { /* the colour scheme is a setting now */ }
    else if (l.rfind(L"font ", 0) == 0) g_sessionFont = std::max(7, std::min(48, _wtoi(l.c_str() + 5)));
    else if (l.rfind(L"sidebar ", 0) == 0) { g_sessionUi = true; g_sbVisible = _wtoi(l.c_str() + 8) != 0; size_t sp = l.find(L' ', 8); if (sp != wstring::npos) g_sbW = (float)std::max(160, std::min(600, _wtoi(l.c_str() + sp + 1))); }
    else if (l.rfind(L"minimap ", 0) == 0) g_mapVisible = _wtoi(l.c_str() + 8) != 0;
    else if (l.rfind(L"notes ", 0) == 0) {
      int a = 0, b = 0;
      if (swscanf_s(l.c_str() + 6, L"%d %d", &a, &b) == 2) { g_noteMode = std::max(0, std::min(3, a)); g_noteSort = std::max(0, std::min(3, b)); }
    }
    else if (l.rfind(L"root ", 0) == 0) { if (l.size() > 5) rootList.push_back(l.substr(5)); }
    else if (l.rfind(L"active ", 0) == 0) active = _wtoi(l.c_str() + 7);
    else if (l.rfind(L"layout ", 0) == 0) {
      int v[5] = {0, 500, 0, 0, 0};
      swscanf_s(l.c_str() + 7, L"%d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4]);
      g_layout = std::max(0, std::min(2, v[0]));
      g_splitRatio = std::max(0.15f, std::min(0.85f, v[1] / 1000.f));
      g_curGroup = std::max(0, std::min(1, v[2]));
      g_groupActive[0] = v[3];
      g_groupActive[1] = v[4];
    }
    else if (l.rfind(L"tab\t", 0) == 0) {
      vector<wstring> f2;
      size_t a = 4;
      f2.push_back(L"");
      size_t pos = a;
      for (;;) {
        size_t t2 = l.find(L'\t', pos);
        if (t2 == wstring::npos) { f2.push_back(l.substr(pos)); break; }
        f2.push_back(l.substr(pos, t2 - pos));
        pos = t2 + 1;
      }
      // f2[1]=path, [2]=line, [3]=col, [4]=scroll, [5]=hot file
      if (f2.size() < 6) continue;
      Doc* d = new Doc();
      bool ok = true;
      if (f2.size() >= 8 && !f2[6].empty()) {
        // a note: the text lives in the notes folder
        wstring np = dataDir() + L"\\Notes\\" + f2[6];
        ok = GetFileAttributesW(np.c_str()) != INVALID_FILE_ATTRIBUTES && loadFile(np, *d);
        if (ok) {
          d->path.clear();
          d->noteId = f2[6];
          d->isNote = true;
          d->vis = true;  // notes always open in Visual mode (the old session flag is ignored)
          d->lang = L_MD;
          d->langName = L"Markdown";
          d->dirty = false;
          d->crlf = false;
          d->bom = false;
          d->invalidate(0);
        }
      } else if (!f2[5].empty()) {
        ok = loadFile(f2[5], *d);
        if (ok) {
          if (!f2[1].empty()) d->setPath(f2[1]); else { d->path.clear(); d->name = L"untitled"; d->lang = L_PLAIN; d->langName = L"Plain Text"; }
          d->dirty = true;
        }
      } else if (!f2[1].empty()) ok = loadFile(f2[1], *d);
      else ok = true;
      if (!ok) { delete d; continue; }
      d->cur.l = std::max(0, std::min((int)d->lines.size() - 1, _wtoi(f2[2].c_str())));
      d->cur.c = std::max(0, std::min((int)d->lines[d->cur.l].size(), _wtoi(f2[3].c_str())));
      d->anc = d->cur;
      d->scrollY = (float)_wtoi(f2[4].c_str());
      if (f2.size() >= 9) d->group = std::max(0, std::min(1, _wtoi(f2[8].c_str())));
      docs.push_back(d);
    }
  }
  if (docs.empty()) return false;
  for (auto* d : g_docs) delete d;
  g_docs = docs;
  g_active = std::max(0, std::min((int)g_docs.size() - 1, active));
  g_curGroup = g_docs[g_active]->group;
  normalizeGroups(g_active);
  {
    bool vis = g_sbVisible;
    for (auto& r : rootList) if (GetFileAttributesW(r.c_str()) != INVALID_FILE_ATTRIBUTES) addRoot(r);
    g_sbVisible = vis;
  }
  return true;
}

/* ---------------------------------------------------------------- notes */
struct NoteInfo {
  wstring id, title, preview, color;  // color: a name from NOTE_PALETTE ("red"), empty for none
  ULONGLONG mtime = 0;
  bool pinned = false;
  wstring snippet;
  vector<wstring> tags;
};
// the same colours as Electron's notes.js
struct NoteColor { const wchar_t* name; DWORD rgb; };
static const NoteColor NOTE_PALETTE[] = {{L"red", 0xef4444}, {L"orange", 0xf97316}, {L"yellow", 0xeab308}, {L"green", 0x22c55e},
                                         {L"teal", 0x14b8a6}, {L"blue", 0x3b82f6},   {L"purple", 0xa855f7}, {L"pink", 0xec4899}};
static DWORD noteColorRgb(const wstring& name, DWORD fallback) {
  for (auto& c : NOTE_PALETTE) if (name == c.name) return c.rgb;
  return fallback;
}
// notes saved by an early build kept a hex colour: map it to the nearest name
static wstring noteColorName(const wstring& c) {
  if (c.size() == 7 && c[0] == L'#') {
    static const std::pair<const wchar_t*, const wchar_t*> old[] = {{L"#ec5f66", L"red"}, {L"#f99157", L"orange"}, {L"#fac863", L"yellow"},
                                                                    {L"#99c794", L"green"}, {L"#6699cc", L"blue"}, {L"#c594c5", L"purple"}};
    for (auto& o : old) if (c == o.first) return o.second;
    return L"";
  }
  return c;
}
struct NoteMeta { wstring title, color; bool pinned = false; };
static std::map<wstring, NoteMeta> g_meta;
static bool g_metaLoaded = false;
static vector<NoteInfo> g_notes;
static LineEdit g_nq;
static bool g_nqFocus = false;
static float g_nbScroll = 0;
static int g_noteHover = -1;

static wstring notesDir() {
  wstring r = dataDir() + L"\\Notes";
  CreateDirectoryW(r.c_str(), nullptr);
  return r;
}
static wstring u8w(const char* p, int n) {
  if (n <= 0) return L"";
  int m = MultiByteToWideChar(CP_UTF8, 0, p, n, nullptr, 0);
  wstring w(m, 0);
  MultiByteToWideChar(CP_UTF8, 0, p, n, w.data(), m);
  return w;
}
static bool readAll(const wstring& path, vector<char>& out) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD sz = GetFileSize(h, nullptr), got = 0;
  out.resize(sz);
  if (sz) ReadFile(h, out.data(), sz, &got, nullptr);
  CloseHandle(h);
  out.resize(got);
  return true;
}
#include "settings.inc"
// minimal reader for the notes meta file: { "id": { "title": "...", "pinned": true, "color": "#rrggbb" }, ... }
static void parseMeta(const wstring& t) {
  size_t i = 0;
  auto ws = [&] { while (i < t.size() && iswspace(t[i])) i++; };
  auto str = [&](wstring& out) {
    out.clear();
    if (i >= t.size() || t[i] != L'"') return false;
    i++;
    while (i < t.size() && t[i] != L'"') {
      if (t[i] == L'\\' && i + 1 < t.size()) {
        i++;
        wchar_t c = t[i];
        if (c == L'n') out += L'\n';
        else if (c == L't') out += L'\t';
        else if (c == L'u' && i + 4 < t.size()) { out += (wchar_t)wcstol(t.substr(i + 1, 4).c_str(), nullptr, 16); i += 4; }
        else out += c;
      } else out += t[i];
      i++;
    }
    i++;
    return true;
  };
  ws();
  if (i >= t.size() || t[i] != L'{') return;
  i++;
  for (;;) {
    ws();
    if (i >= t.size() || t[i] == L'}') break;
    wstring id;
    if (!str(id)) break;
    ws();
    if (i >= t.size() || t[i] != L':') break;
    i++;
    ws();
    if (i >= t.size() || t[i] != L'{') break;
    i++;
    NoteMeta m;
    for (;;) {
      ws();
      if (i >= t.size() || t[i] == L'}') { i++; break; }
      wstring k;
      if (!str(k)) break;
      ws();
      if (i < t.size() && t[i] == L':') i++;
      ws();
      wstring v;
      bool b = false;
      if (i < t.size() && t[i] == L'"') str(v);
      else {
        size_t j = i;
        while (j < t.size() && t[j] != L',' && t[j] != L'}') j++;
        v = t.substr(i, j - i);
        b = v.find(L"true") != wstring::npos;
        i = j;
      }
      if (k == L"title") m.title = v;
      else if (k == L"color") m.color = noteColorName(v);
      else if (k == L"pinned") m.pinned = b;
      ws();
      if (i < t.size() && t[i] == L',') i++;
    }
    g_meta[id] = m;
    ws();
    if (i < t.size() && t[i] == L',') i++;
  }
}
static wstring jsonEsc(const wstring& s) {
  wstring o;
  for (wchar_t c : s) {
    if (c == L'"' || c == L'\\') { o += L'\\'; o += c; }
    else if (c == L'\n') o += L"\\n";
    else if (c == L'\t') o += L"\\t";
    else o += c;
  }
  return o;
}
static void loadMeta() {
  if (g_metaLoaded) return;
  g_metaLoaded = true;
  vector<char> b;
  if (readAll(notesDir() + L"\\.meta.json", b)) parseMeta(u8w(b.data(), (int)b.size()));
}
static void saveMeta() {
  wstring o = L"{\n";
  bool first = true;
  for (auto& kv : g_meta) {
    if (kv.second.title.empty() && kv.second.color.empty() && !kv.second.pinned) continue;
    if (!first) o += L",\n";
    first = false;
    o += L" \"" + jsonEsc(kv.first) + L"\": {";
    bool f2 = true;
    auto add = [&](const wstring& k, const wstring& v, bool raw) {
      if (!f2) o += L", ";
      f2 = false;
      o += L"\"" + k + L"\": " + (raw ? v : L"\"" + jsonEsc(v) + L"\"");
    };
    if (!kv.second.title.empty()) add(L"title", kv.second.title, false);
    if (kv.second.pinned) add(L"pinned", L"true", true);
    if (!kv.second.color.empty()) add(L"color", kv.second.color, false);
    o += L"}";
  }
  o += L"\n}\n";
  int n = WideCharToMultiByte(CP_UTF8, 0, o.c_str(), (int)o.size(), nullptr, 0, nullptr, nullptr);
  vector<char> b(n);
  WideCharToMultiByte(CP_UTF8, 0, o.c_str(), (int)o.size(), b.data(), n, nullptr, nullptr);
  wstring f = notesDir() + L"\\.meta.json", tmp = f + L".tmp";
  HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD wr;
  WriteFile(h, b.data(), (DWORD)b.size(), &wr, nullptr);
  CloseHandle(h);
  MoveFileExW(tmp.c_str(), f.c_str(), MOVEFILE_REPLACE_EXISTING);
}
// a line of a note without Markdown symbols (used for titles and previews)
static wstring plainTitle(wstring t) {
  auto trim = [&] {
    while (!t.empty() && iswspace(t.back())) t.pop_back();
    size_t k = 0;
    while (k < t.size() && iswspace(t[k])) k++;
    t.erase(0, k);
  };
  trim();
  for (bool again = true; again;) {
    again = false;
    size_t k = 0;
    while (k < t.size() && t[k] == L'#') k++;
    if (k > 0 && k <= 6 && k < t.size() && t[k] == L' ') { t.erase(0, k + 1); again = true; trim(); continue; }
    if (t.size() > 1 && (t[0] == L'-' || t[0] == L'*' || t[0] == L'+') && t[1] == L' ') {
      t.erase(0, 2);
      if (t.size() >= 3 && t[0] == L'[' && t[2] == L']') t.erase(0, 3);
      again = true;
      trim();
      continue;
    }
    if (!t.empty() && t[0] == L'>') { t.erase(0, 1); again = true; trim(); continue; }
    size_t dg = 0;
    while (dg < t.size() && iswdigit(t[dg])) dg++;
    if (dg > 0 && dg + 1 < t.size() && (t[dg] == L'.' || t[dg] == L')') && t[dg + 1] == L' ') { t.erase(0, dg + 2); again = true; trim(); }
  }
  wstring o;
  for (size_t i = 0; i < t.size(); i++) {
    wchar_t c = t[i];
    if (c == L'!' && i + 1 < t.size() && t[i + 1] == L'[') continue;
    if (c == L'[') continue;
    if (c == L']' && i + 1 < t.size() && t[i + 1] == L'(') {
      size_t j = t.find(L')', i);
      if (j != wstring::npos) { i = j; continue; }
    }
    if (c == L']') continue;
    if (c == L'*' || c == L'`' || c == L'~') continue;
    if (c == L'_' && (i == 0 || i + 1 == t.size() || t[i + 1] == L'_' || t[i - 1] == L'_')) continue;
    o += c;
  }
  t = o;
  trim();
  return t;
}
static void noteTitles(const wstring& text, wstring& title, wstring& preview) {
  title.clear();
  preview.clear();
  size_t p = 0;
  int found = 0;
  while (p <= text.size() && found < 2) {
    size_t q = text.find(L'\n', p);
    if (q == wstring::npos) q = text.size();
    wstring line = plainTitle(text.substr(p, q - p));
    if (!line.empty()) {
      if (found == 0) title = line.substr(0, 80);
      else preview = line.substr(0, 100);
      found++;
    }
    p = q + 1;
  }
}
static wstring lowerW(wstring s) {
  for (auto& c : s) c = (wchar_t)towlower(c);
  return s;
}
// #tags of a note: after a space or "(", two or more letters, digits, _ or -; lower case, at most 8
static vector<wstring> tagsOf(const wstring& text) {
  vector<wstring> out;
  for (size_t i = 0; i < text.size() && out.size() < 8; i++) {
    if (text[i] != L'#' || (i > 0 && !iswspace(text[i - 1]) && text[i - 1] != L'(')) continue;
    size_t j = i + 1;
    while (j < text.size() && (iswalnum(text[j]) || text[j] == L'_' || text[j] == L'-')) j++;
    if (j - i - 1 >= 2) {
      wstring t = lowerW(text.substr(i + 1, j - i - 1));
      if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
    }
    i = j - 1;
  }
  return out;
}
static void refreshNotes() {
  loadMeta();
  g_notes.clear();
  wstring dir = notesDir();
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((dir + L"\\*.md").c_str(), &fd);
  wstring q = lowerW(g_nq.t);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      NoteInfo n;
      n.id = fd.cFileName;
      n.mtime = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
      vector<char> b;
      if (!readAll(dir + L"\\" + n.id, b)) continue;
      wstring text = u8w(b.data(), (int)std::min<size_t>(b.size(), 200000));
      bool blank = true;
      for (wchar_t c : text) if (!iswspace(c)) { blank = false; break; }
      if (blank) {
        // an empty note nobody typed into is tidied away once it is clearly abandoned
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        ULONGLONG nt = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
        bool open = false;
        for (auto* dd : g_docs) if (dd->noteId == n.id) open = true;
        if (!open && nt - n.mtime > 300000000ULL) DeleteFileW((dir + L"\\" + n.id).c_str());
        continue;
      }
      noteTitles(text, n.title, n.preview);
      auto m = g_meta.find(n.id);
      if (m != g_meta.end()) {
        if (!m->second.title.empty()) n.title = m->second.title;
        n.pinned = m->second.pinned;
        n.color = m->second.color;
      }
      if (n.title.empty()) n.title = L"Untitled";
      n.tags = tagsOf(text);
      if (!q.empty()) {
        wstring hay = lowerW(text + L"\n" + n.title);
        size_t at = hay.find(q);
        if (at == wstring::npos) continue;
        if (at < text.size()) {
          wstring sn = text.substr(at > 30 ? at - 30 : 0, 100);
          for (auto& c : sn) if (c == L'\n' || c == L'\r') c = L' ';
          n.snippet = sn;
        }
      }
      g_notes.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  std::sort(g_notes.begin(), g_notes.end(), [](const NoteInfo& a, const NoteInfo& b) {
    if (a.pinned != b.pinned) return a.pinned;
    if (g_noteSort == 1) return a.mtime < b.mtime;
    if (g_noteSort == 2) return _wcsicmp(a.title.c_str(), b.title.c_str()) < 0;
    if (g_noteSort == 3) return a.id > b.id;  // ids start with the creation time
    return a.mtime > b.mtime;
  });
}
static wstring noteStamp() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  static int ctr = 0;
  wchar_t b[64];
  swprintf_s(b, L"%04d%02d%02d-%02d%02d%02d-%03x%x.md", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, ctr++ & 15);
  return b;
}
static void refreshNoteTitle(Doc& d) {
  if (!d.isNote) return;
  wstring title, prev, text;
  size_t lim = std::min<size_t>(d.lines.size(), 12);
  for (size_t i = 0; i < lim; i++) { text += d.lines[i]; text += L'\n'; }
  noteTitles(text, title, prev);
  auto m = g_meta.find(d.noteId);
  if (m != g_meta.end() && !m->second.title.empty()) title = m->second.title;
  d.name = title.empty() ? L"New note" : title;
}
static void flushNote(Doc& d) {
  if (!d.isNote || !d.dirty) return;
  wstring f = notesDir() + L"\\" + d.noteId, tmp = f + L".tmp";
  if (writeDocTo(d, tmp) && MoveFileExW(tmp.c_str(), f.c_str(), MOVEFILE_REPLACE_EXISTING)) d.dirty = false;
}
static void flushNotes() {
  bool any = false;
  for (auto* dd : g_docs) if (dd->isNote && dd->dirty) { flushNote(*dd); any = true; }
  if (any && g_sbVisible && g_sbView == 1) { refreshNotes(); redraw(); }
}
static Doc* openNoteDoc(const wstring& id) {
  loadMeta();
  for (size_t i = 0; i < g_docs.size(); i++)
    if (g_docs[i]->noteId == id) { activateDoc((int)i); return g_docs[i]; }
  Doc* nd = new Doc();
  if (!loadFile(notesDir() + L"\\" + id, *nd)) nd->lines = {L""};
  nd->path.clear();
  nd->noteId = id;
  nd->isNote = true;
  nd->vis = true;
  nd->lang = L_MD;
  nd->langName = L"Markdown";
  nd->dirty = false;
  nd->crlf = false;
  nd->bom = false;
  nd->invalidate(0);
  refreshNoteTitle(*nd);
  if (g_docs.size() == 1 && g_docs[0]->path.empty() && !g_docs[0]->isNote && !g_docs[0]->dirty && g_docs[0]->lines.size() == 1 && g_docs[0]->lines[0].empty()) {
    delete g_docs[0];
    g_docs.clear();
  }
  addDoc(nd);
  return nd;
}
static void newNote() {
  wstring id = noteStamp();
  HANDLE h = CreateFileW((notesDir() + L"\\" + id).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  openNoteDoc(id);
  if (g_sbVisible && g_sbView == 1) refreshNotes();
  redraw();
}
static void importElectronNotes() {
  PWSTR p = nullptr;
  wstring src;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) { src = p; CoTaskMemFree(p); }
  src += L"\\sublime-clone\\Notes";
  if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) { appMsgBox(g_hwnd, L"No notes folder found for the Electron edition.", L"Scriba", MB_OK); return; }
  wstring dst = notesDir();
  int copied = 0;
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((src + L"\\*.md").c_str(), &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      if (CopyFileW((src + L"\\" + fd.cFileName).c_str(), (dst + L"\\" + fd.cFileName).c_str(), TRUE)) copied++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  vector<char> b;
  if (readAll(src + L"\\.meta.json", b)) {
    loadMeta();
    auto keep = g_meta;
    g_meta.clear();
    parseMeta(u8w(b.data(), (int)b.size()));
    for (auto& kv : keep) g_meta[kv.first] = kv.second;
    saveMeta();
  }
  wchar_t msg[96];
  swprintf_s(msg, L"Imported %d notes.", copied);
  appMsgBox(g_hwnd, msg, L"Scriba", MB_OK);
  g_sbVisible = true;
  g_sbView = 1;
  refreshNotes();
  redraw();
}
// the colour commands of the Notes menu: none and the first six names
static const wchar_t* noteColorByIndex(int i) {
  static const wchar_t* n[] = {L"", L"red", L"orange", L"yellow", L"green", L"blue", L"purple"};
  return n[std::max(0, std::min(6, i))];
}
static void openPrompt(const wstring& title, const wstring& initial, std::function<void(const wstring&)> cb) {
  g_promptCb = cb;
  g_promptTitle = title;
  g_pickProvider = nullptr;
  g_pickCancel = nullptr;
  g_pickHighlight = nullptr;
  g_palOpen = true;
  g_pq.set(initial);
  g_pitems.clear();
  caretReset();
  redraw();
}
#include "popup.inc"
#include "notes_ui.inc"

static bool sbExtraMouse(float x, float y) {
  float top = MENU_H;
  g_nqFocus = false;
  for (int i = 0; i < 3; i++) {
    D2D1_RECT_F pr = sbPill(i);
    if (inRect(pr, x, y)) {
      g_sbView = i;
      if (i == 1) refreshNotes();
      redraw();
      return true;
    }
  }
  if (g_sbView == 2) {
    if (y >= top + SB_HEAD) {
      auto& syms = symbolsOf(D());
      int i = (int)std::floor((y - top - SB_HEAD + g_olScroll) / 24.f);
      if (i >= 0 && i < (int)syms.size()) gotoLine(syms[i].line);
    }
    return true;
  }
  if (g_sbView != 1) {
    if (!g_root && inRect(openFolderBtnRect(g_openBtnY), x, y)) { exec(C_OPENFOLDER); return true; }
    return y < top + SB_HEAD ? true : false;
  }
  return notesMouse(x, y);
}

/* ---------------------------------------------------------------- Markdown formatting */
static void fmtWrap(const wchar_t* pre, const wchar_t* post) {
  Doc& d = D();
  wstring a = pre, b = post;
  struct Seg { int l, c0, c1; };
  vector<Seg> segs;
  Pos sa = d.selA(), sb = d.selB();
  if (d.hasSel()) {
    for (int l = sa.l; l <= sb.l; l++) {
      const wstring& ln = d.lines[l];
      int c0 = l == sa.l ? sa.c : 0, c1 = l == sb.l ? sb.c : (int)ln.size();
      int pl = mdPrefixLen(ln);
      if (c0 < pl) c0 = std::min(pl, c1);
      while (c0 < c1 && wcschr(L"*_~`", ln[c0])) c0++;  // markers already around the text stay outside the new ones
      while (c1 > c0 && wcschr(L"*_~`", ln[c1 - 1])) c1--;
      if (c1 <= c0 || trimW(ln.substr(c0, c1 - c0)).empty()) continue;  // an empty line gets nothing
      d.ensureStates(l);
      if (d.st[l] == 3 || trimW(ln).compare(0, 3, L"```") == 0) continue;  // code and its fences stay as they are
      segs.push_back({l, c0, c1});
    }
  } else {
    // no selection: the word at the caret, or an empty pair to type into
    const wstring& ln = d.lines[sa.l];
    int c0 = sa.c, c1 = sa.c;
    while (c0 > 0 && isWordCh(ln[c0 - 1])) c0--;
    while (c1 < (int)ln.size() && isWordCh(ln[c1])) c1++;
    if (c1 > c0) segs.push_back({sa.l, c0, c1});
  }
  d.snapshot(0);
  if (segs.empty()) {
    d.replaceRange(sa, sa, a + b);
    d.cur = d.anc = {sa.l, sa.c + (int)a.size()};
    edited();
    return;
  }
  // is the segment (with the markers next to it, if the selection stopped in front of them) one wrapped span?
  struct Span2 { int s0, s1; bool wrapped; };
  auto analyse = [&](const Seg& g) {
    const wstring& ln = d.lines[g.l];
    int n = (int)ln.size(), as = (int)a.size(), bs = (int)b.size();
    auto isRun = [&](int from, const wstring& m, bool before) {  // a "*" that is half of "**" is not an italic marker
      if (m.size() != 1) return false;
      int at = before ? from - 1 : from + 1;
      return at >= 0 && at < n && ln[at] == m[0];
    };
    int s0 = g.c0, s1 = g.c1;
    if (s0 >= as && ln.compare(s0 - as, as, a) == 0 && !isRun(s0 - as, a, true)) s0 -= as;
    if (s1 + bs <= n && ln.compare(s1, bs, b) == 0 && !isRun(s1 + bs - 1, b, false)) s1 += bs;
    bool w = s1 - s0 >= as + bs && ln.compare(s0, as, a) == 0 && ln.compare(s1 - bs, bs, b) == 0 && !isRun(s0, a, false) && !isRun(s1 - 1, b, true);
    // "**a**" is bold, not italic: when the marker is one character, a second one right next to it means a longer marker
    if (w && a.size() == 1 && ((s0 + 1 < n && ln[s0 + 1] == a[0]) || (s1 - 2 >= 0 && ln[s1 - 2] == b[0]))) w = false;
    return Span2{s0, s1, w};
  };
  vector<Span2> an;
  bool all = true;
  for (auto& g : segs) { an.push_back(analyse(g)); if (!an.back().wrapped) all = false; }
  int firstStart = 0, lastEnd = 0;
  for (size_t k = 0; k < segs.size(); k++) {
    Seg g = segs[k];
    int ns, ne;
    const wstring text = d.lines[g.l].substr(g.c0, g.c1 - g.c0);
    if (all) {
      wstring inner = d.lines[g.l].substr(an[k].s0 + a.size(), an[k].s1 - an[k].s0 - a.size() - b.size());
      d.replaceRange({g.l, an[k].s0}, {g.l, an[k].s1}, inner);
      ns = an[k].s0;
      ne = an[k].s0 + (int)inner.size();
    } else if (an[k].wrapped) {  // some lines have it already: leave those as they are
      ns = an[k].s0;
      ne = an[k].s1;
    } else {
      d.replaceRange({g.l, g.c0}, {g.l, g.c1}, a + text + b);
      ns = g.c0 + (int)a.size();
      ne = g.c1 + (int)a.size();
    }
    if (k == 0) firstStart = ns;
    lastEnd = ne;
  }
  d.anc = {segs.front().l, firstStart};
  d.cur = {segs.back().l, lastEnd};
  if (!d.hasSel() && d.anc == d.cur) d.cur = d.anc;
  edited();
}
static void fmtLinePrefix(const std::function<wstring(const wstring&, int)>& fn) {
  Doc& d = D();
  d.snapshot(0);
  Pos a = d.selA(), b = d.selB();
  int l1 = (b.c == 0 && b.l > a.l) ? b.l - 1 : b.l;
  int curDelta = 0, ancDelta = 0;
  for (int l = a.l; l <= l1; l++) {
    int before = (int)d.lines[l].size();
    d.lines[l] = fn(d.lines[l], l - a.l);
    int diff = (int)d.lines[l].size() - before;
    if (l == d.cur.l) curDelta = diff;
    if (l == d.anc.l) ancDelta = diff;
  }
  d.dirty = true;
  d.invalidate(a.l);
  // keep the caret on the same character when a prefix was added or removed in front of it
  d.cur = {d.cur.l, std::max(0, std::min(d.cur.c + curDelta, (int)d.lines[d.cur.l].size()))};
  d.anc = {d.anc.l, std::max(0, std::min(d.anc.c + ancDelta, (int)d.lines[d.anc.l].size()))};
  edited();
}
static wstring stripBlockPrefix(const wstring& s) {
  size_t i = 0;
  while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) i++;
  size_t k = i;
  while (k < s.size() && s[k] == L'#') k++;
  if (k > i && k < s.size() && s[k] == L' ') return s.substr(k + 1);
  if (i + 1 < s.size() && (s[i] == L'-' || s[i] == L'*' || s[i] == L'+') && s[i + 1] == L' ') {
    size_t j = i + 2;
    if (j + 2 < s.size() && s[j] == L'[' && s[j + 2] == L']') j += 3;
    if (j < s.size() && s[j] == L' ') j++;
    return s.substr(j);
  }
  if (i < s.size() && s[i] == L'>') return s.substr(i + (i + 1 < s.size() && s[i + 1] == L' ' ? 2 : 1));
  size_t dg = i;
  while (dg < s.size() && iswdigit(s[dg])) dg++;
  if (dg > i && dg + 1 < s.size() && s[dg] == L'.' && s[dg + 1] == L' ') return s.substr(dg + 2);
  return s;
}
static void fmtHeading(int level) {
  fmtLinePrefix([&](const wstring& s, int) {
    wstring body = stripBlockPrefix(s);
    return level > 0 ? wstring(level, L'#') + L" " + body : body;
  });
}
static void fmtToggle(const wchar_t* prefix, bool numbered = false) {
  Doc& d = D();
  bool all = true;
  Pos a = d.selA(), b = d.selB();
  for (int l = a.l; l <= b.l; l++) {
    const wstring& s = d.lines[l];
    bool has = numbered ? (!s.empty() && iswdigit(s[0])) : s.compare(0, wcslen(prefix), prefix) == 0;
    if (!has) all = false;
  }
  fmtLinePrefix([&](const wstring& s, int idx) {
    if (all) return stripBlockPrefix(s);
    wstring body = stripBlockPrefix(s);
    return numbered ? std::to_wstring(idx + 1) + L". " + body : wstring(prefix) + body;
  });
}
static void fmtInsert(const wstring& text, int caretBack = 0) {
  Doc& d = D();
  d.snapshot(0);
  d.replaceRange(d.selA(), d.selB(), text);
  if (caretBack) { d.cur.c -= caretBack; d.anc = d.cur; }
  edited();
}
static void fmtCodeBlock() {
  Doc& d = D();
  d.snapshot(0);
  Pos a = d.selA(), b = d.selB();
  if (!d.hasSel()) {
    d.replaceRange(a, a, L"```\n\n```");
    d.cur = d.anc = {a.l + 1, 0};
  } else {
    wstring sel = d.selectedText();
    d.replaceRange(a, b, L"```\n" + sel + L"\n```");
  }
  edited();
}
static void fmtLink() {
  Doc& d = D();
  d.snapshot(0);
  Pos a = d.selA(), b = d.selB();
  wstring text = d.hasSel() ? d.selectedText() : L"text";
  d.replaceRange(a, b, L"[" + text + L"](url)");
  d.anc = {a.l, a.c + (int)text.size() + 3};
  d.cur = {a.l, a.c + (int)text.size() + 6};
  edited();
}
// a picture goes on a line of its own: after text on the caret line it starts a new line, and what follows moves down
static void fmtInsertImage(const wstring& md) {
  Doc& d = D();
  Pos p = d.selA();
  bool after = p.c > 0 && !trimW(d.lines[p.l].substr(0, p.c)).empty();
  fmtInsert((after ? L"\n" : L"") + md + L"\n");
}
static wstring g_testImagePick;  // test driver: the file the image button "chooses"
static void fmtImage() {
  wchar_t buf[MAX_PATH * 2] = {};
  OPENFILENAMEW o{sizeof(o)};
  o.hwndOwner = g_hwnd;
  o.lpstrFile = buf;
  o.nMaxFile = MAX_PATH * 2;
  o.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.gif;*.webp;*.bmp;*.svg\0\0";
  o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (!g_testImagePick.empty()) wcsncpy_s(buf, g_testImagePick.c_str(), MAX_PATH * 2 - 1);
  else if (!GetOpenFileNameW(&o)) return;
  Doc& d = D();
  wstring src = buf, base = src.substr(src.find_last_of(L'\\') + 1);
  wstring dir = (d.path.empty() ? notesDir() : d.path.substr(0, d.path.find_last_of(L'\\'))) + L"\\attachments";
  CreateDirectoryW(dir.c_str(), nullptr);
  wstring dst = dir + L"\\" + base;
  CopyFileW(src.c_str(), dst.c_str(), TRUE);
  wstring alt = base.substr(0, base.find_last_of(L'.'));
  wstring link;  // spaces and brackets are written %-encoded so the Markdown stays valid
  for (wchar_t c : base) {
    if (c == L' ') link += L"%20";
    else if (c == L'(') link += L"%28";
    else if (c == L')') link += L"%29";
    else link += c;
  }
  for (auto& c : alt) if (c == L'[' || c == L']') c = L' ';
  fmtInsertImage(L"![" + alt + L"](attachments/" + link + L")");
}

/* ---------------------------------------------------------------- formatting toolbar */
struct FmtBtn { const wchar_t* label; int cmd; float x, w; };
static D2D1_RECT_F fmtPill(int i);
static vector<FmtBtn> fmtButtons() {
  struct Def { const wchar_t* l; int c; };
  // a label is drawn as text, an empty label is drawn as a vector icon chosen by the command
  static const Def defs[] = {{L"H1", C_H1}, {L"H2", C_H2}, {L"H3", C_H3}, {nullptr, 0},
                             {L"B", C_BOLD}, {L"I", C_ITALIC}, {L"S", C_STRIKE}, {L"</>", C_CODE}, {nullptr, 0},
                             {L"", C_UL}, {L"", C_OL}, {L"", C_TASK}, {L"", C_QUOTE}, {nullptr, 0},
                             {L"{ }", C_CODEBLOCK}, {L"", C_LINK}, {L"", C_IMAGE}, {L"", C_TABLE}, {L"", C_HR}};
  vector<FmtBtn> out;
  float x = L0() + 10;
  for (auto& df : defs) {
    if (!df.l) { x += 10; continue; }
    float w = df.l[0] ? 30.f : 28.f;
    if (x + w > g_vp.right - 10) break;  // a narrow editor: what does not fit is left out
    out.push_back({df.l, df.c, x, w});
    x += w + 1;
  }
  return out;
}
static void drawFmtIcon(int cmd, const D2D1_RECT_F& r, DWORD col) {
  float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
  g_br->SetColor(Rgb(col));
  auto ln = [&](float x1, float y1, float x2, float y2, float w = 1.4f) { g_rt->DrawLine(D2D1::Point2F(cx + x1, cy + y1), D2D1::Point2F(cx + x2, cy + y2), g_br, w); };
  auto sq = [&](float x, float y, float w, float h, float rad = 1.5f, float sw = 1.3f) {
    g_rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx + x, cy + y, cx + x + w, cy + y + h), rad, rad), g_br, sw);
  };
  switch (cmd) {
    case C_UL:
      g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 6, cy - 4), 1.3f, 1.3f), g_br);
      g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 6, cy + 4), 1.3f, 1.3f), g_br);
      ln(-2, -4, 7, -4);
      ln(-2, 4, 7, 4);
      break;
    case C_OL:
      ln(-2, -4, 7, -4);
      ln(-2, 4, 7, 4);
      ln(-6, -6, -6, -2, 1.2f);
      ln(-8, 2.5f, -4, 2.5f, 1.2f);
      ln(-4, 2.5f, -4, 6.5f, 1.2f);
      break;
    case C_TASK:
      sq(-6, -6, 12, 12, 2.5f);
      ln(-3, 0, -1, 2.5f);
      ln(-1, 2.5f, 3.5f, -2.5f);
      break;
    case C_QUOTE:
      ln(-6, -5, -6, 5, 1.8f);
      ln(-2, -4, 7, -4);
      ln(-2, 0, 5, 0);
      ln(-2, 4, 7, 4);
      break;
    case C_LINK:
      sq(-8, -3, 9, 6, 3.f);
      sq(-1, -3, 9, 6, 3.f);
      ln(-3, 0, 3, 0);
      break;
    case C_IMAGE:
      sq(-7, -6, 14, 12, 2.f);
      ln(-5, 4, -1, -1);
      ln(-1, -1, 2, 2);
      ln(2, 2, 3.5f, 0.5f);
      g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 3, cy - 2.5f), 1.2f, 1.2f), g_br);
      break;
    case C_TABLE:
      sq(-7, -6, 14, 12, 2.f);
      ln(-7, 0, 7, 0, 1.1f);
      ln(0, -6, 0, 6, 1.1f);
      break;
    case C_HR:
      ln(-7, 0, 7, 0, 1.8f);
      break;
  }
}
static D2D1_RECT_F fmtPill(int i) {
  static const wchar_t* names[4] = {L"Visual", L"Preview", L"Lines", L"Symbols"};
  float x = g_vp.right - (g_mapVisible && mdCut() == 0 ? MAP_W : 0.f) - 8;
  float y = g_vp.top + TAB_H + g_crumbH + 6;
  for (int k = 3; k >= i; k--) {
    float w = textWidth(names[k], g_uiSmall) + 24;
    x -= w;
    if (k == i) return rc(x, y, w, 24);
    x -= 6;
  }
  return rc(0, 0, 0, 0);
}
static bool fmtPillOn(int i) {
  Doc& d = D();
  return i == 0 ? d.vis : i == 1 ? d.mdPrev : i == 2 ? g_vLines : i == 3 ? g_vSyms : false;
}
static void drawFmtBar() {
  if (!fmtVisible()) return;
  const Theme& t = T();
  float y = g_vp.top + TAB_H + g_crumbH;
  fillRect(L0(), y, g_vp.right - L0(), FMT_H, t.bg);
  fillRect(L0(), y + FMT_H - 1, g_vp.right - L0(), 1, t.fg, 0.1f);
  DWORD ic = mixc(t.bg, t.fg, 0.7f);
  float ty0 = y + (FMT_H - 1 - 24) / 2;
  const auto btns = fmtButtons();
  for (size_t k = 1; k < btns.size(); k++) {  // a thin line between the groups of buttons
    float gap = btns[k].x - (btns[k - 1].x + btns[k - 1].w);
    if (gap > 4) fillRect(btns[k - 1].x + btns[k - 1].w + gap / 2 - 0.5f, ty0 + 4, 1, 16, 0x808080, 0.3f);
  }
  for (size_t k = 0; k < btns.size(); k++) {
    const FmtBtn& b = btns[k];
    D2D1_RECT_F r = rc(b.x, ty0, b.w, 24);
    if (inRect(r, g_mx, g_my)) fillR(r, t.fg, 0.1f, 5);
    if (!b.label[0]) { drawFmtIcon(b.cmd, r, ic); continue; }
    float tw;
    int style = b.cmd == C_BOLD ? 1 : b.cmd == C_ITALIC ? 2 : 0;
    IDWriteTextFormat* f = g_uiSmall;
    if (style) {
      // bold / italic toolbar labels use a fixed UI size, so zooming the text never resizes the buttons
      static IDWriteTextFormat* uiStyled[4] = {};
      if (!uiStyled[style]) {
        g_dw->CreateTextFormat(L"Segoe UI", nullptr, (style & 1) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                               (style & 2) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us", &uiStyled[style]);
        if (uiStyled[style]) uiStyled[style]->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
      }
      if (uiStyled[style]) f = uiStyled[style];
    }
    wstring lb = b.label;
    tw = textWidth(lb, f);
    drawText(lb.c_str(), (int)lb.size(), b.x + (b.w - tw) / 2, ty0 + 3, ic, f, b.w + 8, 20);
    if (b.cmd == C_STRIKE) fillRect(b.x + (b.w - tw) / 2 - 1, ty0 + 12, tw + 2, 1.2f, ic, 0.9f);
  }
  // Visual | Preview | Lines | Symbols pills on the right
  static const wchar_t* pn[4] = {L"Visual", L"Preview", L"Lines", L"Symbols"};
  for (int i = 0; i < 0; i++) {  // the pills are switched off in this edition: notes stay in Visual mode
    D2D1_RECT_F pr = fmtPill(i);
    bool on = fmtPillOn(i);
    bool hv = inRect(pr, g_mx, g_my);
    fillR(pr, on ? t.accent : t.fg, on ? (hv ? 0.3f : 0.2f) : (hv ? 0.08f : 0.0f), 12);
    strokeR(pr, on ? t.accent : t.fg, on ? 0.75f : 0.22f, 12);
    float tw = textWidth(pn[i], g_uiSmall);
    drawText(pn[i], (int)wcslen(pn[i]), pr.left + (pr.right - pr.left - tw) / 2, pr.top + 3, on ? t.fg : mixc(t.bg, t.fg, 0.7f), g_uiSmall, tw + 8, 20);
  }
}
static bool fmtBarMouse(float x, float y) {
  if (!fmtVisible() || y < g_vp.top + TAB_H + g_crumbH || y >= g_vp.top + TAB_H + g_crumbH + FMT_H) return false;
  if (x < L0()) return false;
  for (auto& b : fmtButtons())
    if (x >= b.x && x < b.x + b.w) { exec(b.cmd); return true; }
  for (int i = 0; i < 0; i++) {
    if (!inRect(fmtPill(i), x, y)) continue;
    if (i == 0) exec(C_TOGGLEVISUAL);
    else if (i == 1) exec(C_MDPREVIEW);
    else if (i == 2) { g_vLines = !g_vLines; ensureVisible(); redraw(); }
    else if (i == 3) { g_vSyms = !g_vSyms; D().invalidate(0); ensureVisible(); redraw(); }
    return true;
  }
  return true;
}

#include "edit_cmds.inc"
#include "extras.inc"
#include "exec.inc"
#include "settings_ui.inc"
#include "panel.inc"
#include "files.inc"
#include "git.inc"
#include "pawn.inc"
#include "amxx.inc"
#include "amxx_tools.inc"
#include "hud_editor.inc"
#include "amxx_unpack.inc"
#include "license.inc"
#include "lsp.inc"
#include "claude_proc.inc"
#include "claude_md.inc"
#include "claude_svg.inc"
#include "claude_chat.inc"
#include "claude_view.inc"
#include "claude_ui.inc"
#include "mdpreview.inc"
static bool g_wdDrag = false;
static bool vpIsFocusedPane();
#include "widgets.inc"
#include "spell.inc"
#include "modal.inc"


/* ---------------------------------------------------------------- layout helpers */
struct TabRect { float x, w; int idx; };
static vector<TabRect> tabRects(int grp) {
  vector<TabRect> r;
  float x = L0() + 52;  // after the two scroll arrows
  for (size_t i = 0; i < g_docs.size(); i++) {
    Doc* d = g_docs[i];
    if (d->group != grp) continue;
    float w = std::min(320.f, std::max(90.f, textWidth(d->name, g_ui) + 56 + (d->isNote ? 20.f : 0.f)));
    r.push_back({x, w, (int)i});
    x += w + 1;
  }
  return r;
}
// bottom status bar: the sidebar icon and the position on the left, then the clickable settings on the right
struct SItem { D2D1_RECT_F r; int id; wstring text; };
static vector<SItem> statusItems(Doc& d) {
  vector<SItem> out;
  float sy = ch() - STATUS_H, W = cw();
  out.push_back({rc(6, sy + 2, 24, STATUS_H - 4), 0, L""});
  wchar_t buf[160];
  int cnum = d.vis ? d.cur.c : d.vcol(d.cur.l, d.cur.c);
  int n;
  if (!d.extra.empty()) n = swprintf_s(buf, L"%d selections", (int)d.extra.size() + 1);
  else if (d.hasSel()) n = swprintf_s(buf, L"Line %d, Column %d  (%d chars selected)", d.cur.l + 1, cnum + 1, (int)d.selectedText().size());
  else n = swprintf_s(buf, L"Line %d, Column %d", d.cur.l + 1, cnum + 1);
  wstring pos(buf, n);
  float lx = 34 + textWidth(pos, g_uiSmall) + 14;
  out.push_back({rc(34, sy + 2, textWidth(pos, g_uiSmall) + 14, STATUS_H - 4), 1, pos});
  auto leftItem = [&](int id, const wstring& txt) {
    float iw = textWidth(txt, g_uiSmall) + 14;
    out.push_back({rc(lx, sy + 2, iw, STATUS_H - 4), id, txt});
    lx += iw;
  };
  if (g_recording) leftItem(20, L"\x25CF REC");
  if (!g_lspStatus.empty()) leftItem(21, g_lspStatus);
  if (!g_gitBranch.empty()) leftItem(22, L"\x2387 " + g_gitBranch + (g_gitChanges ? L"  \x00B1" + std::to_wstring(g_gitChanges) : L""));
  if (!g_statusMsg.empty() && GetTickCount() - g_statusMsgAt < 3500) leftItem(23, g_statusMsg);
  wstring encName = d.enc == 0 ? wstring(L"UTF-8") + (d.bom ? L" with BOM" : L"") : d.enc == 1 ? L"UTF-16 LE" + wstring(d.bom ? L" with BOM" : L"") : d.enc == 2 ? L"UTF-16 BE" + wstring(d.bom ? L" with BOM" : L"") : wstring(ENC_NAMES[3]);
  wstring tabText = d.spaces ? L"Spaces: " + std::to_wstring(d.tabW) : L"Tab Size: " + std::to_wstring(d.tabW);
  wstring items[4] = {d.langName, tabText, d.crlf ? L"Windows" : L"Unix", encName};
  int ids[4] = {13, 12, 11, 10};
  float xr = W - 6;
  for (int k = 0; k < 4; k++) {
    float iw = textWidth(items[k], g_uiSmall) + 14;
    xr -= iw;
    out.push_back({rc(xr, sy + 2, iw, STATUS_H - 4), ids[k], items[k]});
  }
  return out;
}
static bool statusClick(float x, float y) {
  if (y < ch() - STATUS_H) return false;
  Doc& d = D();
  for (auto& it : statusItems(d)) {
    if (!inRect(it.r, x, y)) continue;
    switch (it.id) {
      case 0: exec(C_SIDEBAR); break;
      case 1: exec(C_GOTOLINE); break;
      case 10: pickEncoding(); break;
      case 11: pickEol(); break;
      case 12: pickIndent(); break;
      case 13: pickSyntax(); break;
      case 20: exec(C_MACROTOGGLE); break;
      case 21: exec(C_LSPPROBLEMS); break;
      case 22: exec(C_GITCHANGED); break;
    }
    return true;
  }
  return true;
}
static int tabAt(float x, float y) {
  for (int gi = 0; gi < paneCount(); gi++) {
    const VP& p = g_paneRect[gi];
    if (y < p.top || y >= p.top + TAB_H) continue;
    VP saved = g_vp;
    g_vp = p;
    int found = -1;
    for (auto& r : tabRects(gi)) if (x >= r.x && x < r.x + r.w) { found = r.idx; break; }
    g_vp = saved;
    if (found >= 0) return found;
  }
  return -1;
}
static void layoutMenus() {
  float x = g_uiStyle == 3 ? 76.f : 4.f;
  for (auto& m : g_menus) {
    m.w = textWidth(m.label, g_ui) + 16;
    m.x = x;
    x += m.w;
  }
}
static void menuDropRect(int mi, float* w, float* h) {
  float mw = 160;
  for (auto& it : g_menus[mi].items)
    if (!it.label.empty()) mw = std::max(mw, textWidth(it.label, g_ui) + textWidth(effectiveKeys(it.cmd), g_ui) + 60);
  float mh = 8;
  for (auto& it : g_menus[mi].items) mh += !it.label.empty() ? 26 : 9;
  *w = mw;
  *h = mh;
}
static Pos hit(float mx, float my) {
  Doc& d = D();
  int row = rowAtY(d, my - edTop() + d.scrollY);
  return posFromRowX(d, row, mx - textX() + d.scrollX);
}

/* ---------------------------------------------------------------- painting */
/* the vertical scrollbar of the editor */
static bool g_dragVs = false;
static float g_vsOff = 0;
static bool vpIsFocusedPane() { return g_vp.left == g_paneRect[std::min(g_curGroup, 1)].left && g_vp.right == g_paneRect[std::min(g_curGroup, 1)].right; }
static bool vsGeom(Doc& d, float& ty, float& th) {
  float top = edTop(), h = edH();
  int nrows = totalRows(d);
  float total = rowTop(d, nrows), maxS = maxScrollY();
  if (!(total > h && maxS > 0)) return false;
  th = std::max(30.f, h * h / total);
  ty = top + (h - th) * (d.scrollY / maxS);
  return true;
}
static void vsDrag(float y) {
  Doc& d = D();
  float ty, th;
  if (!vsGeom(d, ty, th)) return;
  float top = edTop(), h = edH(), maxS = maxScrollY();
  float frac = (y - g_vsOff - top) / std::max(1.f, h - th);
  d.scrollY = std::max(0.f, std::min(maxS, frac * maxS));
  redraw();
}
/* breadcrumbs (Refined and Modern): the folder path of the file and the symbol the caret is in */
static void drawCrumbs(Doc& d) {
  if (g_crumbH < 1) return;
  const Theme& t = T();
  float y = g_vp.top + TAB_H;
  fillRect(L0(), y, g_vp.right - L0(), g_crumbH, t.bg);
  vector<wstring> parts;
  if (!d.path.empty()) {
    bool found = false;
    for (auto* r : g_roots) {
      const wstring& rp = r->path;
      if (d.path.size() > rp.size() && _wcsnicmp(d.path.c_str(), rp.c_str(), rp.size()) == 0 && d.path[rp.size()] == L'\\') {
        parts.push_back(r->name);
        wstring rest = d.path.substr(rp.size() + 1);
        size_t p = 0;
        while (p <= rest.size()) { size_t q = rest.find_first_of(L"\\/", p); if (q == wstring::npos) q = rest.size(); parts.push_back(rest.substr(p, q - p)); p = q + 1; }
        found = true;
        break;
      }
    }
    if (!found) {
      vector<wstring> all;
      size_t p = 0;
      while (p <= d.path.size()) { size_t q = d.path.find_first_of(L"\\/", p); if (q == wstring::npos) q = d.path.size(); all.push_back(d.path.substr(p, q - p)); p = q + 1; }
      for (size_t i = all.size() > 3 ? all.size() - 3 : 0; i < all.size(); i++) parts.push_back(all[i]);
    }
  } else parts.push_back(d.name);
  wstring sym;
  if (!d.path.empty()) {
    int best = -1;
    auto& syms = symbolsOf(d);
    for (size_t i = 0; i < syms.size(); i++) if (syms[i].line <= d.cur.l) best = (int)i;
    if (best >= 0) sym = trimW(syms[best].name);
  }
  float fs = g_uiStyle == 2 ? 12.f : 11.5f;
  IDWriteTextFormat* f = uiFont(fs);
  float x = L0() + (g_uiStyle == 2 ? 18.f : 16.f), ty = y + (g_crumbH - fs * 1.33f) / 2;
  DWORD dim = t.gutterFg;
  auto put = [&](const wstring& s, DWORD col) {
    float w = textWidth(s, f);
    if (x + w > g_vp.right - 10) return false;
    drawText(s.c_str(), (int)s.size(), x, ty, col, f, w + 4, 20);
    x += w + 6;
    return true;
  };
  for (size_t i = 0; i < parts.size(); i++) {
    if (i && !put(L"›", mixc(t.bg, t.fg, 0.28f))) break;
    if (!put(parts[i], i + 1 == parts.size() && sym.empty() ? t.fg : (i + 1 == parts.size() ? t.fg : dim))) break;
  }
  if (!sym.empty()) { if (put(L"›", mixc(t.bg, t.fg, 0.28f))) put(sym, mixc(t.bg, t.fg, 0.8f)); }
}
static void drawChromeTip();  // chrometip.inc
static void drawTabDrop();  // ctxmenu.inc
static void showTabList(int gi);  // ctxmenu.inc
static void wdFrameStart();  // widgets.inc

static bool wdMouseDown(float x, float y);
static void wdDragMove(float x);
static void paint() {
  wdFrameStart();
  const Theme& t = T();
  Doc& d = D();
  float W = cw(), H = ch();
  g_rt->BeginDraw();
  g_rt->Clear(Rgb(t.bg));
  computeLayout();

  // one group (pane): editor, gutter, minimap, tab bar and the formatting toolbar; g_vp and g_active are set by the caller
  auto paintPane = [&](int grp) {
  Doc& d = D();
  /* editor */
  float top = edTop(), h = edH(), tx = textX(), gw = gutterW();
  ensureRows(d);
  g_rt->PushAxisAlignedClip(D2D1::RectF(L0(), top, mapX(), top + h), D2D1_ANTIALIAS_MODE_ALIASED);
  int nrows = d.rowStart.back();
  int rowFirst = std::max(0, rowAtY(d, d.scrollY));
  int rowLast = std::min(nrows - 1, rowAtY(d, d.scrollY + h));
  int lFirst = lineOfRow(d, rowFirst), lLast = lineOfRow(d, rowLast);
  vector<Doc::Sel> sels;
  sels.push_back({d.cur, d.anc});
  for (auto& e : d.extra) sels.push_back(e);
  if (g_findOpen) computeMatches();
  bool colorBr = g_brackets && d.lang != L_PLAIN && d.lang != L_MD;
  d.ensureStates(lLast);
  const DWORD brCol[4] = {t.syn[3], t.syn[5], t.syn[1], t.syn[2]};
  const float ty = (g_lineH - g_fontSize * 1.25f) / 2;
  // matching bracket next to the caret
  Pos bm1{-1, -1}, bm2{-1, -1};
  if (g_brackets && !d.hasSel() && d.extra.empty() && d.lang != L_PLAIN && d.lang != L_MD) {
    const wstring& cs = d.lines[d.cur.l];
    auto isOpen = [](wchar_t c) { return c == L'(' || c == L'[' || c == L'{'; };
    auto isClose = [](wchar_t c) { return c == L')' || c == L']' || c == L'}'; };
    auto partner = [](wchar_t c) { switch (c) { case L'(': return L')'; case L'[': return L']'; case L'{': return L'}'; case L')': return L'('; case L']': return L'['; default: return L'{'; } };
    for (int pass = 0; pass < 2 && bm1.l < 0; pass++) {
      int c = pass == 0 ? d.cur.c - 1 : d.cur.c;
      if (c < 0 || c >= (int)cs.size()) continue;
      wchar_t ch0 = cs[c];
      if (!isOpen(ch0) && !isClose(ch0)) continue;
      bool fwd = isOpen(ch0);
      wchar_t other = partner(ch0);
      int depth = 0, budget = 30000;
      int l = d.cur.l, i = c;
      while (l >= 0 && l < (int)d.lines.size() && budget > 0) {
        const wstring& ls = d.lines[l];
        for (; fwd ? i < (int)ls.size() : i >= 0; i += fwd ? 1 : -1, budget--) {
          wchar_t x = ls[i];
          if (x == ch0) depth++;
          else if (x == other) { depth--; if (depth == 0) { bm1 = {d.cur.l, c}; bm2 = {l, i}; goto found; } }
          if (budget <= 0) break;
        }
        l += fwd ? 1 : -1;
        if (l < 0 || l >= (int)d.lines.size()) break;
        i = fwd ? 0 : (int)d.lines[l].size() - 1;
      }
    found:;
    }
  }
  if (!d.vis) {  // rulers: thin vertical lines at the configured columns
    for (int rc2 : g_rulerCols) {
      float rx = tx - d.scrollX + rc2 * g_charW;
      if (rx > L0() + gw && rx < mapX()) fillRect(rx, top, 1, h, t.fg, 0.1f);
    }
  }
  const vector<LspDiag>* lspDiagsOfDoc = nullptr;
  if (!d.path.empty()) { auto dit = g_lspDiags.find(lspKey(d.path)); if (dit != g_lspDiags.end() && !dit->second.empty()) lspDiagsOfDoc = &dit->second; }
  vector<Span> spans;
  vector<signed char> bc;
  for (int l = lFirst; l <= lLast; l++) {
    if (d.isHidden(l)) continue;
    const wstring& s = d.lines[l];
    d.tokensOf(l, spans);
    if (colorBr) {
      bc.assign(s.size(), -1);
      int depth = d.bd[l];
      for (auto& sp : spans) {
        if (sp.kind != 0) continue;
        for (int i = sp.a; i < sp.b; i++) {
          wchar_t c = s[i];
          if (c == L'(' || c == L'[' || c == L'{') { bc[i] = (signed char)(((depth % 4) + 4) % 4); depth++; }
          else if (c == L')' || c == L']' || c == L'}') { depth--; bc[i] = (signed char)(((depth % 4) + 4) % 4); }
        }
      }
    }
    int nseg = (int)d.brk[l].size() + 1;
    int indentV = 0;
    for (wchar_t c : s) { if (c == L' ') indentV++; else if (c == L'\t') indentV = (indentV / d.tabW + 1) * d.tabW; else break; }
    for (int k = 0; k < nseg; k++) {
      int row = d.rowStart[l] + k;
      if (row < rowFirst || row > rowLast) continue;
      int a, b;
      segOf(d, l, k, a, b);
      const float scl = d.vis ? d.vstyle(l).scale : 1.f;
      // in Visual mode positions are pixels (proportional font) and rows have their own heights
      const float cwL = d.vis ? 1.f : g_charW * scl;
      const float rh = d.vis ? rowH(d, row) : g_lineH * scl;
      float y = top + rowTop(d, row) - d.scrollY;
      if (const WBlock* wbk = widgetStart(d, l)) { drawWidget(d, *wbk, tx - d.scrollX, y, top, top + h); continue; }
      float baseX = tx - d.scrollX - d.vcol(l, a) * cwL;
      bool lastSeg = k == nseg - 1;
      auto X = [&](int col) { return baseX + d.vcol(l, col) * cwL; };
      if (g_findOpen && !g_matches.empty()) {
        auto it = std::lower_bound(g_matches.begin(), g_matches.end(), l, [](const Match& m, int line) { return m.l < line; });
        for (; it != g_matches.end() && it->l == l; ++it) {
          int m0 = std::max(it->a, a), m1 = std::min(it->b, b);
          if (m1 > m0) fillRect(X(m0), y + 1, X(m1) - X(m0), rh - 2, t.accent, 0.28f);
        }
      }
      if (g_hlLine && l == d.cur.l && grp == g_curGroup && !d.hasSel()) fillRect(L0() + gw, y, mapX() - L0() - gw, rh, t.fg, 0.045f);
      for (auto& mk : d.amxMarks)  // the last build: a tinted line with a bar on the left
        if (mk.line == l + 1 && k == 0) {
          DWORD mc = mk.type == 1 ? 0xec5f66 : 0xe5c07b;
          fillRect(L0() + gw, y, mapX() - L0() - gw, rh, mc, mk.type == 1 ? 0.14f : 0.12f);
          fillRect(L0() + gw, y, 3, rh, mc);
        }
      for (auto& sl : sels) {
        Pos pa = sl.c < sl.a ? sl.c : sl.a, pb = sl.c < sl.a ? sl.a : sl.c;
        if (pa == pb || l < pa.l || l > pb.l) continue;
        int c0 = l == pa.l ? pa.c : 0, c1 = l == pb.l ? pb.c : (int)s.size();
        c0 = std::max(c0, a);
        c1 = std::min(c1, b);
        bool nl = l != pb.l && lastSeg;
        if (c1 < c0 || (c1 == c0 && !nl)) continue;
        float x0 = X(c0), x1 = X(c1);
        if (nl) x1 += d.vis ? 6.f : cwL * 0.6f;
        fillRect(x0, y, std::max(2.f, x1 - x0), rh, t.sel);
      }
      if (g_guides && k == 0 && !d.vis) {
        for (int stop = d.tabW; stop < indentV; stop += d.tabW) {
          float gx = tx - d.scrollX + stop * g_charW - 1;
          if (gx > L0() + gw) fillRect(gx, y, 1, rh, t.fg, 0.1f);
        }
      }
      for (Pos bp : {bm1, bm2}) {
        if (bp.l == l && bp.c >= a && bp.c < b) strokeR(D2D1::RectF(X(bp.c), y + 1.5f, X(bp.c) + cwL, y + rh - 1.5f), t.fg, 0.5f);
      }
      auto drawRun = [&](int a0, int b0, DWORD col, int style = 0) {
        float x = X(a0);
        IDWriteTextFormat* mf = style == 3 && g_monoBI ? g_monoBI : style == 2 && g_monoI ? g_monoI : style == 1 && g_monoB ? g_monoB : g_mono;
        if (x < mapX() && x + (b0 - a0) * g_charW > L0() + gw) drawText(s.c_str() + a0, b0 - a0, x, y + ty, col, mf, (b0 - a0) * g_charW + 8, g_lineH);
      };
      if (g_wsMode && !d.vis) {  // white space: a dot for a space, a line for a tab
        for (int i = a; i < b; i++) {
          if (s[i] != L' ' && s[i] != L'\t') continue;
          if (g_wsMode == 1) {
            bool inSel = false;
            for (auto& sl : sels) {
              Pos pa = sl.c < sl.a ? sl.c : sl.a, pb = sl.c < sl.a ? sl.a : sl.c;
              Pos me{l, i};
              if (!(pa == pb) && !(me < pa) && me < pb) { inSel = true; break; }
            }
            if (!inSel) continue;
          }
          float wx = X(i), wx2 = X(i + 1);
          if (wx2 < L0() + gw || wx > mapX()) continue;
          if (s[i] == L' ') fillR(rc((wx + wx2) / 2 - 1.f, y + rh / 2 - 1.f, 2.f, 2.f), t.ws, 1.f, 1.f);
          else fillRect(wx + 3, y + rh / 2, std::max(2.f, wx2 - wx - 6), 1, t.ws);
        }
      }
      if (d.vis) {
        // Visual Markdown: concealed markup, headings, bold/italic, code, links, quotes, rules
        const VStyle& vs = d.vstyle(l);
        const bool conc = d.concealed(l);
        const float vty = (rh - g_fontSize * 1.03f * scl * 1.3f) / 2;
        if (vs.block == 2) fillR(rc(tx - 6 - d.scrollX, y, std::max(10.f, mapX() - tx - 8), rh), t.fg, 0.07f);
        if (vs.block == 1) fillRect(tx - 10 - d.scrollX, y, 3, rh, t.accent, 0.55f);
        if (vs.block == 3 && conc) fillRect(tx - d.scrollX, y + rh / 2, std::max(20.f, mapX() - tx - 30), 1.5f, t.gutterFg, 0.5f);
        int i0 = a;
        while (i0 < b) {
          if (conc && vs.hid[i0]) { i0++; continue; }
          if (s[i0] == L'\t') { i0++; continue; }
          if (conc && vs.rep[i0]) {
            wchar_t gch = vs.rep[i0];
            float gx = X(i0), gw2 = X(i0 + 1) - gx;
            if (gx < mapX() && gx + gw2 > L0() + gw) {
              if (gch == 0x2022) {
                float bw = textWidth(L"\x2022", propFor(1, scl));
                drawText(&gch, 1, gx + (gw2 - bw) / 2, y + vty, t.accent, propFor(1, scl), gw2 + 8, rh);
              } else {  // a check box: a rounded square, filled with the accent colour and a tick when ticked
                float sz = g_fontSize * 1.07f * scl * 1.05f, bx = gx + g_fontSize * 1.07f * scl * 0.15f, by = y + (rh - sz) / 2 + 1;
                D2D1_RECT_F br2 = rc(bx, by, sz, sz);
                if (gch == 0x2611) {
                  fillR(br2, t.accent, 1.f, 3.5f);
                  g_br->SetColor(Rgb(0x1f2933));
                  float u = sz / 16.f;
                  g_rt->DrawLine(D2D1::Point2F(bx + 3.6f * u, by + 8.4f * u), D2D1::Point2F(bx + 6.8f * u, by + 11.6f * u), g_br, 1.9f);
                  g_rt->DrawLine(D2D1::Point2F(bx + 6.8f * u, by + 11.6f * u), D2D1::Point2F(bx + 12.4f * u, by + 4.8f * u), g_br, 1.9f);
                } else {
                  fillR(br2, mixc(t.bg, t.fg, 0.05f), 1.f, 3.5f);
                  strokeR(br2, t.fg, 0.55f, 3.5f);
                }
              }
            }
            i0++;
            continue;
          }
          unsigned f = vs.fl[i0];
          bool hd = !conc && vs.hid[i0];
          int j = i0;
          while (j < b && s[j] != L'\t' && vs.fl[j] == f && (conc ? (!vs.hid[j] && !vs.rep[j]) : (vs.hid[j] != 0) == hd)) j++;
          if (j == i0) j = i0 + 1;
          DWORD col = hd ? t.gutterFg : (f & VF_ACCENT) ? t.accent : (f & VF_HEAD) ? t.syn[8] : ((f & VF_CODE) && (f & VF_DIM)) ? t.gutterFg : (f & VF_CODE) ? t.syn[0] : (f & VF_LINK) ? t.syn[5] : (f & (VF_QUOTE | VF_DIM)) ? t.gutterFg : t.syn[0];
          int style = flagStyle(f);
          float x = X(i0), wpx = X(j) - X(i0);
          if ((f & VF_CODE) && vs.block != 2) fillR(rc(x - 2, y + rh / 2 - 10, wpx + 4, 20), t.fg, 0.11f, 4);
          if (x < mapX() && x + wpx > L0() + gw) drawText(s.c_str() + i0, j - i0, x, y + vty, col, propFor(style, scl), wpx + 8, rh);
          if (f & VF_LINK) fillRect(x, y + rh - 4, wpx, 1, col, 0.8f);
          if (f & VF_STRIKE) fillRect(x, y + rh / 2, wpx, 1.2f, col, 0.9f);
          i0 = j;
        }
      } else
      for (auto& sp : spans) {
        int sa = std::max(sp.a, a), sb2 = std::min(sp.b, b);
        int p0 = sa;
        while (p0 < sb2) {
          while (p0 < sb2 && (s[p0] == L' ' || s[p0] == L'\t')) p0++;
          int p1 = p0;
          while (p1 < sb2 && s[p1] != L' ' && s[p1] != L'\t') p1++;
          if (p1 > p0) {
            if (colorBr && sp.kind == 0) {
              int q = p0;
              for (int i = p0; i < p1; i++) {
                if (bc[i] >= 0) {
                  if (i > q) drawRun(q, i, t.syn[0]);
                  drawRun(i, i + 1, brCol[(int)bc[i]]);
                  q = i + 1;
                }
              }
              if (p1 > q) drawRun(q, p1, t.syn[0]);
            } else {
              if (sp.kind == AK_INVALID) {  // a stray ) or a bad directive: the scheme's "invalid" background
                const AmxPal* ip = amxPal();
                fillRect(X(p0), y + 1, (p1 - p0) * g_charW, rh - 2, ip && ip->invBg != 0xffffffff ? ip->invBg : 0xe5534b, 0.6f);
              }
              drawRun(p0, p1, synCol(sp.kind), synStyle(sp.kind, d.lang == L_PAWN));
            }
          }
          p0 = p1;
        }
      }
      for (auto& uk : d.unknown) {  // calls of unknown functions: a wavy underline
        if (uk.line != l || uk.c0 < a || uk.c0 >= b) continue;
        float ux0 = X(uk.c0), ux1 = X(std::min(uk.c0 + uk.len, b)), uy = y + rh - 3.5f;
        g_br->SetColor(Rgb(0xe5c07b));
        for (float px = ux0; px < ux1; px += 2.f) {
          float py0 = uy + (((int)((px - ux0) / 2.f)) % 2 ? 1.5f : 0.f), py1 = uy + (((int)((px - ux0) / 2.f) + 1) % 2 ? 1.5f : 0.f);
          g_rt->DrawLine(D2D1::Point2F(px, py0), D2D1::Point2F(std::min(px + 2.f, ux1), py1), g_br, 1.f);
        }
      }
      if (d.isNote && g_spellOn) {  // misspelled words in notes: red wavy underline
        vector<std::pair<int, int>> miss;
        d.ensureStates(l);
        bool inCode = d.st[l] == 3 || trimW(s).compare(0, 3, L"```") == 0;  // no spell check in code
        if (!inCode) spellMisses(s, a, b, miss);
        const VStyle* svs = d.vis ? &d.vstyle(l) : nullptr;
        for (auto& mw : miss) {
          if (svs && mw.first < (int)svs->fl.size() && (svs->fl[mw.first] & VF_CODE)) continue;
          int c0 = std::max(mw.first, a), c1 = std::min(mw.second, b);
          if (c1 <= c0) continue;
          float ux0 = X(c0), ux1 = X(c1), uy = y + rh - 3.5f;
          g_br->SetColor(Rgb(0xe0596a));
          for (float px = ux0; px < ux1; px += 2.f) {
            float py0 = uy + (((int)((px - ux0) / 2.f)) % 2 ? 1.5f : 0.f), py1 = uy + (((int)((px - ux0) / 2.f) + 1) % 2 ? 1.5f : 0.f);
            g_rt->DrawLine(D2D1::Point2F(px, py0), D2D1::Point2F(std::min(px + 2.f, ux1), py1), g_br, 1.f);
          }
        }
      }
      if (lspDiagsOfDoc) {  // errors and warnings of the language server: wavy (dotted for hints) underlines
        for (auto& dg : *lspDiagsOfDoc) {
          if (l < dg.l0 || l > dg.l1) continue;
          int c0 = l == dg.l0 ? dg.c0 : 0, c1 = l == dg.l1 ? dg.c1 : (int)s.size();
          c0 = std::min(c0, (int)s.size());
          c1 = std::min(c1, (int)s.size());
          if (c1 <= c0) { c1 = std::min((int)s.size(), c0 + 1); if (c1 <= c0) { c0 = std::max(0, c0 - 1); c1 = (int)s.size(); } }
          if (c1 <= c0) continue;
          c0 = std::max(c0, a);
          c1 = std::min(c1, b);
          if (c1 <= c0) continue;
          float ux0 = X(c0), ux1 = X(c1), uy = y + rh - 3.5f;
          DWORD uc = dg.sev == 1 ? 0xe0596a : dg.sev == 2 ? 0xe5c07b : 0x6fb7e8;
          g_br->SetColor(Rgb(uc));
          if (dg.sev <= 2) {
            for (float px = ux0; px < ux1; px += 2.f) {
              float py0 = uy + (((int)((px - ux0) / 2.f)) % 2 ? 1.5f : 0.f), py1 = uy + (((int)((px - ux0) / 2.f) + 1) % 2 ? 1.5f : 0.f);
              g_rt->DrawLine(D2D1::Point2F(px, py0), D2D1::Point2F(std::min(px + 2.f, ux1), py1), g_br, 1.f);
            }
          } else for (float px = ux0; px < ux1; px += 3.f) g_rt->FillRectangle(D2D1::RectF(px, uy + 1, std::min(px + 1.2f, ux1), uy + 2.2f), g_br);
        }
      }
      if (lastSeg && d.folded.count(l)) {  // a collapsed block: a small chip after the line
        float fx = X((int)s.size()) + 10;
        fillR(rc(fx, y + rh / 2 - 7, 26, 14), t.fg, 0.14f, 4);
        for (int dot = 0; dot < 3; dot++) fillRect(fx + 7 + dot * 5, y + rh / 2 + 1, 2, 2, t.fg, 0.7f);
      }
    }
  }
  if (g_caretOn && !(g_ccOpen && (g_ccFocusInput || g_ccFocusBlock)) && (GetFocus() == g_hwnd || g_testing)) {
    for (auto& sl : sels) {
      if (const WBlock* wbk = d.wAt.size() > (size_t)sl.c.l && d.wAt[sl.c.l] >= 0 ? &d.wb[d.wAt[sl.c.l]] : nullptr) if (wbk->kind == 1) continue;  // a selected picture has no caret
      int crow = rowOfPos(d, sl.c);
      float x = tx - d.scrollX + rowVcol(d, sl.c) * (d.vis ? 1.f : g_charW * lineScale(d, sl.c.l)), y = top + rowTop(d, crow) - d.scrollY;
      if (x >= L0() + gw) fillRect(x, y + 1, 2, rowH(d, crow) - 2, t.caret);
    }
  }
  g_rt->PopAxisAlignedClip();

  /* gutter (drawn after the text so horizontally scrolled text goes under it) */
  fillRect(L0(), top, gw, h, t.bg);
  for (int l = lFirst; l <= lLast && g_gutterOn; l++) {
    if (d.isHidden(l)) continue;
    float y = top + rowTop(d, d.rowStart[l]) - d.scrollY, rhg = rowH(d, d.rowStart[l]);
    wchar_t num[16];
    int n = swprintf_s(num, L"%d", l + 1);
    float x = L0() + gw - 16 - n * g_charW * 0.85f;
    if (l == d.cur.l && grp == g_curGroup && !(d.vis && !g_vLines)) fillRect(L0(), y, gw, rhg, t.fg, 0.045f);
    if (d.marks.count(l)) fillR(rc(L0() + 3, y + rhg / 2 - 6, 9, 12), t.accent, 0.95f, 3);  // bookmark
    if (l < (int)d.gitMark.size() && d.gitMark[l]) {  // Git: added, modified, or lines removed below
      char gm = d.gitMark[l];
      DWORD gc = gm == 1 ? 0x3fb950 : gm == 2 ? 0xe0a030 : 0xe5534b;
      if (gm == 3) fillRect(L0() + gw - 5, y + rhg - 2, 5, 2, gc, 0.95f);
      else fillRect(L0() + gw - 4, y + 1, 3, rhg - 2, gc, 0.95f);
    }
    {  // fold marker: a chevron for a collapsed block, or for a foldable line under the mouse
      bool hov = g_mx >= L0() && g_mx < L0() + gw && g_my >= y && g_my < y + rhg;
      bool isFolded = d.folded.count(l) > 0;
      int fe;
      if (isFolded || (hov && foldRangeOf(d, l, fe))) {
        float cx = L0() + gw - 8.5f, cy = y + rhg / 2;
        g_br->SetColor(Rgb(t.gutterFg));
        if (isFolded) { g_rt->DrawLine(D2D1::Point2F(cx - 1.5f, cy - 3.5f), D2D1::Point2F(cx + 2.f, cy), g_br, 1.3f); g_rt->DrawLine(D2D1::Point2F(cx - 1.5f, cy + 3.5f), D2D1::Point2F(cx + 2.f, cy), g_br, 1.3f); }
        else { g_rt->DrawLine(D2D1::Point2F(cx - 3.5f, cy - 1.5f), D2D1::Point2F(cx, cy + 2.f), g_br, 1.3f); g_rt->DrawLine(D2D1::Point2F(cx + 3.5f, cy - 1.5f), D2D1::Point2F(cx, cy + 2.f), g_br, 1.3f); }
      }
    }
    if (d.vis && !g_vLines) continue;
    if (!g_lineNums && !d.vis) continue;
    {
      // line numbers are a little smaller than the code; in Visual mode they sit on the first text line
      // centred in its row; the draw rectangle is always taller than the glyphs, so a big zoom never clips the digits
      float th = g_fontSize * 0.85f * 1.35f;
      drawText(num, n, x, y + (rhg - th) / 2, l == d.cur.l ? t.gutterActive : t.gutterFg, fmtFor(l == d.cur.l ? 1 : 0, 0.85f), 80, th + 6);
    }
  }

  {  // vertical scrollbar next to the minimap: a thin thumb that turns brighter under the pointer and can be dragged
    float ty, th;
    if (vsGeom(d, ty, th)) {
      bool hv = g_dragVs || (g_mx >= mapX() - 12 && g_mx < mapX() && g_my >= ty && g_my < ty + th && vpIsFocusedPane());
      fillR(rc(mapX() - 9, ty, 6, th), hv ? t.scrollHover : t.scroll, 1.f, 3);
    }
  }

  /* minimap */
  float mx = mapX();
  if (mapShown()) {
  fillRect(mx, top, MAP_W, h, t.bg);
  fillRect(mx, top, 1, h, t.border, 0.5f);
  {
    int total = (int)d.lines.size();
    float contentH = total * MAP_LINE;
    float maxS = maxScrollY();
    float off = std::max(0.f, contentH - h) * (maxS > 0 ? d.scrollY / maxS : 0);
    int a = std::max(0, (int)(off / MAP_LINE)), b = std::min(total - 1, (int)((off + h) / MAP_LINE));
    g_rt->PushAxisAlignedClip(D2D1::RectF(mx, top, g_vp.right, top + h), D2D1_ANTIALIAS_MODE_ALIASED);
    for (int l = a; l <= b; l++) {
      float y = top + l * MAP_LINE - off;
      d.tokensOf(l, spans);
      const wstring& s = d.lines[l];
      for (auto& sp : spans) {
        int a0 = sp.a;
        while (a0 < sp.b) {
          while (a0 < sp.b && (s[a0] == L' ' || s[a0] == L'\t')) a0++;
          int b0 = a0;
          while (b0 < sp.b && s[b0] != L' ' && s[b0] != L'\t') b0++;
          if (b0 > a0) {
            float x = mx + 6 + std::min(90, d.vcol(l, a0)) * 0.9f;
            float w = std::min(MAP_W - 6 - (x - mx), (b0 - a0) * 0.9f);
            if (w > 0) fillRect(x, y, w, 1.4f, synCol(sp.kind), 0.6f);
          }
          a0 = b0;
        }
      }
    }
    // viewport slider
    int lt = lineOfRow(d, rowAtY(d, d.scrollY)), lb = lineOfRow(d, std::min(nrows - 1, rowAtY(d, d.scrollY + h)));
    float sy = top + lt * MAP_LINE - off, sh = (lb - lt + 1) * MAP_LINE;
    fillRect(mx, sy, MAP_W, sh, t.fg, 0.07f);
    g_rt->PopAxisAlignedClip();
  }
  }

  /* tab bar */
  if (TAB_H > 0.5f) {
  fillRect(g_vp.left, g_vp.top, g_vp.right - g_vp.left, TAB_H, styTabBar());
  if (g_uiStyle == 1 || g_uiStyle == 3) fillRect(g_vp.left, g_vp.top + TAB_H - 1, g_vp.right - g_vp.left, 1, t.fg, 0.1f);
  auto tr = tabRects(grp);
  static const float TG_Y[4] = {3, 4, 8, 5}, TG_H[4] = {31, 32, 30, 30};
  const float gy0 = g_vp.top + TG_Y[g_uiStyle], gth = TG_H[g_uiStyle];
  for (size_t k = 0; k < tr.size(); k++) {
    size_t i = (size_t)tr[k].idx;
    bool act = (int)i == g_active;
    bool hovT = (int)i == g_hoverTab;
    float x = tr[k].x, w = tr[k].w;
    float y0 = gy0;
    const float th = gth;
    // tab shapes are clipped to the tab bar so they never reach into the text below
    g_rt->PushAxisAlignedClip(D2D1::RectF(g_vp.left, g_vp.top, g_vp.right, g_vp.top + TAB_H), D2D1_ANTIALIAS_MODE_ALIASED);
    if (g_uiStyle == 0) {
      if (act) fillR(rc(x, y0, w, th + 8), t.bg, 1.f, 5);  // rounded top corners, flush with the editor
      else if (hovT) fillR(rc(x, y0, w, th + 8), t.bg, 0.22f, 5);
    } else if (g_uiStyle == 1) {
      if (act) fillR(rc(x, y0, w, th + 8), t.bg, 1.f, 7);
      else if (hovT) fillR(rc(x, y0, w, th + 8), mixc(styTabBar(), t.fg, 0.09f), 1.f, 7);
    } else if (g_uiStyle == 2) {
      if (act) fillR(rc(x, y0, w, th), mixc(styTabBar(), t.fg, 0.09f), 1.f, 9);
      else if (hovT) fillR(rc(x, y0, w, th), mixc(styTabBar(), t.fg, 0.09f), 0.7f, 9);
    } else {
      if (act) fillR(rc(x, y0, w, th), t.fg, 0.11f, 8);
      else if (hovT) fillR(rc(x, y0, w, th), t.fg, 0.07f, 8);
    }
    g_rt->PopAxisAlignedClip();
    float tx0 = x + 14;
    if (g_docs[i]->isNote) {  // the N badge of a note tab
      DWORD nc = t.accent;
      auto nm = g_meta.find(g_docs[i]->noteId);
      if (nm != g_meta.end() && !nm->second.color.empty()) nc = noteColorRgb(nm->second.color, t.accent);
      fillR(rc(tx0, y0 + 9, 14, 14), mixc(styTabBar(), nc, 0.35f), 1.f, 3);
      g_br->SetColor(Rgb(nc));
      drawText(L"N", 1, tx0 + 3, y0 + 8, nc, g_uiSmall, 14, 16);
      tx0 += 22;
    }
    drawText(g_docs[i]->name.c_str(), (int)g_docs[i]->name.size(), tx0, y0 + 6, act ? t.tabActive : (g_uiStyle == 3 ? mixc(t.bg, t.fg, 0.68f) : t.tabText), g_ui, w - (tx0 - x) - 28, 22);
    float cx = x + w - 17, cy = y0 + th / 2;
    if (g_docs[i]->dirty && !g_docs[i]->isNote && !((int)i == g_hoverTab)) {
      g_br->SetColor(Rgb(act ? t.tabActive : t.tabText));
      g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 3.5f, 3.5f), g_br);
    } else if (act || (int)i == g_hoverTab) {
      g_br->SetColor(Rgb(act ? t.tabActive : t.tabText));
      g_rt->DrawLine(D2D1::Point2F(cx - 3.5f, cy - 3.5f), D2D1::Point2F(cx + 3.5f, cy + 3.5f), g_br, 1.3f);
      g_rt->DrawLine(D2D1::Point2F(cx - 3.5f, cy + 3.5f), D2D1::Point2F(cx + 3.5f, cy - 3.5f), g_br, 1.3f);
    }
  }
  {  // tab bar buttons: scroll arrows on the left, new tab and the tab list on the right
    DWORD bc2 = mixc(styTabBar(), t.tabText, 0.85f);
    {  // hover feedback on the four buttons
      float by = gy0 + 2, bh = gth - 4;
      D2D1_RECT_F brs[4] = {rc(g_vp.left + 2, by, 24, bh), rc(g_vp.left + 26, by, 24, bh), rc(g_vp.right - 52, by, 26, bh), rc(g_vp.right - 26, by, 24, bh)};
      for (auto& br : brs) if (inRect(br, g_mx, g_my)) fillR(br, t.fg, 0.1f, 5);
    }
    g_br->SetColor(Rgb(bc2));
    float cy = gy0 + gth / 2, ax = g_vp.left + 10;
    g_rt->DrawLine(D2D1::Point2F(ax + 4, cy - 4), D2D1::Point2F(ax, cy), g_br, 1.4f);
    g_rt->DrawLine(D2D1::Point2F(ax + 4, cy + 4), D2D1::Point2F(ax, cy), g_br, 1.4f);
    float bx = g_vp.left + 34;
    g_rt->DrawLine(D2D1::Point2F(bx, cy - 4), D2D1::Point2F(bx + 4, cy), g_br, 1.4f);
    g_rt->DrawLine(D2D1::Point2F(bx, cy + 4), D2D1::Point2F(bx + 4, cy), g_br, 1.4f);
    float px = g_vp.right - 38;
    g_rt->DrawLine(D2D1::Point2F(px - 5, cy), D2D1::Point2F(px + 5, cy), g_br, 1.5f);
    g_rt->DrawLine(D2D1::Point2F(px, cy - 5), D2D1::Point2F(px, cy + 5), g_br, 1.5f);
    float dx = g_vp.right - 14;
    g_rt->DrawLine(D2D1::Point2F(dx - 4, cy - 2), D2D1::Point2F(dx, cy + 2), g_br, 1.4f);
    g_rt->DrawLine(D2D1::Point2F(dx + 4, cy - 2), D2D1::Point2F(dx, cy + 2), g_br, 1.4f);
  }
  }  // tab bar

  if (TAB_H > 0.5f) { drawCrumbs(d); drawFmtBar(); }
  };  // paintPane
  {
    int focusedActive = g_active, focusedGroup = g_curGroup;
    for (int gi = 0; gi < paneCount(); gi++) {
      g_vp = g_paneRect[gi];
      g_active = gi == focusedGroup ? focusedActive : g_groupActive[gi];
      if (g_active < 0 || g_active >= (int)g_docs.size()) g_active = focusedActive;
      paintPane(gi);
    }
    g_active = focusedActive;
    g_vp = g_paneRect[focusedGroup];
    if (paneCount() > 1) {
      // divider and a thin accent on the focused group
      if (g_layout == 1) fillRect(g_paneRect[0].right, MENU_H, 1, H - STATUS_H - MENU_H, t.border, 0.9f);
      else fillRect(g_paneRect[0].left, g_paneRect[0].bottom, g_paneRect[0].right - g_paneRect[0].left, 1, t.border, 0.9f);
      fillRect(g_vp.left, g_vp.top, g_vp.right - g_vp.left, 2, t.accent, 0.8f);
    }
  }

  /* menu bar */
  fillRect(0, 0, W, MENU_H, styChrome());
  if (g_uiStyle == 1 && MENU_H > TITLE_H) fillRect(0, MENU_H - 1, W, 1, t.fg, 0.1f);
  {
    // title bar: app icon, title, window buttons (the native frame is removed)
    D2D1_RECT_F ir = rc(10, 7, 16, 16);
    g_br->SetColor(Rgb(0x14b8a6));
    g_rt->FillRoundedRectangle(D2D1::RoundedRect(ir, 3.6f, 3.6f), g_br);
    g_br->SetColor(Rgb(0x3b82f6, 0.55f));
    g_rt->FillRoundedRectangle(D2D1::RoundedRect(rc(18, 7, 8, 16), 3.f, 3.f), g_br);
    g_br->SetColor(Rgb(0xffffff));
    ID2D1PathGeometry* pg = nullptr;
    g_d2d->CreatePathGeometry(&pg);
    if (pg) {
      ID2D1GeometrySink* sk = nullptr;
      pg->Open(&sk);
      sk->BeginFigure(D2D1::Point2F(18, 9.6f), D2D1_FIGURE_BEGIN_FILLED);
      sk->AddLine(D2D1::Point2F(21.3f, 14.3f));
      sk->AddLine(D2D1::Point2F(18, 20.6f));
      sk->AddLine(D2D1::Point2F(14.7f, 14.3f));
      sk->EndFigure(D2D1_FIGURE_END_CLOSED);
      sk->Close();
      sk->Release();
      g_rt->FillGeometry(pg, g_br);
      pg->Release();
    }
    wstring ttl = d.name;
    if (g_root && !d.isNote) ttl += L" (" + g_root->name + L")";
    ttl += L" - Scriba";
    if (!(g_uiStyle == 3 && g_zenMenu)) drawText(ttl.c_str(), (int)ttl.size(), g_uiStyle == 3 ? 76.f : 34.f, 6, t.chromeFg, g_ui, W - 76 - 3 * CAP_W, 20);
    const wchar_t* glyphs[3] = {L"", L"", L""};
    bool zoomed = IsZoomed(g_hwnd);
    if (zoomed) glyphs[1] = L"";
    for (int b = 0; b < 3; b++) {
      float bx = W - (3 - b) * CAP_W;
      if (g_hoverCaption == b + 1) fillRect(bx, 0, CAP_W, TITLE_H, b == 2 ? 0xe81123 : t.chromeHover, 1.f);
      g_dw->CreateTextFormat(L"Segoe MDL2 Assets", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.f, L"en-us", &g_capFmt_tmp);
      if (g_capFmt_tmp) {
        g_capFmt_tmp->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        g_capFmt_tmp->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        g_br->SetColor(Rgb(t.chromeFg));
        g_rt->DrawText(glyphs[b], 1, g_capFmt_tmp, rc(bx, 0, CAP_W, TITLE_H), g_br);
        g_capFmt_tmp->Release();
        g_capFmt_tmp = nullptr;
      }
    }
  }
  if (g_uiStyle == 3 && !g_distFree) {  // Zen: a menu button in the title row; the menus replace the title while it is on
    D2D1_RECT_F mb = zenMenuBtn();
    if (g_zenMenu || inRect(mb, g_mx, g_my)) fillR(mb, t.fg, g_zenMenu ? 0.11f : 0.08f, 6);
    g_br->SetColor(Rgb(g_zenMenu ? t.fg : t.gutterFg));
    for (float ly : {11.f, 17.f, 23.f}) g_rt->DrawLine(D2D1::Point2F(44, ly), D2D1::Point2F(58, ly), g_br, 1.5f);
  }
  for (size_t i = 0; i < g_menus.size() && !g_distFree; i++) {
    if (g_uiStyle == 3 && !g_zenMenu) break;
    auto& m = g_menus[i];
    if (g_uiStyle == 3) {
      if ((int)i == g_openMenu || (int)i == g_hoverMenu) fillR(rc(m.x, 4, m.w, 26), t.fg, 0.11f, 6);
      drawText(m.label.c_str(), (int)m.label.size(), m.x + 8, (TITLE_H - 18.f) / 2 - 1, t.chromeFg, g_ui, m.w, 20);
      continue;
    }
    if ((int)i == g_openMenu || (int)i == g_hoverMenu) fillRect(m.x, TITLE_H, m.w, MENU_H - TITLE_H, t.chromeHover);
    drawText(m.label.c_str(), (int)m.label.size(), m.x + 8, TITLE_H + (MENU_H - TITLE_H - 18.f) / 2, t.chromeFg, g_ui, m.w, 20);
  }

  /* status bar */
  fillRect(0, H - STATUS_H, W, STATUS_H, styStatus());
  if (g_uiStyle == 1 || g_uiStyle == 3) fillRect(0, H - STATUS_H, W, 1, t.fg, 0.1f);
  {
    float sy = H - STATUS_H;
    for (auto& it : statusItems(d)) {
      bool hv = inRect(it.r, g_mx, g_my);
      if (hv) fillR(it.r, t.fg, 0.08f, 4);
      DWORD sc = hv ? t.fg : t.statusText;
      if (it.id == 0) {  // sidebar toggle icon
        strokeR(rc(it.r.left + 6, sy + 7, 12, 10), sc, 0.9f, 2);
        fillRect(it.r.left + 6, sy + 8, 4, 8, sc, 0.9f);
      } else {
        drawText(it.text.c_str(), (int)it.text.size(), it.r.left + 7, sy + 3, sc, g_uiSmall, it.r.right - it.r.left, 20);
      }
    }
  }

  drawPanels();
  drawSidebar();
  drawFind();
  drawMdPreview();
  drawClaude();

  /* word completion list under the word being typed (with the documentation of the selected entry beside it) */
  if (g_comp.open && !g_palOpen && g_comp.line < (int)d.lines.size()) {
    ensureRows(d);
    int crow = rowOfPos(d, {g_comp.line, g_comp.col});
    float cx = textX() - d.scrollX + rowVcol(d, {g_comp.line, g_comp.col}) * (d.vis ? 1.f : g_charW), cy = edTop() + rowTop(d, crow) - d.scrollY + rowH(d, crow);
    int n = (int)g_comp.items.size(), vis = std::min(n, 8);
    float w = 160;
    IDWriteTextFormat* cf = monoFont(13.f);
    for (int i = 0; i < n && i < 60; i++) {
      const CompItem& ci = g_comp.items[i];
      w = std::max(w, textWidth(ci.label, cf) + (ci.detail.empty() ? 0.f : textWidth(ci.detail, g_uiSmall) + 20) + 30);
    }
    w = std::min(w, 440.f);
    float h = vis * 22 + 8;
    if (cy + h > ch() - STATUS_H) cy = cy - rowH(d, crow) - h;
    cx = std::min(cx, cw() - w - 8);
    fillR(D2D1::RectF(cx + 2, cy + 3, cx + w + 2, cy + h + 3), 0x000000, 0.25f, 8);
    fillR(rc(cx, cy, w, h), t.menu, 1.f, 8);
    strokeR(rc(cx, cy, w, h), t.border, 0.9f, 8);
    for (int i = 0; i < vis; i++) {
      int idx = g_comp.scroll + i;
      if (idx >= n) break;
      float ry = cy + 4 + i * 22;
      if (idx == g_comp.sel) fillR(rc(cx + 3, ry, w - 6, 22), t.menuHi, 1.f, 5);
      const CompItem& it = g_comp.items[idx];
      drawText(it.label.c_str(), (int)it.label.size(), cx + 12, ry + 2, t.chromeFg, cf, w - 20, 20);
      if (!it.detail.empty()) {
        float dw = textWidth(it.detail, g_uiSmall);
        drawText(it.detail.c_str(), (int)it.detail.size(), cx + w - dw - 14, ry + 3, t.gutterFg, g_uiSmall, dw + 4, 18);
      }
    }
    if (n > vis) {
      float th = std::max(14.f, (h - 8) * vis / n), ty = cy + 4 + (h - 8 - th) * (g_comp.scroll / (float)(n - vis));
      fillR(rc(cx + w - 6, ty, 3, th), t.gutterFg, 0.5f, 1.5f);
    }
    // the info panel of the selected entry: signature, documentation, where it comes from
    const CompItem& sel = g_comp.items[std::min(g_comp.sel, n - 1)];
    if (!sel.sig.empty() || !sel.doc.empty()) {
      float iw = 340;
      float ix = cx + w + 4;
      if (ix + iw > cw() - 6) ix = cx - iw - 4;
      IDWriteTextFormat* sf = monoFont(12.5f);
      float sh = wrappedHeight(sel.sig, sf, iw - 20, 17.f);
      float dh = sel.doc.empty() ? 0.f : wrappedHeight(sel.doc, uiFont(12.5f), iw - 20, 17.f) + 6;
      float srh = sel.src.empty() ? 0.f : 22.f;
      float ih = 10 + sh + dh + srh + 4;
      float iy = cy;
      fillR(D2D1::RectF(ix + 2, iy + 3, ix + iw + 2, iy + ih + 3), 0x000000, 0.25f, 8);
      fillR(rc(ix, iy, iw, ih), t.menu, 1.f, 8);
      strokeR(rc(ix, iy, iw, ih), t.border, 0.9f, 8);
      float yy = iy + 8;
      yy += drawWrapped(sel.sig, sf, ix + 10, yy, iw - 20, t.fg, 1.f, 17.f);
      if (!sel.doc.empty()) yy += 6 + drawWrapped(sel.doc, uiFont(12.5f), ix + 10, yy + 6, iw - 20, t.fg, 0.85f, 17.f);
      if (!sel.src.empty()) drawText(sel.src.c_str(), (int)sel.src.size(), ix + 10, yy + 8, mixc(t.menu, t.fg, 0.5f), uiFont(11.5f), iw - 20, 16);
    }
  }
  drawParamHint(d);
  drawTooltip(d);

  /* open drop-down */
  if (g_openMenu >= 0) {
    auto& m = g_menus[g_openMenu];
    float w, hh;
    menuDropRect(g_openMenu, &w, &hh);
    fillRect(m.x - 1, MENU_H - 1, w + 2, hh + 2, t.border);
    fillRect(m.x, MENU_H, w, hh, t.menu);
    float y = MENU_H + 4;
    for (size_t i = 0; i < m.items.size(); i++) {
      auto& it = m.items[i];
      if (it.label.empty()) { fillRect(m.x + 8, y + 4, w - 16, 1, t.border, 0.8f); y += 9; continue; }
      bool off = !cmdAvailable(it.cmd);
      if ((int)i == g_hoverItem && !off) fillRect(m.x + 3, y, w - 6, 26, t.menuHi);
      if (!off && cmdChecked(it.cmd)) {  // check mark of a toggle that is on
        g_br->SetColor(Rgb(t.chromeFg));
        g_rt->DrawLine(D2D1::Point2F(m.x + 7, y + 13), D2D1::Point2F(m.x + 10, y + 16.5f), g_br, 1.5f);
        g_rt->DrawLine(D2D1::Point2F(m.x + 10, y + 16.5f), D2D1::Point2F(m.x + 15.5f, y + 9.5f), g_br, 1.5f);
      }
      drawText(it.label.c_str(), (int)it.label.size(), m.x + 22, y + 4, off ? mixc(t.menu, t.chromeFg, 0.38f) : t.chromeFg, g_ui, w, 22);
      wstring ac = effectiveKeys(it.cmd);
      if (!ac.empty()) {
        float aw = textWidth(ac, g_ui);
        drawText(ac.c_str(), (int)ac.size(), m.x + w - aw - 16, y + 4, off ? mixc(t.menu, t.gutterFg, 0.5f) : t.gutterFg, g_ui, aw + 4, 22);
      }
      y += 26;
    }
  }
  drawSettingsPage();
  drawHudEditor();
  drawPalette();
  drawTabDrop();
  drawChromeTip();
  drawLicense();
  drawModal();
  HRESULT hr = g_rt->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET && g_rt == g_hrt) {
    g_br->Release(); g_br = nullptr;
    g_hrt->Release(); g_hrt = nullptr; g_rt = nullptr;
  }
}

static void ensureTarget() {
  if (g_rt) return;
  RECT rc;
  GetClientRect(g_hwnd, &rc);
  g_d2d->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
                                D2D1::HwndRenderTargetProperties(g_hwnd, D2D1::SizeU(rc.right, rc.bottom)), &g_hrt);
  g_rt = g_hrt;
  g_rt->CreateSolidColorBrush(D2D1::ColorF(0), &g_br);
  g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  UINT dpi = GetDpiForWindow(g_hwnd);
  g_dpiScale = 96.f / (float)dpi;
}

/* ---------------------------------------------------------------- input */
static void mapScroll(float my) {
  Doc& d = D();
  float h = edH(), total = d.lines.size() * MAP_LINE, maxS = maxScrollY();
  float off = std::max(0.f, total - h) * (maxS > 0 ? d.scrollY / maxS : 0);
  float line = (my - edTop() + off) / MAP_LINE;
  int ln = std::max(0, std::min((int)d.lines.size() - 1, (int)line));
  ensureRows(d);
  d.scrollY = std::max(0.f, std::min(maxS, rowTop(d, d.rowStart[ln]) - h / 2));
  redraw();
}

#include "ctxmenu.inc"
#include "chrometip.inc"
#include "input.inc"

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
  switch (m) {
    case WM_NCCALCSIZE:
      if (wp) {
        // no native frame: the client area is the whole window (shrunk by the sizing border when maximised)
        if (IsZoomed(h)) {
          auto* np = (NCCALCSIZE_PARAMS*)lp;
          int fx = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
          int fy = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
          np->rgrc[0].left += fx;
          np->rgrc[0].right -= fx;
          np->rgrc[0].top += fy;
          np->rgrc[0].bottom -= fy;
        }
        return 0;
      }
      break;
    case WM_NCHITTEST: {
      POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      ScreenToClient(h, &pt);
      RECT crc;
      GetClientRect(h, &crc);
      float dpi = (float)GetDpiForWindow(h) / 96.f;
      int bw = (int)(6 * dpi);
      if (!IsZoomed(h)) {
        bool L = pt.x < bw, R = pt.x >= crc.right - bw, T = pt.y < bw, B = pt.y >= crc.bottom - bw;
        if (T && L) return HTTOPLEFT;
        if (T && R) return HTTOPRIGHT;
        if (B && L) return HTBOTTOMLEFT;
        if (B && R) return HTBOTTOMRIGHT;
        if (L) return HTLEFT;
        if (R) return HTRIGHT;
        if (T) return HTTOP;
        if (B) return HTBOTTOM;
      }
      float x = pt.x * g_dpiScale, y = pt.y * g_dpiScale, W = (float)crc.right * g_dpiScale;
      if (y < TITLE_H) {
        if (g_uiStyle == 3 && !g_distFree) {
          D2D1_RECT_F mb = zenMenuBtn();
          if (x >= mb.left && x < mb.right && y >= mb.top && y < mb.bottom) return HTCLIENT;
          if (g_zenMenu && !g_menus.empty() && x >= g_menus.front().x && x < g_menus.back().x + g_menus.back().w) return HTCLIENT;
        }
        if (x >= W - CAP_W) return HTCLOSE;
        if (x >= W - 2 * CAP_W) return HTMAXBUTTON;
        if (x >= W - 3 * CAP_W) return HTMINBUTTON;
        return HTCAPTION;
      }
      return HTCLIENT;
    }
    case WM_NCMOUSEMOVE: {
      int hv = wp == HTMINBUTTON ? 1 : wp == HTMAXBUTTON ? 2 : wp == HTCLOSE ? 3 : 0;
      if (hv != g_hoverCaption) { g_hoverCaption = hv; redraw(); }
      TRACKMOUSEEVENT te{sizeof(te), TME_LEAVE | TME_NONCLIENT, h, 0};
      TrackMouseEvent(&te);
      break;
    }
    case WM_NCMOUSELEAVE:
      if (g_hoverCaption) { g_hoverCaption = 0; redraw(); }
      break;
    case WM_NCLBUTTONDOWN:
      // the caption buttons are drawn by us: act on press (the system would only highlight its own)
      if (wp == HTMINBUTTON) { ShowWindow(h, SW_MINIMIZE); return 0; }
      if (wp == HTMAXBUTTON) { ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE); return 0; }
      if (wp == HTCLOSE) { PostMessageW(h, WM_CLOSE, 0, 0); return 0; }
      break;
    case WM_NCLBUTTONUP:
      if (wp == HTMINBUTTON || wp == HTMAXBUTTON || wp == HTCLOSE) return 0;
      break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(h, &ps);
      ensureTarget();
      if (g_rt) paint();
      EndPaint(h, &ps);
      return 0;
    }
    case WM_SIZE:
      if (g_hrt) { g_hrt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp))); }
      if (!g_docs.empty()) ensureVisible();
      redraw();
      return 0;
    case WM_TIMER:
      if (wp == 1) { g_caretOn = !g_caretOn; redraw(); }
      else if (wp == 2) { flushNotes(); saveSession(); }
      else if (wp == 3) flushNotes();
      else if (wp == 4) { KillTimer(h, 4); g_statusMsg.clear(); redraw(); }
      else if (wp == 5) settingsTick(true);
      else if (wp == 6) { settingsTick(false); fileWatchTick(); }
      else if (wp == 7) { gitTick(); amxxTick(); lspTick(); }
      else if (wp == 10) hoverTick();
      else if (wp == 11) chromeTipTimer();
      else if (wp == 12) ccTimer();
      else if (wp == 13) hudTick();
      else if (wp == 14) licTick();
      else if (wp == 8) gitStatusRefresh();
      else if (wp == 9) testTick();
      return 0;
    case WM_ACTIVATEAPP:
      if (wp) { checkExternalChanges(); gitStatusRefresh(); }
      else if (g_autoSave == 2) for (auto* dd : g_docs) autoSaveDoc(*dd);  // saved when the window loses the focus
      break;
    case WM_SETFOCUS: case WM_KILLFOCUS: redraw(); return 0;
    case WM_DROPFILES: {
      HDROP dr = (HDROP)wp;
      UINT n = DragQueryFileW(dr, 0xFFFFFFFF, nullptr, 0);
      for (UINT i = 0; i < n; i++) {
        wchar_t p[MAX_PATH * 2];
        DragQueryFileW(dr, i, p, MAX_PATH * 2);
        openPath(p);
      }
      DragFinish(dr);
      return 0;
    }
    case WM_MOUSEWHEEL: {
      // scrolling goes to the group under the mouse, without moving the focus
      struct Restore { int a; VP v; ~Restore() { g_active = a; g_vp = v; } } restoreGuard{g_active, g_vp};
      {
        POINT p0{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &p0);
        int gi = paneAt(p0.x * g_dpiScale, p0.y * g_dpiScale);
        if (gi >= 0 && gi != g_curGroup && g_groupActive[gi] >= 0 && g_groupActive[gi] < (int)g_docs.size()) {
          g_active = g_groupActive[gi];
          g_vp = g_paneRect[gi];
        }
      }
      Doc& d = D();
      int delta = GET_WHEEL_DELTA_WPARAM(wp);
      if (g_licOpen) return 0;
      if (g_hudOpen && !g_palOpen) { hudWheel(delta, g_mx, g_my); return 0; }
      if (g_suOpen && !g_palOpen) { suWheel(delta); return 0; }
      tipHide();
      {
        POINT pp{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &pp);
        if (!g_palOpen && g_ccOpen && pp.x * g_dpiScale >= ccLeft() && ccWheel(delta)) return 0;
        if (!g_palOpen && pvWheel(pp.x * g_dpiScale, pp.y * g_dpiScale, delta)) return 0;
        if (!g_palOpen && panelsMouse(pp.x * g_dpiScale, pp.y * g_dpiScale, 3, delta)) return 0;
      }
      {
        POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &pt);
        float mxp = pt.x * g_dpiScale;
        if (g_palOpen) {
          int n = (int)g_pitems.size();
          g_pscroll = std::max(0, std::min(std::max(0, n - PAL_MAX), g_pscroll - (delta > 0 ? 3 : -3)));
          redraw();
          return 0;
        }
        if (g_sbVisible && mxp < g_sbW) {
          if (g_sbView == 1) {
            notesLayout();
            g_nbScroll = std::max(0.f, std::min(notesMaxScroll(), g_nbScroll - delta / 120.f * 120.f));
          } else if (g_sbView == 2) {
            g_olScroll = std::max(0.f, g_olScroll - delta / 120.f * 72.f);  // clamped when drawn
          } else {
            g_sbScroll = std::max(0.f, std::min(std::max(0.f, sbContentH() - (ch() - MENU_H - STATUS_H - SB_HEAD)), g_sbScroll - delta / 120.f * 3 * SB_ROW));
          }
          redraw();
          return 0;
        }
      }
      if (GetKeyState(VK_CONTROL) & 0x8000) {
        g_fontSize = std::max(8.f, std::min(48.f, g_fontSize + (delta > 0 ? 1 : -1)));
        makeFonts();
      } else if (GetKeyState(VK_SHIFT) & 0x8000) {
        d.scrollX = std::max(0.f, d.scrollX - delta / 120.f * 60);
      } else {
        d.scrollY -= delta / 120.f * 3 * g_lineH;
        d.scrollY = std::max(0.f, std::min(d.scrollY, maxScrollY()));
      }
      redraw();
      return 0;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: {
      SetFocus(h);
      float x = GET_X_LPARAM(lp) * g_dpiScale, y = GET_Y_LPARAM(lp) * g_dpiScale;
      if (g_modal.open) { modalMouse(x, y); return 0; }
      if (g_palOpen && palMouse(x, y)) return 0;
      if (g_licOpen && y >= TITLE_H) { licMouse(x, y, 0); return 0; }
      if (g_hudOpen && y >= TITLE_H) { hudMouse(x, y, 0); return 0; }
      if (g_suOpen && y >= TITLE_H) { suMouse(x, y, 0); return 0; }
      tipHide();
      chromeTipHide();
      if (g_openMenu < 0 && y >= MENU_H) {
        if (panelsMouse(x, y, 0, 0)) return 0;
        g_panelFocus = 0;
      }
      if (g_uiStyle == 3 && y < TITLE_H && inRect(zenMenuBtn(), x, y)) { g_zenMenu = !g_zenMenu; g_openMenu = -1; layoutMenus(); redraw(); return 0; }
      // open menu: pick an item or close
      if (g_openMenu >= 0) {
        auto& mm = g_menus[g_openMenu];
        float w, hh;
        menuDropRect(g_openMenu, &w, &hh);
        int chosen = 0;
        if (x >= mm.x && x < mm.x + w && y >= MENU_H && y < MENU_H + hh) {
          float yy = MENU_H + 4;
          for (auto& it : mm.items) {
            float ih = !it.label.empty() ? 26.f : 9.f;
            if (y >= yy && y < yy + ih) { chosen = (it.label.empty() || !cmdAvailable(it.cmd)) ? 0 : it.cmd; break; }
            yy += ih;
          }
        }
        g_openMenu = -1;
        g_hoverItem = -1;
        if (g_uiStyle == 3 && !inMenuRow(y)) g_zenMenu = false;
        redraw();
        if (chosen) exec(chosen);
        if (inMenuRow(y)) {
          // fallthrough lets a click on another menu label open it
        } else return 0;
      }
      if (y < MENU_H) {
        if (inMenuRow(y))
          for (size_t i = 0; i < g_menus.size(); i++)
            if (x >= g_menus[i].x && x < g_menus[i].x + g_menus[i].w) { g_openMenu = (int)i; redraw(); return 0; }
        if (g_uiStyle == 3 && g_zenMenu) { g_zenMenu = false; redraw(); }
        return 0;
      }
      if (ccMouseDown(x, y)) return 0;
      if (pvMouse(x, y)) return 0;
      if (sidebarMouse(x, y, 0)) return 0;
      computeLayout();
      if (paneCount() > 1) {
        bool onDiv = g_layout == 1 ? (std::fabs(x - g_paneRect[0].right) <= 3 && y >= MENU_H && y < ch() - STATUS_H)
                                   : (std::fabs(y - g_paneRect[0].bottom) <= 3 && x >= g_paneRect[0].left);
        if (onDiv) { g_dragSplit = true; SetCapture(h); return 0; }
        int gi = paneAt(x, y);
        if (gi >= 0 && gi != g_curGroup) focusGroup(gi);
      }
      for (int gi = 0; gi < paneCount(); gi++) {
        const VP& p = g_paneRect[gi];
        if (y < p.top || y >= p.top + TAB_H || x < p.left || x >= p.right) continue;
        if (x < p.left + 48) { focusGroup(gi); exec(x < p.left + 24 ? C_PREVTAB : C_NEXTTAB); return 0; }
        if (x >= p.right - 52 && x < p.right - 26) { focusGroup(gi); exec(C_NEW); return 0; }
        if (x >= p.right - 26) { focusGroup(gi); showTabList(gi); return 0; }
      }
      {
        int ti = -1, tg = -1;
        bool onX = false;
        float tw0 = 0;
        for (int gi = 0; gi < paneCount() && ti < 0; gi++) {
          VP saved = g_vp;
          g_vp = g_paneRect[gi];
          for (auto& r : tabRects(gi)) {
            if (y >= g_vp.top && y < g_vp.top + TAB_H && x >= r.x && x < r.x + r.w) { ti = r.idx; tg = gi; onX = x > r.x + r.w - 28; tw0 = r.w; break; }
          }
          g_vp = saved;
        }
        (void)tw0;
        if (ti >= 0) {
          if (onX) closeDoc(ti);
          else { g_curGroup = tg; activateDoc(ti); tabDragStart(g_docs[ti], x, y); }
          return 0;
        }
        if (y < g_vp.top + TAB_H) return 0;
      }
      if (fmtBarMouse(x, y)) return 0;
      if (y >= ch() - STATUS_H) { statusClick(x, y); return 0; }
      if (findMouse(x, y)) return 0;
      g_findFocus = false;
      compClose();
      if (x >= mapX() - 12 && x < mapX() && y >= edTop() && y < edTop() + edH()) {
        float ty, th;
        if (vsGeom(D(), ty, th)) {
          g_vsOff = (y >= ty && y < ty + th) ? y - ty : th / 2;
          g_dragVs = true;
          SetCapture(h);
          vsDrag(y);
          return 0;
        }
      }
      if (mapShown() && x >= mapX()) { g_dragMap = true; SetCapture(h); mapScroll(y); return 0; }
      if (x >= L0() && x < L0() + gutterW() && y >= edTop() && y < edTop() + edH()) {
        // the gutter: the fold chevron toggles a block, anywhere else selects the line
        Doc& gd = D();
        int gl = hit(L0() + gutterW() + 8, y).l;
        int fe;
        if (x > L0() + gutterW() - 20 && (gd.folded.count(gl) || foldRangeOf(gd, gl, fe))) { foldToggleAt(gl); return 0; }
        gd.extra.clear();
        gd.anc = {gl, 0};
        gd.cur = gl + 1 < (int)gd.lines.size() ? Pos{gl + 1, 0} : Pos{gl, (int)gd.lines[gl].size()};
        moved(true);
        return 0;
      }
      if (wdMouseDown(x, y)) return 0;
      Doc& d = D();
      Pos p = hit(x, y);
      if (d.vis && !g_vSyms && taskBoxClick(d, p, x)) return 0;
      DWORD now = GetTickCount();
      bool shift = GetKeyState(VK_SHIFT) & 0x8000;
      if (m == WM_LBUTTONDBLCLK) {
        const wstring& s = d.lines[p.l];
        int a = p.c, b = p.c;
        while (a > 0 && isWordCh(s[a - 1])) a--;
        while (b < (int)s.size() && isWordCh(s[b])) b++;
        if (a == b && b < (int)s.size()) b++;
        d.anc = {p.l, a};
        d.cur = {p.l, b};
        g_lastDbl = now;
        moved(true);
        return 0;
      }
      if (now - g_lastDbl < 500) {  // triple click: whole line
        d.anc = {p.l, 0};
        d.cur = p.l + 1 < (int)d.lines.size() ? Pos{p.l + 1, 0} : Pos{p.l, (int)d.lines[p.l].size()};
        g_lastDbl = 0;
        moved(true);
        return 0;
      }
      if (GetKeyState(VK_CONTROL) & 0x8000) {
        d.extra.push_back({d.cur, d.anc});
        d.cur = d.anc = p;
        moved(false);
        return 0;
      }
      d.extra.clear();
      d.cur = p;
      moved(shift);
      g_dragSel = true;
      SetCapture(h);
      return 0;
    }
    case WM_MOUSELEAVE:
      if (!g_testOut.empty()) return 0;  // the scripted test moves a pointer that is not really over the window
      g_mx = g_my = -1;
      chromeTipHide();
      redraw();
      return 0;
    case WM_MOUSEMOVE: {
      float x = GET_X_LPARAM(lp) * g_dpiScale, y = GET_Y_LPARAM(lp) * g_dpiScale;
      if (g_licOpen) {
        g_mx = x;
        g_my = y;
        redraw();
        SetCursor(LoadCursor(nullptr, y >= TITLE_H ? IDC_ARROW : IDC_ARROW));
        return 0;
      }
      if (g_hudOpen) {
        g_mx = x;
        g_my = y;
        TRACKMOUSEEVENT te{sizeof(te), TME_LEAVE, h, 0};
        TrackMouseEvent(&te);
        hudMove(x, y);
        SetCursor(LoadCursor(nullptr, hudCursor(x, y)));
        return 0;
      }
      if (g_suOpen) {
        g_mx = x;
        g_my = y;
        TRACKMOUSEEVENT te{sizeof(te), TME_LEAVE, h, 0};
        TrackMouseEvent(&te);
        suMove(x, y);
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return 0;
      }
      chromeTipMove(x, y);
      {
        // hover feedback on buttons: repaint when the pointer moves over chrome (not over the text itself)
        bool overChrome = y >= ch() - STATUS_H || x < g_sbW * (g_sbVisible ? 1 : 0) || y < edTop() || x >= mapX() - 12 || g_palOpen || g_findOpen ||
                          (x >= L0() && x < L0() + gutterW());
        g_mx = x;
        g_my = y;
        TRACKMOUSEEVENT te{sizeof(te), TME_LEAVE, h, 0};
        TrackMouseEvent(&te);
        if (overChrome) redraw();
      }
      if (ccMouseMove(x, y)) return 0;
      if (g_wdDrag) { SetCursor(LoadCursor(nullptr, IDC_SIZENWSE)); wdDragMove(x); return 0; }
      if (g_wdHandleLine >= 0 && inRect(g_wdHandle, x, y)) { SetCursor(LoadCursor(nullptr, IDC_SIZENWSE)); redraw(); return 0; }
      for (auto& wb : g_wdBtns) if (inRect(wb.r, x, y)) { SetCursor(LoadCursor(nullptr, IDC_HAND)); redraw(); return 0; }
      if (g_dragDoc) { tabDragMove(x, y); return 0; }
      if (g_dragSb) { g_sbW = std::max(160.f, std::min(600.f, x)); redraw(); return 0; }
      if (panelsMouse(x, y, 1, 0)) { SetCursor(LoadCursor(nullptr, IDC_SIZENS)); return 0; }
      if (g_fifOpen || g_runOpen) {
        // over the top edge of a panel: the resize cursor; any move repaints the hover of the panel buttons
        for (auto& ph : g_pHits) if ((ph.kind == P_RESIZE_FIF || ph.kind == P_RESIZE_RUN) && inRect(ph.r, x, y)) { SetCursor(LoadCursor(nullptr, IDC_SIZENS)); return 0; }
        D2D1_RECT_F pa = D2D1::RectF(panelsLeft(), ch() - STATUS_H - panelsHeight(), cw(), ch() - STATUS_H);
        if (inRect(pa, x, y)) { redraw(); SetCursor(LoadCursor(nullptr, IDC_ARROW)); return 0; }
      }
      if (g_dragSplit) {
        float lo = g_sbVisible ? g_sbW : 0.f;
        if (g_layout == 1) g_splitRatio = std::max(0.15f, std::min(0.85f, (x - lo) / std::max(1.f, cw() - lo)));
        else g_splitRatio = std::max(0.15f, std::min(0.85f, (y - MENU_H) / std::max(1.f, ch() - STATUS_H - MENU_H)));
        computeLayout();
        redraw();
        return 0;
      }
      if (g_dragVs) { vsDrag(y); return 0; }
      if (g_dragMap) { mapScroll(y); return 0; }
      if (g_sbVisible) {
        int hv = -1;
        if (x < g_sbW && y >= MENU_H + SB_HEAD && y < ch() - STATUS_H) hv = (int)std::floor((y - MENU_H - SB_HEAD + g_sbScroll) / SB_ROW);
        if (hv != g_sbHover) { g_sbHover = hv; redraw(); }
      }
      if (g_dragSel) {
        Doc& d = D();
        if (y < edTop()) d.scrollY = std::max(0.f, d.scrollY - 12);
        else if (y > edTop() + edH()) d.scrollY = std::min(maxScrollY(), d.scrollY + 12);
        d.cur = hit(x, y);
        d.lastKind = 0;
        d.wantV = rowVcol(d, d.cur);
        redraw();
        return 0;
      }
      int hm = -1, hi = -1, ht = -1;
      if (y < MENU_H) {
        if (inMenuRow(y))
          for (size_t i = 0; i < g_menus.size(); i++)
            if (x >= g_menus[i].x && x < g_menus[i].x + g_menus[i].w) hm = (int)i;
        if (g_openMenu >= 0 && hm >= 0) g_openMenu = hm;
      } else if (g_openMenu >= 0) {
        auto& mm = g_menus[g_openMenu];
        float w, hh;
        menuDropRect(g_openMenu, &w, &hh);
        if (x >= mm.x && x < mm.x + w && y >= MENU_H) {
          float yy = MENU_H + 4;
          for (size_t i = 0; i < mm.items.size(); i++) {
            float ih = !mm.items[i].label.empty() ? 26.f : 9.f;
            if (y >= yy && y < yy + ih && !mm.items[i].label.empty()) hi = (int)i;
            yy += ih;
          }
        }
      } else {
        ht = tabAt(x, y);
      }
      if (g_sbVisible && g_sbView == 1) {
        int nh = -1;
        if (x < g_sbW && y >= notesListTop() && y < ch() - STATUS_H) nh = notesRowAt(y);
        if (nh >= 0 && g_nrows[nh].head) nh = -1;
        if (nh != g_noteHover) { g_noteHover = nh; redraw(); }
        if (x < g_sbW) redraw();  // the hover of the tag chips and header buttons
      }
      if (hm != g_hoverMenu || hi != g_hoverItem || ht != g_hoverTab) {
        g_hoverMenu = hm; g_hoverItem = hi; g_hoverTab = ht;
        redraw();
      }
      bool overText = y >= edTop() && y < ch() - STATUS_H && x > L0() + gutterW() && x < mapX();
      hoverMove(x, y, overText && !g_dragSel);
      bool onEdge = g_sbVisible && std::fabs(x - g_sbW) < 4 && y >= MENU_H && y < ch() - STATUS_H;
      SetCursor(LoadCursor(nullptr, onEdge ? IDC_SIZEWE : (overText && g_openMenu < 0 && !g_palOpen) ? IDC_IBEAM : IDC_ARROW));
      return 0;
    }
    case WM_LBUTTONUP:
      if (g_hudOpen) hudMouse(0, 0, 1);
      if (g_suOpen) suMouse(0, 0, 1);
      panelsMouse(0, 0, 2, 0);
      ccMouseUp();
      if (g_dragDoc) tabDragEnd();
      g_dragSel = g_dragMap = g_dragSb = g_dragSplit = g_dragVs = false;
      g_wdDrag = false;
      ReleaseCapture();
      return 0;
    case WM_RBUTTONDOWN: {
      SetFocus(h);
      contextMenuAt(GET_X_LPARAM(lp) * g_dpiScale, GET_Y_LPARAM(lp) * g_dpiScale, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
      return 0;
    }
    case WM_MBUTTONDOWN: {
      float x = GET_X_LPARAM(lp) * g_dpiScale, y = GET_Y_LPARAM(lp) * g_dpiScale;
      int ti = tabAt(x, y);
      if (ti >= 0) closeDoc(ti);
      return 0;
    }
    case WM_SETCURSOR:
      if (LOWORD(lp) == HTCLIENT) return TRUE;
      break;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
      if (wp == VK_CONTROL || wp == VK_SHIFT || wp == VK_MENU) break;
      if (onKey(wp, (GetKeyState(VK_CONTROL) & 0x8000) != 0, (GetKeyState(VK_SHIFT) & 0x8000) != 0, (GetKeyState(VK_MENU) & 0x8000) != 0, m == WM_SYSKEYDOWN)) return 0;
      break;
    case WM_CHAR:
      if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) return 0;
      onChar((wchar_t)wp);
      return 0;
    case WM_LSP_MSG: lspMessage((LspMsg*)lp); return 0;
    case WM_LSP_EXIT: lspExited((wstring*)lp); return 0;
    case WM_AMX_DOC: amxxDone((AmxResult*)lp); return 0;
    case WM_AMX_BUILD: amxBuildDone((AmxBuildResult*)lp); return 0;
    case WM_LICENSE: licDone((LicResult*)lp); return 0;
    case WM_GIT_DOC: gitDocDone((GitDocResult*)lp); return 0;
    case WM_GIT_STATUS: gitStatusDone((GitStatusResult*)lp); return 0;
    case WM_FIF_DONE: fifDone((FifResult*)lp); return 0;
    case WM_RUN_DATA: runData((RunChunk*)lp); return 0;
    case WM_RUN_EXIT: runExit((int)wp, (DWORD)lp); return 0;
    case WM_CLAUDE_EV: { std::string* s = (std::string*)lp; ccHandleEvent((int)wp, *s); delete s; return 0; }
    case WM_CLAUDE_AUTH: { ClAuthMsg* a = (ClAuthMsg*)lp; ccAuthMessage(a); delete a; return 0; }
    case WM_CLAUDE_AUTHST: {
      ClAuthSt* a = (ClAuthSt*)lp;
      json j = json::parse(a->out, nullptr, false);
      if (!j.is_object()) j = {{"error", a->out.empty() ? "No answer" : "Unexpected answer"}};
      auto it = g_clAuthCbs.find(a->token);
      if (it != g_clAuthCbs.end()) { auto cb = it->second; g_clAuthCbs.erase(it); cb(j); }
      delete a;
      return 0;
    }
    case WM_CLOSE:
      if (g_modal.open) { modalFinish(g_modal.cancelId); return 0; }
      // hot exit: unsaved text stays in the session and comes back on the next start
      flushNotes();
      if (g_settingsDirty) saveSettingsFileNow();
      saveSession();
      ccShutdown();
      DestroyWindow(h);
      return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(h, m, wp, lp);
}

// a crash writes its address (relative to the program, to look up in Scriba.map) next to the data folder
static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
  wstring path = dataDir() + L"\\crash.txt";
  HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    char buf[2048];
    ULONG_PTR base = (ULONG_PTR)GetModuleHandleW(nullptr);
    int n = snprintf(buf, sizeof(buf), "crash code %08lx at rva %llx\n", ep->ExceptionRecord->ExceptionCode, (unsigned long long)((ULONG_PTR)ep->ExceptionRecord->ExceptionAddress - base));
    void* frames[32];
    USHORT cnt = CaptureStackBackTrace(0, 32, frames, nullptr);
    for (USHORT i = 0; i < cnt && n < 1900; i++) n += snprintf(buf + n, sizeof(buf) - n, "  frame %u rva %llx\n", i, (unsigned long long)((ULONG_PTR)frames[i] - base));
    DWORD wr;
    WriteFile(h, buf, n, &wr, nullptr);
    CloseHandle(h);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  SetUnhandledExceptionFilter(crashHandler);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  claimInstance();
  D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d);
  DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw);
  g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.f, L"en-us", &g_ui);
  g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us", &g_uiSmall);
  g_dw->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.f, L"en-us", &g_uiBold);
  g_uiBold->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  g_ui->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  g_uiSmall->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  makeFonts();
  buildMenus();
  rebuildKeymap();
  layoutMenus();

  WNDCLASSEXW wc{sizeof(wc)};
  wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  wc.hIconSm = wc.hIcon;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"ScribaCppWnd";
  RegisterClassExW(&wc);
  g_hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, L"ScribaCppWnd", L"Scriba", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1184, 800, nullptr, nullptr, inst, nullptr);
  {
    // keep the drop shadow of the frameless window
    MARGINS mg{0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(g_hwnd, &mg);
    SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
  }

  (void)0;
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  loadSettingsFile();
  g_applySettings = applySettingsNow;
  g_docs.push_back(new Doc());
  bool fresh = wcsstr(GetCommandLineW(), L"--fresh") != nullptr;
  bool restored = !fresh && restoreSession();
  (void)restored;
  applySettingsNow(false);
  SetTimer(g_hwnd, 6, 1000, nullptr);
  SetTimer(g_hwnd, 7, 250, nullptr);
  SetTimer(g_hwnd, 10, 100, nullptr);
  SetTimer(g_hwnd, 12, 120, nullptr);
  SetTimer(g_hwnd, 14, 60000, nullptr);
  ccLoadStore();
  g_compExtra = [](Doc& dd, const wstring& pre, vector<CompItem>& out) { amxxCompletion(dd, pre, out); lspCompletionExtra(dd, pre, out); };
  SetTimer(g_hwnd, 8, 6000, nullptr);
  SetTimer(g_hwnd, 2, 4000, nullptr);
  SetTimer(g_hwnd, 3, 700, nullptr);
  DragAcceptFiles(g_hwnd, TRUE);
  for (int i = 1; i < argc; i++) {
    if (wcsncmp(argv[i], L"--", 2) == 0) continue;
    openPath(argv[i]);
  }
  LocalFree(argv);
  updateTitle();
  if (GetEnvironmentVariableW(L"SCRIBA_TEST", nullptr, 0)) {
    // scripted test run: a window of a fixed size far outside the screen, without taking the focus from the user
    g_testing = true;
    SetWindowPos(g_hwnd, HWND_BOTTOM, -3000, 40, 1300, 840, SWP_NOACTIVATE | SWP_SHOWWINDOW);
  } else ShowWindow(g_hwnd, show);
  caretReset();
  licInit();
  if (g_ccOpen && !g_testing) { g_ccOpen = false; ccShow(); }
  g_ccOpen = g_ccOpen && !g_testing;
  testStart();
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}
