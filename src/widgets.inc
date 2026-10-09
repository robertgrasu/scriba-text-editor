// widgets.inc: Visual mode of notes draws a line that holds only an image ![alt](src) as the picture, and a Markdown table as a grid, like
// Electron's visual.js. The lines of the block are folded into one row; with the caret inside the block the plain text shows instead.

struct ImgEntry {
  ID2D1Bitmap* bmp = nullptr;
  ID2D1RenderTarget* owner = nullptr;
  IWICFormatConverter* conv = nullptr;
  UINT w = 0, h = 0;
  bool failed = false;
  FILETIME stamp{};
};
static std::map<wstring, ImgEntry> g_imgs;
static bool wdUrlIsRemote(const wstring& u) { return u.compare(0, 7, L"http://") == 0 || u.compare(0, 8, L"https://") == 0 || u.compare(0, 5, L"data:") == 0; }
static wstring wdResolve(Doc& d, const wstring& src) {
  if (src.empty() || wdUrlIsRemote(src)) return L"";
  wstring s = src;
  if (s.compare(0, 8, L"file:///") == 0) s = s.substr(8);
  for (auto& c : s) if (c == L'/') c = L'\\';
  // %20 and friends
  wstring dec;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == L'%' && i + 2 < s.size() && iswxdigit(s[i + 1]) && iswxdigit(s[i + 2])) { dec += (wchar_t)wcstol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
    else dec += s[i];
  }
  if ((dec.size() > 1 && dec[1] == L':') || (!dec.empty() && dec[0] == L'\\')) return dec;
  wstring base = d.isNote ? notesDir() : d.path.empty() ? L"" : dirNameOf(d.path);
  return base.empty() ? L"" : base + L"\\" + dec;
}
static ImgEntry* wdImage(const wstring& path) {
  if (path.empty()) return nullptr;
  ImgEntry& e = g_imgs[path];
  WIN32_FILE_ATTRIBUTE_DATA fa{};
  GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa);
  if (CompareFileTime(&e.stamp, &fa.ftLastWriteTime) != 0) {  // changed on disk: load again
    if (e.bmp) { e.bmp->Release(); e.bmp = nullptr; }
    if (e.conv) { e.conv->Release(); e.conv = nullptr; }
    e.failed = false;
    e.w = e.h = 0;
    e.stamp = fa.ftLastWriteTime;
  }
  if (e.failed) return nullptr;
  if (!e.conv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);  // the session is restored before the main routine sets up COM
    IWICImagingFactory* f = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f)))) { e.failed = true; return nullptr; }
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* fr = nullptr;
    bool ok = SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) && dec && SUCCEEDED(dec->GetFrame(0, &fr)) && fr &&
              SUCCEEDED(f->CreateFormatConverter(&e.conv)) && SUCCEEDED(e.conv->Initialize(fr, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut));
    if (fr) fr->Release();
    if (dec) dec->Release();
    f->Release();
    if (!ok) { if (e.conv) { e.conv->Release(); e.conv = nullptr; } e.failed = true; return nullptr; }
    e.conv->GetSize(&e.w, &e.h);
  }
  if (e.bmp && e.owner != g_rt) { e.bmp->Release(); e.bmp = nullptr; }
  if (!e.bmp && g_rt) {  // before the window has a render target only the size is known
    if (FAILED(g_rt->CreateBitmapFromWicBitmap(e.conv, nullptr, &e.bmp))) { e.failed = true; return nullptr; }
    e.owner = g_rt;
  }
  return &e;
}

