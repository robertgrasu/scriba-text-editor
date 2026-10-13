// exec.inc: runs a command (menu, key, palette). Included by main.cpp after edit_cmds.inc.

static void layoutMenus();
static void applyChrome() {
  static const float STY_TITLE[4] = {30, 32, 34, 34}, STY_MENU[4] = {22, 26, 30, 0}, STY_TAB[4] = {34, 36, 44, 40}, STY_STATUS[4] = {24, 24, 30, 28}, STY_CRUMB[4] = {0, 26, 30, 0};
  int s = std::max(0, std::min(3, g_uiStyle));
  TITLE_H = STY_TITLE[s];
  MENU_H = g_distFree ? TITLE_H : TITLE_H + STY_MENU[s];
  TAB_H = g_distFree ? 0.f : STY_TAB[s];
  g_crumbH = g_distFree ? 0.f : STY_CRUMB[s];
  STATUS_H = (g_statusOn && !g_distFree) ? STY_STATUS[s] : 0.f;
  if (s != 3) g_zenMenu = false;
  layoutMenus();
  computeLayout();
  ensureVisible();
  redraw();
}
static DWORD g_accentOverride = 0;
static bool g_hasAccent = false;
static void amxFuncList(); static void amxTree(); static void amxSearchAll(); static void amxPickStyle(); static void amxNewPlugin(); static void amxNewInclude(); static void hudOpen(); static void amxPickLook(int which); static void amxxUncompress(); static void amxxLysis();  // amxx_tools.inc, hud_editor.inc
static void applyScheme(int i) {
  g_theme = std::max(0, std::min(i, (int)(sizeof(THEMES) / sizeof(THEMES[0])) - 1));
  g_curTheme = THEMES[g_theme];
  if (g_hasAccent) g_curTheme.accent = g_curTheme.caret = g_curTheme.bookmark = g_accentOverride;
  // the lines and borders of the window are a soft step from the background towards the text colour (the schemes had a near-black one, which looked harsh on every scheme)
  g_curTheme.border = mixc(g_curTheme.bg, g_curTheme.fg, g_curTheme.dark ? 0.17f : 0.2f);
  BOOL dark = T().dark;
  DwmSetWindowAttribute(g_hwnd, 20, &dark, sizeof(dark));
  COLORREF cc = RGB((T().chrome >> 16) & 255, (T().chrome >> 8) & 255, T().chrome & 255);
  DwmSetWindowAttribute(g_hwnd, 35, &cc, sizeof(cc));
  COLORREF tc = RGB((T().chromeFg >> 16) & 255, (T().chromeFg >> 8) & 255, T().chromeFg & 255);
  DwmSetWindowAttribute(g_hwnd, 36, &tc, sizeof(tc));
  redraw();
}
static void chooseScheme(int i) { setSetting("color_scheme", toUtf8(THEMES[i].name)); }
static void pickScheme() {
  int orig = g_theme;
  vector<PItem> all;
  for (int i = 0; i < (int)(sizeof(THEMES) / sizeof(THEMES[0])); i++) {
    PItem it;
    it.label = THEMES[i].name;
    it.right = i == orig ? L"current" : (THEMES[i].dark ? L"dark" : L"light");
    it.cmd = 0;
    it.run = [i] { chooseScheme(i); };
    all.push_back(it);
  }
  openPick(L"Select color scheme", listProvider(all), [orig] { if (g_theme != orig) applyScheme(orig); },
           [all](const PItem& it) { for (size_t i = 0; i < all.size(); i++) if (all[i].label == it.label) applyScheme((int)i); });
}

// pushes settings.json into the editor: scheme, fonts, indentation defaults, view toggles, editing options
static void applySettingsNow(bool previewOnly) {
  (void)previewOnly;
  static bool first = true;
  static int lastSize = -1;
  static bool lastSb = true, lastMap = true;
  // colour scheme and accent
  wstring cs = sStr("color_scheme");
  int idx = 0;
  for (int i = 0; i < (int)(sizeof(THEMES) / sizeof(THEMES[0])); i++) if (cs == THEMES[i].name) idx = i;
  wstring ac = sStr("accent_color");
  g_hasAccent = ac.size() == 7 && ac[0] == L'#';
  if (g_hasAccent) g_accentOverride = (DWORD)wcstoul(ac.c_str() + 1, nullptr, 16);
  applyScheme(idx);
  // fonts
  wstring face = sStr("font_face");
  if (face.empty()) face = L"Consolas";
  int fs = std::max(7, std::min(48, sInt("font_size")));
  bool fontChanged = first || face != g_fontFace || fs != lastSize;
  g_fontFace = face;
  if (first) g_fontSize = (float)(g_sessionFont ? g_sessionFont : fs);
  else if (fs != lastSize) g_fontSize = (float)fs;
  lastSize = fs;
  if (fontChanged) makeFonts();
  // defaults for new files
  g_defTab = std::max(1, std::min(16, sInt("tab_size")));
  g_defSpaces = sBool("translate_tabs_to_spaces");
  g_detectIndent = sBool("detect_indentation");
  g_wrap = sBool("word_wrap");
  g_guides = sBool("indent_guides");
  g_brackets = sBool("bracket_colors");
  g_gutterOn = sBool("gutter");
  g_lineNums = sBool("line_numbers");
  g_hlLine = sBool("highlight_line");
  wstring ws = sStr("draw_white_space");
  g_wsMode = ws == L"all" ? 2 : ws == L"selection" ? 1 : 0;
  g_rulerCols.clear();
  if (g_settings["rulers"].is_array()) for (auto& r : g_settings["rulers"]) if (r.is_number()) g_rulerCols.push_back((int)r.get<double>());
  g_ruler = g_rulerCols.empty() ? 80 : g_rulerCols[0];
  g_scrollPastEnd = sBool("scroll_past_end");
  g_autoComplete = sBool("auto_complete");
  g_autoClose = sBool("auto_close_brackets");
  g_matchBr = sBool("match_brackets");
  g_trimOnSave = sBool("trim_trailing_white_space_on_save");
  g_eofNewline = sBool("ensure_newline_at_eof_on_save");
  g_spellOn = sBool("spell_check");
  {
    wstring us = sStr("ui_style");
    int ns = us == L"Refined" ? 1 : us == L"Modern" ? 2 : us == L"Zen" ? 3 : 0;
    if (ns != g_uiStyle) { g_uiStyle = ns; applyChrome(); }
  }
  g_notesVisual = sBool("notes_visual");
  g_vLines = sBool("visual_line_numbers");
  g_vSyms = sBool("visual_show_syntax");
  g_visualMaxW = sInt("visual_max_width");
  wstring as = sStr("auto_save");
  g_autoSave = as == L"after delay" ? 1 : as == L"on focus loss" ? 2 : 0;
  g_autoSaveDelay = std::max(1, sInt("auto_save_delay"));
  g_folderEx = sList("folder_exclude_patterns");
  g_fileEx = sList("file_exclude_patterns");
  g_binaryPat = sList("binary_file_patterns");
  bool sb = sBool("show_sidebar"), mp = sBool("show_minimap");
  if (first && !g_sessionUi) { g_sbVisible = sb; g_mapVisible = mp; }
  else if (!first) { if (sb != lastSb) g_sbVisible = sb; if (mp != lastMap) g_mapVisible = mp; }
  lastSb = sb;
  lastMap = mp;
  first = false;
  g_projScanned = false;
  for (auto* d : g_docs) d->invalidate(0);
  if (!g_docs.empty()) { computeLayout(); ensureVisible(); }
  redraw();
}

