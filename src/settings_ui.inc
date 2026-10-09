// settings_ui.inc: the Settings and Key Bindings page (a full page with the categories on the left), like the Electron edition.
// Immediate-mode drawing: every frame rebuilds the list of clickable rectangles (g_suHits). Included by main.cpp.

/* ---------------------------------------------------------------- what can be set */
enum { SU_BOOL, SU_NUM, SU_SELECT, SU_COLOR, SU_TEXT, SU_LIST, SU_NUMLIST };
struct SuItem {
  const char* key;
  const wchar_t* label;
  const wchar_t* desc;
  int type;
  vector<const wchar_t*> opts;
  int mn = 0, mx = 0;
  const wchar_t* ph = L"";
  bool fonts = false;
};
struct SuSection { const wchar_t* name; vector<SuItem> items; };
static vector<SuSection> g_suSpec;
static void suInitSpec() {
  if (!g_suSpec.empty()) return;
  vector<const wchar_t*> schemes;
  for (auto& t : THEMES) schemes.push_back(t.name);
  g_suSpec = {
      {L"Appearance", {
          {"color_scheme", L"Color scheme", L"Syntax colours and window theme.", SU_SELECT, schemes},
          {"ui_style", L"Interface style", L"Classic is Sublime Text. Refined keeps its layout but flatter and cleaner. Modern uses floating cards and pill tabs. Zen hides the clutter: press Alt for the menu.", SU_SELECT, {L"Classic", L"Refined", L"Modern", L"Zen"}},
          {"accent_color", L"Accent colour", L"Cursor, highlights and bookmarks. Reset to use the colour of the scheme.", SU_COLOR, {}},
          {"font_face", L"Font", L"Editor font family. Any installed monospace font works.", SU_TEXT, {L"Consolas", L"Cascadia Mono", L"Cascadia Code", L"Courier New", L"Lucida Console", L"Fira Code", L"JetBrains Mono", L"Source Code Pro", L"Menlo", L"Monaco"}, 0, 0, L"", true},
          {"font_size", L"Font size", L"In pixels. Ctrl+= / Ctrl+- change it temporarily for one window.", SU_NUM, {}, 7, 48},
          {"show_sidebar", L"Show side bar", L"The folders panel on the left.", SU_BOOL, {}},
          {"show_minimap", L"Show minimap", L"Overview of the file on the right.", SU_BOOL, {}},
      }},
      {L"Editor", {
          {"tab_size", L"Tab size", L"Width of a tab character, also used for new indentation.", SU_NUM, {}, 1, 16},
          {"translate_tabs_to_spaces", L"Indent using spaces", L"Pressing Tab inserts spaces instead of a tab character.", SU_BOOL, {}},
          {"detect_indentation", L"Detect indentation", L"Guess tabs/spaces and width from each file when it opens.", SU_BOOL, {}},
          {"word_wrap", L"Word wrap", L"Wrap long lines at the window edge.", SU_BOOL, {}},
          {"gutter", L"Show gutter", L"Line numbers, folding arrows and bookmarks.", SU_BOOL, {}},
          {"line_numbers", L"Line numbers", L"Needs the gutter.", SU_BOOL, {}},
          {"highlight_line", L"Highlight current line", L"", SU_BOOL, {}},
          {"draw_white_space", L"Show white space", L"Draw dots for spaces and lines for tabs.", SU_SELECT, {L"none", L"selection", L"all"}},
          {"rulers", L"Rulers", L"Vertical guide lines at these columns. Also sets the width for Wrap Paragraph (Alt+Q).", SU_NUMLIST, {}, 0, 0, L"80, 100"},
          {"scroll_past_end", L"Scroll past end", L"Allow scrolling the last line up to the top of the window.", SU_BOOL, {}},
          {"auto_complete", L"Suggest words while typing", L"Pop up a list of words from your open files as you type. Ctrl+Space shows it on demand either way.", SU_BOOL, {}},
          {"lsp_enabled", L"Language servers", L"Use an installed language server (typescript-language-server, pyright, clangd, rust-analyzer, gopls...) for errors, completion, hover help, go to definition, rename and format. Nothing starts if none is installed. Own commands: \"lsp_servers\" in settings.json, e.g. { \"Python\": { \"command\": \"pylsp\" } }.", SU_BOOL, {}},
          {"indent_guides", L"Indentation guides", L"Thin vertical lines at each indentation level in code files.", SU_BOOL, {}},
          {"bracket_colors", L"Colour bracket pairs", L"Nested (), [] and {} get different colours so matching pairs are easy to see.", SU_BOOL, {}},
          {"auto_close_brackets", L"Auto-close brackets and quotes", L"", SU_BOOL, {}},
          {"match_brackets", L"Highlight matching brackets", L"", SU_BOOL, {}},
      }},
      {L"Notes", {
          {"notes_visual", L"Visual editor for notes", L"Show notes formatted (big headings, bold, tick boxes, pictures) instead of Markdown symbols. Toggle per note with Ctrl+Alt+E. Applies to notes opened from now on.", SU_BOOL, {}},
          {"spell_check", L"Check spelling in notes", L"Underline misspelled words in notes in red; right-click a word for suggestions. Uses the dictionaries of the operating system (English and Romanian when installed).", SU_BOOL, {}},
          {"visual_line_numbers", L"Line numbers in Visual mode and Preview", L"Show the line number of each line (Visual) or block (Preview). Also the \u201CLines\u201D switch above the editor.", SU_BOOL, {}},
          {"visual_show_syntax", L"Show Markdown symbols while editing a line", L"Off by default: headings, bold, lists and links stay drawn formatted even on the line you are typing in. Turn on to see #, ** and - on that line.", SU_BOOL, {}},
          {"visual_max_width", L"Text width in Visual mode (pixels)", L"0 = use the whole width of the editor, like the plain editor. Set e.g. 800 for a narrower, easier-to-read column.", SU_NUM, {}, 0, 3000},
      }},
      {L"Claude Code", {
          {"claude_path", L"Claude Code program", L"Leave empty to find \u201Cclaude\u201D on the PATH, in the usual install folders or inside the VS Code extension. Ctrl+Alt+J opens the chat panel; sign in to Claude Code once from a terminal or VS Code.", SU_TEXT, {}},
          {"claude_permission_mode", L"What Claude may do", L"Edit files = changes files without asking; Plan only = only proposes a plan; Read only = no changes; Everything = also runs any command (use with care).", SU_SELECT, {L"acceptEdits", L"auto", L"plan", L"dontAsk", L"bypassPermissions"}},
          {"claude_model", L"Model", L"Empty = the default of your Claude Code. Or a name such as sonnet, opus or a full model id.", SU_TEXT, {}},
          {"claude_allowed_tools", L"Allowed tools", L"Commands Claude may run without asking, one per line, e.g. Bash(npm test) or Bash(git status).", SU_LIST, {}},
      }},
      {L"AMX Mod X", {
          {"amxx_profile", L"Build profile", L"Name of a profile from \u201Camxx_profiles\u201D in settings.json (several compilers / include folders). Empty = the fields below. Switch quickly with the command \u201CAMX Mod X: Select Build Profile\u2026\u201D.", SU_TEXT, {}},
          {"amxx_popup_style", L"Popup style", L"Colours of the hover help and parameter hints.", SU_SELECT, {L"Default", L"AMXXStudio", L"Basic Black", L"Green Notepad++", L"Notepad++"}},
          {"amxx_console_style", L"Compile results style", L"Colours of the compile results panel.", SU_SELECT, {L"Default", L"AMXXStudio", L"DarkColorize", L"MS-DOS", L"Matrix", L"SmallFonts Terminal", L"Ubuntu"}},
          {"amxx_build_version", L"Raise the build number on compile", L"Before compiling, 1.0.0-1 in the VERSION line of the plugin becomes 1.0.0-2.", SU_BOOL, {}},
          {"amxx_check_invalid", L"Underline unknown functions", L"In .sma files, mark calls to functions that are not declared in the file or its #include files.", SU_BOOL, {}},
          {"amxx_compiler_path", L"Compiler (amxxpc.exe)", L"Leave empty to find it automatically next to the plugin (its \u201Cscripting\u201D folder). F7 compiles the plugin you are editing.", SU_TEXT, {}},
          {"amxx_include_dir", L"Include folder", L"Where #include <...> files are searched, for autocomplete, hover help and compiling. Empty = the \u201Cinclude\u201D folder next to the compiler.", SU_TEXT, {}},
          {"amxx_output_dir", L"Output folder", L"Where the .amxx file is written. Empty = next to the .sma.", SU_TEXT, {}},
          {"amxx_debug_level", L"Debug level", L"0 = none, 1 = asserts, 2 = full (default), 3 = full + extra checks.", SU_NUM, {}, 0, 3},
      }},
      {L"Saving", {
          {"auto_save", L"Auto save", L"off = only when you save. after delay = a moment after you stop typing. on focus loss = when you switch to another window. Notes are always saved automatically.", SU_SELECT, {L"off", L"after delay", L"on focus loss"}},
          {"auto_save_delay", L"Auto save delay (seconds)", L"For \u201Cafter delay\u201D.", SU_NUM, {}, 1, 60},
          {"trim_trailing_white_space_on_save", L"Trim trailing white space on save", L"", SU_BOOL, {}},
          {"ensure_newline_at_eof_on_save", L"Ensure a final newline on save", L"", SU_BOOL, {}},
      }},
      {L"Project", {
          {"git_integration", L"Git integration", L"Show changed lines in the gutter, the branch in the status bar and changed files in the side bar. Needs git installed.", SU_BOOL, {}},
          {"folder_exclude_patterns", L"Hidden folders", L"Folders hidden from the side bar and skipped by search. One pattern per line.", SU_LIST, {}},
          {"file_exclude_patterns", L"Hidden files", L"Files hidden from the side bar and skipped by search. Wildcards allowed (*.pyc).", SU_LIST, {}},
          {"binary_file_patterns", L"Binary files", L"Skipped by Find in Files and Goto Anything.", SU_LIST, {}},
      }},
  };
}

