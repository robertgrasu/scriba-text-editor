// The Find / Replace panel docked under the editor. Same rows, buttons, shortcuts and wording as Electron's findpanel.js.
struct FRects { D2D1_RECT_F panel, q, r, bRe, bCase, bWord, bSel, bPrev, bNext, bAll, count, bClose, bRepl, bReplAll; };
static IDWriteTextFormat* findMonoFmt() {
  static IDWriteTextFormat* f = nullptr;
  if (!f) {
    g_dw->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us", &f);
    if (f) f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  }
  return f ? f : g_ui;
}
static const float FP_IN_H = 27, FP_BTN_H = 25, FP_GAP = 6;
static float fpButtonW(const wchar_t* s) { return std::ceil(textWidth(s, g_ui)) + 26; }
static FRects findRects() {
  FRects f{};
  const VP& s = g_findStrip;
  f.panel = rc(s.left, s.top, s.right - s.left, s.bottom - s.top);
  float x0 = s.left + 10, W = s.right - s.left - 20;
  float y1 = s.top + 8, y2 = y1 + FP_IN_H + FP_GAP;
  auto mid = [&](float y, float h) { return y + (FP_IN_H - h) / 2; };
  // Sublime layout: the toggles on the left, the search field takes all the width that is left, the buttons stay at the right
  float tw = 28, selW = std::ceil(textWidth(L"In sel.", findMonoFmt())) + 12;
  float bw = std::max({fpButtonW(L"Find Prev"), fpButtonW(L"Find"), fpButtonW(L"Find All"), 98.f});  // the three buttons are equally wide, as in Sublime
  float pw = bw, nw = bw, aw = bw, cw0 = 78, xw = 24;
  float rw = bw, raw = bw;
  float left = tw * 3 + selW + FP_GAP * 4;
  float right = pw + nw + aw + xw + FP_GAP * 4;
  float inW = std::max(80.f, W - left - right);
  float x = x0;
  f.bRe = rc(x, mid(y1, FP_BTN_H), tw, FP_BTN_H); x += tw + FP_GAP;
  f.bCase = rc(x, mid(y1, FP_BTN_H), tw, FP_BTN_H); x += tw + FP_GAP;
  f.bWord = rc(x, mid(y1, FP_BTN_H), tw, FP_BTN_H); x += tw + FP_GAP;
  f.bSel = rc(x, mid(y1, FP_BTN_H), selW, FP_BTN_H); x += selW + FP_GAP;
  f.q = rc(x, y1, inW, FP_IN_H); x += inW + FP_GAP;
  f.count = rc(f.q.right - cw0 - 6, y1, cw0, FP_IN_H);  // the match count sits inside the right end of the field
  f.bPrev = rc(x, mid(y1, FP_BTN_H), pw, FP_BTN_H); x += pw + FP_GAP;
  f.bNext = rc(x, mid(y1, FP_BTN_H), nw, FP_BTN_H); x += nw + FP_GAP;
  f.bAll = rc(x, mid(y1, FP_BTN_H), aw, FP_BTN_H); x += aw + FP_GAP;
  f.bClose = rc(x, mid(y1, FP_BTN_H), xw, FP_BTN_H);
  // second row: the replace field under the search field, its buttons under Find Prev / Find / Find All
  f.r = rc(f.q.left, y2, inW, FP_IN_H);
  f.bReplAll = rc(f.bAll.right - raw, mid(y2, FP_BTN_H), raw, FP_BTN_H);
  f.bRepl = rc(f.bReplAll.left - FP_GAP - rw, mid(y2, FP_BTN_H), rw, FP_BTN_H);
  return f;
}
static int findCountText(wstring& out) {
  out.clear();
  if (!g_fMsg.empty()) { out = g_fMsg; return 0; }
  if (g_regexBad) { out = L"Invalid pattern"; return 1; }
  if (g_fq.t.empty()) return 0;
  int n = (int)std::min<size_t>(g_matches.size(), 5000);
  if (!n) { out = L"No matches"; return 0; }
  int ci = curMatchIndex();
  wchar_t b[48];
  if (ci >= 0 && ci < 5000) swprintf_s(b, L"%d of %d%s", ci + 1, n, g_matches.size() >= 5000 ? L"+" : L"");
  else swprintf_s(b, L"%d%s", n, g_matches.size() >= 5000 ? L"+" : L"");
  out = b;
  return 0;
}
static void drawFind() {
  if (!g_findOpen) return;
  const Theme& t = T();
  computeMatches();
  FRects f = findRects();
  fillR(f.panel, t.status, 1.f);
  fillRect(f.panel.left, f.panel.top, f.panel.right - f.panel.left, 1, t.border);
  drawInput(g_fq, f.q, g_findFocus && g_fField == 0, L"Find", g_regexBad);
  auto toggle = [&](const D2D1_RECT_F& r, const wchar_t* s, bool on, bool underline = false) {
    bool hv = inRect(r, g_mx, g_my);
    if (on || hv) fillR(r, t.btn, 1.f, 3);
    if (on) strokeR(r, t.accent, 1.f, 3);
    IDWriteTextFormat* fm = findMonoFmt();
    float w = textWidth(s, fm);
    float tx = r.left + (r.right - r.left - w) / 2, ty = r.top + (r.bottom - r.top - 16) / 2 - 1;
    DWORD col = on ? t.fg : t.statusText;
    drawText(s, (int)wcslen(s), tx, ty, col, fm, w + 8, 20);
    if (underline) fillRect(tx, ty + 15, w, 1, col);
  };
  auto button = [&](const D2D1_RECT_F& r, const wchar_t* s) {
    bool hv = inRect(r, g_mx, g_my);
    fillR(r, hv ? t.btnHi : t.btn, 1.f, 3);
    strokeR(r, t.border, 1.f, 3);
    float w = textWidth(s, g_ui);
    drawText(s, (int)wcslen(s), r.left + (r.right - r.left - w) / 2, r.top + (r.bottom - r.top - 18) / 2 - 1, t.fg, g_ui, w + 8, 20);
  };
  toggle(f.bRe, L".*", g_fRegex);
  toggle(f.bCase, L"Aa", g_fCase);
  toggle(f.bWord, L"ab", g_fWord, true);
  toggle(f.bSel, L"In sel.", g_fSelOn);
  button(f.bPrev, L"Find Prev");
  button(f.bNext, L"Find");
  button(f.bAll, L"Find All");
  wstring ct;
  findCountText(ct);
  { float cw = textWidth(ct.c_str(), g_ui); drawText(ct.c_str(), (int)ct.size(), f.count.right - cw, f.count.top + (FP_IN_H - 18) / 2, t.statusText, g_ui, cw + 8, 20); }
  {  // close
    bool hv = inRect(f.bClose, g_mx, g_my);
    float cx = (f.bClose.left + f.bClose.right) / 2, cy = (f.bClose.top + f.bClose.bottom) / 2;
    g_br->SetColor(Rgb(hv ? t.fg : t.statusText));
    g_rt->DrawLine(D2D1::Point2F(cx - 4.5f, cy - 4.5f), D2D1::Point2F(cx + 4.5f, cy + 4.5f), g_br, 1.4f);
    g_rt->DrawLine(D2D1::Point2F(cx - 4.5f, cy + 4.5f), D2D1::Point2F(cx + 4.5f, cy - 4.5f), g_br, 1.4f);
  }
  if (g_replOpen) {
    drawInput(g_fr, f.r, g_findFocus && g_fField == 1, L"Replace");
    button(f.bRepl, L"Replace");
    button(f.bReplAll, L"Replace All");
  }
}
// every match becomes a selection
static void findSelectAll() {
  computeMatches();
  if (g_matches.empty()) return;
  Doc& d = D();
  d.extra.clear();
  d.anc = {g_matches[0].l, g_matches[0].a};
  d.cur = {g_matches[0].l, g_matches[0].b};
  for (size_t i = 1; i < g_matches.size(); i++) d.extra.push_back({Pos{g_matches[i].l, g_matches[i].b}, Pos{g_matches[i].l, g_matches[i].a}});
  d.lastKind = 0;
  d.wantV = rowVcol(d, d.cur);
  g_findFocus = false;
  revealCenter();
  caretReset();
  redraw();
}
static void findToggleSel() {
  Doc& d = D();
  if (g_fSelOn) g_fSelOn = false;
  else {
    if (!d.hasSel()) { g_fMsg = L"Select text first"; redraw(); return; }
    g_fSelA = d.selA();
    g_fSelB = d.selB();
    g_fSelOn = true;
  }
  g_fMsg.clear();
  g_mKey.clear();
  redraw();
}
// an option changed: search again from where the search started
static void findOptionChanged() {
  g_mKey.clear();
  g_fMsg.clear();
  findStep(1, true);
  redraw();
}
// puts the caret of a text field where it was clicked
static void lineEditClick(LineEdit& e, const D2D1_RECT_F& r, float x) {
  float rel = x - (r.left + 9);
  int best = (int)e.t.size();
  for (int i = 0; i <= (int)e.t.size(); i++)
    if (textWidth(e.t.substr(0, i), g_ui) >= rel) {
      best = i;
      if (i > 0 && rel - textWidth(e.t.substr(0, i - 1), g_ui) < textWidth(e.t.substr(0, i), g_ui) - rel) best = i - 1;
      break;
    }
  e.c = e.a = best;
}
static bool findMouse(float x, float y) {
  if (!g_findOpen) return false;
  FRects f = findRects();
  if (!inRect(f.panel, x, y)) return false;
  g_findFocus = true;
  g_fMsg.clear();
  if (inRect(f.q, x, y)) { g_fField = 0; lineEditClick(g_fq, f.q, x); }
  else if (g_replOpen && inRect(f.r, x, y)) { g_fField = 1; lineEditClick(g_fr, f.r, x); }
  else if (inRect(f.bRe, x, y)) { g_fRegex = !g_fRegex; findOptionChanged(); }
  else if (inRect(f.bCase, x, y)) { g_fCase = !g_fCase; findOptionChanged(); }
  else if (inRect(f.bWord, x, y)) { g_fWord = !g_fWord; findOptionChanged(); }
  else if (inRect(f.bSel, x, y)) findToggleSel();
  else if (inRect(f.bPrev, x, y)) findStep(-1);
  else if (inRect(f.bNext, x, y)) findStep(1);
  else if (inRect(f.bAll, x, y)) findSelectAll();
  else if (inRect(f.bClose, x, y)) { closeFind(); return true; }
  else if (g_replOpen && inRect(f.bRepl, x, y)) replaceOne();
  else if (g_replOpen && inRect(f.bReplAll, x, y)) replaceAll();
  caretReset();
  redraw();
  return true;
}
static bool findKey(WPARAM wp, bool ctrl, bool shift, bool alt) {
  if (!g_findOpen || !g_findFocus) return false;
  LineEdit& e = g_fField == 1 ? g_fr : g_fq;
  if (wp == VK_ESCAPE) { closeFind(); return true; }
  if (wp == VK_RETURN) {
    if (g_fField == 1 && g_replOpen) { if (ctrl && alt) replaceAll(); else replaceOne(); }
    else if (alt) findSelectAll();
    else findStep(shift ? -1 : 1);
    return true;
  }
  if (alt && !ctrl) {
    if (wp == 'S') { findToggleSel(); return true; }
    if (wp == 'R') { g_fRegex = !g_fRegex; findOptionChanged(); return true; }
    if (wp == 'C') { g_fCase = !g_fCase; findOptionChanged(); return true; }
    if (wp == 'W') { g_fWord = !g_fWord; findOptionChanged(); return true; }
  }
  if (wp == VK_TAB && g_replOpen) { g_fField = 1 - g_fField; redraw(); return true; }
  if (wp == VK_F3) { findStep(shift ? -1 : 1); return true; }
  if (ctrl && (wp == 'F' || wp == 'H')) {
    g_fField = wp == 'H' ? 1 : 0;
    if (wp == 'H' && !g_replOpen) { g_replOpen = true; computeLayout(); }
    redraw();
    return true;
  }
  if (ctrl && !shift && wp == 'G') return false;
  if (ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V') return false;
  if (wp == VK_UP || wp == VK_DOWN || wp == VK_PRIOR || wp == VK_NEXT) return false;
  bool changed = editKey(e, wp, ctrl, shift);
  if (changed && g_fField == 0) { g_mKey.clear(); g_fMsg.clear(); findStep(1, true); }
  caretReset();
  redraw();
  return true;
}
static bool findChar(wchar_t c) {
  if (!g_findOpen || !g_findFocus) return false;
  LineEdit& e = g_fField == 1 ? g_fr : g_fq;
  e.insert(wstring(1, c));
  if (g_fField == 0) { g_mKey.clear(); g_fMsg.clear(); findStep(1, true); }
  caretReset();
  redraw();
  return true;
}