static void suOpen(int tab);  // the Settings page (settings_ui.inc)
static void fifOpenPanel(const wstring& text);  // the panels under the editor (panel.inc)
static bool fifNext(int dir);
static void runOpenPanel(const wstring& prefill);
static void runStop();
static void pickRecent();  // files.inc
static void reopenClosed();
static void newWindow();
static void saveProject();
static void openProject();
static void addFolderDialog();
static void removeFolders();
static void revealActive();
static void ccCommand(int cmd);  // claude_ui.inc
static void toggleMdPreview();  // mdpreview.inc
static bool imageLineActive(const Doc& d);  // widgets.inc
static void imageLineEscape(Doc& d);
// a picture on the clipboard (and no text) pasted into a Markdown file or note: saved as attachments\pasted-<time>.png and linked
static bool pasteClipboardImage() {
  Doc& d = D();
  if (d.lang != L_MD || !OpenClipboard(g_hwnd)) return false;
  HBITMAP hb = nullptr;
  if (!IsClipboardFormatAvailable(CF_UNICODETEXT) && IsClipboardFormatAvailable(CF_BITMAP))
    if (HANDLE h = GetClipboardData(CF_BITMAP)) hb = (HBITMAP)CopyImage(h, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
  CloseClipboard();
  if (!hb) return false;
  wstring dir = (d.path.empty() ? notesDir() : d.path.substr(0, d.path.find_last_of(L'\\'))) + L"\\attachments";
  CreateDirectoryW(dir.c_str(), nullptr);
  wstring base = L"pasted-" + std::to_wstring(GetTickCount64()) + L".png";
  wstring file = dir + L"\\" + base;
  bool ok = false;
  IWICImagingFactory* f = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) && f) {
    IWICBitmap* bmp = nullptr;
    IWICStream* st = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* fr = nullptr;
    if (SUCCEEDED(f->CreateBitmapFromHBITMAP(hb, nullptr, WICBitmapIgnoreAlpha, &bmp)) && SUCCEEDED(f->CreateStream(&st)) && SUCCEEDED(st->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) &&
        SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && SUCCEEDED(enc->Initialize(st, WICBitmapEncoderNoCache)) && SUCCEEDED(enc->CreateNewFrame(&fr, nullptr)) &&
        SUCCEEDED(fr->Initialize(nullptr))) {
      UINT w = 0, h = 0;
      bmp->GetSize(&w, &h);
      WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR;
      fr->SetSize(w, h);
      fr->SetPixelFormat(&pf);
      ok = SUCCEEDED(fr->WriteSource(bmp, nullptr)) && SUCCEEDED(fr->Commit()) && SUCCEEDED(enc->Commit());
    }
    if (fr) fr->Release();
    if (enc) enc->Release();
    if (st) st->Release();
    if (bmp) bmp->Release();
    f->Release();
  }
  DeleteObject(hb);
  if (!ok) return false;
  fmtInsertImage(L"![pasted image](attachments/" + base + L")");
  return true;
}
static void foldersChanged();
static void recentClear();
static bool lspGotoDefinition();  // lsp.inc
static void lspFindReferences();
static void lspRename();
static void lspFormat();
static void lspShowProblems();
static void lspShowStatus();
static void lspRestart();
static void amxxCompile();  // amxx.inc
static bool amxxGotoDefinition();
static void amxxChooseCompiler();
static void amxxPickProfile();
static void amxxHide();
static void gitNextChange(int dir);  // git.inc
static void gitRevertChange();
static void gitShowDiff();
static void gitChangedFiles();
static void gitStatusRefresh();
static void exec(int cmd);
static vector<KeyChord> g_pending;  // chords of a multi-key binding typed so far
// looks the pressed key up in the key map; true when it was handled (or is the first key of a chord)
static bool handleKeymap(const KeyChord& c) {
  vector<KeyChord> seq = g_pending;
  seq.push_back(c);
  bool prefix = false;
  const Binding* hit = nullptr;
  for (auto& b : g_keymap) {
    if (b.chords.size() < seq.size()) continue;
    bool match = true;
    for (size_t i = 0; i < seq.size(); i++) if (!(b.chords[i] == seq[i])) { match = false; break; }
    if (!match) continue;
    const CmdDef* def = cmdDef(b.cmd);
    if (def && (def->flags & 1) && D().lang != L_MD) continue;  // Markdown-only command
    if (b.chords.size() == seq.size()) { if (!hit) hit = &b; }
    else prefix = true;
  }
  if (hit) {
    int cmd = hit->cmd;
    if (!g_pending.empty()) { g_pending.clear(); g_statusMsg.clear(); }
    if (!cmdAvailable(cmd)) return true;
    // the keys that control the macros are not part of the macro; Ctrl+V is recorded as the pasted text
    if (g_recording && !g_macroRec.empty() && (cmd == C_MACROTOGGLE || cmd == C_MACROPLAY || cmd == C_MACROSAVE || cmd == C_MACRORUN || cmd == C_PASTE)) g_macroRec.pop_back();
    exec(cmd);
    return true;
  }
  if (prefix) {
    g_pending = seq;
    flash(keysText(seq) + L" was pressed. Waiting for the next key of the chord...");
    return true;
  }
  if (!g_pending.empty()) { g_pending.clear(); g_statusMsg.clear(); redraw(); return true; }  // a chord that leads nowhere: swallow the key
  return false;
}

