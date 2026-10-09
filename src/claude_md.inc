// claude_md.inc: the Markdown subset of the chat (fenced code, inline code, bold, italic, links, lists, headings, paragraphs) and the
// DirectWrite text with per-range colours that the chat is made of. Same subset as renderMd() in Electron's claude.js.

struct CcSpan { int a, b; unsigned fl; wstring url; };  // fl: 1 bold, 2 italic, 4 code, 8 link
struct CcPara { wstring text; vector<CcSpan> spans; };
struct CcMd {
  int kind = 0;  // 0 paragraph, 1 heading, 2 list item, 3 code block
  int level = 0;
  wstring marker;  // "\x2022" or "3."
  bool lastOfList = false;
  CcPara p;
  wstring code;
};

static void mdAdd(CcPara& p, const wstring& s, unsigned fl, const wstring& url = L"") {
  if (s.empty()) return;
  int a = (int)p.text.size();
  p.text += s;
  if (fl) p.spans.push_back({a, (int)p.text.size(), fl, url});
}
static bool mdIsPunctAfter(wchar_t c) { return iswspace(c) || wcschr(L").,;:!?", c) != nullptr; }
static bool g_mdExtras = false;  // preview: also ~~strike~~, __bold__, _italic_ and #tags
static DWORD g_mdLinkCol = 0xd97757, g_mdTagCol = 0xd97757;
static void mdInline(const wstring& s, unsigned fl, CcPara& out) {
  size_t i = 0, n = s.size();
  wstring run;
  auto flush = [&] { mdAdd(out, run, fl); run.clear(); };
  while (i < n) {
    wchar_t c = s[i];
    if (g_mdExtras && c == L'~' && i + 1 < n && s[i + 1] == L'~') {
      size_t e = s.find(L"~~", i + 2);
      if (e != wstring::npos && e > i + 2 && s.find(L'\n', i) > e) { flush(); mdInline(s.substr(i + 2, e - i - 2), fl | 16, out); i = e + 2; continue; }
    } else if (g_mdExtras && c == L'_' && i + 1 < n && s[i + 1] == L'_') {
      size_t e = s.find(L"__", i + 2);
      if (e != wstring::npos && e > i + 2 && s.find(L'\n', i) > e) { flush(); mdInline(s.substr(i + 2, e - i - 2), fl | 1, out); i = e + 2; continue; }
    } else if (g_mdExtras && c == L'_' && (i == 0 || (!iswalnum(s[i - 1]) && s[i - 1] != L'_'))) {
      size_t e = i + 1;
      while (e < n && s[e] != L'_' && s[e] != L'\n') e++;
      if (e < n && e > i + 1 && (e + 1 == n || (!iswalnum(s[e + 1]) && s[e + 1] != L'_'))) { flush(); mdInline(s.substr(i + 1, e - i - 1), fl | 2, out); i = e + 1; continue; }
    } else if (g_mdExtras && c == L'#' && (i == 0 || iswspace(s[i - 1]) || s[i - 1] == L'(')) {
      size_t e = i + 1;
      while (e < n && (iswalnum(s[e]) || s[e] == L'_' || s[e] == L'-')) e++;
      if (e - i - 1 >= 2) { flush(); mdAdd(out, s.substr(i, e - i), fl | 32); i = e; continue; }
    }
    if (c == L'`') {
      size_t e = i + 1;
      while (e < n && s[e] != L'`' && s[e] != L'\n') e++;
      if (e < n && s[e] == L'`' && e > i + 1) { flush(); mdAdd(out, s.substr(i + 1, e - i - 1), fl | 4); i = e + 1; continue; }
    } else if (c == L'*' && i + 1 < n && s[i + 1] == L'*') {
      size_t e = i + 2;
      while (e < n && s[e] != L'*' && s[e] != L'\n') e++;
      if (e + 1 < n && s[e] == L'*' && s[e + 1] == L'*' && e > i + 2) { flush(); mdInline(s.substr(i + 2, e - i - 2), fl | 1, out); i = e + 2; continue; }
    } else if (c == L'*' && (i == 0 || iswspace(s[i - 1]) || s[i - 1] == L'(') && i + 1 < n && s[i + 1] != L'*') {
      size_t e = i + 1;
      while (e < n && s[e] != L'*' && s[e] != L'\n') e++;
      if (e < n && s[e] == L'*' && e > i + 1 && (e + 1 == n || mdIsPunctAfter(s[e + 1]))) { flush(); mdInline(s.substr(i + 1, e - i - 1), fl | 2, out); i = e + 1; continue; }
    } else if (c == L'[') {
      size_t e = s.find(L']', i + 1);
      if (e != wstring::npos && e > i + 1 && e + 1 < n && s[e + 1] == L'(' && s.find(L'\n', i) > e) {
        size_t u = e + 2, ue = u;
        while (ue < n && s[ue] != L')' && !iswspace(s[ue])) ue++;
        wstring url = s.substr(u, ue - u);
        if (ue < n && s[ue] == L')' && (url.compare(0, 5, L"http:") == 0 || url.compare(0, 6, L"https:") == 0)) {
          flush();
          mdAdd(out, s.substr(i + 1, e - i - 1), fl | 8, url);
          i = ue + 1;
          continue;
        }
      }
    }
    run += c;
    i++;
  }
  flush();
}
static vector<wstring> mdSplitLines(const wstring& s) {
  vector<wstring> v;
  size_t p = 0;
  for (;;) {
    size_t q = s.find(L'\n', p);
    if (q == wstring::npos) { v.push_back(s.substr(p)); break; }
    v.push_back(s.substr(p, q - p));
    p = q + 1;
  }
  return v;
}
static bool mdIsBlank(const wstring& s) {
  for (wchar_t c : s) if (!iswspace(c)) return false;
  return true;
}
static vector<CcMd> mdParse(const wstring& src) {
  vector<CcMd> out;
  size_t p = 0;
  while (p <= src.size()) {
    size_t f = src.find(L"```", p);
    wstring part = f == wstring::npos ? src.substr(p) : src.substr(p, f - p);
    // prose before the fence
    int list = 0;  // 1 ul, 2 ol
    int olNo = 0;
    vector<wstring> para;
    auto flushPara = [&] {
      if (para.empty()) return;
      CcMd m;
      wstring j;
      for (size_t i = 0; i < para.size(); i++) { if (i) j += L'\n'; j += para[i]; }
      mdInline(j, 0, m.p);
      out.push_back(m);
      para.clear();
    };
    auto closeList = [&] {
      if (list && !out.empty()) out.back().lastOfList = true;
      list = 0;
      olNo = 0;
    };
    for (auto& line : mdSplitLines(part)) {
      size_t k = 0;
      while (k < line.size() && line[k] == L'#') k++;
      if (k >= 1 && k <= 4 && k < line.size() && iswspace(line[k])) {
        flushPara();
        closeList();
        size_t s2 = k;
        while (s2 < line.size() && iswspace(line[s2])) s2++;
        CcMd m;
        m.kind = 1;
        m.level = (int)k;
        mdInline(line.substr(s2), 1, m.p);
        out.push_back(m);
        continue;
      }
      size_t i = 0;
      while (i < line.size() && iswspace(line[i])) i++;
      if (i + 1 < line.size() && (line[i] == L'-' || line[i] == L'*') && iswspace(line[i + 1])) {
        flushPara();
        if (list != 1) { closeList(); list = 1; }
        size_t s2 = i + 1;
        while (s2 < line.size() && iswspace(line[s2])) s2++;
        CcMd m;
        m.kind = 2;
        m.marker = L"\x2022";
        mdInline(line.substr(s2), 0, m.p);
        out.push_back(m);
        continue;
      }
      size_t d = i;
      while (d < line.size() && iswdigit(line[d])) d++;
      if (d > i && d + 1 < line.size() && (line[d] == L'.' || line[d] == L')') && iswspace(line[d + 1])) {
        flushPara();
        if (list != 2) { closeList(); list = 2; olNo = 0; }
        size_t s2 = d + 1;
        while (s2 < line.size() && iswspace(line[s2])) s2++;
        CcMd m;
        m.kind = 2;
        m.marker = std::to_wstring(++olNo) + L".";
        mdInline(line.substr(s2), 0, m.p);
        out.push_back(m);
        continue;
      }
      if (mdIsBlank(line)) { flushPara(); closeList(); continue; }
      closeList();
      para.push_back(line);
    }
    flushPara();
    closeList();
    if (f == wstring::npos) break;
    // the fence: ```lang\ncode```
    size_t e = src.find(L"```", f + 3);
    if (e == wstring::npos) {  // not closed yet (streaming): the rest is code
      size_t nl = src.find(L'\n', f + 3);
      CcMd m;
      m.kind = 3;
      m.code = nl == wstring::npos ? L"" : src.substr(nl + 1);
      if (!m.code.empty() && m.code.back() == L'\n') m.code.pop_back();
      out.push_back(m);
      break;
    }
    size_t nl = src.find(L'\n', f + 3);
    CcMd m;
    m.kind = 3;
    if (nl != wstring::npos && nl < e) m.code = src.substr(nl + 1, e - nl - 1);
    else m.code = src.substr(f + 3, e - f - 3);
    if (!m.code.empty() && m.code.back() == L'\n') m.code.pop_back();
    out.push_back(m);
    p = e + 3;
  }
  return out;
}