/* ---------------------------------------------------------------- state */
// a small multi-line text box for the list settings
struct TextArea {
  vector<wstring> lines{L""};
  int l = 0, c = 0;
  void set(const wstring& s) {
    lines.clear();
    size_t p = 0;
    while (true) {
      size_t q = s.find(L'\n', p);
      if (q == wstring::npos) { lines.push_back(s.substr(p)); break; }
      lines.push_back(s.substr(p, q - p));
      p = q + 1;
    }
    l = (int)lines.size() - 1;
    c = (int)lines.back().size();
  }
  wstring text() const { wstring r; for (size_t i = 0; i < lines.size(); i++) { if (i) r += L"\n"; r += lines[i]; } return r; }
  void insert(const wstring& s) {
    for (wchar_t ch : s) {
      if (ch == L'\n') { wstring rest = lines[l].substr(c); lines[l].erase(c); lines.insert(lines.begin() + l + 1, rest); l++; c = 0; }
      else if (ch >= 32) { lines[l].insert(c, 1, ch); c++; }
    }
  }
  bool key(WPARAM wp, bool ctrl) {  // true when the text changed
    switch (wp) {
      case VK_LEFT: if (c > 0) c--; else if (l > 0) { l--; c = (int)lines[l].size(); } return false;
      case VK_RIGHT: if (c < (int)lines[l].size()) c++; else if (l + 1 < (int)lines.size()) { l++; c = 0; } return false;
      case VK_UP: if (l > 0) { l--; c = std::min(c, (int)lines[l].size()); } return false;
      case VK_DOWN: if (l + 1 < (int)lines.size()) { l++; c = std::min(c, (int)lines[l].size()); } return false;
      case VK_HOME: c = 0; return false;
      case VK_END: c = (int)lines[l].size(); return false;
      case VK_RETURN: insert(L"\n"); return true;
      case VK_BACK:
        if (c > 0) { lines[l].erase(c - 1, 1); c--; return true; }
        if (l > 0) { c = (int)lines[l - 1].size(); lines[l - 1] += lines[l]; lines.erase(lines.begin() + l); l--; return true; }
        return false;
      case VK_DELETE:
        if (c < (int)lines[l].size()) { lines[l].erase(c, 1); return true; }
        if (l + 1 < (int)lines.size()) { lines[l] += lines[l + 1]; lines.erase(lines.begin() + l + 1); return true; }
        return false;
      case 'V': if (ctrl) { insert(clipGet()); return true; } return false;
      case 'A': return false;
    }
    return false;
  }
};

struct SuHit { D2D1_RECT_F r; int kind; std::string key; int idx; };
enum { H_TAB = 1, H_SEARCH, H_CLOSE, H_NAV, H_SWITCH, H_NUM, H_SELECT, H_COLOR, H_TEXT, H_AREA, H_RESET, H_LINK, H_KCHANGE, H_KUNBIND, H_KRESET,
       H_DROPITEM, H_SWATCH, H_HEX, H_FONTARROW, H_NUMUP, H_NUMDOWN, H_SCROLL, H_BLOCK };
static bool g_suOpen = false;
static int g_suTab = 0;  // 0 settings, 1 key bindings
static LineEdit g_suSearch, g_suEdit, g_suHex;
static TextArea g_suArea;
static bool g_suSearchFocus = true;
static std::string g_suFocus;  // key of the focused field ("" = none, "#hex" the colour popover field)
static std::string g_suDrop;   // key of the select whose list is open
static std::string g_suColor;  // key of the colour popover
static float g_suScroll = 0, g_suContent = 0, g_suViewH = 0;
static vector<SuHit> g_suHits;
static vector<std::pair<std::wstring, float>> g_suNavY;  // section name -> y in the content
static int g_suNavActive = 0;
static int g_suCapture = -1;  // command being rebound
static vector<KeyChord> g_suChords;
static std::string g_suHover;
static bool g_suDragScroll = false;
static float g_suDragOff = 0;
static std::wstring g_suJump;  // section to scroll to on the next frame

static IDWriteTextFormat* uiFont(float size, bool semibold = false, bool mono = false) {
  static std::map<int, IDWriteTextFormat*> cache;
  int key = (int)std::lround(size * 10) * 4 + (semibold ? 1 : 0) + (mono ? 2 : 0);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  IDWriteTextFormat* f = nullptr;
  g_dw->CreateTextFormat(mono ? L"Consolas" : L"Segoe UI", nullptr, semibold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &f);
  if (f) f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  cache[key] = f;
  return f;
}
// height of a text wrapped to a width
static float wrappedHeight(const wstring& s, IDWriteTextFormat* f, float w, float lineH) {
  if (s.empty()) return 0;
  IDWriteTextLayout* lay = nullptr;
  g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, std::max(40.f, w), 2000, &lay);
  if (!lay) return lineH;
  lay->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
  DWRITE_TEXT_METRICS tm{};
  lay->GetMetrics(&tm);
  lay->Release();
  return std::max(lineH, tm.height);
}
static float drawWrapped(const wstring& s, IDWriteTextFormat* f, float x, float y, float w, DWORD col, float alpha, float lineH) {
  if (s.empty()) return 0;
  IDWriteTextLayout* lay = nullptr;
  g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, std::max(40.f, w), 2000, &lay);
  if (!lay) return lineH;
  lay->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
  if (lineH > 0) lay->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineH, lineH * 0.8f);
  DWRITE_TEXT_METRICS tm{};
  lay->GetMetrics(&tm);
  g_br->SetColor(Rgb(col, alpha));
  g_rt->DrawTextLayout(D2D1::Point2F(x, y), lay, g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
  lay->Release();
  return tm.height;
}