static bool licBlock(int cmd);
static void licOpen();
static void ccTrustFolder();  // claude_chat.inc
static std::map<const Doc*, Pos> g_marks;  // Ctrl+K Ctrl+Space: one mark per file
static void cmdFindUnder(int dir) {
  Doc& d = D();
  if (!d.hasSel()) {
    const wstring& s = d.lines[d.cur.l];
    int a = d.cur.c, b = d.cur.c;
    while (a > 0 && isWordCh(s[a - 1])) a--;
    while (b < (int)s.size() && isWordCh(s[b])) b++;
    if (a == b) { flash(L"No word under the caret"); return; }
    d.anc = {d.cur.l, a};
    d.cur = {d.cur.l, b};
  }
  if (d.selA().l != d.selB().l) { flash(L"Find Under works on one line"); return; }
  // a plain search of the word, whole words only, wrapping around; the find bar is not needed
  wstring w = d.selectedText();
  auto whole = [&](const wstring& s, size_t p) { return (p == 0 || !isWordCh(s[p - 1])) && (p + w.size() >= s.size() || !isWordCh(s[p + w.size()])); };
  int n = (int)d.lines.size();
  Pos from = dir > 0 ? d.selB() : d.selA();
  for (int k = 0; k <= n; k++) {
    int l = ((dir > 0 ? from.l + k : from.l - k) % n + n) % n;
    const wstring& s = d.lines[l];
    size_t p = wstring::npos;
    if (dir > 0) {
      size_t start = k == 0 ? from.c : 0;
      for (size_t q = s.find(w, start); q != wstring::npos; q = s.find(w, q + 1)) if (whole(s, q)) { p = q; break; }
      if (k == n && p != wstring::npos && (int)p >= from.c) p = wstring::npos;
    } else {
      size_t limit = k == 0 ? (size_t)from.c : s.size() + 1;
      for (size_t q = s.find(w); q != wstring::npos && q + w.size() <= limit; q = s.find(w, q + 1)) if (whole(s, q)) p = q;
      if (k == n && p != wstring::npos && (int)p < from.c) p = wstring::npos;
    }
    if (p == wstring::npos) continue;
    d.anc = {l, (int)p};
    d.cur = {l, (int)(p + w.size())};
    revealCenter();
    moved(true);
    return;
  }
}
static void cmdMark(int what) {  // 0 set, 1 select to mark, 2 swap, 3 delete to mark
  Doc& d = D();
  auto it = g_marks.find(&d);
  if (what == 0) { g_marks[&d] = d.cur; flash(L"Mark set"); return; }
  if (it == g_marks.end()) { flash(L"No mark set (Ctrl+K Ctrl+Space)"); return; }
  Pos m = it->second;
  m.l = std::min(m.l, (int)d.lines.size() - 1);
  m.c = std::min(m.c, (int)d.lines[m.l].size());
  if (what == 1) { d.anc = m; moved(true); }
  else if (what == 2) { it->second = d.cur; d.cur = d.anc = m; moved(false); }
  else {
    Pos a = m < d.cur ? m : d.cur, b = m < d.cur ? d.cur : m;
    if (a == b) return;
    d.snapshot(0);
    d.replaceRange(a, b, L"");
    d.cur = d.anc = a;
    edited();
  }
}
static void cmdSwitchFile() {
  Doc& d = D();
  static const wchar_t* SRC[] = {L"cpp", L"cxx", L"cc", L"c", L"m", L"mm"};
  static const wchar_t* HDR[] = {L"h", L"hpp", L"hxx", L"hh", L"ipp", L"inl"};
  size_t dot = d.path.rfind(L'.'), sl = d.path.find_last_of(L"\\/");
  if (d.path.empty() || dot == wstring::npos || (sl != wstring::npos && dot < sl)) { flash(L"This file has no counterpart"); return; }
  wstring ext = lowerStr(d.path.substr(dot + 1)), stem = d.path.substr(0, dot + 1);
  bool isSrc = false, isHdr = false;
  for (auto* e : SRC) if (ext == e) isSrc = true;
  for (auto* e : HDR) if (ext == e) isHdr = true;
  if (!isSrc && !isHdr) { flash(L"This file has no counterpart"); return; }
  for (auto* e : isSrc ? HDR : SRC) {
    wstring cand = stem + e;
    if (GetFileAttributesW(cand.c_str()) != INVALID_FILE_ATTRIBUTES) { openPath(cand); return; }
  }
  flash(isSrc ? L"No header next to this file" : L"No source file next to this file");
}
// inserts the closing tag of the innermost element that is still open before the caret
static void cmdCloseTag() {
  Doc& d = D();
  static const wchar_t* VOID_TAGS[] = {L"br", L"hr", L"img", L"input", L"meta", L"link", L"area", L"base", L"col", L"embed", L"source", L"track", L"wbr", L"param"};
  vector<wstring> stack;
  wstring txt;
  for (int l = 0; l <= d.cur.l; l++) txt += (l == d.cur.l ? d.lines[l].substr(0, d.cur.c) : d.lines[l] + L"\n");
  size_t i = 0, n = txt.size();
  while (i < n) {
    if (txt[i] != L'<') { i++; continue; }
    if (txt.compare(i, 4, L"<!--") == 0) { size_t e = txt.find(L"-->", i + 4); if (e == wstring::npos) return; i = e + 3; continue; }
    if (i + 1 < n && (txt[i + 1] == L'!' || txt[i + 1] == L'?')) { size_t e = txt.find(L'>', i); if (e == wstring::npos) break; i = e + 1; continue; }
    bool closing = i + 1 < n && txt[i + 1] == L'/';
    size_t j = i + (closing ? 2 : 1), k = j;
    while (k < n && (iswalnum(txt[k]) || txt[k] == L'-' || txt[k] == L'_' || txt[k] == L':' || txt[k] == L'.')) k++;
    if (k == j) { i++; continue; }
    wstring name = txt.substr(j, k - j), low = lowerStr(name);
    wchar_t q = 0;
    size_t e = k;
    for (; e < n; e++) {  // to the closing '>', skipping quoted attribute values
      if (q) { if (txt[e] == q) q = 0; }
      else if (txt[e] == L'"' || txt[e] == L'\'') q = txt[e];
      else if (txt[e] == L'>') break;
    }
    if (e >= n) break;  // the tag the caret is inside of
    bool selfClosed = e > 0 && txt[e - 1] == L'/';
    bool isVoid = false;
    for (auto* v : VOID_TAGS) if (low == v) isVoid = true;
    if (closing) {
      for (int s = (int)stack.size() - 1; s >= 0; s--) if (lowerStr(stack[s]) == low) { stack.resize(s); break; }
    } else if (!selfClosed && !isVoid) stack.push_back(name);
    i = e + 1;
  }
  if (stack.empty()) { flash(L"No open tag to close"); return; }
  multi([&] { D().insertText(L"</" + stack.back() + L">", 0); });
  edited();
}
static void cmdPasteHistory() {
  if (g_clipHist.empty()) { flash(L"Nothing copied yet"); return; }
  vector<PItem> all;
  for (auto& t : g_clipHist) {
    PItem it;
    wstring one;
    for (wchar_t c : t) { if (c == L'\n' || c == L'\r') { if (!one.empty() && one.back() != L' ') one += L' '; } else if (c == L'\t') one += L' '; else one += c; if (one.size() > 90) break; }
    it.label = one;
    int lines = 1;
    for (wchar_t c : t) if (c == L'\n') lines++;
    it.right = lines > 1 ? std::to_wstring(lines) + L" lines" : std::to_wstring(t.size()) + L" chars";
    wstring tt = t;
    it.run = [tt] { multi([&] { D().insertText(tt, 0); }); edited(); };
    all.push_back(it);
  }
  openPick(L"Paste from history", listProvider(all));
}
static void cmdScrollLine(int dir) {
  Doc& d = D();
  d.scrollY = std::max(0.f, std::min(d.scrollY + dir * g_lineH, maxScrollY()));
  redraw();
}
static void exec(int cmd) {
  if (licBlock(cmd)) { flash(L"Activate your license first."); licOpen(); return; }  // nothing runs without a license
  Doc& d = D();
  switch (cmd) {
    case C_LICENSE: licOpen(); break;
    case C_CLAUDETRUST: ccTrustFolder(); break;
    case C_FINDUNDER: cmdFindUnder(1); break;
    case C_FINDUNDERPREV: cmdFindUnder(-1); break;
    case C_REPLACENEXT: if (!g_findOpen || !g_replOpen) openFind(true); else replaceOne(); break;
    case C_SETMARK: cmdMark(0); break;
    case C_SELTOMARK: cmdMark(1); break;
    case C_SWAPMARK: cmdMark(2); break;
    case C_DELTOMARK: cmdMark(3); break;
    case C_SHOWCENTER: revealCenter(); redraw(); break;
    case C_SCROLLUP: cmdScrollLine(-1); break;
    case C_SCROLLDOWN: cmdScrollLine(1); break;
    case C_SWITCHFILE: cmdSwitchFile(); break;
    case C_CLOSETAG: cmdCloseTag(); break;
    case C_OVERWRITE: g_overwrite = !g_overwrite; caretReset(); flash(g_overwrite ? L"Overwrite mode on (Insert to turn it off)" : L"Overwrite mode off"); break;
    case C_JUMPBACK: cmdJump(-1); break;
    case C_JUMPFWD: cmdJump(1); break;
    case C_PASTEHIST: cmdPasteHistory(); break;
    case C_FOCUSG1: case C_FOCUSG2:
      if (g_layout == 0) flash(L"Only one group: use View > Layout to split"); else focusGroup(cmd == C_FOCUSG1 ? 0 : 1);
      break;
    case C_MOVEG1: case C_MOVEG2: {
      int dst = cmd == C_MOVEG1 ? 0 : 1;
      if (g_layout == 0 && dst == 0) break;
      if (g_layout == 0 || d.group != dst) moveTabToOtherGroup();
      break;
    }
    case C_NEWPANE: if (g_layout == 0) setLayout(1); else flash(L"Already two groups"); break;
    case C_CLOSEPANE: if (g_layout != 0) setLayout(0); break;
    case C_FOLDL2: case C_FOLDL3: case C_FOLDL4: case C_FOLDL5: case C_FOLDL6: case C_FOLDL7: case C_FOLDL8: case C_FOLDL9:
      cmdFoldLevel(cmd - C_FOLDL2 + 2);
      break;
    case C_NEW: addDoc(new Doc()); break;
    case C_OPEN: {
      wchar_t buf[MAX_PATH * 40] = {};
      OPENFILENAMEW o{sizeof(o)};
      o.hwndOwner = g_hwnd;
      o.lpstrFile = buf;
      o.nMaxFile = (DWORD)std::size(buf);
      o.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
      if (!GetOpenFileNameW(&o)) break;
      wstring first = buf;
      wchar_t* nx = buf + first.size() + 1;
      if (!*nx) openPath(first);
      else for (; *nx; nx += wcslen(nx) + 1) openPath(first + L"\\" + nx);
      break;
    }
    case C_SAVE: doSave(d); updateTitle(); redraw(); break;
    case C_SAVEAS: doSaveAs(d); updateTitle(); redraw(); break;
    case C_SAVEENC: pickEncoding(); break;
    case C_CLOSE: closeDoc(g_active); break;
    case C_EXIT: case C_CLOSEWINDOW: PostMessageW(g_hwnd, WM_CLOSE, 0, 0); break;
    case C_UNDO: d.doUndo(); edited(); break;
    case C_REDO: d.doRedo(); edited(); break;
    case C_COPY:
      if (!d.extra.empty()) {
        vector<std::pair<Pos, wstring>> parts;
        auto add = [&](Pos c, Pos a) { Pos x = c < a ? c : a, y = c < a ? a : c; if (!(x == y)) parts.push_back({x, d.textOf(x, y)}); };
        add(d.cur, d.anc);
        for (auto& e : d.extra) add(e.c, e.a);
        std::sort(parts.begin(), parts.end(), [](auto& p, auto& q) { return p.first < q.first; });
        wstring j;
        for (size_t i = 0; i < parts.size(); i++) { if (i) j += L"\n"; j += parts[i].second; }
        clipSet(j);
      }
      else if (d.hasSel()) clipSet(d.selectedText());
      else clipSet(d.lines[d.cur.l] + L"\n");
      break;
    case C_CUT:
      if (!d.extra.empty()) {
        exec(C_COPY);
        multi([&] { if (D().hasSel()) D().replaceRange(D().selA(), D().selB(), L""); });
      }
      else if (d.hasSel()) { clipSet(d.selectedText()); d.snapshot(0); d.replaceRange(d.selA(), d.selB(), L""); }
      else { clipSet(d.lines[d.cur.l] + L"\n"); delLine(); return; }
      edited();
      break;
    case C_PASTE: {
      if (imageLineActive(d)) imageLineEscape(d);
      if (pasteClipboardImage()) break;
      wstring t = clipGet();
      if (!t.empty()) { macroRecordPaste(t); multi([&] { D().insertText(t, 0); }); edited(); }
      break;
    }
    case C_MACROTOGGLE: if (g_recording) macroStop(); else macroStart(); break;
    case C_MACROPLAY: macroPlay(g_macro, 1); break;
    case C_MACROSAVE: macroSaveAs(); break;
    case C_MACRORUN: macroRunSaved(); break;
    case C_PASTEINDENT: cmdPasteIndent(); break;
    case C_SELALL: d.anc = {0, 0}; d.cur = {(int)d.lines.size() - 1, (int)d.lines.back().size()}; moved(true); break;
    case C_DUP: dupLine(); break;
    case C_DELLINE: delLine(); break;
    case C_DELEOL: cmdDeleteEol(); break;
    case C_DELBOL: cmdDeleteBol(); break;
    case C_TRANSPOSE: cmdTranspose(); break;
    case C_NEXTTAB: case C_PREVTAB: case C_NEXTTAB2: case C_PREVTAB2: {
      int n = (int)g_docs.size(), step = (cmd == C_NEXTTAB || cmd == C_NEXTTAB2) ? 1 : n - 1;
      for (int k = 1; k <= n; k++) {
        int idx = (g_active + k * step) % n;
        if (g_docs[idx]->group == g_curGroup) { activateDoc(idx); break; }
      }
      break;
    }
    case C_TAB1: case C_TAB2: case C_TAB3: case C_TAB4: case C_TAB5: case C_TAB6: case C_TAB7: case C_TAB8: case C_TAB9: {
      vector<int> mine;
      for (int i = 0; i < (int)g_docs.size(); i++) if (g_docs[i]->group == g_curGroup) mine.push_back(i);
      if (mine.empty()) break;
      int n = cmd - C_TAB1;
      activateDoc(n == 8 ? mine.back() : (n < (int)mine.size() ? mine[n] : mine.back()));
      break;
    }
    case C_LAYOUT1: setLayout(0); break;
    case C_LAYOUT2: setLayout(1); break;
    case C_LAYOUT3: setLayout(2); break;
    case C_OTHERGROUP: if (g_layout != 0) focusGroup(1 - g_curGroup); else flash(L"Only one group: use View > Layout to split"); break;
    case C_MOVETAB: moveTabToOtherGroup(); break;
    case C_PICKSCHEME: pickScheme(); break;
    case C_NOTECOLORPICK:
      if (d.isNote) {
        wstring id = d.noteId;
        wstring cur = g_meta[id].color;
        vector<PItem> items;
        auto add = [&](const wstring& label, const wstring& col) {
          PItem it;
          it.label = label;
          it.right = col == cur ? L"current" : L"";
          it.run = [id, col] { g_meta[id].color = col; saveMeta(); refreshNotes(); redraw(); };
          items.push_back(it);
        };
        add(L"None", L"");
        for (auto& c : NOTE_PALETTE) add(capFirst(c.name), c.name);
        openPick(L"Note color", listProvider(items));
      }
      break;
    case C_CLOSEOTHERS:
      for (int i = (int)g_docs.size() - 1; i >= 0; i--) {
        if (i >= (int)g_docs.size() || g_docs[i] == &d || g_docs[i]->group != g_curGroup) continue;
        if (!closeDoc(i)) break;
      }
      break;
    case C_CLOSEALL:
      for (int i = (int)g_docs.size() - 1; i >= 0; i--) {
        if (i >= (int)g_docs.size() || g_docs[i]->group != g_curGroup) continue;
        if (!closeDoc(i)) break;
      }
      break;
    case C_JOIN:
      if (d.cur.l + 1 < (int)d.lines.size()) {
        d.snapshot(0);
        int l = d.cur.l;
        wstring nx = d.lines[l + 1];
        size_t k = 0;
        while (k < nx.size() && (nx[k] == L' ' || nx[k] == L'\t')) k++;
        int at = (int)d.lines[l].size();
        d.lines[l] += (at && !nx.empty() ? L" " : L"") + nx.substr(k);
        d.lines.erase(d.lines.begin() + l + 1);
        d.cur = d.anc = {l, at};
        d.dirty = true;
        d.invalidate(l);
        edited();
      }
      break;
    case C_SWAPUP: case C_SWAPDOWN: {
      int dir = cmd == C_SWAPUP ? -1 : 1;
      int a = d.selA().l, b = d.selB().l;
      if (a + dir < 0 || b + dir >= (int)d.lines.size()) break;
      d.snapshot(0);
      if (dir < 0) { wstring mv = d.lines[a - 1]; d.lines.erase(d.lines.begin() + a - 1); d.lines.insert(d.lines.begin() + b, mv); }
      else { wstring mv = d.lines[b + 1]; d.lines.erase(d.lines.begin() + b + 1); d.lines.insert(d.lines.begin() + a, mv); }
      d.cur.l += dir;
      d.anc.l += dir;
      d.dirty = true;
      d.invalidate(std::min(a, a + dir));
      edited();
      break;
    }
    case C_INSAFTER: case C_INSBEFORE: {
      d.snapshot(0);
      int l = d.cur.l;
      wstring ind;
      for (wchar_t c : d.lines[l]) { if (c == L' ' || c == L'\t') ind += c; else break; }
      int at = cmd == C_INSAFTER ? l + 1 : l;
      d.lines.insert(d.lines.begin() + at, ind);
      d.cur = d.anc = {at, (int)ind.size()};
      d.dirty = true;
      d.invalidate(l);
      edited();
      break;
    }
    case C_COMMENT: {
      const wchar_t* pre = isClike(d.lang) ? L"// " : (d.lang == L_PY || d.lang == L_SH) ? L"# " : d.lang == L_HTML ? L"<!-- " : nullptr;
      if (!pre) break;
      size_t pl = wcslen(pre);
      Pos a = d.selA(), b = d.selB();
      int l1 = (b.c == 0 && b.l > a.l) ? b.l - 1 : b.l;
      bool all = true;
      for (int l = a.l; l <= l1; l++) {
        const wstring& s = d.lines[l];
        size_t k = 0;
        while (k < s.size() && (s[k] == L' ' || s[k] == L'\t')) k++;
        if (!s.empty() && s.compare(k, wcslen(pre) - 1, pre, wcslen(pre) - 1) != 0) all = false;
      }
      bool html = d.lang == L_HTML;
      fmtLinePrefix([&](const wstring& s, int) {
        size_t k = 0;
        while (k < s.size() && (s[k] == L' ' || s[k] == L'\t')) k++;
        if (all) {
          if (html) {
            wstring r = s.substr(0, k) + s.substr(k + pl - (s.compare(k, pl, pre) == 0 ? 0 : 1));
            size_t e = r.rfind(L"-->");
            if (e != wstring::npos) { r.erase(e, 3); while (!r.empty() && r.back() == L' ') r.pop_back(); }
            return r;
          }
          if (s.compare(k, pl, pre) == 0) return s.substr(0, k) + s.substr(k + pl);
          if (s.compare(k, pl - 1, pre, pl - 1) == 0) return s.substr(0, k) + s.substr(k + pl - 1);
          return s;
        }
        return s.empty() ? s : s.substr(0, k) + pre + s.substr(k) + (html ? L" -->" : L"");
      });
      break;
    }
    case C_BLOCKCOMMENT: cmdBlockComment(); break;
    case C_INDENT: { wstring u = indentUnit(d); fmtLinePrefix([u](const wstring& s, int) { return u + s; }); break; }
    case C_UNINDENT:
      fmtLinePrefix([](const wstring& s, int) {
        if (!s.empty() && s[0] == L'\t') return s.substr(1);
        size_t k = 0;
        while (k < (size_t)D().tabW && k < s.size() && s[k] == L' ') k++;
        return s.substr(k);
      });
      break;
    case C_REINDENT: cmdReindent(); break;
    case C_WRAPPARA: cmdWrapParagraph(); break;
    case C_SORT: cmdSortLines(false); break;
    case C_SORTCI: cmdSortLines(true); break;
    case C_UPPER: cmdCase(0); break;
    case C_LOWER: cmdCase(1); break;
    case C_TITLECASE: cmdCase(2); break;
    case C_TRIMWS: {
      d.snapshot(0);
      for (auto& ln : d.lines) while (!ln.empty() && (ln.back() == L' ' || ln.back() == L'\t')) ln.pop_back();
      d.cur.c = std::min(d.cur.c, (int)d.lines[d.cur.l].size());
      d.anc = d.cur;
      d.dirty = true;
      d.invalidate(0);
      edited();
      break;
    }
    case C_COMPLETE: compUpdate(true); break;
    case C_VLINES: g_vLines = !g_vLines; ensureVisible(); redraw(); break;
    case C_VSYMS: g_vSyms = !g_vSyms; d.invalidate(0); ensureVisible(); redraw(); break;
    case C_USESEL: openFind(false); break;
    case C_SBOUTLINE: g_sbVisible = true; g_sbView = 2; redraw(); break;
    case C_ZOOMIN: g_fontSize = std::min(48.f, g_fontSize + 1); makeFonts(); ensureVisible(); redraw(); break;
    case C_ZOOMOUT: g_fontSize = std::max(8.f, g_fontSize - 1); makeFonts(); ensureVisible(); redraw(); break;
    case C_ZOOM0: g_fontSize = 15.f; makeFonts(); ensureVisible(); redraw(); break;
    case C_STATUSBAR: g_statusOn = !g_statusOn; applyChrome(); break;
    case C_DISTFREE: {
      static bool sbWas = true, mapWas = true;
      if (!g_distFree) { sbWas = g_sbVisible; mapWas = g_mapVisible; g_sbVisible = false; g_mapVisible = false; }
      else { g_sbVisible = sbWas; g_mapVisible = mapWas; }
      g_distFree = !g_distFree;
      applyChrome();
      break;
    }
    case C_FULLSCREEN: {
      static WINDOWPLACEMENT wpPrev{sizeof(WINDOWPLACEMENT)};
      static bool full = false;
      if (!full) {
        GetWindowPlacement(g_hwnd, &wpPrev);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        ShowWindow(g_hwnd, SW_RESTORE);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
      } else {
        SetWindowPlacement(g_hwnd, &wpPrev);
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
      }
      full = !full;
      break;
    }
    case C_TABLEFT: case C_TABRIGHT: {
      int step = cmd == C_TABLEFT ? -1 : 1, j = g_active + step;
      while (j >= 0 && j < (int)g_docs.size() && g_docs[j]->group != g_curGroup) j += step;
      if (j < 0 || j >= (int)g_docs.size()) break;
      std::swap(g_docs[g_active], g_docs[j]);
      for (int g = 0; g < 2; g++) { if (g_groupActive[g] == g_active) g_groupActive[g] = j; else if (g_groupActive[g] == j) g_groupActive[g] = g_active; }
      g_active = j;
      redraw();
      break;
    }
    case C_REFRESHFOLDER:
      if (g_root) {
        vector<wstring> ps;
        for (auto* r : g_roots) ps.push_back(r->path);
        bool vis = g_sbVisible;
        clearRoots();
        for (auto& p : ps) addRoot(p);
        g_sbVisible = vis;
        redraw();
      }
      break;
    case C_SELWORD: {
      if (!d.hasSel()) {
        const wstring& s = d.lines[d.cur.l];
        int a = d.cur.c, b = d.cur.c;
        while (a > 0 && isWordCh(s[a - 1])) a--;
        while (b < (int)s.size() && isWordCh(s[b])) b++;
        if (a != b) { d.anc = {d.cur.l, a}; d.cur = {d.cur.l, b}; moved(true); }
      }
      break;
    }
    case C_EXPANDSCOPE: cmdExpandScope(); break;
    case C_SELBRACKETS: cmdSelectBrackets(); break;
    case C_GOTOBRACKET: cmdGotoBracket(); break;
    case C_SKIPOCC: cmdSkipOccurrence(); break;
    case C_FOLD: cmdFold(); break;
    case C_UNFOLD: cmdUnfold(); break;
    case C_FOLDALL: cmdFoldAll(); break;
    case C_UNFOLDALL: cmdUnfoldAll(); break;
    case C_BMTOGGLE: bmToggle(); break;
    case C_BMNEXT: bmGoto(1); break;
    case C_BMPREV: bmGoto(-1); break;
    case C_BMSELECT: bmSelectAll(); break;
    case C_BMCLEAR: bmClear(); break;
    case C_PICKSYNTAX: pickSyntax(); break;
    case C_PICKINDENT: pickIndent(); break;
    case C_PICKEOL: pickEol(); break;
    case C_TOSPACES: convertIndent(true); break;
    case C_TOTABS: convertIndent(false); break;
    case C_GUESSINDENT: guessIndent(d); d.invalidate(0); redraw(); break;
    case C_GOTOSYMBOL: openPalette(L"@"); break;
    case C_FIF: fifOpenPanel(d.hasSel() && d.selA().l == d.selB().l ? d.selectedText() : L""); break;
    case C_NEXTRES: if (!fifNext(1)) flash(L"No search results: use Find in Files first"); break;
    case C_PREVRES: if (!fifNext(-1)) flash(L"No search results: use Find in Files first"); break;
    case C_RUNCMD: runOpenPanel(L""); break;
    case C_RUNSTOP: runStop(); break;
    case C_AMXCOMPILE: amxxCompile(); break;
    case C_AMXGOTODEF: if (!amxxGotoDefinition()) flash(L"Go to Definition works in Pawn (.sma) files"); break;
    case C_GOTODEF: jumpPush(); if (!amxxGotoDefinition() && !lspGotoDefinition()) flash(L"No language server for this file. Install one (see Preferences: LSP) or use a Pawn file for AMX Mod X."); break;
    case C_LSPREFS: lspFindReferences(); break;
    case C_LSPRENAME: lspRename(); break;
    case C_LSPFORMAT: lspFormat(); break;
    case C_LSPPROBLEMS: lspShowProblems(); break;
    case C_LSPSTATUS: lspShowStatus(); break;
    case C_LSPRESTART: lspRestart(); flash(L"Language servers restarted"); break;
    case C_AMXCOMPILER: amxxChooseCompiler(); break;
    case C_AMXPROFILE: amxxPickProfile(); break;
    case C_AMXCLOSE: amxxHide(); break;
    case C_AMXFUNCS: amxFuncList(); break;
    case C_AMXHUD: hudOpen(); break;
    case C_AMXUNCOMP: amxxUncompress(); break;
    case C_AMXLYSIS: amxxLysis(); break;
    case C_AMXPOPUP: amxPickLook(0); break;
    case C_AMXCONSOLE: amxPickLook(1); break;
    case C_AMXTREE: amxTree(); break;
    case C_AMXSEARCH: amxSearchAll(); break;
    case C_AMXSTYLE: amxPickStyle(); break;
    case C_AMXNEWPLUGIN: amxNewPlugin(); break;
    case C_AMXNEWINC: amxNewInclude(); break;
    case C_GITNEXT: gitNextChange(1); break;
    case C_GITPREV: gitNextChange(-1); break;
    case C_GITREVERT: gitRevertChange(); break;
    case C_GITDIFF: gitShowDiff(); break;
    case C_GITCHANGED: gitChangedFiles(); break;
    case C_SETTINGS: suOpen(0); break;
    case C_KEYBINDINGS: suOpen(1); break;
    case C_SETTINGSJSON: ensureUserFiles(); openPath(settingsPath()); break;
    case C_KEYSJSON: ensureUserFiles(); openPath(keymapPath()); break;
    case C_SNIPPETSJSON: ensureUserFiles(); openPath(snippetsPath()); break;
    case C_NEWNOTE: newNote(); break;
    case C_PICKUI: {
      int orig = g_uiStyle;
      static const wchar_t* names[4] = {L"Classic", L"Refined", L"Modern", L"Zen"};
      vector<PItem> items;
      for (int i = 0; i < 4; i++) {
        PItem it;
        it.label = names[i];
        it.right = i == orig ? L"current" : L"";
        it.line = i;
        std::string nm = toUtf8(names[i]);
        it.run = [nm] { setSetting("ui_style", nm); };
        items.push_back(it);
      }
      openPick(L"Select interface style", listProvider(items), [orig] { g_uiStyle = orig; applyChrome(); }, [](const PItem& it) { if (it.line >= 0) { g_uiStyle = it.line; applyChrome(); } });
      break;
    }
    case C_OPENNOTE: {
      wstring sq = g_nq.t;
      g_nq.t.clear();
      refreshNotes();
      vector<PItem> items;
      for (auto& n : g_notes) {
        PItem it;
        it.label = n.title.empty() ? L"New note" : n.title;
        it.right = n.preview;
        wstring id = n.id;
        it.run = [id] { openNoteDoc(id); redraw(); };
        items.push_back(it);
      }
      g_nq.t = sq;
      refreshNotes();
      openPick(L"Open a note", listProvider(items));
      break;
    }
    case C_SEARCHNOTES:
      g_sbVisible = true;
      g_sbView = 1;
      refreshNotes();
      g_nqFocus = true;
      g_ccFocusInput = false;
      g_nq.a = 0;
      g_nq.c = (int)g_nq.t.size();
      computeLayout();
      caretReset();
      redraw();
      break;
    case C_TOGGLEVISUAL:
      if (d.lang == L_MD) { d.vis = !d.vis; d.invalidate(0); d.scrollX = 0; ensureVisible(); redraw(); }
      break;
    case C_FMTBAR: g_fmtBar = !g_fmtBar; ensureVisible(); redraw(); break;
    case C_SBFILES: g_sbVisible = true; g_sbView = 0; redraw(); break;
    case C_SBNOTES: g_sbVisible = true; g_sbView = 1; refreshNotes(); redraw(); break;
    case C_IMPORTNOTES: importElectronNotes(); break;
    case C_NOTEPIN:
      if (d.isNote) { g_meta[d.noteId].pinned = !g_meta[d.noteId].pinned; saveMeta(); refreshNotes(); redraw(); }
      break;
    case C_NOTERENAME:
      if (d.isNote) {
        std::wstring id = d.noteId;
        openPrompt(L"Note title (empty = automatic)", g_meta[id].title, [id](const std::wstring& v) {
          g_meta[id].title = v;
          saveMeta();
          for (auto* x : g_docs) if (x->noteId == id) refreshNoteTitle(*x);
          refreshNotes();
          updateTitle();
          redraw();
        });
      }
      break;
    case C_NOTEDELETE:
      if (d.isNote && appMsgBox(g_hwnd, (L"Move \"" + d.name + L"\" to the Recycle Bin?").c_str(), L"Scriba", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        std::wstring id = d.noteId;
        std::wstring f = notesDir() + L"\\" + id;
        f.push_back(L'\0');
        SHFILEOPSTRUCTW op{};
        op.wFunc = FO_DELETE;
        op.pFrom = f.c_str();
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
        SHFileOperationW(&op);
        g_meta.erase(id);
        saveMeta();
        d.dirty = false;
        closeDoc(g_active);
        refreshNotes();
        redraw();
      }
      break;
    case C_NOTECOLOR0: case C_NOTECOLOR1: case C_NOTECOLOR2: case C_NOTECOLOR3: case C_NOTECOLOR4: case C_NOTECOLOR5: case C_NOTECOLOR6:
      if (d.isNote) { g_meta[d.noteId].color = noteColorByIndex(cmd - C_NOTECOLOR0); saveMeta(); refreshNotes(); redraw(); }
      break;
    case C_CLAUDE: case C_CLAUDENEW: case C_CLAUDEASK: case C_CLAUDESTOP: case C_CLAUDEADD: case C_CLAUDEEXPLAIN: case C_CLAUDEFIX:
    case C_CLAUDETESTS: case C_CLAUDEREFACTOR: case C_CLAUDEDOC:
      ccCommand(cmd);
      break;
    case C_MDPREVIEW: toggleMdPreview(); break;
    case C_H1: fmtHeading(1); break;
    case C_H2: fmtHeading(2); break;
    case C_H3: fmtHeading(3); break;
    case C_PARA: fmtHeading(0); break;
    case C_BOLD: fmtWrap(L"**", L"**"); break;
    case C_ITALIC: fmtWrap(L"*", L"*"); break;
    case C_STRIKE: fmtWrap(L"~~", L"~~"); break;
    case C_CODE: fmtWrap(L"`", L"`"); break;
    case C_UL: fmtToggle(L"* "); break;
    case C_OL: fmtToggle(L"", true); break;
    case C_TASK: fmtToggle(L"- [ ] "); break;
    case C_QUOTE: fmtToggle(L"> "); break;
    case C_CODEBLOCK: fmtCodeBlock(); break;
    case C_LINK: fmtLink(); break;
    case C_IMAGE: fmtImage(); break;
    case C_TABLE: fmtInsert(L"| Column | Column |\n| ------ | ------ |\n| Cell   | Cell   |\n"); break;
    case C_HR: fmtInsert(L"\n---\n"); break;
    case C_ADDNEXT: addNextOccurrence(); break;
    case C_ADDABOVE: addCursorVert(-1); break;
    case C_ADDBELOW: addCursorVert(1); break;
    case C_SPLITSEL: splitSelection(); break;
    case C_WRAP: g_wrap = !g_wrap; d.scrollX = 0; ensureVisible(); redraw(); break;
    case C_GUIDES: g_guides = !g_guides; redraw(); break;
    case C_BRKCOL: g_brackets = !g_brackets; redraw(); break;
    case C_FIND: openFind(false); break;
    case C_REPLACE: openFind(true); break;
    case C_FINDNEXT: if (!g_findOpen) openFind(false); else findStep(1); break;
    case C_FINDPREV: if (!g_findOpen) openFind(false); else findStep(-1); break;
    case C_PALETTE: openPalette(L">"); break;
    case C_GOTOANY: openPalette(L""); break;
    case C_GOTOLINE: openPalette(L":"); break;
    case C_OPENFOLDER: { wstring p = pickFolder(); if (!p.empty()) { setRoot(p); foldersChanged(); } break; }
    case C_ADDFOLDER: addFolderDialog(); break;
    case C_CLOSEFOLDER: removeFolders(); break;
    case C_REVEAL: revealActive(); break;
    case C_PROJSAVE: saveProject(); break;
    case C_PROJOPEN: openProject(); break;
    case C_OPENRECENT: pickRecent(); break;
    case C_RECENTCLEAR: recentClear(); flash(L"Recent items cleared"); break;
    case C_REOPEN: reopenClosed(); break;
    case C_NEWWINDOW: newWindow(); break;
    case C_SIDEBAR: g_sbVisible = !g_sbVisible; ensureVisible(); redraw(); break;
    case C_MINIMAP: g_mapVisible = !g_mapVisible; redraw(); break;
    case C_SAVEALL: for (auto* x : g_docs) if (x->dirty) { if (x->path.empty()) { int keep = g_active; g_active = (int)(std::find(g_docs.begin(), g_docs.end(), x) - g_docs.begin()); doSave(*x); g_active = keep; } else doSave(*x); } updateTitle(); redraw(); break;
    case C_SELLINE: {
      d.anc = {d.cur.l, 0};
      d.cur = d.cur.l + 1 < (int)d.lines.size() ? Pos{d.cur.l + 1, 0} : Pos{d.cur.l, (int)d.lines[d.cur.l].size()};
      moved(true);
      break;
    }
    case C_GOTOSTART: d.cur = {0, 0}; moved(false); break;
    case C_GOTOEND: d.cur = {(int)d.lines.size() - 1, (int)d.lines.back().size()}; moved(false); break;
    case C_ABOUT:
      appMsgBox(g_hwnd, L"Scriba (C++ edition)\nNative Win32 + Direct2D. No browser engine.", L"About Scriba", MB_OK);
      break;
    default:
      if (cmd >= C_SCHEME_BASE && cmd < C_SCHEME_BASE + 100) chooseScheme(cmd - C_SCHEME_BASE);
      else if (cmd >= C_SYNTAX_BASE && cmd < C_SYNTAX_BASE + 100) {
        int i = cmd - C_SYNTAX_BASE;
        if (i < (int)(sizeof(SYNTAXES) / sizeof(SYNTAXES[0]))) setSyntax(d, SYNTAXES[i].name, SYNTAXES[i].lang);
      }
      break;
  }
}
