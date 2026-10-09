// Tooltips of buttons (the title="..." texts of Electron's index.html and panels). A text shows after the pointer rests on a button.
static wstring chromeTipAt(float x, float y) {
  if (g_ctxOpen || g_palOpen || g_suOpen || g_openMenu >= 0 || g_dragSel || g_dragSb || g_dragSplit || g_dragMap) return L"";
  if (g_ccOpen && x >= ccLeft() && y >= MENU_H && y < ch() - STATUS_H) {
    CcChat* c = ccCur();
    for (int i = (int)g_ccHits.size() - 1; i >= 0; i--) {
      if (!inRect(g_ccHits[i].r, x, y)) continue;
      switch (g_ccHits[i].kind) {
        case HD_ADD: return L"New conversation";
        case HD_HIDE: return L"Hide Claude (" + effectiveKeys(C_CLAUDE) + L")";
        case HD_ACCOUNT: return L"Account: sign in, switch account, log out";
        case HD_HISTORY: return L"Past conversations";
        case HD_NEW: return L"New conversation";
        case HD_CLOSECHAT: return L"Close this conversation";
        case HD_PLUS: return L"Attach a file or an image";
        case HD_SLASH: return L"Commands";
        case HD_MODEL: return L"Model and effort";
        case HD_MODE: return L"What Claude may do without asking (Shift+Tab)";
        case HD_SEND: return c && ccBusy(c) ? L"Stop (Esc)" : L"Send (Enter)";
        case HD_CTXOFF: return L"Do not send the open file";
        case HD_TAB: return g_ccHits[i].a < (int)g_ccChats.size() ? g_ccChats[g_ccHits[i].a]->title : L"";
        case HD_TABX: return L"Close";
        case HT_UNDO: return L"Restore the files to how they were before this message";
        case HD_COST: return c ? c->costTip : L"";
        case HD_USAGE: return c ? c->usageTip : L"";
        default: return L"";
      }
    }
    return L"";
  }
  // status bar
  if (y >= ch() - STATUS_H) {
    for (auto& it : statusItems(D())) {
      if (!inRect(it.r, x, y)) continue;
      if (it.id == 20) return L"Recording a macro";
      if (it.id == 21) return L"Language server: errors and warnings in this file (Ctrl+Shift+M lists them)";
      if (it.id == 22) return L"Git branch and number of changed blocks";
    }
    return L"";
  }
  // find / replace panel
  if (g_findOpen && inRect(rc(g_findStrip.left, g_findStrip.top, g_findStrip.right - g_findStrip.left, g_findStrip.bottom - g_findStrip.top), x, y)) {
    FRects f = findRects();
    if (inRect(f.bRe, x, y)) return L"Regular expression (Alt+R)";
    if (inRect(f.bCase, x, y)) return L"Case sensitive (Alt+C)";
    if (inRect(f.bWord, x, y)) return L"Whole word (Alt+W)";
    if (inRect(f.bSel, x, y)) return L"Search only in the selection (Alt+S): select the text first";
    if (inRect(f.bClose, x, y)) return L"Close (Esc)";
    return L"";
  }
  // panels under the editor
  for (int i = (int)g_pHits.size() - 1; i >= 0; i--) {
    if (!inRect(g_pHits[i].r, x, y)) continue;
    switch (g_pHits[i].kind) {
      case P_FIF_RE: return L"Regular expression";
      case P_FIF_CASE: return L"Case sensitive";
      case P_FIF_WORD: return L"Whole word";
      case P_FIF_CLOSE: return L"Close (Esc)";
      case P_RUN_RUN: return L"Run (Enter)";
      case P_RUN_STOP: return L"Stop (Ctrl+C)";
      case P_RUN_CLEAR: return L"Clear the output";
      case P_RUN_CLOSE: return L"Close (Esc)";
      case P_BUILD_AGAIN: return L"Compile again (F7)";
      case P_BUILD_RAW: return L"Show the compiler output";
      case P_BUILD_CLOSE: return L"Close";
      default: break;
    }
    return L"";
  }
  // side bar
  if (g_sbVisible && x < g_sbW) {
    static const wchar_t* pill[3] = {L"Files and folders (Alt+Shift+1)", L"Notes (Alt+Shift+2)", L"Outline of this file (Alt+Shift+3)"};
    for (int i = 0; i < 3; i++) if (inRect(sbPill(i), x, y)) return pill[i];
    if (g_sbView == 1 && y >= MENU_H + 41 && y < MENU_H + 65) {
      if (x >= g_sbW - 32) return L"New note (" + effectiveKeys(C_NEWNOTE) + L")";
      if (x >= g_sbW - 64) return L"Sort";
      if (x >= g_sbW - 92) return L"How notes are shown";
    }
    return L"";
  }
  // tab bar
  for (int gi = 0; gi < paneCount(); gi++) {
    const VP& p = g_paneRect[gi];
    if (y < p.top || y >= p.top + TAB_H || x < p.left || x >= p.right) continue;
    if (x < p.left + 24) return L"Scroll tabs left";
    if (x < p.left + 48) return L"Scroll tabs right";
    if (x >= p.right - 26) return L"Show tabs";
    if (x >= p.right - 52) return L"New tab";
    int ti = tabAt(x, y);
    if (ti >= 0) return g_docs[ti]->path.empty() ? g_docs[ti]->name : g_docs[ti]->path;
    return L"";
  }
  // Markdown toolbar
  if (fmtVisible() && y >= g_vp.top + TAB_H && y < g_vp.top + TAB_H + FMT_H) {
    for (auto& b : fmtButtons()) {
      if (x < b.x || x >= b.x + b.w) continue;
      switch (b.cmd) {
        case C_H1: return L"Heading 1 (Ctrl+Alt+1)";
        case C_H2: return L"Heading 2 (Ctrl+Alt+2)";
        case C_H3: return L"Heading 3 (Ctrl+Alt+3)";
        case C_BOLD: return L"Bold (Ctrl+B)";
        case C_ITALIC: return L"Italic (Ctrl+I)";
        case C_STRIKE: return L"Strikethrough (Ctrl+Shift+X)";
        case C_CODE: return L"Inline code (Ctrl+Alt+C)";
        case C_UL: return L"Bulleted list (Ctrl+Alt+U)";
        case C_OL: return L"Numbered list";
        case C_TASK: return L"To-do list (Ctrl+Alt+T)";
        case C_QUOTE: return L"Quote (Ctrl+Alt+Q)";
        case C_CODEBLOCK: return L"Code block";
        case C_LINK: return L"Link (Ctrl+Alt+L)";
        case C_IMAGE: return L"Insert image (or just paste one)";
        case C_TABLE: return L"Table";
        case C_HR: return L"Divider";
      }
    }
    static const wchar_t* pt[4] = {L"Show formatted text instead of Markdown symbols (Ctrl+Alt+E)", L"Side-by-side preview (Ctrl+Alt+M)",
                                   L"Line numbers in Visual mode and in the Preview", L"Also show the Markdown symbols (#, **, -) on the line you are editing"};
    for (int i = 0; i < 4; i++) if (inRect(fmtPill(i), x, y)) return pt[i];
  }
  return L"";
}
static void chromeTipMove(float x, float y) {
  wstring t = chromeTipAt(x, y);
  if (t != g_ctipText) {
    bool wasShown = g_ctipShow;
    g_ctipText = t;
    g_ctipShow = false;
    KillTimer(g_hwnd, 11);
    if (!t.empty()) SetTimer(g_hwnd, 11, 550, nullptr);
    if (wasShown) redraw();
  }
}
static void chromeTipTimer() {
  KillTimer(g_hwnd, 11);
  if (g_ctipText.empty()) return;
  g_ctipShow = true;
  redraw();
}
static void drawChromeTip() {
  if (!g_ctipShow || g_ctipText.empty()) return;
  const Theme& t = T();
  IDWriteTextFormat* f = uiFont(12.f);
  float maxW = std::min(380.f, cw() - 24);
  float tw = std::min(maxW, textWidth(g_ctipText, f) + 2);
  float th = wrappedHeight(g_ctipText, f, tw, 16.f);
  float w = tw + 16, h = th + 10;
  float x = std::min(g_mx + 12, cw() - w - 6), y = g_my + 22;
  if (y + h > ch() - 4) y = g_my - h - 8;
  fillR(D2D1::RectF(x + 1, y + 2, x + w + 1, y + h + 2), 0x000000, 0.22f, 4);
  fillR(rc(x, y, w, h), t.menu, 1.f, 4);
  strokeR(rc(x, y, w, h), t.border, 1.f, 4);
  drawWrapped(g_ctipText, f, x + 8, y + 5, tw, t.chromeFg, 1.f, 16.f);
}