/* ---------------------------------------------------------------- DirectWrite text with per-range colours */
struct CcColorFx : IUnknown {
  LONG refs = 1;
  DWORD col;
  explicit CcColorFx(DWORD c) : col(c) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID r, void** o) override {
    if (r == __uuidof(IUnknown)) { *o = this; AddRef(); return S_OK; }
    *o = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs); }
  ULONG STDMETHODCALLTYPE Release() override {
    ULONG r = InterlockedDecrement(&refs);
    if (!r) delete this;
    return r;
  }
};
struct CcRenderer : IDWriteTextRenderer {
  DWORD base = 0xffffff;
  float alpha = 1.f;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID r, void** o) override {
    if (r == __uuidof(IUnknown) || r == __uuidof(IDWriteTextRenderer) || r == __uuidof(IDWritePixelSnapping)) { *o = this; return S_OK; }
    *o = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
  ULONG STDMETHODCALLTYPE Release() override { return 1; }
  HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* d) override { *d = FALSE; return S_OK; }
  HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* m) override {
    g_rt->GetTransform((D2D1_MATRIX_3X2_F*)m);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* p) override { *p = 1.f; return S_OK; }
  DWORD colorOf(IUnknown* fx) { return fx ? static_cast<CcColorFx*>(fx)->col : base; }
  HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mode, const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown* fx) override {
    g_br->SetColor(Rgb(colorOf(fx), alpha));
    g_rt->DrawGlyphRun(D2D1::Point2F(x, y), run, g_br, mode);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT x, FLOAT y, const DWRITE_UNDERLINE* u, IUnknown* fx) override {
    g_br->SetColor(Rgb(colorOf(fx), alpha));
    g_rt->FillRectangle(D2D1::RectF(x, y + u->offset, x + u->width, y + u->offset + std::max(1.f, u->thickness)), g_br);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH* u, IUnknown* fx) override {
    g_br->SetColor(Rgb(colorOf(fx), alpha));
    g_rt->FillRectangle(D2D1::RectF(x, y + u->offset, x + u->width, y + u->offset + std::max(1.f, u->thickness)), g_br);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return E_NOTIMPL; }
};
static CcRenderer g_ccRenderer;

