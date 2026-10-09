// modal.inc: the message boxes of the editor, drawn like Electron's .modal (a card near the top with the text and buttons on the right)
// instead of the system MessageBox. appMsgBox() has the signature of MessageBoxW and runs its own small message loop until a button is chosen.

struct ModalBtn { wstring label; int id; bool primary; };
struct ModalState {
  bool open = false;
  wstring text;
  vector<ModalBtn> btns;
  int focus = 0, result = 0, cancelId = 0;
};
static ModalState g_modal;
static std::deque<int> g_modalAuto;  // test driver: answers given without showing the box
static vector<D2D1_RECT_F> g_modalRects;

static float modalW() { return std::min(440.f, cw() - 40); }
static void modalLayout(float& x, float& y, float& w, float& h, float& textH) {
  w = modalW();
  x = (cw() - w) / 2;
  y = 120;
  textH = wrappedHeight(g_modal.text, uiFont(13.f), w - 42, 19.5f);
  h = 18 + textH + 16 + 30 + 18;
}
static void drawModal() {
  if (!g_modal.open) return;
  const Theme& t = T();
  float x, y, w, h, th;
  modalLayout(x, y, w, h, th);
  fillR(D2D1::RectF(x + 2, y + 6, x + w + 2, y + h + 8), 0x000000, 0.3f, 4);
  fillR(rc(x, y, w, h), t.menu, 1.f, 2);
  strokeR(rc(x, y, w, h), t.border, 1.f, 2);
  drawWrapped(g_modal.text, uiFont(13.f), x + 21, y + 18, w - 42, t.chromeFg, 1.f, 19.5f);
  // buttons from the right
  g_modalRects.assign(g_modal.btns.size(), rc(0, 0, 0, 0));
  float bx = x + w - 20, by = y + 18 + th + 16;
  for (int i = (int)g_modal.btns.size() - 1; i >= 0; i--) {
    const ModalBtn& b = g_modal.btns[i];
    float bw = textWidth(b.label, g_ui) + 34;
    D2D1_RECT_F r = rc(bx - bw, by, bw, 30);
    g_modalRects[i] = r;
    bool hov = inRect(r, g_mx, g_my), foc = i == g_modal.focus;
    DWORD bg = b.primary ? (hov || foc ? 0xd98a38 : 0xc97d2f) : (hov || foc ? t.btnHi : t.btn);
    fillR(r, bg, 1.f, 3);
    strokeR(r, foc && !b.primary ? t.accent : t.border, 1.f, 3);
    float lw = textWidth(b.label, g_ui);
    drawText(b.label.c_str(), (int)b.label.size(), r.left + (bw - lw) / 2, r.top + 5, b.primary ? 0xffffff : t.chromeFg, g_ui, bw, 22);
    bx -= bw + 8;
  }
}
static void modalFinish(int id) {
  g_modal.result = id;
  g_modal.open = false;
  redraw();
}
static bool modalKey(WPARAM wp) {
  if (!g_modal.open) return false;
  int n = (int)g_modal.btns.size();
  if (wp == VK_ESCAPE) modalFinish(g_modal.cancelId);
  else if (wp == VK_RETURN || wp == VK_SPACE) modalFinish(g_modal.btns[g_modal.focus].id);
  else if (wp == VK_TAB || wp == VK_RIGHT || wp == VK_LEFT) {
    bool back = wp == VK_LEFT || (wp == VK_TAB && (GetKeyState(VK_SHIFT) & 0x8000));
    g_modal.focus = (g_modal.focus + (back ? n - 1 : 1)) % n;
    redraw();
  } else if (wp == 'Y' || wp == 'N') {
    for (auto& b : g_modal.btns) if ((wp == 'Y' && b.id == IDYES) || (wp == 'N' && b.id == IDNO)) modalFinish(b.id);
  }
  return true;
}
static bool modalMouse(float x, float y) {
  if (!g_modal.open) return false;
  for (size_t i = 0; i < g_modalRects.size(); i++)
    if (inRect(g_modalRects[i], x, y)) { modalFinish(g_modal.btns[i].id); break; }
  return true;
}
static int appMsgBox(HWND, const wchar_t* text, const wchar_t*, UINT flags) {
  UINT kind = flags & 0xF;
  g_modal = ModalState();
  g_modal.text = text;
  auto add = [&](const wchar_t* l, int id, bool primary) { g_modal.btns.push_back({l, id, primary}); };
  if (kind == MB_YESNOCANCEL) { add(L"Yes", IDYES, true); add(L"No", IDNO, false); add(L"Cancel", IDCANCEL, false); g_modal.cancelId = IDCANCEL; }
  else if (kind == MB_YESNO) { add(L"Yes", IDYES, true); add(L"No", IDNO, false); g_modal.cancelId = IDNO; }
  else if (kind == MB_OKCANCEL) { add(L"OK", IDOK, true); add(L"Cancel", IDCANCEL, false); g_modal.cancelId = IDCANCEL; }
  else { add(L"OK", IDOK, true); g_modal.cancelId = IDOK; }
  if (!g_modalAuto.empty()) {  // scripted test
    int a = g_modalAuto.front();
    g_modalAuto.pop_front();
    return a;
  }
  g_modal.open = true;
  redraw();
  UpdateWindow(g_hwnd);
  MSG msg;
  while (g_modal.open) {
    if (!GetMessageW(&msg, nullptr, 0, 0)) { PostQuitMessage((int)msg.wParam); g_modal.open = false; g_modal.result = g_modal.cancelId; break; }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return g_modal.result ? g_modal.result : g_modal.cancelId;
}
