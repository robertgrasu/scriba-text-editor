// mdpreview.inc: the side-by-side Markdown preview (View > Toggle Markdown Preview). Same blocks as Electron's mdpreview.js:
// headings, paragraphs, lists (nested, to-do boxes you can tick), quotes, tables, code, rules, links and #tags.

enum { HT_CHECK = 60 };
static CcBlock g_pvBlock;
static const Doc* g_pvDoc = nullptr;
static unsigned g_pvVer = ~0u;
static float g_pvW = -1;
static bool g_pvLines = false;
static vector<CcHit> g_pvHits;
static float g_pvContentH = 0;

static bool pvIsTableSep(const wstring& l) {
  if (l.find(L'-') == wstring::npos) return false;
  size_t dashes = 0;
  for (wchar_t c : l) {
    if (c == L'-') dashes++;
    else if (c != L'|' && c != L':' && !iswspace(c)) return false;
  }
  return dashes >= 2;
}
static vector<wstring> pvCells(wstring l) {
  l = trimW(l);
  if (!l.empty() && l[0] == L'|') l.erase(0, 1);
  if (!l.empty() && l.back() == L'|') l.pop_back();
  vector<wstring> out;
  size_t p = 0;
  for (;;) {
    size_t q = l.find(L'|', p);
    out.push_back(trimW(l.substr(p, q == wstring::npos ? wstring::npos : q - p)));
    if (q == wstring::npos) break;
    p = q + 1;
  }
  return out;
}
struct PvList { int indent; bool ol; int count; };

