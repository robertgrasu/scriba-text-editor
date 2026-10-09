// native popup menus (also used by the context menus); the test driver can pick an item by label
static void testLog(const wstring& s);
static void tipHide();
struct CtxItem {
  wstring label;
  std::function<void()> run;
  bool enabled = true, sep = false, checked = false;
  vector<CtxItem> sub;
};
static wstring g_ctipText;  // tooltip of the button under the pointer (chrometip.inc)
static bool g_ctipShow = false;
static void chromeTipHide() {
  if (g_ctipShow) { g_ctipShow = false; redraw(); }
  g_ctipText.clear();
  KillTimer(g_hwnd, 11);
}
static wstring g_ctxTestPick;  // when set (test driver), the item with this label is chosen instead of showing the menu
static bool g_ctxOpen = false;

static CtxItem ctxSep() { CtxItem i; i.sep = true; return i; }
static CtxItem ctxItem(const wstring& label, std::function<void()> run, bool enabled = true) {
  CtxItem i;
  i.label = label;
  i.run = std::move(run);
  i.enabled = enabled;
  return i;
}
static const CtxItem* ctxFind(const vector<CtxItem>& items, const wstring& label) {
  for (auto& i : items) {
    wstring l = i.label;
    size_t t = l.find(L'\t');
    if (t != wstring::npos) l = l.substr(0, t);
    if (l == label && !i.sep) return &i;
    if (const CtxItem* s = ctxFind(i.sub, label)) return s;
  }
  return nullptr;
}
static void ctxBuild(HMENU m, const vector<CtxItem>& items, vector<const CtxItem*>& flat) {
  for (auto& i : items) {
    if (i.sep) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); continue; }
    UINT fl = MF_STRING | (i.enabled ? 0 : MF_GRAYED) | (i.checked ? MF_CHECKED : 0);
    if (!i.sub.empty()) {
      HMENU sm = CreatePopupMenu();
      ctxBuild(sm, i.sub, flat);
      AppendMenuW(m, fl | MF_POPUP, (UINT_PTR)sm, i.label.c_str());
    } else {
      flat.push_back(&i);
      AppendMenuW(m, fl, flat.size(), i.label.c_str());
    }
  }
}
// uxtheme's undocumented dark-mode switch for native menus
static void ctxApplyDark() {
  static auto setMode = (int(WINAPI*)(int))GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(135));
  static auto flush = (void(WINAPI*)())GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(136));
  if (setMode) setMode(T().dark ? 2 : 3);  // ForceDark / ForceLight
  if (flush) flush();
}
static void ctxShow(const vector<CtxItem>& items, int px, int py) {
  if (items.empty()) return;
  if (!g_ctxTestPick.empty()) {
    wstring want = g_ctxTestPick;
    g_ctxTestPick.clear();
    const CtxItem* it = ctxFind(items, want);
    if (it && it->enabled && it->run) it->run();
    else testLog(L"ctx: no item " + want);
    return;
  }
  ctxApplyDark();
  HMENU m = CreatePopupMenu();
  vector<const CtxItem*> flat;
  ctxBuild(m, items, flat);
  POINT pt{px, py};
  ClientToScreen(g_hwnd, &pt);
  g_ctxOpen = true;
  tipHide();
  int cmd = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NOANIMATION, pt.x, pt.y, g_hwnd, nullptr);
  g_ctxOpen = false;
  DestroyMenu(m);
  if (cmd > 0 && cmd <= (int)flat.size() && flat[cmd - 1]->run) flat[cmd - 1]->run();
  redraw();
}