/* ---------------------------------------------------------------- values of the settings */
static bool suChanged(const char* key) { return g_settings[key] != g_defaults[key]; }
static wstring suValueText(const SuItem& it) {
  const json& v = g_settings[it.key];
  if (it.type == SU_NUMLIST) {
    wstring r;
    if (v.is_array()) for (size_t i = 0; i < v.size(); i++) { if (i) r += L", "; if (v[i].is_number()) r += std::to_wstring((int)v[i].get<double>()); }
    return r;
  }
  if (it.type == SU_LIST) {
    wstring r;
    if (v.is_array()) for (size_t i = 0; i < v.size(); i++) { if (i) r += L"\n"; if (v[i].is_string()) r += fromUtf8(v[i].get<std::string>()); }
    return r;
  }
  if (it.type == SU_NUM) return v.is_number() ? std::to_wstring((int)v.get<double>()) : L"";
  return v.is_string() ? fromUtf8(v.get<std::string>()) : L"";
}
static void suCommitField(const SuItem& it) {
  if (it.type == SU_NUM) {
    wchar_t* end = nullptr;
    const wchar_t* s = g_suEdit.t.c_str();
    long n = wcstol(s, &end, 10);
    if (!g_suEdit.t.empty() && end && !*end && n >= it.mn && n <= it.mx) setSetting(it.key, (int)n);
  } else if (it.type == SU_TEXT) {
    wstring t = trimW(g_suEdit.t);
    if (!t.empty()) setSetting(it.key, toUtf8(t));
  }
}
static void suCommitArea(const SuItem& it) {
  wstring t = g_suArea.text();
  json arr = json::array();
  if (it.type == SU_NUMLIST) {
    wstring cur;
    for (size_t i = 0; i <= t.size(); i++) {
      wchar_t ch = i < t.size() ? t[i] : L',';
      if (ch == L',' || iswspace(ch)) {
        if (!cur.empty()) { int n = _wtoi(cur.c_str()); if (n > 0) arr.push_back(n); cur.clear(); }
      } else cur += ch;
    }
  } else {
    size_t p = 0;
    while (p <= t.size()) {
      size_t q = t.find(L'\n', p);
      if (q == wstring::npos) q = t.size();
      wstring ln = trimW(t.substr(p, q - p));
      if (!ln.empty()) arr.push_back(toUtf8(ln));
      p = q + 1;
    }
  }
  setSetting(it.key, arr);
}
static const SuItem* suFind(const std::string& key) {
  for (auto& s : g_suSpec) for (auto& it : s.items) if (key == it.key) return &it;
  return nullptr;
}
static void suFocusField(const std::string& key) {
  g_suSearchFocus = false;
  g_suFocus = key;
  g_suDrop.clear();
  g_suColor.clear();
  const SuItem* it = suFind(key);
  if (!it) return;
  if (it->type == SU_LIST || it->type == SU_NUMLIST) g_suArea.set(suValueText(*it));
  else g_suEdit.set(suValueText(*it));
  caretReset();
}
static std::string suChordFile(const vector<KeyChord>& cs) {  // the format of keymap.json: "ctrl+shift+x"
  std::string r;
  for (size_t i = 0; i < cs.size(); i++) {
    if (i) r += " ";
    std::string s;
    if (cs[i].ctrl) s += "ctrl+";
    if (cs[i].alt) s += "alt+";
    if (cs[i].shift) s += "shift+";
    wstring k = chordText(KeyChord{false, false, false, cs[i].vk});
    std::string kk = toUtf8(k);
    for (auto& ch : kk) ch = (char)tolower((unsigned char)ch);
    if (kk == "esc") kk = "escape";
    r += s + kk;
  }
  return r;
}
static void suBind(int cmd, const vector<KeyChord>* chords, bool reset) {
  const CmdDef* def = cmdDef(cmd);
  if (!def) return;
  vector<UserKey> list;
  for (auto& u : g_userKeys) if (u.command != def->id) list.push_back(u);
  if (!reset) {
    UserKey u;
    u.command = def->id;
    if (chords && !chords->empty()) u.keys.push_back(fromUtf8(suChordFile(*chords)));
    list.push_back(u);
  }
  saveKeymapFile(list);
}
static bool suIsRebound(int cmd) {
  const CmdDef* def = cmdDef(cmd);
  if (!def) return false;
  for (auto& u : g_userKeys) if (u.command == def->id) return true;
  return false;
}

/* ---------------------------------------------------------------- drawing */
static void suIcon(const wstring& name, float x, float y, DWORD col, float a = 0.9f) {
  g_br->SetColor(Rgb(col, a));
  auto L2 = [&](float x1, float y1, float x2, float y2) { g_rt->DrawLine(D2D1::Point2F(x + x1, y + y1), D2D1::Point2F(x + x2, y + y2), g_br, 1.3f); };
  auto RR = [&](float rx, float ry, float rw, float rh, float r) { g_rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + rx, y + ry, x + rx + rw, y + ry + rh), r, r), g_br, 1.3f); };
  auto CI = [&](float cx, float cy, float r) { g_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x + cx, y + cy), r, r), g_br, 1.3f); };
  if (name == L"Appearance") { CI(8, 8, 6.2f); CI(5.2f, 7.6f, .5f); CI(7.6f, 4.8f, .5f); CI(10.6f, 5.8f, .5f); }
  else if (name == L"Editor") { L2(2.4f, 13.6f, 3.2f, 10.8f); L2(3.2f, 10.8f, 11.4f, 2.6f); L2(11.4f, 2.6f, 13.4f, 4.6f); L2(13.4f, 4.6f, 5.2f, 12.8f); L2(5.2f, 12.8f, 2.4f, 13.6f); }
  else if (name == L"Notes") { L2(3.5f, 1.8f, 9.5f, 1.8f); L2(9.5f, 1.8f, 12.5f, 4.8f); L2(12.5f, 4.8f, 12.5f, 14.2f); L2(12.5f, 14.2f, 3.5f, 14.2f); L2(3.5f, 14.2f, 3.5f, 1.8f); L2(9.5f, 1.8f, 9.5f, 4.8f); L2(9.5f, 4.8f, 12.5f, 4.8f); L2(5.5f, 8, 10.5f, 8); L2(5.5f, 10.5f, 10.5f, 10.5f); }
  else if (name == L"AMX Mod X") { CI(8, 8, 2.4f); L2(8, 1, 8, 3.3f); L2(8, 12.7f, 8, 15); L2(1, 8, 3.3f, 8); L2(12.7f, 8, 15, 8); L2(3, 3, 4.6f, 4.6f); L2(11.4f, 11.4f, 13, 13); L2(13, 3, 11.4f, 4.6f); L2(4.6f, 11.4f, 3, 13); }
  else if (name == L"Saving") { RR(2.5f, 2.5f, 11, 11, 1.2f); L2(5, 2.5f, 5, 6); L2(5, 6, 10, 6); L2(10, 6, 10, 2.5f); L2(5, 13.5f, 5, 9.5f); L2(5, 9.5f, 11, 9.5f); L2(11, 9.5f, 11, 13.5f); }
  else if (name == L"Project") { L2(1.5f, 3.5f, 6, 3.5f); L2(6, 3.5f, 7.5f, 5); L2(7.5f, 5, 14, 5); L2(14, 5, 14, 13); L2(14, 13, 1.5f, 13); L2(1.5f, 13, 1.5f, 3.5f); }
  else if (name == L"keys") { RR(1.5f, 4, 13, 8, 1.5f); L2(4, 7, 5, 7); L2(7, 7, 8, 7); L2(10, 7, 11, 7); L2(4.5f, 9.5f, 11.5f, 9.5f); }
  else CI(8, 8, 5.5f);
}