// x, w: the column of the content; y: where the next block starts (it moves down); base: number of lines above `lines` in the note
static void pvRender(CcBuild& bd, const vector<wstring>& lines, int base, float x, float w, float& y, float alpha, float lnX) {
  const Theme& t = T();
  const float FS = 14.5f, LH = 24.f;
  float prevMb = -1;
  auto gap = [&](float mt, float mb) {
    if (prevMb >= 0) y += std::max(prevMb, mt);
    else y += mt;
    prevMb = mb;
  };
  auto lineNo = [&](int ln, float yy) {
    if (!g_pvLines || lnX < 0) return;
    wstring s = std::to_wstring(ln);
    bd.str(s, lnX, yy + 4, 40, 16, 11.f, t.gutterFg, 0.9f);
  };
  auto textAt = [&](const wstring& raw, unsigned fl, float tx, float tw, float size, float lineH, bool bold, DWORD col, float a) {
    CcPara para;
    mdInline(raw, fl, para);
    CcText* tt = bd.mk(para.text.empty() ? L" " : para.text, para.spans, tw, size, lineH, bold);
    CcPrim& p = bd.put(tt, tx, y, col, a);
    for (auto& sp : para.spans) if (sp.fl & 8) p.hit = HT_TEXT;
    return tt;
  };
  size_t i = 0;
  vector<wstring> para;
  int paraStart = 0;
  auto flush = [&] {
    if (para.empty()) return;
    gap(14.5f * 0.6f, 14.5f * 0.6f);
    wstring joined;
    for (size_t k = 0; k < para.size(); k++) { if (k) joined += L'\n'; joined += para[k]; }
    lineNo(base + paraStart + 1, y);
    CcText* tt = textAt(joined, 0, x, w, FS, LH, false, t.fg, alpha);
    y += tt->h;
    para.clear();
  };
  while (i < lines.size()) {
    const wstring& line = lines[i];
    int L = base + (int)i + 1;
    wstring tl = trimW(line);
    // fenced code
    if (tl.compare(0, 3, L"```") == 0 || tl.compare(0, 3, L"~~~") == 0) {
      wstring fence = tl.substr(0, 3);
      wstring rest = trimW(tl.substr(3));
      bool okLang = true;
      for (wchar_t c : rest) if (!(iswalnum(c) || c == L'_' || c == L'-')) okLang = false;
      if (okLang) {
        flush();
        wstring body;
        i++;
        bool first = true;
        while (i < lines.size() && trimW(lines[i]) != fence) { if (!first) body += L'\n'; body += lines[i]; first = false; i++; }
        i++;
        gap(0.8f * 14.5f, 0.8f * 14.5f);
        CcText* tt = bd.mk(body.empty() ? L" " : body, {}, w - 28, 13.f, 19.5f, false, false, true, false);
        float h = tt->h + 24;
        lineNo(L, y);
        bd.fill(x, y, w, h, mixc(t.bg, t.fg, 0.07f), alpha, 8);
        bd.stroke(x, y, w, h, t.fg, 0.2f * alpha, 8);
        bd.put(tt, x + 14, y + 12, t.fg, alpha, tt->h);
        y += h;
        continue;
      }
    }
    if (tl.empty()) { flush(); i++; continue; }
    // heading
    {
      size_t k = 0;
      while (k < line.size() && line[k] == L'#') k++;
      if (k >= 1 && k <= 6 && k < line.size() && iswspace(line[k])) {
        flush();
        wstring txt = trimW(line.substr(k));
        while (!txt.empty() && txt.back() == L'#') txt.pop_back();
        txt = trimW(txt);
        float sz = k == 1 ? FS * 1.75f : k == 2 ? FS * 1.4f : k == 3 ? FS * 1.15f : FS;
        gap(k == 1 && prevMb < 0 ? 0.f : 1.3f * sz, 0.5f * sz);
        lineNo(L, y);
        CcText* tt = textAt(txt, 0, x, w, sz, sz * 1.25f, true, t.fg, alpha);
        y += tt->h;
        if (k == 1) { y += sz * 0.25f; bd.fill(x, y, w, 1, 0x808080, 0.25f * alpha); y += 1; }
        i++;
        continue;
      }
    }
    // horizontal rule
    {
      wchar_t c0 = 0;
      int cnt = 0;
      bool rule = !tl.empty() && (tl[0] == L'-' || tl[0] == L'*' || tl[0] == L'_');
      if (rule) {
        c0 = tl[0];
        for (wchar_t c : tl) { if (c == c0) cnt++; else if (!iswspace(c)) rule = false; }
        rule = rule && cnt >= 3;
      }
      if (rule) {
        flush();
        gap(1.4f * FS, 1.4f * FS);
        lineNo(L, y);
        bd.fill(x, y, w, 1, 0x808080, 0.3f * alpha);
        y += 1;
        i++;
        continue;
      }
    }
    // quote
    if (tl[0] == L'>') {
      flush();
      vector<wstring> q;
      size_t start = i;
      while (i < lines.size() && trimW(lines[i]).size() && trimW(lines[i])[0] == L'>') {
        wstring s = trimW(lines[i]).substr(1);
        if (!s.empty() && s[0] == L' ') s.erase(0, 1);
        q.push_back(s);
        i++;
      }
      gap(0.8f * FS, 0.8f * FS);
      float y0 = y;
      float qy = y + 0.1f * FS;
      float pm = prevMb;
      float inner = qy;
      pvRender(bd, q, base + (int)start, x + 19, w - 19, inner, alpha * 0.85f, -1);
      lineNo(base + (int)start + 1, y0);
      float h = std::max(10.f, inner - y0 + 0.1f * FS);
      bd.fill(x, y0, 3, h, t.accent, 0.6f * alpha);
      y = y0 + h;
      prevMb = std::max(pm, 0.8f * FS);
      prevMb = 0.8f * FS;
      continue;
    }
    // table
    if (line.find(L'|') != wstring::npos && i + 1 < lines.size() && pvIsTableSep(lines[i + 1])) {
      flush();
      vector<wstring> head = pvCells(line);
      size_t start = i;
      i += 2;
      vector<vector<wstring>> rows;
      while (i < lines.size() && lines[i].find(L'|') != wstring::npos && !trimW(lines[i]).empty()) rows.push_back(pvCells(lines[i++]));
      size_t cols = head.size();
      for (auto& r : rows) cols = std::max(cols, r.size());
      vector<float> nat(cols, 40.f);
      auto measure = [&](const vector<wstring>& r, bool bold) {
        for (size_t c = 0; c < r.size(); c++) nat[c] = std::max(nat[c], ccTextW(r[c], FS, bold) + 26);
      };
      measure(head, true);
      for (auto& r : rows) measure(r, false);
      float sum = 0;
      for (float v : nat) sum += v;
      float scale = sum > w ? w / sum : 1.f;
      gap(0.8f * FS, 0.8f * FS);
      lineNo(base + (int)start + 1, y);
      float ty = y;
      auto drawRow = [&](const vector<wstring>& r, bool isHead) {
        float rowH = 0, cx = x;
        vector<CcText*> tts;
        for (size_t c = 0; c < cols; c++) {
          CcPara pp;
          mdInline(c < r.size() ? r[c] : L"", isHead ? 1u : 0u, pp);
          CcText* tt = bd.mk(pp.text.empty() ? L" " : pp.text, pp.spans, nat[c] * scale - 24, FS, 21.f, isHead);
          tts.push_back(tt);
          rowH = std::max(rowH, tt->h + 12);
        }
        for (size_t c = 0; c < cols; c++) {
          float cw2 = nat[c] * scale;
          if (isHead) bd.fill(cx, ty, cw2, rowH, t.fg, 0.07f * alpha);
          bd.stroke(cx, ty, cw2 + 1, rowH + 1, t.fg, 0.3f * alpha);
          CcPrim& p = bd.put(tts[c], cx + 12, ty + 6, t.fg, alpha);
          if (c < r.size()) { CcPara pp; mdInline(r[c], 0, pp); for (auto& sp : pp.spans) if (sp.fl & 8) p.hit = HT_TEXT; }
          cx += cw2;
        }
        ty += rowH;
      };
      drawRow(head, true);
      for (auto& r : rows) drawRow(r, false);
      y = ty + 1;
      continue;
    }
    // lists (nested by indentation)
    {
      size_t k = 0, ind = 0;
      while (k < line.size() && iswspace(line[k])) { ind += line[k] == L'\t' ? 4 : 1; k++; }
      bool bullet = k + 1 < line.size() && (line[k] == L'-' || line[k] == L'*' || line[k] == L'+') && iswspace(line[k + 1]);
      size_t d = k;
      while (d < line.size() && iswdigit(line[d])) d++;
      bool numbered = d > k && d + 1 < line.size() && (line[d] == L'.' || line[d] == L')') && iswspace(line[d + 1]);
      if (bullet || numbered) {
        flush();
        vector<PvList> stack;
        gap(0.5f * FS, 0.5f * FS);
        while (i < lines.size()) {
          const wstring& ln = lines[i];
          size_t kk = 0, ii = 0;
          while (kk < ln.size() && iswspace(ln[kk])) { ii += ln[kk] == L'\t' ? 4 : 1; kk++; }
          bool b2 = kk + 1 < ln.size() && (ln[kk] == L'-' || ln[kk] == L'*' || ln[kk] == L'+') && iswspace(ln[kk + 1]);
          size_t dd = kk;
          while (dd < ln.size() && iswdigit(ln[dd])) dd++;
          bool n2 = dd > kk && dd + 1 < ln.size() && (ln[dd] == L'.' || ln[dd] == L')') && iswspace(ln[dd + 1]);
          if (!b2 && !n2) break;
          while (!stack.empty() && (int)ii < stack.back().indent) stack.pop_back();
          if (stack.empty() || (int)ii > stack.back().indent) stack.push_back({(int)ii, n2, 0});
          PvList& lv = stack.back();
          lv.count++;
          size_t ts = n2 ? dd + 1 : kk + 1;
          while (ts < ln.size() && iswspace(ln[ts])) ts++;
          wstring txt = ln.substr(ts);
          bool task = false, checked = false;
          if (txt.size() >= 3 && txt[0] == L'[' && (txt[1] == L' ' || txt[1] == L'x' || txt[1] == L'X') && txt[2] == L']' && (txt.size() == 3 || iswspace(txt[3]))) {
            task = true;
            checked = txt[1] != L' ';
            txt = trimW(txt.substr(3));
          }
          int depth = (int)stack.size() - 1;
          float ix = x + 21.75f * (depth + 1) - (task ? 18.85f : 0);
          float tx = ix + (task ? 0 : 0);
          float rowTop = y + 0.2f * FS;
          if (i > 0 && depth >= 0) {}
          lineNo(base + (int)i + 1, y);
          float textX = ix + (task ? 21 : 0);
          CcText* tt = textAt(txt, 0, textX, w - (textX - x), FS, LH, false, t.fg, alpha);
          (void)tx;
          // the marker
          if (task) {
            float by = y + (LH - 13) / 2 + 1;
            bd.stroke(ix + 1, by, 13, 13, checked ? t.accent : t.fg, checked ? 1.f : 0.6f * alpha, 3);
            if (checked) {
              bd.fill(ix + 1, by, 13, 13, t.accent, 0.9f, 3);
              CcPrim& chk = bd.add(PK_ICON, ix + 2, by + 1, 11, 11);
              chk.icon = CI_CHECK;
              chk.col = 0x1f1f1f;
            }
            bd.hit(ix, y, 16, LH, HT_CHECK, base + (int)i + 1);
          } else if (lv.ol) {
            wstring m = std::to_wstring(lv.count) + L".";
            float mw = ccTextW(m, FS);
            bd.str(m, ix - 6 - mw, y + (LH - FS * 1.33f) / 2, mw + 2, LH, FS, t.fg, alpha);
          } else {
            const wchar_t* m = depth == 0 ? L"\x2022" : depth == 1 ? L"\x25E6" : L"\x25AA";
            float mw = ccTextW(m, FS);
            bd.str(m, ix - 6 - mw, y + (LH - FS * 1.33f) / 2, mw + 2, LH, FS, t.fg, alpha);
          }
          (void)rowTop;
          y += tt->h + 0.4f * FS;
          i++;
        }
        y -= 0.4f * FS;
        prevMb = 0.5f * FS;
        continue;
      }
    }
    if (para.empty()) paraStart = (int)i;
    para.push_back(tl);
    i++;
  }
  flush();
}
static void pvRebuild(Doc& d, float W) {
  ccFreeLayout(&g_pvBlock);
  g_pvBlock.prims = new vector<CcPrim>();
  g_pvBlock.texts = new vector<CcText*>();
  g_pvBlock.kind = CB_AI;
  const Theme& t = T();
  g_mdExtras = true;
  g_mdLinkCol = t.accent;
  g_mdTagCol = t.accent;
  CcBuild bd{&g_pvBlock, W, nullptr, *g_pvBlock.prims, *g_pvBlock.texts};
  g_pvLines = g_vLines;
  float lnW = 0, x = 0;
  if (g_pvLines) {
    int digits = (int)std::to_wstring(std::max<size_t>(1, d.lines.size())).size();
    lnW = digits * 7.4f + 2 + 10;
    x = lnW;
  }
  float y = 0;
  pvRender(bd, d.lines, 0, x, W - x, y, 1.f, g_pvLines ? 0.f : -1.f);
  g_mdExtras = false;
  g_pvBlock.h = y;
  g_pvBlock.lastW = W;
  g_pvBlock.dirty = false;
  g_pvContentH = y;
}

