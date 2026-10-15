// input.inc: keyboard input (shared by the window procedure and the test driver) and the scripted test driver.
// Included by main.cpp before the window procedure.

// returns true when the key was handled
static bool onKey(WPARAM wp, bool ctrl, bool shift, bool alt, bool sysKey) {
  if (g_modal.open) return modalKey(wp);
  if (g_licOpen) return licKey(wp, ctrl, shift, alt);  // the license page owns the keyboard
  Doc& d = D();
  tipHide();
  chromeTipHide();
  if (g_openMenu >= 0 && wp == VK_ESCAPE) { g_openMenu = -1; g_zenMenu = false; redraw(); return true; }
  if (g_palOpen) { palKey(wp, ctrl, shift); return true; }
  if (g_hudOpen) {  // the HUD Editor owns the keyboard
    if (ctrl && shift && !alt && wp == 'P') { openPalette(L">"); return true; }
    return hudKey(wp, ctrl, shift, alt);
  }
  if (g_suOpen) {  // the Settings page owns the keyboard (Ctrl+Shift+P still opens the palette)
    if (ctrl && shift && !alt && wp == 'P' && g_suCapture < 0) { openPalette(L">"); return true; }
    return suKey(wp, ctrl, shift, alt);
  }
  if (g_sbFindFocus) {  // the file-name search of the side bar
    if (wp == VK_ESCAPE) { g_sbFind = false; g_sbFindFocus = false; redraw(); return true; }
    if (wp == VK_RETURN) { sbFindOpenResult(g_sbFindSel); redraw(); return true; }
    if (wp == VK_UP || wp == VK_DOWN) {
      int n = (int)g_sbFindRes.size();
      if (n) { g_sbFindSel = std::max(0, std::min(n - 1, g_sbFindSel + (wp == VK_UP ? -1 : 1))); sbFindEnsure(); }
      redraw();
      return true;
    }
    if (!(ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V')) {
      if (editKey(g_sbFindQ, wp, ctrl, shift)) sbFindUpdate();
      caretReset();
      redraw();
      return true;
    }
  }
  if (g_nqFocus) {
    if (wp == VK_RETURN) { g_nqFocus = false; notesOpenFirst(); redraw(); return true; }
    if (wp == VK_ESCAPE) { g_nqFocus = false; notesClearSearch(); redraw(); return true; }
    if (!(ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V') && wp != VK_UP && wp != VK_DOWN) {
      if (editKey(g_nq, wp, ctrl, shift)) { g_nbScroll = 0; refreshNotes(); }
      caretReset();
      redraw();
      return true;
    }
  }
  if (ccKey(wp, ctrl, shift, alt)) return true;
  if (g_panelFocus && panelsKey(wp, ctrl, shift, alt)) return true;
  if (findKey(wp, ctrl, shift, alt)) return true;
  if (compKey(wp)) return true;  // the suggestions list
  // a snippet in progress: Tab / Shift+Tab move between its stops, Esc leaves it
  if (g_snip.active && g_snip.doc == &d && !ctrl && !alt) {
    if (wp == VK_TAB) { snipNext(shift ? -1 : 1); return true; }
    if (wp == VK_ESCAPE) { d.extra.clear(); d.anc = d.cur; snipEnd(); redraw(); return true; }
  }
  if (wp == VK_TAB && !ctrl && !alt && !shift && !g_snip.active && snipExpand()) return true;
  if (wp == VK_TAB && !ctrl && !alt && !shift && !g_snip.active && d.lang == L_PAWN && !d.hasSel() && d.extra.empty()) {  // AMX Mod X: Tab skips a closing ) ] } or "
    const wstring& ln = d.lines[d.cur.l];
    bool word = false;
    for (int i = 0; i < d.cur.c && i < (int)ln.size(); i++) if (iswalnum(ln[i]) || ln[i] == L'_') word = true;
    if (word && d.cur.c < (int)ln.size() && wcschr(L")]}\"", ln[d.cur.c])) { d.cur.c++; d.anc = d.cur; d.wantV = rowVcol(d, d.cur); ensureVisible(); caretReset(); redraw(); return true; }
  }
  macroRecordKey(wp, ctrl, shift, alt);
  if (handleKeymap(KeyChord{ctrl, alt, shift, (int)wp})) return true;
  if (alt && !ctrl && !shift && sysKey) {
    // Alt+letter opens the menu that starts with it, like the underlined mnemonics
    for (size_t i = 0; i < g_menus.size(); i++)
      if (wp == (WPARAM)g_menus[i].label[0]) { if (g_uiStyle == 3) { g_zenMenu = true; layoutMenus(); } g_openMenu = (int)i; redraw(); return true; }
  }
  if (!ctrl && !alt && imageLineKey(wp)) return true;
  if (ctrl && !alt && wp == VK_HOME) { exec(C_GOTOSTART); return true; }
  if (ctrl && !alt && wp == VK_END) { exec(C_GOTOEND); return true; }
  switch (wp) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_PRIOR: case VK_NEXT: case VK_HOME: case VK_END:
      multi([&] { moveKey(wp, ctrl, shift); }, false);
      return true;
    case VK_BACK: multi([&] { doBackspace(ctrl); }); if (g_comp.open) compUpdate(false); return true;
    case VK_DELETE: if (shift) { exec(C_CUT); } else multi([&] { doDelete(ctrl); }); return true;
    case VK_RETURN: multi([&] { doEnter(); }); return true;
    case VK_TAB: multi([&] { doTab(shift); }); return true;
    case VK_ESCAPE:
      if (g_findOpen) { closeFind(); return true; }
      if (!d.extra.empty()) { d.extra.clear(); redraw(); return true; }
      d.anc = d.cur;
      redraw();
      return true;
    case VK_INSERT: if (ctrl) { exec(C_COPY); return true; } if (shift) { exec(C_PASTE); return true; } break;
  }
  return false;
}
static void onChar(wchar_t c) {
  if (g_modal.open) return;
  if (c >= 32 && c != 127 && !g_palOpen && !g_suOpen && !g_hudOpen && !g_licOpen && !g_findFocus && !g_panelFocus && !g_nqFocus && !g_sbFindFocus && !g_ccFocusInput && imageLineActive(D())) imageLineEscape(D());
  static wchar_t hi = 0;
  if (c < 32 || c == 127) return;
  if (g_licOpen) { licChar(c); return; }
  if (g_palOpen) { palChar(c); return; }
  if (g_hudOpen) { hudChar(c); return; }
  if (g_suOpen) { suChar(c); return; }
  if (g_panelFocus && panelsChar(c)) return;
  if (g_sbFindFocus) { g_sbFindQ.insert(wstring(1, c)); sbFindUpdate(); caretReset(); redraw(); return; }
  if (g_nqFocus) { g_nq.insert(wstring(1, c)); g_nbScroll = 0; refreshNotes(); caretReset(); redraw(); return; }
  if (ccChar(c)) return;
  if (findChar(c)) return;
  macroRecordText(c);
  if (c >= 0xD800 && c <= 0xDBFF) { hi = c; return; }
  if (hi && c >= 0xDC00 && c <= 0xDFFF) { wstring t; t += hi; t += c; hi = 0; multi([&] { D().insertText(t, 1); }); edited(); return; }
  multi([&] { typeChar(c); });
  if (isIdCh(c) && (g_comp.open || g_autoComplete)) compUpdate(false);
  else if (g_comp.open) compClose();
}

/* ---------------------------------------------------------------- scripted tests (SCRIBA_TEST=<script file>) */
// One step per line: wait N | cmd ID | key CHORD... | type TEXT | open PATH | click X,Y | move X,Y | shot NAME | dump NAME | log TEXT | quit
// Results go to the folder given by SCRIBA_TEST_OUT (default: next to the script).
static vector<wstring> g_testLines;
static size_t g_testIdx = 0;
static DWORD g_testResume = 0;
static wstring g_testOut;
static void testLog(const wstring& s) {
  wstring path = g_testOut + L"\\log.txt";
  HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
  vector<char> b(n + 1);
  WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), b.data(), n, nullptr, nullptr);
  b[n] = '\n';
  DWORD wr;
  WriteFile(h, b.data(), n + 1, &wr, nullptr);
  CloseHandle(h);
}
static void writeUtf8(const wstring& path, const wstring& text) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
  vector<char> b(n);
  if (n) WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), b.data(), n, nullptr, nullptr);
  DWORD wr;
  WriteFile(h, b.data(), (DWORD)b.size(), &wr, nullptr);
  CloseHandle(h);
}
// draws the whole window into an off-screen bitmap and saves it as PNG (works while the window is outside the screen)
static bool savePng(HWND hw, const wstring& path) {
  (void)hw;
  if (!g_rt) return false;
  D2D1_SIZE_F sz = g_rt->GetSize();
  int w = (int)sz.width, hgt = (int)sz.height;
  bool ok = false;
  IWICImagingFactory* f = nullptr;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f)))) return false;
  IWICBitmap* wb = nullptr;
  ID2D1RenderTarget* wrt = nullptr;
  ID2D1SolidColorBrush* br = nullptr;
  if (SUCCEEDED(f->CreateBitmap(w, hgt, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wb)) &&
      SUCCEEDED(g_d2d->CreateWicBitmapRenderTarget(wb, D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f), &wrt)) &&
      SUCCEEDED(wrt->CreateSolidColorBrush(D2D1::ColorF(0), &br))) {
    wrt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ID2D1RenderTarget* sRt = g_rt;
    ID2D1SolidColorBrush* sBr = g_br;
    g_rt = wrt;
    g_br = br;
    paint();
    g_rt = sRt;
    g_br = sBr;
    IWICStream* st = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* fr = nullptr;
    if (SUCCEEDED(f->CreateStream(&st)) && SUCCEEDED(st->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
        SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) && SUCCEEDED(enc->Initialize(st, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(enc->CreateNewFrame(&fr, nullptr)) && SUCCEEDED(fr->Initialize(nullptr)) && SUCCEEDED(fr->SetSize(w, hgt))) {
      WICPixelFormatGUID pf = GUID_WICPixelFormat32bppPBGRA;
      fr->SetPixelFormat(&pf);
      if (SUCCEEDED(fr->WriteSource(wb, nullptr)) && SUCCEEDED(fr->Commit()) && SUCCEEDED(enc->Commit())) ok = true;
    }
    if (fr) fr->Release();
    if (enc) enc->Release();
    if (st) st->Release();
  }
  if (br) br->Release();
  if (wrt) wrt->Release();
  if (wb) wb->Release();
  f->Release();
  return ok;
}
static wstring docDump(Doc& d) {
  wstring s;
  for (size_t i = 0; i < d.lines.size(); i++) { s += d.lines[i]; if (i + 1 < d.lines.size()) s += L"\n"; }
  s += L"\n--- cursor " + std::to_wstring(d.cur.l) + L"," + std::to_wstring(d.cur.c) + L" anchor " + std::to_wstring(d.anc.l) + L"," + std::to_wstring(d.anc.c);
  s += L" extra " + std::to_wstring(d.extra.size()) + L" dirty " + std::to_wstring(d.dirty ? 1 : 0) + L" lang " + d.langName;
  s += L" tab " + std::to_wstring(d.tabW) + (d.spaces ? L" spaces" : L" tabs") + L" enc " + std::to_wstring(d.enc);
  if (!d.folded.empty()) { s += L" folded"; for (int f : d.folded) s += L" " + std::to_wstring(f); }
  if (!d.marks.empty()) { s += L" marks"; for (int f : d.marks) s += L" " + std::to_wstring(f); }
  return s;
}
static void testStart() {
  wchar_t sp[MAX_PATH];
  if (!GetEnvironmentVariableW(L"SCRIBA_TEST", sp, MAX_PATH)) return;
  vector<char> raw;
  if (!readAll(sp, raw)) return;
  wstring all = u8w(raw.data(), (int)raw.size());
  size_t p = 0;
  while (p < all.size()) {
    size_t q = all.find(L'\n', p);
    if (q == wstring::npos) q = all.size();
    wstring ln = all.substr(p, q - p);
    while (!ln.empty() && (ln.back() == L'\r')) ln.pop_back();
    if (!ln.empty() && ln[0] != L'#') g_testLines.push_back(ln);
    p = q + 1;
  }
  wchar_t out[MAX_PATH];
  if (GetEnvironmentVariableW(L"SCRIBA_TEST_OUT", out, MAX_PATH)) g_testOut = out;
  else { g_testOut = sp; g_testOut += L".out"; }
  CreateDirectoryW(g_testOut.c_str(), nullptr);
  DeleteFileW((g_testOut + L"\\log.txt").c_str());
  SetTimer(g_hwnd, 9, 40, nullptr);
}
static void testTick() {
  if (GetTickCount() < g_testResume) return;
  while (g_testIdx < g_testLines.size()) {
    wstring ln = g_testLines[g_testIdx++];
    size_t sp = ln.find(L' ');
    wstring op = sp == wstring::npos ? ln : ln.substr(0, sp), arg = sp == wstring::npos ? L"" : ln.substr(sp + 1);
    if (op == L"wait") { g_testResume = GetTickCount() + (DWORD)_wtoi(arg.c_str()); return; }
    else if (op == L"cmd") {
      const CmdDef* c = cmdById(std::string(arg.begin(), arg.end()));
      if (c) exec(c->cmd); else testLog(L"unknown command " + arg);
    } else if (op == L"key") {
      size_t a = 0;
      while (a < arg.size()) {
        size_t b = arg.find(L' ', a);
        if (b == wstring::npos) b = arg.size();
        wstring one = arg.substr(a, b - a);
        a = b + 1;
        vector<KeyChord> cs = parseKeys(one);
        for (auto& c : cs) onKey((WPARAM)c.vk, c.ctrl, c.shift, c.alt, c.alt);
      }
    } else if (op == L"type") {
      for (size_t i = 0; i < arg.size(); i++) {
        if (arg[i] == L'\\' && i + 1 < arg.size()) {
          wchar_t n = arg[++i];
          if (n == L'n') onKey(VK_RETURN, false, false, false, false);
          else if (n == L't') onKey(VK_TAB, false, false, false, false);
          else if (n == L'b') onKey(VK_BACK, false, false, false, false);
          else onChar(n);
        } else onChar(arg[i]);
      }
    } else if (op == L"open") openPath(arg);
    else if (op == L"folder") { setRoot(arg); foldersChanged(); }
    else if (op == L"addfolder") { addRoot(arg); foldersChanged(); }
    else if (op == L"ctx") {  // ctx x,y,Label: right click at x,y and choose the item
      int x = _wtoi(arg.c_str());
      size_t c1 = arg.find(L','), c2 = c1 == wstring::npos ? c1 : arg.find(L',', c1 + 1);
      if (c2 != wstring::npos) {
        int y = _wtoi(arg.c_str() + c1 + 1);
        g_ctxTestPick = arg.substr(c2 + 1);
        contextMenuAt(x * g_dpiScale, y * g_dpiScale, x, y);
        g_ctxTestPick.clear();
      }
    }
    else if (op == L"tipinfo") testLog(L"tip: text=[" + g_ctipText + L"] shown=" + std::to_wstring(g_ctipShow) + L" mouse=" + std::to_wstring((int)g_mx) + L"," + std::to_wstring((int)g_my));
    else if (op == L"fmtimage") { g_testImagePick = arg; fmtImage(); g_testImagePick.clear(); }
    else if (op == L"amxpick") g_testAmxPick = arg;
    else if (op == L"licstate") testLog(L"lic: state=" + std::to_wstring(g_lic.state) + L" locked=" + std::to_wstring(licLocked() ? 1 : 0));
    else if (op == L"tsall") { for (size_t i = 0; i < sizeof(TS_LANGS) / sizeof(TS_LANGS[0]); i++) tsLoad((int)i); testLog(L"ts: " + (g_tsLog.empty() ? wstring(L"all queries loaded") : g_tsLog)); }
    else if (op == L"crash") {  // crash av|terminate|abort|stack: to see the report that is written
      if (arg == L"av") *(volatile int*)nullptr = 1;
      else if (arg == L"terminate") { try { throw std::runtime_error("test exception"); } catch (...) { std::terminate(); } }
      else if (arg == L"abort") abort();
      else { struct R { static int f(int n) { volatile char pad[4096]; pad[0] = (char)n; return f(n + 1) + pad[0]; } }; R::f(0); }
    }
    else if (op == L"pick") g_ctxTestPick = arg;  // the next popup menu (notes view buttons) chooses this item
    else if (op == L"modal") {  // modal Yes|No|Cancel|OK: the next message box is answered without being shown
      g_modalAuto.push_back(arg == L"Yes" ? IDYES : arg == L"No" ? IDNO : arg == L"Cancel" ? IDCANCEL : IDOK);
    }
    else if (op == L"modalshot") {  // modalshot name|text: draw a box with a text, take a picture, close it
      size_t bar = arg.find(L'|');
      wstring name = arg.substr(0, bar), text = bar == wstring::npos ? L"Save changes to untitled?" : arg.substr(bar + 1);
      g_modal = ModalState();
      g_modal.text = text;
      g_modal.btns = {{L"Yes", IDYES, true}, {L"No", IDNO, false}, {L"Cancel", IDCANCEL, false}};
      g_modal.open = true;
      redraw();
      UpdateWindow(g_hwnd);
      savePng(g_hwnd, g_testOut + L"\\" + name + L".png");
      g_modal.open = false;
    }
    else if (op == L"drag") {  // drag x1,y1,x2,y2: press, move, release
      int v[4] = {0, 0, 0, 0}, k = 0;
      const wchar_t* c = arg.c_str();
      while (k < 4 && *c) { wchar_t* e; v[k++] = (int)wcstol(c, &e, 10); c = *e ? e + 1 : e; }
      auto lp = [](int x, int y) { return MAKELPARAM((int)(x / g_dpiScale), (int)(y / g_dpiScale)); };
      SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp(v[0], v[1]));
      SendMessageW(g_hwnd, WM_MOUSEMOVE, MK_LBUTTON, lp((v[0] + v[2]) / 2, (v[1] + v[3]) / 2));
      SendMessageW(g_hwnd, WM_MOUSEMOVE, MK_LBUTTON, lp(v[2], v[3]));
      SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp(v[2], v[3]));
    }
    else if (op == L"showsetting") testLog(fromUtf8(g_settings[toUtf8(arg)].dump()));
    else if (op == L"setting") {  // setting key=value (a JSON value)
      size_t eq = arg.find(L'=');
      if (eq != wstring::npos) setSetting(toUtf8(arg.substr(0, eq)).c_str(), json::parse(toUtf8(arg.substr(eq + 1)), nullptr, false));
    }
    else if (op == L"cursor") {  // which mouse pointer the last move chose
      HCURSOR c = GetCursor();
      const struct { LPCWSTR id; const wchar_t* n; } k[] = {{IDC_ARROW, L"arrow"}, {IDC_HAND, L"hand"}, {IDC_IBEAM, L"ibeam"}, {IDC_SIZEWE, L"sizewe"}, {IDC_SIZENS, L"sizens"}};
      wstring r = L"other";
      for (auto& e : k) if (c == LoadCursor(nullptr, e.id)) r = e.n;
      testLog(L"cursor: " + r);
    }
    else if (op == L"hwheel") {  // sideways wheel at x,y,delta
      int x = _wtoi(arg.c_str()), y = 0, dl = 120;
      size_t c = arg.find(L',');
      if (c != wstring::npos) { y = _wtoi(arg.c_str() + c + 1); size_t c2 = arg.find(L',', c + 1); if (c2 != wstring::npos) dl = _wtoi(arg.c_str() + c2 + 1); }
      POINT pt{(int)(x / g_dpiScale), (int)(y / g_dpiScale)};
      ClientToScreen(g_hwnd, &pt);
      SendMessageW(g_hwnd, WM_MOUSEHWHEEL, MAKEWPARAM(0, (short)dl), MAKELPARAM(pt.x, pt.y));
    }
    else if (op == L"wheel") {  // wheel x,y,delta (delta -120 = one notch down)
      int x = _wtoi(arg.c_str()), y = 0, dl = -120;
      size_t c = arg.find(L',');
      if (c != wstring::npos) { y = _wtoi(arg.c_str() + c + 1); size_t c2 = arg.find(L',', c + 1); if (c2 != wstring::npos) dl = _wtoi(arg.c_str() + c2 + 1); }
      POINT pt{(int)(x / g_dpiScale), (int)(y / g_dpiScale)};
      ClientToScreen(g_hwnd, &pt);
      g_mx = (float)x; g_my = (float)y;
      SendMessageW(g_hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, (short)dl), MAKELPARAM(pt.x, pt.y));
    }
    else if (op == L"click" || op == L"move") {
      int x = _wtoi(arg.c_str()), y = 0;
      size_t c = arg.find(L',');
      if (c != wstring::npos) y = _wtoi(arg.c_str() + c + 1);
      LPARAM lp = MAKELPARAM((int)(x / g_dpiScale), (int)(y / g_dpiScale));
      if (op == L"move") SendMessageW(g_hwnd, WM_MOUSEMOVE, 0, lp);
      else { SendMessageW(g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp); SendMessageW(g_hwnd, WM_LBUTTONUP, 0, lp); }
    } else if (op == L"shot") { redraw(); UpdateWindow(g_hwnd); if (!savePng(g_hwnd, g_testOut + L"\\" + arg + L".png")) testLog(L"shot failed " + arg); }
    else if (op == L"dump") writeUtf8(g_testOut + L"\\" + arg + L".txt", docDump(D()));
    else if (op == L"log") testLog(arg);
    else if (op == L"size") {
      int w = _wtoi(arg.c_str()), h = 0;
      size_t c = arg.find(L' ');
      if (c != wstring::npos) h = _wtoi(arg.c_str() + c + 1);
      SetWindowPos(g_hwnd, nullptr, 40, 40, w, h, SWP_NOZORDER);
    }
    else if (op == L"quit") { testLog(L"done"); KillTimer(g_hwnd, 9); PostMessageW(g_hwnd, WM_CLOSE, 0, 0); return; }
    else testLog(L"unknown step " + ln);
    // let the window repaint between steps
    g_testResume = GetTickCount() + 60;
    return;
  }
  KillTimer(g_hwnd, 9);
}