static void suButton(const D2D1_RECT_F& r, const wstring& label, bool hover, bool on = false) {
  const Theme& t = T();
  fillR(r, hover ? t.btnHi : t.btn, 1.f, 3);
  strokeR(r, on ? t.accent : t.border, 1.f, 3);
  float w = textWidth(label, g_ui);
  drawText(label.c_str(), (int)label.size(), r.left + (r.right - r.left - w) / 2, r.top + (r.bottom - r.top - 18) / 2, t.fg, g_ui, w + 6, 20);
}

static void suDrawControl(const SuItem& it, float rx, float cy, float& rowH, bool anyHover) {
  const Theme& t = T();
  std::string key = it.key;
  bool changed = suChanged(it.key);
  float right = rx;  // the right edge of the control; the reset link sits left of it
  auto hov = [&](const D2D1_RECT_F& r) { return inRect(r, g_mx, g_my); };
  switch (it.type) {
    case SU_BOOL: {
      bool on = sBool(it.key);
      D2D1_RECT_F r = rc(right - 38, cy - 10, 38, 20);
      fillR(r, on ? t.accent : t.btn, 1.f, 10);
      strokeR(r, t.border, 1.f, 10);
      float kx = on ? r.left + 20 : r.left + 4;
      fillR(rc(kx, cy - 7, 14, 14), on ? 0x1d2226 : t.fg, on ? 1.f : 0.85f, 7);
      g_suHits.push_back({r, H_SWITCH, key, 0});
      right = r.left;
      break;
    }
    case SU_NUM: {
      D2D1_RECT_F r = rc(right - 80, cy - 14, 80, 28);
      bool foc = g_suFocus == key;
      LineEdit tmp;
      tmp.set(foc ? g_suEdit.t : suValueText(it));
      if (foc) tmp = g_suEdit;
      drawInput(tmp, r, foc, L"");
      g_suHits.push_back({r, H_NUM, key, 0});
      right = r.left;
      break;
    }
    case SU_SELECT: {
      D2D1_RECT_F r = rc(right - 160, cy - 14, 160, 28);
      bool open = g_suDrop == key;
      fillR(r, inputBg(), 1.f, 3);
      strokeR(r, open ? t.accent : t.border, 1.f, 3);
      wstring v = suValueText(it);
      drawText(v.c_str(), (int)v.size(), r.left + 9, r.top + 5, t.fg, g_ui, 120, 20);
      g_br->SetColor(Rgb(t.fg, 0.8f));
      g_rt->DrawLine(D2D1::Point2F(r.right - 20, cy - 2), D2D1::Point2F(r.right - 16, cy + 2), g_br, 1.4f);
      g_rt->DrawLine(D2D1::Point2F(r.right - 12, cy - 2), D2D1::Point2F(r.right - 16, cy + 2), g_br, 1.4f);
      g_suHits.push_back({r, H_SELECT, key, 0});
      right = r.left;
      break;
    }
    case SU_COLOR: {
      D2D1_RECT_F r = rc(right - 44, cy - 14, 44, 28);
      wstring v = suValueText(it);
      DWORD c = (v.size() == 7 && v[0] == L'#') ? (DWORD)wcstoul(v.c_str() + 1, nullptr, 16) : t.accent;
      fillR(r, c, 1.f, 6);
      strokeR(r, t.border, 1.f, 6);
      g_suHits.push_back({r, H_COLOR, key, 0});
      right = r.left;
      break;
    }
    case SU_TEXT: {
      float w = 220;
      D2D1_RECT_F r = rc(right - w - (it.fonts ? 24 : 0), cy - 14, w, 28);
      bool foc = g_suFocus == key;
      LineEdit tmp;
      tmp.set(suValueText(it));
      if (foc) tmp = g_suEdit;
      drawInput(tmp, r, foc, L"");
      g_suHits.push_back({r, H_TEXT, key, 0});
      right = r.left;
      if (it.fonts) {
        D2D1_RECT_F ar = rc(r.right + 2, cy - 14, 22, 28);
        g_br->SetColor(Rgb(t.fg, 0.7f));
        g_rt->DrawLine(D2D1::Point2F(ar.left + 6, cy - 2), D2D1::Point2F(ar.left + 10, cy + 2), g_br, 1.4f);
        g_rt->DrawLine(D2D1::Point2F(ar.left + 14, cy - 2), D2D1::Point2F(ar.left + 10, cy + 2), g_br, 1.4f);
        g_suHits.push_back({ar, H_FONTARROW, key, 0});
      }
      break;
    }
    case SU_LIST: case SU_NUMLIST: {
      float w = 260;
      int rows = it.type == SU_NUMLIST ? 1 : 4;
      float h = rows * 18.f + 12;
      D2D1_RECT_F r = rc(right - w, cy - h / 2, w, h);
      bool foc = g_suFocus == key;
      fillR(r, inputBg(), 1.f, 3);
      strokeR(r, foc ? t.accent : t.border, 1.f, 3);
      IDWriteTextFormat* mf = uiFont(12.5f, false, true);
      TextArea ta;
      if (foc) ta = g_suArea; else ta.set(suValueText(it));
      g_rt->PushAxisAlignedClip(D2D1::RectF(r.left + 2, r.top + 2, r.right - 2, r.bottom - 2), D2D1_ANTIALIAS_MODE_ALIASED);
      int first = foc ? std::max(0, ta.l - rows + 1) : 0;
      if (ta.lines.size() == 1 && ta.lines[0].empty() && it.ph[0]) drawText(it.ph, (int)wcslen(it.ph), r.left + 8, r.top + 6, t.gutterFg, mf, w, 18);
      for (int i = 0; i < rows && first + i < (int)ta.lines.size(); i++) {
        const wstring& ln = ta.lines[first + i];
        drawText(ln.c_str(), (int)ln.size(), r.left + 8, r.top + 6 + i * 18, t.fg, mf, w - 12, 18);
        if (foc && first + i == ta.l && g_caretOn) fillRect(r.left + 8 + textWidth(ln.substr(0, ta.c), mf), r.top + 7 + i * 18, 1.5f, 15, t.caret);
      }
      g_rt->PopAxisAlignedClip();
      g_suHits.push_back({r, H_AREA, key, 0});
      right = r.left;
      rowH = std::max(rowH, h + 20);
      break;
    }
  }
  // the dot and the "Reset" link of a changed setting
  if (changed) {
    wstring rs = L"Reset";
    float rw = textWidth(rs, g_ui);
    D2D1_RECT_F rr = rc(right - rw - 14, cy - 11, rw + 8, 22);
    bool h = hov(rr);
    drawText(rs.c_str(), 5, rr.left + 4, rr.top + 2, t.accent, g_ui, rw + 6, 20);
    if (h) fillRect(rr.left + 4, rr.top + 19, rw, 1, t.accent);
    g_suHits.push_back({rr, H_RESET, key, 0});
    g_br->SetColor(Rgb(t.accent));
    g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rr.left - 8, cy), 3.f, 3.f), g_br);
  }
  (void)anyHover;
}