struct CcText {
  IDWriteTextLayout* lay = nullptr;
  wstring text;
  vector<CcSpan> spans;
  float w = 0, h = 0, boxW = 0;
  void release() { if (lay) { lay->Release(); lay = nullptr; } }
};
static const DWORD CC_ORANGE = 0xd97757;
// size in DIPs; lineH 0 = natural line height. Bold / italic apply to the whole text (headings, the thinking text)
static void ccBuild(CcText& t, float width, float size, float lineH, bool bold = false, bool italic = false, bool mono = false, bool wrap = true) {
  t.release();
  t.boxW = width;
  IDWriteTextFormat* f = uiFont(size, false, false);
  if (mono) f = monoFont(size);
  if (!f) { t.w = t.h = 0; return; }
  g_dw->CreateTextLayout(t.text.c_str(), (UINT32)t.text.size(), f, std::max(20.f, width), 100000.f, &t.lay);
  if (!t.lay) { t.w = t.h = 0; return; }
  t.lay->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
  if (lineH > 0) t.lay->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineH, lineH * 0.8f);
  DWRITE_TEXT_RANGE all{0, (UINT32)t.text.size()};
  if (bold) t.lay->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, all);
  if (italic) t.lay->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, all);
  for (auto& sp : t.spans) {
    DWRITE_TEXT_RANGE r{(UINT32)sp.a, (UINT32)(sp.b - sp.a)};
    if (sp.fl & 1) t.lay->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, r);
    if (sp.fl & 2) t.lay->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, r);
    if (sp.fl & 4) {
      t.lay->SetFontFamilyName(g_fontFace.c_str(), r);
      t.lay->SetFontSize(size * 0.92f, r);
    }
    if (sp.fl & 16) t.lay->SetStrikethrough(TRUE, r);
    if (sp.fl & 8) {
      t.lay->SetUnderline(TRUE, r);
      auto* fx = new CcColorFx(g_mdLinkCol);
      t.lay->SetDrawingEffect(fx, r);
      fx->Release();
    }
    if (sp.fl & 32) {
      auto* fx = new CcColorFx(g_mdTagCol);
      t.lay->SetDrawingEffect(fx, r);
      fx->Release();
    }
  }
  DWRITE_TEXT_METRICS m{};
  t.lay->GetMetrics(&m);
  t.w = m.widthIncludingTrailingWhitespace;
  t.h = m.height;
}
// the rectangles (per line) of a character range, relative to the text origin
static vector<D2D1_RECT_F> ccRangeRects(CcText& t, int a, int b) {
  vector<D2D1_RECT_F> out;
  if (!t.lay || b <= a) return out;
  UINT32 n = 0;
  t.lay->HitTestTextRange((UINT32)a, (UINT32)(b - a), 0, 0, nullptr, 0, &n);
  if (!n) return out;
  vector<DWRITE_HIT_TEST_METRICS> hm(n);
  t.lay->HitTestTextRange((UINT32)a, (UINT32)(b - a), 0, 0, hm.data(), n, &n);
  for (UINT32 i = 0; i < n; i++) out.push_back(D2D1::RectF(hm[i].left, hm[i].top, hm[i].left + hm[i].width, hm[i].top + hm[i].height));
  return out;
}
static void ccDrawText(CcText& t, float x, float y, DWORD col, float alpha = 1.f) {
  if (!t.lay) return;
  // the grey boxes behind inline code
  for (auto& sp : t.spans) {
    if (!(sp.fl & 4)) continue;
    for (auto& r : ccRangeRects(t, sp.a, sp.b)) fillR(D2D1::RectF(x + r.left - 2, y + r.top + 2, x + r.right + 2, y + r.bottom - 2), T().fg, 0.12f * alpha, 4);
  }
  g_ccRenderer.base = col;
  g_ccRenderer.alpha = alpha;
  t.lay->Draw(nullptr, &g_ccRenderer, x, y);
}
// the link under a point of a text drawn at (x, y); "" when there is none
static wstring ccLinkAt(CcText& t, float x, float y, float px, float py) {
  if (!t.lay) return L"";
  BOOL trailing, inside;
  DWRITE_HIT_TEST_METRICS hm{};
  t.lay->HitTestPoint(px - x, py - y, &trailing, &inside, &hm);
  if (!inside) return L"";
  for (auto& sp : t.spans) if ((sp.fl & 8) && (int)hm.textPosition >= sp.a && (int)hm.textPosition < sp.b) return sp.url;
  return L"";
}