static void drawMdPreview() {
  g_pvHits.clear();
  if (g_mdCut <= 0 || g_docs.empty()) return;
  Doc& d = D();
  if (!d.mdPrev || d.lang != L_MD) return;
  const Theme& t = T();
  D2D1_RECT_F s = g_mdStrip;
  fillR(s, t.bg, 1.f);
  fillRect(s.left, s.top, 1, s.bottom - s.top, 0x808080, 0.22f);
  float padX = 28, padTop = 22;
  float W = std::min(780.f, s.right - s.left - 2 * padX - 8);
  if (g_pvDoc != &d || g_pvVer != d.ver || std::fabs(g_pvW - W) > 0.5f || g_pvLines != g_vLines || !g_pvBlock.prims) {
    g_pvDoc = &d;
    g_pvVer = d.ver;
    g_pvW = W;
    pvRebuild(d, W);
  }
  float viewH = s.bottom - s.top;
  float total = g_pvContentH + padTop + 60;
  float maxS = std::max(0.f, total - viewH);
  // follows the editor: the same fraction of the way down
  if (d.pvEdScroll != d.scrollY) {
    float em = maxScrollY();
    d.pvScroll = em > 0 ? maxS * (d.scrollY / em) : 0;
    d.pvEdScroll = d.scrollY;
  }
  d.pvScroll = std::max(0.f, std::min(d.pvScroll, maxS));
  g_rt->PushAxisAlignedClip(D2D1::RectF(s.left + 1, s.top, s.right, s.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
  size_t before = g_ccHits.size();
  ccDrawBlock(nullptr, &g_pvBlock, s.left + padX, s.top + padTop - d.pvScroll, s.top, s.bottom);
  for (size_t i = before; i < g_ccHits.size(); i++) g_pvHits.push_back(g_ccHits[i]);
  g_ccHits.resize(before);
  g_rt->PopAxisAlignedClip();
  if (total > viewH) {
    float th = std::max(30.f, viewH * viewH / total), ty = s.top + (viewH - th) * (d.pvScroll / maxS);
    fillR(rc(s.right - 9, ty, 6, th), mixc(t.bg, t.fg, 0.32f), 0.85f, 3);
  }
}
static bool pvMouse(float x, float y) {
  if (g_mdCut <= 0 || !inRect(g_mdStrip, x, y)) return false;
  for (int i = (int)g_pvHits.size() - 1; i >= 0; i--) {
    const CcHit& h = g_pvHits[i];
    if (!inRect(h.r, x, y)) continue;
    if (h.kind == HT_TEXT && g_pvBlock.prims && h.a >= 0 && h.a < (int)g_pvBlock.prims->size() && (*g_pvBlock.prims)[h.a].text) {
      wstring url = ccLinkAt(*(*g_pvBlock.prims)[h.a].text, h.ox, h.oy, x, y);
      if (!url.empty()) ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (h.kind == HT_CHECK) {
      // tick or untick the item in the note itself
      Doc& d = D();
      int ln = h.a - 1;
      if (ln >= 0 && ln < (int)d.lines.size()) {
        size_t p = d.lines[ln].find(L'[');
        if (p != wstring::npos && p + 2 < d.lines[ln].size() && d.lines[ln][p + 2] == L']') {
          d.snapshot(0);
          wchar_t cur = d.lines[ln][p + 1];
          d.replaceRange({ln, (int)p + 1}, {ln, (int)p + 2}, cur == L' ' ? L"x" : L" ");
          edited();
        }
      }
    }
    break;
  }
  redraw();
  return true;
}
static bool pvWheel(float x, float y, int delta) {
  if (g_mdCut <= 0 || !inRect(g_mdStrip, x, y)) return false;
  Doc& d = D();
  d.pvScroll -= delta / 120.f * 60;
  redraw();
  return true;
}
static void toggleMdPreview() {
  if (g_docs.empty()) return;
  Doc& d = D();
  if (d.lang != L_MD) { flash(L"Preview works on Markdown files and notes"); return; }
  d.mdPrev = !d.mdPrev;
  d.pvEdScroll = -1;
  g_pvDoc = nullptr;
  computeLayout();
  ensureVisible();
  redraw();
}