static bool wdImageLine(const wstring& line, wstring& alt, wstring& src) {
  wstring t = trimW(line);
  if (t.size() < 6 || t[0] != L'!' || t[1] != L'[' || t.back() != L')') return false;
  size_t close = t.find(L"](");
  if (close == wstring::npos) return false;
  alt = t.substr(2, close - 2);
  src = t.substr(close + 2, t.size() - close - 3);
  if (alt.find(L']') != wstring::npos || src.find(L')') != wstring::npos || src.empty()) return false;
  size_t q = src.find(L" \"");
  if (q != wstring::npos) src = src.substr(0, q);  // a title after the address
  if (src.size() > 1 && src[0] == L'<' && src.back() == L'>') src = src.substr(1, src.size() - 2);
  return true;
}
static void computeWidgets(Doc& d) {
  d.wAt.assign(d.lines.size(), -1);
  d.wb.clear();
  if (!d.vis || !d.isNote) return;
  int n = (int)d.lines.size();
  float W = std::max(120.f, edW() - 40);
  bool fence = false;
  for (int l = 0; l < n; l++) {
    wstring tl = trimW(d.lines[l]);
    if (tl.compare(0, 3, L"```") == 0 || tl.compare(0, 3, L"~~~") == 0) { fence = !fence; continue; }
    if (fence || d.isHidden(l)) continue;
    int l1 = l, kind = 0;
    wstring alt, src;
    if (wdImageLine(d.lines[l], alt, src)) kind = 1;
    else if (d.lines[l].find(L'|') != wstring::npos && l + 1 < n && pvIsTableSep(d.lines[l + 1])) {
      kind = 2;
      l1 = l + 2;
      while (l1 < n && d.lines[l1].find(L'|') != wstring::npos && !trimW(d.lines[l1]).empty()) l1++;
      l1--;
    }
    if (!kind) continue;
    // with the caret inside, the text itself is shown (that is how a table is edited)
    bool inside = (d.cur.l >= l && d.cur.l <= l1) || (d.anc.l >= l && d.anc.l <= l1);
    if (inside && kind == 2) { l = l1; continue; }
    WBlock wbk;
    wbk.l0 = l;
    wbk.l1 = l1;
    wbk.kind = kind;
    float fs = g_fontSize / 15.f;
    if (kind == 1) {
      size_t bar = alt.find(L'|');
      wbk.alt = alt.substr(0, bar);
      while (bar != wstring::npos) {
        size_t nx = alt.find(L'|', bar + 1);
        wstring tok = trimW(alt.substr(bar + 1, nx == wstring::npos ? wstring::npos : nx - bar - 1));
        bar = nx;
        if (tok == L"center") wbk.align = 1;
        else if (tok == L"right") wbk.align = 2;
        else if (!tok.empty() && (iswdigit(tok[0]))) {
          wbk.optPct = tok.back() == L'%';
          wbk.optW = (float)_wtof(tok.c_str());
        }
      }
      wbk.srcRaw = src;
      wbk.boxW = W;
      wbk.path = wdResolve(d, src);
      ImgEntry* im = wdImage(wbk.path);
      if (im && im->w && im->h) {
        float sc = std::min(1.f, std::min(W / im->w, 480.f / im->h));
        if (wbk.optW > 0) sc = std::min(W, wbk.optPct ? W * std::min(100.f, wbk.optW) / 100.f : wbk.optW) / im->w;
        wbk.iw = im->w * sc;
        wbk.ih = im->h * sc;
        wbk.h = wbk.ih + 13.f * fs;
      } else wbk.h = 25.f * fs;
    } else {
      auto blk = std::make_shared<CcBlock>();
      blk->prims = new vector<CcPrim>();
      blk->texts = new vector<CcText*>();
      CcBuild bd{blk.get(), W, nullptr, *blk->prims, *blk->texts};
      vector<wstring> tl2(d.lines.begin() + l, d.lines.begin() + l1 + 1);
      g_mdExtras = true;
      g_mdLinkCol = T().accent;
      g_mdTagCol = T().accent;
      float y = 0;
      pvRender(bd, tl2, l, 0, W, y, 1.f, -1.f);
      g_mdExtras = false;
      blk->h = y;
      blk->lastW = W;
      blk->dirty = false;
      wbk.blk = blk;
      wbk.h = y + 4.f - 11.6f;  // pvRender leaves a .8em margin above the table
      wbk.h = std::max(30.f, wbk.h + 8.f * fs);
    }
    for (int j = l; j <= l1; j++) d.wAt[j] = (int)d.wb.size();
    for (int j = l + 1; j <= l1; j++) d.hid[j] = 1;
    d.wb.push_back(wbk);
    l = l1;
  }
}
static const WBlock* widgetStart(const Doc& d, int l) {
  if (l < 0 || l >= (int)d.wAt.size() || d.wAt[l] < 0) return nullptr;
  const WBlock& w = d.wb[d.wAt[l]];
  return w.l0 == l ? &w : nullptr;
}
static float widgetHeightOf(const Doc& d, int l) {
  const WBlock* w = widgetStart(d, l);
  return w ? w->h : 0.f;
}
// buttons of the small bar over a selected picture
struct WdBtn { D2D1_RECT_F r; int id; int line; };
static vector<WdBtn> g_wdBtns;
static D2D1_RECT_F g_wdHandle{0, 0, 0, 0};
static int g_wdHandleLine = -1, g_wdAlign = 0;
static float g_wdImgX = 0, g_wdBoxX = 0, g_wdBoxW = 0;
static void wdFrameStart() { g_wdBtns.clear(); g_wdHandleLine = -1; }