static void suDrawSettingsBody(float bx, float by, float bw, float bh) {
  const Theme& t = T();
  suInitSpec();
  wstring q = lowerStr(trimW(g_suSearch.t));
  float cardX = bx + 32, cardW = std::min(980.f, bw - 64);
  float y = by + 22 - g_suScroll;
  float contentTop = by + 22;
  g_suNavY.clear();
  bool any = false;
  g_rt->PushAxisAlignedClip(D2D1::RectF(bx, by, bx + bw, by + bh), D2D1_ANTIALIAS_MODE_ALIASED);
  IDWriteTextFormat* fLabel = uiFont(13.5f);
  IDWriteTextFormat* fDesc = uiFont(13.f);
  IDWriteTextFormat* fKey = uiFont(11.f, false, true);
  IDWriteTextFormat* fH2 = uiFont(20.f, true);
  for (auto& sec : g_suSpec) {
    vector<const SuItem*> shown;
    for (auto& it : sec.items) {
      wstring hay = lowerStr(wstring(it.label) + L" " + it.desc + L" " + fromUtf8(it.key) + L" " + sec.name);
      if (q.empty() || hay.find(q) != wstring::npos) shown.push_back(&it);
    }
    if (shown.empty()) continue;
    any = true;
    g_suNavY.push_back({sec.name, y - (by + 22 - g_suScroll)});
    // measure the card
    struct RowM { float h; float textH; };
    vector<RowM> ms;
    float cardH = 6 + 16 + 26 + 6;
    float textW = cardW - 52 - 280;
    for (auto* it : shown) {
      float dh = wrappedHeight(it->desc, fDesc, textW, 18.2f);
      float th = 20 + (dh > 0 ? 2 + dh : 0) + 3 + 14;
      float h = std::max(th + 22, it->type == SU_LIST ? 4 * 18.f + 12 + 22 : 0.f);
      ms.push_back({h, th});
      cardH += h;
    }
    cardH += 12;
    if (y + cardH > by && y < by + bh) {
      fillR(rc(cardX, y, cardW, cardH), t.panel, 1.f, 8);
      strokeR(rc(cardX, y, cardW, cardH), t.border, 1.f, 8);
      drawText(sec.name, (int)wcslen(sec.name), cardX + 26, y + 20, t.fg, fH2, 400, 30);
      float ry = y + 6 + 16 + 26 + 6;
      for (size_t i = 0; i < shown.size(); i++) {
        const SuItem& it = *shown[i];
        float h = ms[i].h;
        if (i + 1 < shown.size()) fillRect(cardX + 26, ry + h - 1, cardW - 52, 1, t.fg, 0.1f);
        float ty = ry + (h - ms[i].textH) / 2;
        drawText(it.label, (int)wcslen(it.label), cardX + 26, ty, t.fg, fLabel, textW, 22);
        float dy = ty + 20;
        if (it.desc[0]) { float dh = drawWrapped(it.desc, fDesc, cardX + 26, dy + 2, textW, t.fg, 0.62f, 18.2f); dy += 2 + dh; }
        std::wstring kk = fromUtf8(it.key);
        drawText(kk.c_str(), (int)kk.size(), cardX + 26, dy + 3, t.fg, fKey, 300, 16);
        g_br->SetColor(Rgb(t.bg, 0));
        float rh = h;
        suDrawControl(it, cardX + cardW - 26, ry + h / 2, rh, false);
        ry += h;
      }
    } else {
      // off-screen cards still register nothing; their height is what counts
    }
    y += cardH + 18;
  }
  if (!any) drawText(L"No settings match your search.", 30, bx + bw / 2 - 90, by + 60, t.fg, g_ui, 300, 20);
  g_suContent = (y - (by + 22 - g_suScroll)) + 70;  // some air at the end, like the page padding
  g_rt->PopAxisAlignedClip();
  (void)contentTop;
}

// the key bindings tab: commands grouped like the menus
static void suDrawKeysBody(float bx, float by, float bw, float bh) {
  const Theme& t = T();
  wstring q = lowerStr(trimW(g_suSearch.t));
  float cardX = bx + 32, cardW = std::min(980.f, bw - 64);
  float y = by + 22 - g_suScroll;
  g_suNavY.clear();
  std::map<wstring, vector<const CmdDef*>> groups;
  vector<wstring> order;
  for (int i = 0; i < N_CMDS; i++) {
    const CmdDef& c = g_cmdDefs[i];
    if (c.flags & 2) continue;
    wstring title = c.title;
    wstring hay = lowerStr(title + L" " + effectiveKeys(c.cmd));
    if (!q.empty() && hay.find(q) == wstring::npos) continue;
    size_t p = title.find(L": ");
    wstring g = p == wstring::npos ? L"Other" : title.substr(0, p);
    if (!groups.count(g)) order.push_back(g);
    groups[g].push_back(&c);
  }
  g_rt->PushAxisAlignedClip(D2D1::RectF(bx, by, bx + bw, by + bh), D2D1_ANTIALIAS_MODE_ALIASED);
  IDWriteTextFormat* fH2 = uiFont(20.f, true);
  IDWriteTextFormat* fLabel = uiFont(13.f);
  IDWriteTextFormat* fKbd = uiFont(12.f, false, true);
  for (auto& g : order) {
    auto& list = groups[g];
    g_suNavY.push_back({g, y - (by + 22 - g_suScroll)});
    float rowH = 40;
    float cardH = 6 + 16 + 26 + 6 + rowH * list.size() + 12;
    if (y + cardH > by && y < by + bh) {
      fillR(rc(cardX, y, cardW, cardH), t.panel, 1.f, 8);
      strokeR(rc(cardX, y, cardW, cardH), t.border, 1.f, 8);
      drawText(g.c_str(), (int)g.size(), cardX + 26, y + 20, t.fg, fH2, 400, 30);
      float ry = y + 6 + 16 + 26 + 6;
      for (size_t i = 0; i < list.size(); i++) {
        const CmdDef& c = *list[i];
        bool mod = suIsRebound(c.cmd);
        if (i + 1 < list.size()) fillRect(cardX + 26, ry + rowH - 1, cardW - 52, 1, t.fg, 0.1f);
        wstring lbl = cmdLabel(c.cmd);
        drawText(lbl.c_str(), (int)lbl.size(), cardX + 26, ry + 10, mod ? t.accent : t.fg, fLabel, cardW - 52 - 560, 22);
        // the keys (kbd chips), or the capture prompt
        float kx = cardX + cardW - 26 - 190 - 16 - 300;
        if (g_suCapture == c.cmd) {
          wstring prompt;
          if (!g_suChords.empty()) prompt = L"Ctrl+K   then press the second key\u2026"; else prompt = L"Press the new shortcut\u2026 (Esc cancels)";
          drawText(prompt.c_str(), (int)prompt.size(), kx, ry + 11, t.accent, fLabel, 300, 20);
        } else {
          wstring keys = effectiveKeys(c.cmd);
          if (keys.empty()) drawText(L"unbound", 7, kx, ry + 11, t.fg, fLabel, 100, 20), g_br->SetColor(Rgb(t.fg, 0));
          else {
            float cx = kx;
            size_t p = 0;
            bool firstChip = true;
            while (p < keys.size()) {
              size_t qq = keys.find(L' ', p);
              if (qq == wstring::npos) qq = keys.size();
              wstring chip = keys.substr(p, qq - p);
              p = qq + 1;
              if (!firstChip) { drawText(L"then", 4, cx + 2, ry + 12, t.fg, uiFont(11.f), 40, 16); cx += 36; }
              firstChip = false;
              float w = textWidth(chip, fKbd) + 14;
              fillR(rc(cx, ry + 8, w, 24), t.btn, 1.f, 4);
              strokeR(rc(cx, ry + 8, w, 24), t.border, 1.f, 4);
              drawText(chip.c_str(), (int)chip.size(), cx + 7, ry + 12, t.fg, fKbd, w, 18);
              cx += w + 4;
            }
          }
        }
        // buttons: Change [Unbind] [Reset]
        float bx2 = cardX + cardW - 26;
        auto btn = [&](const wchar_t* label, int kind) {
          float w = textWidth(label, g_ui) + 22;
          D2D1_RECT_F r = rc(bx2 - w, ry + 8, w, 24);
          suButton(r, label, inRect(r, g_mx, g_my));
          g_suHits.push_back({r, kind, "", c.cmd});
          bx2 -= w + 6;
        };
        if (mod) btn(L"Reset", H_KRESET);
        if (!effectiveKeys(c.cmd).empty()) btn(L"Unbind", H_KUNBIND);
        btn(L"Change", H_KCHANGE);
        ry += rowH;
      }
    }
    y += cardH + 18;
  }
  if (order.empty()) drawText(L"No commands match your search.", 30, bx + bw / 2 - 90, by + 60, t.fg, g_ui, 300, 20);
  g_suContent = (y - (by + 22 - g_suScroll)) + 70;
  g_rt->PopAxisAlignedClip();
}

static void drawSettingsPage() {
  if (!g_suOpen) return;
  const Theme& t = T();
  suInitSpec();
  g_suHits.clear();
  float W = cw(), H = ch(), top = TITLE_H, headH = 56, footH = 36, navW = 248;
  fillRect(0, top, W, H - top, t.bg);
  // header
  fillRect(0, top, W, headH, t.panel);
  fillRect(0, top + headH - 1, W, 1, t.border);
  {
    float x = 18;
    const wchar_t* names[2] = {L"Settings", L"Key Bindings"};
    for (int i = 0; i < 2; i++) {
      float w = textWidth(names[i], uiFont(13.f, true)) + 28;
      D2D1_RECT_F r = rc(x, top + 10, w, 36);
      bool on = g_suTab == i, hv = inRect(r, g_mx, g_my);
      if (on || hv) fillR(r, t.btn, 1.f, 4);
      if (on) fillRect(r.left, r.bottom - 2, w, 2, t.accent);
      drawText(names[i], (int)wcslen(names[i]), r.left + 14, r.top + 8, on ? t.fg : t.statusText, uiFont(13.f, true), w, 20);
      g_suHits.push_back({r, H_TAB, "", i});
      x += w + 4;
    }
    D2D1_RECT_F sr = rc(x + 10, top + 11, W - x - 10 - 18 - 44, 34);
    drawInput(g_suSearch, sr, g_suSearchFocus, g_suTab == 0 ? L"Search settings" : L"Search commands or shortcuts");
    g_suHits.push_back({sr, H_SEARCH, "", 0});
    D2D1_RECT_F xr = rc(W - 18 - 28, top + 14, 28, 28);
    bool xh = inRect(xr, g_mx, g_my);
    g_br->SetColor(Rgb(xh ? t.fg : t.statusText));
    g_rt->DrawLine(D2D1::Point2F(xr.left + 8, xr.top + 8), D2D1::Point2F(xr.right - 8, xr.bottom - 8), g_br, 1.6f);
    g_rt->DrawLine(D2D1::Point2F(xr.right - 8, xr.top + 8), D2D1::Point2F(xr.left + 8, xr.bottom - 8), g_br, 1.6f);
    g_suHits.push_back({xr, H_CLOSE, "", 0});
  }
  // footer
  float fy = H - footH;
  fillRect(0, fy, W, footH, t.panel);
  fillRect(0, fy, W, 1, t.border);
  {
    const wchar_t* note = L"Changes apply immediately and are saved automatically.";
    drawText(note, (int)wcslen(note), 16, fy + 9, t.fg, uiFont(12.f), 500, 20);
    g_br->SetColor(Rgb(t.fg, 0));
    const wchar_t* links[3] = {L"Open settings.json", L"Open keymap.json", L"Open snippets.json"};
    float x = W - 16;
    for (int i = 2; i >= 0; i--) {
      float w = textWidth(links[i], uiFont(12.f));
      D2D1_RECT_F r = rc(x - w, fy + 6, w, 24);
      drawText(links[i], (int)wcslen(links[i]), r.left, fy + 9, t.accent, uiFont(12.f), w + 6, 20);
      if (inRect(r, g_mx, g_my)) fillRect(r.left, fy + 25, w, 1, t.accent);
      g_suHits.push_back({r, H_LINK, "", i});
      x -= w + 16;
    }
  }
  // body first (its layout fills the navigation list), the navigation next
  float bx = navW, by = top + headH, bw = W - navW, bh = H - footH - by;
  g_suViewH = bh;
  g_suScroll = std::max(0.f, std::min(g_suScroll, std::max(0.f, g_suContent - bh)));
  if (g_suTab == 0) suDrawSettingsBody(bx, by, bw, bh); else suDrawKeysBody(bx, by, bw, bh);
  if (!g_suJump.empty()) {
    for (auto& n : g_suNavY) if (n.first == g_suJump) g_suScroll = std::max(0.f, std::min(n.second - 14, std::max(0.f, g_suContent - bh)));
    g_suJump.clear();
    redraw();
  }
  // navigation (left)
  fillRect(0, by, navW, bh, t.panel);
  fillRect(navW - 1, by, 1, bh, t.border);
  {
    int active = 0;
    for (size_t i = 0; i < g_suNavY.size(); i++) if (g_suNavY[i].second <= g_suScroll + 40) active = (int)i;
    if (g_suScroll + bh >= g_suContent - 80 && !g_suNavY.empty()) active = (int)g_suNavY.size() - 1;
    float y = by + 14;
    for (size_t i = 0; i < g_suNavY.size(); i++) {
      const wstring& name = g_suNavY[i].first;
      D2D1_RECT_F r = rc(10, y, navW - 20, 36);
      bool on = (int)i == active, hv = inRect(r, g_mx, g_my);
      if (on || hv) fillR(r, t.btn, 1.f, 6);
      if (on) fillRect(r.left, r.top + 4, 3, 28, t.accent);
      suIcon(g_suTab == 1 ? L"keys" : name, r.left + 12, r.top + 10, t.fg, on ? 0.9f : 0.75f);
      drawText(name.c_str(), (int)name.size(), r.left + 39, r.top + 8, on ? t.fg : t.statusText, uiFont(13.5f), navW - 110, 22);
      int count = 0;
      if (g_suTab == 0) { for (auto& s : g_suSpec) if (name == s.name) { wstring q = lowerStr(trimW(g_suSearch.t)); for (auto& it : s.items) { wstring hay = lowerStr(wstring(it.label) + L" " + it.desc + L" " + fromUtf8(it.key) + L" " + s.name); if (q.empty() || hay.find(q) != wstring::npos) count++; } } }
      wstring cs = count ? std::to_wstring(count) : L"";
      if (!cs.empty()) drawText(cs.c_str(), (int)cs.size(), r.right - 14 - textWidth(cs, uiFont(11.f)), r.top + 10, t.fg, uiFont(11.f), 40, 16);
      // a dot on categories that hold a changed setting
      if (g_suTab == 0) {
        bool ch2 = false;
        for (auto& s : g_suSpec) if (name == s.name) for (auto& it : s.items) if (suChanged(it.key)) ch2 = true;
        if (ch2) { g_br->SetColor(Rgb(t.accent, 0.75f)); g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(r.left + 39 + textWidth(name, uiFont(13.5f)) + 12, r.top + 18), 3.f, 3.f), g_br); }
      }
      g_suHits.push_back({r, H_NAV, toUtf8(name), (int)i});
      y += 38;
    }
  }
  // scrollbar of the body
  if (g_suContent > bh) {
    float th = std::max(30.f, bh * bh / g_suContent), ty = by + (bh - th) * (g_suScroll / std::max(1.f, g_suContent - bh));
    D2D1_RECT_F sr = rc(W - 12, ty, 8, th);
    fillR(sr, t.scroll, 1.f, 4);
    g_suHits.push_back({rc(W - 14, by, 14, bh), H_SCROLL, "", 0});
  }
  // popups: the list of a select, the colour popover, the fonts of the font box
  if (!g_suDrop.empty() || !g_suColor.empty()) {
    const SuItem* it = suFind(!g_suDrop.empty() ? g_suDrop : g_suColor);
    D2D1_RECT_F anchor{0, 0, 0, 0};
    for (auto& h : g_suHits) if (h.key == (!g_suDrop.empty() ? g_suDrop : g_suColor) && (h.kind == H_SELECT || h.kind == H_COLOR || h.kind == H_FONTARROW)) anchor = h.r;
    if (it && anchor.right > 0) {
      if (it->type == SU_COLOR) {
        float w = 220, h = 128;
        D2D1_RECT_F pr = rc(std::min(anchor.right - w, W - w - 10), anchor.bottom + 4, w, h);
        fillR(D2D1::RectF(pr.left + 2, pr.top + 4, pr.right + 2, pr.bottom + 4), 0x000000, 0.3f, 8);
        fillR(pr, t.menu, 1.f, 8);
        strokeR(pr, t.border, 1.f, 8);
        g_suHits.push_back({pr, H_BLOCK, "", 0});
        static const DWORD sw[16] = {0xfac863, 0xf9ae58, 0xec5f66, 0xf92672, 0xc594c5, 0xae81ff, 0x6699cc, 0x61afef, 0x5fb3b3, 0x4ec9b0, 0x99c794, 0xa6e22e, 0xe6db74, 0xcb4b16, 0xd8dee9, 0x8996a3};
        for (int i = 0; i < 16; i++) {
          D2D1_RECT_F sr = rc(pr.left + 12 + (i % 8) * 25, pr.top + 12 + (i / 8) * 28, 22, 22);
          fillR(sr, sw[i], 1.f, 5);
          strokeR(sr, inRect(sr, g_mx, g_my) ? t.fg : t.border, 1.f, 5);
          g_suHits.push_back({sr, H_SWATCH, "", (int)sw[i]});
        }
        D2D1_RECT_F hr = rc(pr.left + 12, pr.top + 74, w - 24, 28);
        drawInput(g_suHex, hr, g_suFocus == "#hex", L"#rrggbb");
        g_suHits.push_back({hr, H_HEX, "", 0});
        wstring hint = L"Enter applies. Reset (in the row) restores the scheme colour.";
        drawText(hint.c_str(), (int)hint.size(), pr.left + 12, pr.top + 106, t.fg, uiFont(10.5f), w - 20, 16);
      } else {
        int n = (int)it->opts.size();
        float w = std::max(160.f, anchor.right - anchor.left), h = n * 28.f + 8;
        float py = anchor.bottom + 2;
        if (py + h > H - 40) py = anchor.top - h - 2;
        D2D1_RECT_F pr = rc(anchor.right - w, py, w, h);
        fillR(D2D1::RectF(pr.left + 2, pr.top + 4, pr.right + 2, pr.bottom + 4), 0x000000, 0.3f, 6);
        fillR(pr, t.menu, 1.f, 6);
        strokeR(pr, t.border, 1.f, 6);
        g_suHits.push_back({pr, H_BLOCK, "", 0});
        wstring cur = suValueText(*it);
        for (int i = 0; i < n; i++) {
          D2D1_RECT_F ir = rc(pr.left + 4, pr.top + 4 + i * 28, w - 8, 28);
          bool hv = inRect(ir, g_mx, g_my);
          if (hv) fillR(ir, t.menuHi, 1.f, 4);
          drawText(it->opts[i], (int)wcslen(it->opts[i]), ir.left + 10, ir.top + 4, t.chromeFg, g_ui, w - 40, 20);
          if (cur == it->opts[i]) { g_br->SetColor(Rgb(t.accent)); g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(ir.right - 12, ir.top + 14), 3.f, 3.f), g_br); }
          g_suHits.push_back({ir, H_DROPITEM, std::string(it->key), i});
        }
      }
    }
  }
}