static void wdDrawBar(float x, float y, int line, int align) {
  const Theme& t = T();
  const float bw = 30, bh = 28;
  static const int ids[] = {1, 2, 0, 3, 4, 5, 0, 6, 7, 8, 0, 9};
  float w = 8;
  for (int id : ids) w += id ? bw : 9;
  D2D1_RECT_F bar = rc(x, y, w, bh + 8);
  fillR(D2D1::RectF(bar.left + 1, bar.top + 3, bar.right + 1, bar.bottom + 3), 0x000000, 0.3f, 9);
  fillR(bar, t.menu, 1.f, 9);
  strokeR(bar, t.border, 1.f, 9);
  float cx = x + 4;
  for (int id : ids) {
    if (!id) { fillRect(cx + 4, y + 10, 1, 16, t.fg, 0.2f); cx += 9; continue; }
    D2D1_RECT_F r = rc(cx, y + 4, bw, bh);
    bool hov = inRect(r, g_mx, g_my), on = (id == 6 && align == 0) || (id == 7 && align == 1) || (id == 8 && align == 2);
    if (hov || on) fillR(r, t.fg, on ? 0.16f : 0.1f, 6);
    float mx = cx + bw / 2, my = y + 4 + bh / 2;
    g_br->SetColor(Rgb(t.chromeFg));
    auto label = [&](const wchar_t* sx) {
      float tw = textWidth(sx, g_uiSmall);
      drawText(sx, (int)wcslen(sx), mx - tw / 2, my - 9, t.chromeFg, g_uiSmall, tw + 6, 18);
    };
    if (id == 1) { g_rt->DrawLine(D2D1::Point2F(mx - 5, my), D2D1::Point2F(mx + 5, my), g_br, 1.6f); }
    else if (id == 2) { g_rt->DrawLine(D2D1::Point2F(mx - 5, my), D2D1::Point2F(mx + 5, my), g_br, 1.6f); g_rt->DrawLine(D2D1::Point2F(mx, my - 5), D2D1::Point2F(mx, my + 5), g_br, 1.6f); }
    else if (id == 3) label(L"25%");
    else if (id == 4) label(L"50%");
    else if (id == 5) label(L"100%");
    else if (id >= 6 && id <= 8) {  // three lines, flush left / centred / flush right
      float lens[3] = {10, 6, 8};
      for (int k = 0; k < 3; k++) {
        float len = lens[k] + (id == 7 ? 0 : 0), lx = id == 6 ? mx - 6 : id == 7 ? mx - len / 2 : mx + 6 - len;
        g_rt->DrawLine(D2D1::Point2F(lx, my - 5 + k * 5), D2D1::Point2F(lx + len, my - 5 + k * 5), g_br, 1.5f);
      }
    } else if (id == 9) {
      g_br->SetColor(Rgb(0xe0707a));
      g_rt->DrawLine(D2D1::Point2F(mx - 4.5f, my - 4.5f), D2D1::Point2F(mx + 4.5f, my + 4.5f), g_br, 1.6f);
      g_rt->DrawLine(D2D1::Point2F(mx - 4.5f, my + 4.5f), D2D1::Point2F(mx + 4.5f, my - 4.5f), g_br, 1.6f);
    }
    g_wdBtns.push_back({r, id, line});
    cx += bw;
  }
}
// draws the widget of a block whose first line is at y (x = left edge of the text)
static void drawWidget(Doc& d, const WBlock& w, float x, float y, float clipTop, float clipBottom) {
  const Theme& t = T();
  float fs = g_fontSize / 15.f;
  bool selected = w.kind == 1 && d.cur.l >= w.l0 && d.cur.l <= w.l1 && vpIsFocusedPane();
  if (w.kind == 1) {
    ImgEntry* im = wdImage(w.path);
    if (im && im->bmp) {
      float iy = y + 6.5f * fs;
      float ix = x + (w.align == 1 ? (w.boxW - w.iw) / 2 : w.align == 2 ? w.boxW - w.iw : 0.f);
      D2D1_RECT_F dst = rc(ix, iy, w.iw, w.ih);
      ID2D1BitmapBrush* br = nullptr;
      if (SUCCEEDED(g_rt->CreateBitmapBrush(im->bmp, &br)) && br) {
        float sx = w.iw / im->w, sy = w.ih / im->h;
        br->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy) * D2D1::Matrix3x2F::Translation(ix, iy));
        br->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        g_rt->FillRoundedRectangle(D2D1::RoundedRect(dst, 8, 8), br);
        br->Release();
      }
      if (selected) {
        g_br->SetColor(Rgb(t.accent));
        g_rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(dst.left - 1, dst.top - 1, dst.right + 1, dst.bottom + 1), 9, 9), g_br, 2.f);
        // the resize handle in the corner and the bar with size / alignment / delete
        D2D1_RECT_F hd = rc(dst.right - 6, dst.bottom - 6, 12, 12);
        fillR(hd, t.accent, 1.f, 3);
        strokeR(hd, t.bg, 1.f, 3);
        g_wdHandle = hd;
        g_wdHandleLine = w.l0;
        g_wdAlign = w.align;
        g_wdImgX = ix;
        g_wdBoxX = x;
        g_wdBoxW = w.boxW;
        float by = dst.top - 44;
        if (by < clipTop + 2) by = dst.top + 8;
        const float barW = 305.f;  // 9 buttons of 30 + 3 separators of 9 + 8 padding, as laid out by wdDrawBar
        float bx = std::max(x, std::min(dst.left + (w.iw - barW) / 2, x + w.boxW - barW));  // centred on the picture, kept inside the text column
        wdDrawBar(bx, by, w.l0, w.align);
      }
    } else {
      wstring s = L"\x25A1 " + (w.alt.empty() ? wstring(L"image not found") : w.alt);
      g_br->SetColor(Rgb(t.fg, 0.6f));
      g_rt->DrawText(s.c_str(), (UINT32)s.size(), ccFont(14.5f * fs, false, true, false), rc(x, y + 2 * fs, 600, 24 * fs), g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
  } else if (w.blk) {
    ccDrawBlock(nullptr, w.blk.get(), x, y - 11.6f + 3.f, clipTop, clipBottom);
  }
}
// rewrites the line of a picture with new size / alignment options:  ![alt|320|center](src)
static void wdWriteLine(Doc& d, int l, const WBlock& w, float widthPx, bool pct, int align) {
  wstring nl = L"![" + w.alt;
  if (widthPx > 0) nl += L"|" + std::to_wstring((int)std::lround(widthPx)) + (pct ? L"%" : L"");
  if (align == 1) nl += L"|center";
  else if (align == 2) nl += L"|right";
  nl += L"](" + w.srcRaw + L")";
  int n = (int)d.lines[l].size();
  d.replaceRange({l, 0}, {l, n}, nl);
  d.cur = d.anc = {l, (int)nl.size()};
  edited();
}
static const WBlock* wdBlockAt(Doc& d, int l) { return widgetStart(d, l); }
static bool wdMouseDown(float x, float y) {
  if (g_docs.empty()) return false;
  Doc& d = D();
  for (int i = (int)g_wdBtns.size() - 1; i >= 0; i--) {
    if (!inRect(g_wdBtns[i].r, x, y)) continue;
    const WBlock* wb = wdBlockAt(d, g_wdBtns[i].line);
    if (!wb) return true;
    WBlock w = *wb;
    int l = g_wdBtns[i].line;
    d.snapshot(0);
    switch (g_wdBtns[i].id) {
      case 1: wdWriteLine(d, l, w, std::max(40.f, w.iw * 0.8f), false, w.align); break;
      case 2: wdWriteLine(d, l, w, std::min(w.boxW, w.iw * 1.25f), false, w.align); break;
      case 3: wdWriteLine(d, l, w, 25, true, w.align); break;
      case 4: wdWriteLine(d, l, w, 50, true, w.align); break;
      case 5: wdWriteLine(d, l, w, 100, true, w.align); break;
      case 6: wdWriteLine(d, l, w, w.optW, w.optPct, 0); break;
      case 7: wdWriteLine(d, l, w, w.optW, w.optPct, 1); break;
      case 8: wdWriteLine(d, l, w, w.optW, w.optPct, 2); break;
      case 9:
        if (d.lines.size() == 1) d.replaceRange({0, 0}, {0, (int)d.lines[0].size()}, L"");
        else if (l + 1 < (int)d.lines.size()) d.replaceRange({l, 0}, {l + 1, 0}, L"");
        else d.replaceRange({l - 1, (int)d.lines[l - 1].size()}, {l, (int)d.lines[l].size()}, L"");
        d.cur = d.anc = {std::min(l, (int)d.lines.size() - 1), 0};
        edited();
        break;
    }
    return true;
  }
  if (g_wdHandleLine >= 0 && inRect(g_wdHandle, x, y)) {
    d.snapshot(0);
    g_wdDrag = true;
    SetCapture(g_hwnd);
    return true;
  }
  return false;
}
// dragging the corner: the width follows the pointer (a centred picture grows on both sides)
static void wdDragMove(float x) {
  if (g_docs.empty() || g_wdHandleLine < 0) return;
  Doc& d = D();
  const WBlock* wb = wdBlockAt(d, g_wdHandleLine);
  if (!wb) return;
  WBlock w = *wb;
  float width = g_wdAlign == 1 ? 2 * (x - (g_wdBoxX + g_wdBoxW / 2)) : g_wdAlign == 2 ? g_wdBoxX + g_wdBoxW - x : x - g_wdBoxX;
  width = std::max(40.f, std::min(g_wdBoxW, width));
  wdWriteLine(d, g_wdHandleLine, w, width, false, w.align);
}
// the caret is on a picture: the picture is "selected"; typing, Enter and paste start a new line below it, Backspace / Delete remove it
static bool imageLineActive(const Doc& d) {
  if (!d.vis || !d.isNote || d.hasSel() || d.cur.l < 0 || d.cur.l >= (int)d.wAt.size() || d.wAt[d.cur.l] < 0) return false;
  return d.wb[d.wAt[d.cur.l]].kind == 1;
}
static void imageLineEscape(Doc& d) {
  int l = d.cur.l, n = (int)d.lines[l].size();
  d.snapshot(0);
  d.replaceRange({l, n}, {l, n}, L"\n");
  d.cur = d.anc = {l + 1, 0};
  edited();
}
static bool imageLineKey(WPARAM wp) {
  Doc& d = D();
  if (!imageLineActive(d)) return false;
  int l = d.cur.l, n = (int)d.lines[l].size();
  if (wp == VK_BACK || wp == VK_DELETE) {
    d.snapshot(0);
    if (d.lines.size() == 1) d.replaceRange({0, 0}, {0, n}, L"");
    else if (l + 1 < (int)d.lines.size()) d.replaceRange({l, 0}, {l + 1, 0}, L"");
    else d.replaceRange({l - 1, (int)d.lines[l - 1].size()}, {l, n}, L"");
    int nl = std::min(l, (int)d.lines.size() - 1);
    d.cur = d.anc = {nl, wp == VK_BACK && l > 0 && l + 1 >= (int)d.lines.size() + 1 ? 0 : 0};
    edited();
    return true;
  }
  if (wp == VK_RETURN) { imageLineEscape(d); return true; }
  if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_HOME || wp == VK_END) {
    int tl = wp == VK_RIGHT || wp == VK_END ? std::min(l + 1, (int)d.lines.size() - 1) : std::max(l - 1, 0);
    d.cur = d.anc = {tl, wp == VK_LEFT && tl != l ? (int)d.lines[tl].size() : 0};
    d.wantV = rowVcol(d, d.cur);
    ensureVisible();
    redraw();
    return true;
  }
  return false;
}