/* ---------------------------------------------------------------- input */
static void suClose() {
  g_suOpen = false;
  g_suFocus.clear();
  g_suDrop.clear();
  g_suColor.clear();
  g_suCapture = -1;
  g_suChords.clear();
  redraw();
}
static void suOpen(int tab) {
  suInitSpec();
  g_suOpen = true;
  g_suTab = tab;
  g_suSearch.set(L"");
  g_suSearchFocus = true;
  g_suFocus.clear();
  g_suDrop.clear();
  g_suColor.clear();
  g_suScroll = 0;
  g_suCapture = -1;
  caretReset();
  redraw();
}
static void openUserFile(const wstring& path);
static void suMouse(float x, float y, int what) {  // 0 down, 1 up
  if (what == 1) { g_suDragScroll = false; return; }
  // clicks go to the topmost hit rectangle (the last one registered)
  const SuHit* hit = nullptr;
  for (int i = (int)g_suHits.size() - 1; i >= 0; i--) if (inRect(g_suHits[i].r, x, y)) { hit = &g_suHits[i]; break; }
  bool popupOpen = !g_suDrop.empty() || !g_suColor.empty();
  if (popupOpen && (!hit || (hit->kind != H_DROPITEM && hit->kind != H_SWATCH && hit->kind != H_HEX && hit->kind != H_BLOCK && hit->kind != H_SELECT && hit->kind != H_COLOR && hit->kind != H_FONTARROW))) {
    g_suDrop.clear();
    g_suColor.clear();
    if (g_suFocus == "#hex") g_suFocus.clear();
    redraw();
    if (hit && hit->kind != H_SCROLL) { /* the click also goes through to what is under the popup */ } else return;
  }
  if (!hit) { g_suFocus.clear(); g_suSearchFocus = false; redraw(); return; }
  const SuItem* it = hit->key.empty() ? nullptr : suFind(hit->key);
  switch (hit->kind) {
    case H_TAB: g_suTab = hit->idx; g_suSearch.set(L""); g_suScroll = 0; g_suFocus.clear(); g_suCapture = -1; break;
    case H_SEARCH: g_suSearchFocus = true; g_suFocus.clear(); caretReset(); break;
    case H_CLOSE: suClose(); return;
    case H_NAV: g_suJump = fromUtf8(hit->key); break;
    case H_SWITCH: if (it) setSetting(it->key, !sBool(it->key)); break;
    case H_NUM: case H_TEXT: case H_AREA: if (it) suFocusField(hit->key); break;
    case H_SELECT: g_suColor.clear(); g_suFocus.clear(); g_suDrop = g_suDrop == hit->key ? "" : hit->key; break;
    case H_FONTARROW: g_suColor.clear(); g_suDrop = g_suDrop == hit->key ? "" : hit->key; break;
    case H_COLOR: g_suDrop.clear(); g_suColor = g_suColor == hit->key ? "" : hit->key; g_suHex.set(suValueText(*it)); g_suFocus.clear(); break;
    case H_DROPITEM:
      if (it && hit->idx < (int)it->opts.size()) setSetting(it->key, toUtf8(it->opts[hit->idx]));
      g_suDrop.clear();
      break;
    case H_SWATCH: {
      char buf[16];
      snprintf(buf, sizeof(buf), "#%06x", (unsigned)hit->idx);
      setSetting("accent_color", std::string(buf));
      g_suColor.clear();
      break;
    }
    case H_HEX: g_suFocus = "#hex"; g_suSearchFocus = false; caretReset(); break;
    case H_RESET: if (it) { setSetting(it->key, g_defaults[it->key]); if (g_suFocus == hit->key) suFocusField(hit->key); } break;
    case H_LINK: {
      wstring p = hit->idx == 0 ? settingsPath() : hit->idx == 1 ? keymapPath() : snippetsPath();
      suClose();
      openPath(p);
      return;
    }
    case H_KCHANGE: g_suCapture = hit->idx; g_suChords.clear(); break;
    case H_KUNBIND: suBind(hit->idx, nullptr, false); break;
    case H_KRESET: suBind(hit->idx, nullptr, true); break;
    case H_SCROLL: g_suDragScroll = true; {
      float bh = g_suViewH, th = std::max(30.f, bh * bh / std::max(1.f, g_suContent));
      float by = TITLE_H + 56;
      g_suScroll = std::max(0.f, std::min(g_suContent - bh, (y - by - th / 2) / std::max(1.f, bh - th) * (g_suContent - bh)));
    } break;
    case H_BLOCK: break;
  }
  redraw();
}
static void suMove(float x, float y) {
  (void)x;
  if (g_suDragScroll) {
    float bh = g_suViewH, th = std::max(30.f, bh * bh / std::max(1.f, g_suContent));
    float by = TITLE_H + 56;
    g_suScroll = std::max(0.f, std::min(std::max(0.f, g_suContent - bh), (y - by - th / 2) / std::max(1.f, bh - th) * (g_suContent - bh)));
  }
  redraw();
}
static void suWheel(int delta) {
  g_suScroll = std::max(0.f, std::min(std::max(0.f, g_suContent - g_suViewH), g_suScroll - delta / 120.f * 90.f));
  redraw();
}
static bool suKey(WPARAM wp, bool ctrl, bool shift, bool alt) {
  // capturing a new shortcut: the next chord (or two, after Ctrl+K) is the binding
  if (g_suCapture >= 0) {
    if (wp == VK_ESCAPE && !ctrl && !alt && !shift) { g_suCapture = -1; g_suChords.clear(); redraw(); return true; }
    if (wp == VK_CONTROL || wp == VK_SHIFT || wp == VK_MENU) return true;
    KeyChord c{ctrl, alt, shift, (int)wp};
    g_suChords.push_back(c);
    if (g_suChords.size() == 1 && c.ctrl && !c.alt && !c.shift && c.vk == 'K') { redraw(); return true; }
    const CmdDef* me = cmdDef(g_suCapture);
    wstring want = keysText(g_suChords);
    wstring clash;
    for (int i = 0; i < N_CMDS; i++) {
      const CmdDef& d = g_cmdDefs[i];
      if (d.cmd != g_suCapture && !(d.flags & 2) && effectiveKeys(d.cmd) == want) { clash = cmdLabel(d.cmd); break; }
    }
    vector<KeyChord> chords = g_suChords;
    int cmd = g_suCapture;
    g_suCapture = -1;
    g_suChords.clear();
    suBind(cmd, &chords, false);
    if (!clash.empty()) flash(L"\u201C" + clash + L"\u201D also used that shortcut; the new binding takes priority.");
    (void)me;
    redraw();
    return true;
  }
  if (wp == VK_ESCAPE) {
    if (!g_suDrop.empty() || !g_suColor.empty()) { g_suDrop.clear(); g_suColor.clear(); g_suFocus.clear(); redraw(); return true; }
    if (!g_suFocus.empty()) { g_suFocus.clear(); redraw(); return true; }
    suClose();
    return true;
  }
  if (g_suFocus == "#hex") {
    if (wp == VK_RETURN) {
      wstring h = trimW(g_suHex.t);
      if (h.size() == 7 && h[0] == L'#') setSetting("accent_color", toUtf8(lowerStr(h)));
      g_suColor.clear();
      g_suFocus.clear();
      redraw();
      return true;
    }
    editKey(g_suHex, wp, ctrl, shift);
    caretReset();
    redraw();
    return true;
  }
  if (!g_suFocus.empty()) {
    const SuItem* it = suFind(g_suFocus);
    if (it && (it->type == SU_LIST || it->type == SU_NUMLIST)) {
      if (it->type == SU_NUMLIST && wp == VK_RETURN) { g_suFocus.clear(); redraw(); return true; }
      if (g_suArea.key(wp, ctrl)) suCommitArea(*it);
      caretReset();
      redraw();
      return true;
    }
    if (it) {
      if (wp == VK_RETURN || wp == VK_TAB) { g_suFocus.clear(); redraw(); return true; }
      if (it->type == SU_NUM && (wp == VK_UP || wp == VK_DOWN)) {
        int n = _wtoi(g_suEdit.t.c_str()) + (wp == VK_UP ? 1 : -1);
        n = std::max(it->mn, std::min(it->mx, n));
        g_suEdit.set(std::to_wstring(n));
        suCommitField(*it);
        redraw();
        return true;
      }
      bool changed = editKey(g_suEdit, wp, ctrl, shift);
      if (changed) suCommitField(*it);
      caretReset();
      redraw();
      return true;
    }
  }
  if (g_suSearchFocus) {
    if (ctrl && !alt && (wp == 'F')) return true;
    bool changed = editKey(g_suSearch, wp, ctrl, shift);
    if (changed) g_suScroll = 0;
    caretReset();
    redraw();
    return true;
  }
  if (wp == VK_PRIOR) { suWheel(300); return true; }
  if (wp == VK_NEXT) { suWheel(-300); return true; }
  if (wp == VK_UP) { suWheel(120); return true; }
  if (wp == VK_DOWN) { suWheel(-120); return true; }
  return true;
}
static void suChar(wchar_t c) {
  if (g_suCapture >= 0) return;
  if (g_suFocus == "#hex") { g_suHex.insert(wstring(1, c)); caretReset(); redraw(); return; }
  if (!g_suFocus.empty()) {
    const SuItem* it = suFind(g_suFocus);
    if (!it) return;
    if (it->type == SU_LIST || it->type == SU_NUMLIST) { g_suArea.insert(wstring(1, c)); suCommitArea(*it); }
    else {
      if (it->type == SU_NUM && !iswdigit(c)) return;
      g_suEdit.insert(wstring(1, c));
      suCommitField(*it);
    }
    caretReset();
    redraw();
    return;
  }
  if (g_suSearchFocus) { g_suSearch.insert(wstring(1, c)); g_suScroll = 0; caretReset(); redraw(); }
}
