// claude_view.inc: layout and painting of the messages of a chat. Every block is turned into a list of drawing primitives for a given
// width (cached until its text or the width changes); painting walks the visible blocks.

enum { PK_FILL, PK_STROKE, PK_TEXT, PK_STR, PK_ICON, PK_DOT, PK_BTN, PK_CHEV, PK_SPIN, PK_HIT };
enum { HT_NONE, HT_TOGGLE, HT_FILE, HT_EXT, HT_ASK, HT_QOPT, HT_QOTHER, HT_DIFF, HT_REVERT, HT_UNDO, HT_AUTHX, HT_TEXT };
struct CcPrim {
  int type = PK_FILL;
  D2D1_RECT_F r{};  // relative to the block
  DWORD col = 0;
  float alpha = 1, radius = 0, sw = 1;
  CcText* text = nullptr;
  wstring s;
  float size = 12.5f;
  bool bold = false, italic = false, mono = false, onlyHover = false, primary = false, sm = false, strike = false, open = false, pulse = false, field = false;
  int icon = 0;
  int hit = 0, a = 0, b = 0;
  wstring hs;
  float clipY = 0;  // > 0: nothing of this is drawn below that y (the scrolling body of a card)
};
static CcBlock* g_ccFocusBlock = nullptr;  // the "Something else..." field that has the keyboard
static int g_ccFocusField = -1;
static wstring ccJoinLinesW(const wstring& s, size_t maxLines) {
  vector<wstring> ls = mdSplitLines(s);
  wstring o;
  for (size_t i = 0; i < ls.size() && i < maxLines; i++) { if (i) o += L'\n'; o += ls[i]; }
  return o;
}
struct CcHit { D2D1_RECT_F r; int kind, a, b; CcBlock* blk; wstring s; float ox, oy; };
static vector<CcHit> g_ccHits;

static DWORD ccBoxCol() { return mixc(T().panel, T().fg, 0.05f); }
static const DWORD CC_RED = 0xe0707a, CC_GREEN = 0x7bc67e, CC_REDDOT = 0xe0596a, CC_YELLOW = 0xe5c07b;

static void ccFreeLayout(CcBlock* b) {
  if (b->texts) { for (auto* t : *b->texts) { t->release(); delete t; } delete b->texts; b->texts = nullptr; }
  if (b->prims) { delete b->prims; b->prims = nullptr; }
}
static IDWriteTextFormat* ccFont(float size, bool bold, bool italic, bool mono) {
  if (mono) return monoFont(size);
  if (!bold && !italic) return uiFont(size);
  static std::map<int, IDWriteTextFormat*> cache;
  int key = (int)std::lround(size * 10) * 4 + (bold ? 1 : 0) + (italic ? 2 : 0);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  IDWriteTextFormat* f = nullptr;
  g_dw->CreateTextFormat(L"Segoe UI", nullptr, bold ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                         DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &f);
  if (f) f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  cache[key] = f;
  return f;
}
static float ccTextW(const wstring& s, float size, bool bold = false, bool italic = false, bool mono = false) { return textWidth(s, ccFont(size, bold, italic, mono)); }
// the text cut with an ellipsis so that it fits a width
static wstring ccEllipsize(const wstring& s, float w, float size, bool bold = false, bool italic = false, bool mono = false) {
  if (ccTextW(s, size, bold, italic, mono) <= w) return s;
  size_t lo = 0, hi = s.size();
  while (lo < hi) {
    size_t mid = (lo + hi + 1) / 2;
    if (ccTextW(s.substr(0, mid) + L"\x2026", size, bold, italic, mono) <= w) lo = mid;
    else hi = mid - 1;
  }
  return s.substr(0, lo) + L"\x2026";
}
static float ccLineTop(float rowY, float rowH, float size) { return rowY + (rowH - size * 1.33f) / 2; }
static wstring ccOneLine(wstring s, size_t max) {
  for (auto& c : s) if (c == L'\n' || c == L'\r' || c == L'\t') c = L' ';
  return s.substr(0, max);
}
static wstring ccJsonStr(const json& j, const char* k) { return j.is_object() && j.contains(k) && j[k].is_string() ? fromUtf8(j[k].get<std::string>()) : L""; }

/* ---------------------------------------------------------------- the builder of a block */
struct CcBuild {
  CcBlock* b;
  float W;
  CcChat* chat;
  vector<CcPrim>& pr;
  vector<CcText*>& tx;
  CcPrim& add(int type, float x, float y, float w, float h) {
    CcPrim p;
    p.type = type;
    p.r = rc(x, y, w, h);
    pr.push_back(p);
    return pr.back();
  }
  void fill(float x, float y, float w, float h, DWORD col, float alpha = 1, float radius = 0) {
    CcPrim& p = add(PK_FILL, x, y, w, h);
    p.col = col;
    p.alpha = alpha;
    p.radius = radius;
  }
  void stroke(float x, float y, float w, float h, DWORD col, float alpha = 1, float radius = 0) {
    CcPrim& p = add(PK_STROKE, x + 0.5f, y + 0.5f, w - 1, h - 1);
    p.col = col;
    p.alpha = alpha;
    p.radius = radius;
  }
  // a laid out text (not yet placed)
  CcText* mk(const wstring& s, vector<CcSpan> spans, float w, float size, float lineH, bool bold = false, bool italic = false, bool mono = false, bool wrap = true, bool center = false) {
    auto* t = new CcText();
    t->text = s;
    t->spans = std::move(spans);
    ccBuild(*t, w, size, lineH, bold, italic, mono, wrap);
    if (center && t->lay) {
      t->lay->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      DWRITE_TEXT_METRICS m{};
      t->lay->GetMetrics(&m);
      t->h = m.height;
    }
    tx.push_back(t);
    return t;
  }
  CcPrim& put(CcText* t, float x, float y, DWORD col, float alpha = 1.f, float visH = -1) {
    CcPrim& p = add(PK_TEXT, x, y, t->boxW, visH < 0 ? t->h : visH);
    p.text = t;
    p.col = col;
    p.alpha = alpha;
    return p;
  }
  CcText* text(const wstring& s, vector<CcSpan> spans, float x, float y, float w, float size, float lineH, DWORD col, float alpha = 1.f, bool bold = false, bool italic = false) {
    CcText* t = mk(s, std::move(spans), w, size, lineH, bold, italic);
    put(t, x, y, col, alpha);
    return t;
  }
  CcPrim& str(const wstring& s, float x, float y, float w, float h, float size, DWORD col, float alpha = 1.f, bool bold = false, bool italic = false, bool mono = false) {
    CcPrim& p = add(PK_STR, x, y, w, h);
    p.s = s;
    p.size = size;
    p.col = col;
    p.alpha = alpha;
    p.bold = bold;
    p.italic = italic;
    p.mono = mono;
    return p;
  }
  void hit(float x, float y, float w, float h, int kind, int a = 0, int bb = 0, const wstring& hs = L"") {
    CcPrim& p = add(PK_HIT, x, y, w, h);
    p.hit = kind;
    p.a = a;
    p.b = bb;
    p.hs = hs;
  }
  void dot(float cx, float cy, float r, DWORD col) {
    CcPrim& p = add(PK_DOT, cx - r, cy - r, 2 * r, 2 * r);
    p.col = col;
  }
  void icon(int id, float x, float y, float size, DWORD col, float alpha = 1.f) {
    CcPrim& p = add(PK_ICON, x, y, size, size);
    p.icon = id;
    p.col = col;
    p.alpha = alpha;
  }
  // a button (like .cc-btn); returns its width
  float btn(const wstring& label, float x, float y, bool primary, bool sm, int hitKind, int a = 0, int bb = 0, int iconId = 0) {
    float fs = sm ? 12.f : 12.5f;
    float w = ccTextW(label, fs, primary) + (sm ? 20 : 26) + (iconId ? 19 : 0), h = sm ? 22.f : 26.f;
    CcPrim& p = add(PK_BTN, x, y, w, h);
    p.s = label;
    p.primary = primary;
    p.sm = sm;
    p.hit = hitKind;
    p.a = a;
    p.b = bb;
    p.icon = iconId;
    return w;
  }
};

/* ---------------------------------------------------------------- pieces: pre boxes, diffs, todo lists, tool headers */
// a box with mono text (a command, the content of a file, the result of a tool); returns its height
static float ccPreBox(CcBuild& bd, float x, float y, float w, const wstring& s, float maxH = 220, float size = 12.f, float lineH = 17.4f) {
  CcText* t = bd.mk(s, {}, w - 22, size, lineH, false, false, true, true);
  float h = std::min(maxH, t->h + 18);
  bd.fill(x, y, w, h, T().input, 1, 7);
  bd.stroke(x, y, w, h, T().border, 1, 7);
  bd.put(t, x + 11, y + 9, T().fg, 1.f, h - 18);
  return h;
}
static float ccDiffBox(CcBuild& bd, float x, float y, float w, const wstring& oldS, const wstring& newS, bool hasOld = true) {
  float cy = y + 1;
  auto rows = [&](const wstring& s, wchar_t sign, DWORD bg) {
    vector<wstring> ls = mdSplitLines(s);
    for (size_t i = 0; i < ls.size() && i < 60; i++) {
      CcText* t = bd.mk(wstring(1, sign) + L" " + ls[i], {}, w - 22, 12.f, 17.4f, false, false, true, true);
      float h = t->h + 2;
      bd.fill(x + 1, cy, w - 2, h, bg, 0.16f);
      bd.put(t, x + 11, cy + 1, T().fg, 1.f);
      cy += h;
    }
  };
  if (hasOld && !oldS.empty()) rows(oldS, L'-', 0xe0596a);
  rows(newS, L'+', 0x50b464);
  float h = cy + 1 - y;
  bd.stroke(x, y, w, h, T().border, 1, 7);
  return h;
}
static float ccTodoList(CcBuild& bd, float x, float y, float w, const json& todos) {
  float cy = y + 4;
  for (auto& t : todos) {
    if (!t.is_object()) continue;
    wstring s = ccJsonStr(t, "content");
    if (s.empty()) s = ccJsonStr(t, "activeForm");
    std::string st = t.value("status", "");
    float rowH = 22;
    DWORD ox = CC_ORANGE;
    float a = st == "completed" ? 0.55f : 1.f;
    if (st == "completed") { bd.fill(x, cy + 5.5f, 11, 11, ox, 1.f, 3); }
    else bd.stroke(x, cy + 5.5f, 11, 11, st == "in_progress" ? ox : T().fg, st == "in_progress" ? 1.f : 0.45f, 3);
    CcText* tx = bd.mk(s, {}, w - 19, 12.f, 18.f);
    bd.put(tx, x + 19, cy + 2, T().fg, a);
    if (st == "completed") bd.fill(x + 19, cy + 2 + 9, std::min(w - 19, tx->w), 1, T().fg, a);
    cy += std::max(rowH, tx->h + 4);
  }
  return cy + 4 - y;
}
// what a tool call shows when it is opened (toolBody of claude.js)
static float ccToolBody(CcBuild& bd, const wstring& name, const json& input, float x, float y, float w) {
  if (name == L"Bash") {
    float h = ccPreBox(bd, x, y + 4, w, ccJsonStr(input, "command")) + 8;
    wstring d = ccJsonStr(input, "description");
    if (!d.empty()) {
      CcText* t = bd.mk(d, {}, w, 12.f, 19.2f);
      bd.put(t, x, y + h + 2, T().fg, 0.6f);
      h += t->h + 4;
    }
    return h;
  }
  if (name == L"Edit") return ccDiffBox(bd, x, y + 4, w, ccJsonStr(input, "old_string"), ccJsonStr(input, "new_string")) + 8;
  if (name == L"MultiEdit" && input.contains("edits") && input["edits"].is_array()) {
    float h = 0;
    for (auto& e : input["edits"]) h += ccDiffBox(bd, x, y + h + 4, w, ccJsonStr(e, "old_string"), ccJsonStr(e, "new_string")) + 8;
    return h;
  }
  if (name == L"Write") return ccPreBox(bd, x, y + 4, w, ccJoinLinesW(ccJsonStr(input, "content"), 60)) + 8;
  if (name == L"TodoWrite" && input.contains("todos") && input["todos"].is_array()) return ccTodoList(bd, x, y, w, input["todos"]);
  std::string j;
  try { j = input.dump(2, ' ', false, json::error_handler_t::replace); } catch (...) {}
  return ccPreBox(bd, x, y + 4, w, fromUtf8(j.substr(0, 3000))) + 8;
}
// the line of a tool: file link, command, pattern ...; returns the x after it
static float ccToolDetail(CcBuild& bd, const wstring& name, const json& input, float x, float rowY, float rowH, float limit) {
  std::string file;
  for (const char* k : {"file_path", "path", "notebook_path"})
    if (input.contains(k) && input[k].is_string() && !input[k].get<std::string>().empty()) { file = input[k].get<std::string>(); break; }
  float avail = limit - x;
  if (avail < 24) return x;
  if (!file.empty()) {
    wstring full = fromUtf8(file), shortP = ccEllipsize(ccShort(bd.chat, full), avail, 11.9f, false, false, true);
    float w = ccTextW(shortP, 11.9f, false, false, true);
    CcPrim& p = bd.str(shortP, x, ccLineTop(rowY, rowH, 11.9f), w + 2, rowH, 11.9f, T().fg, 0.75f, false, false, true);
    (void)p;
    bd.hit(x, rowY, w, rowH, HT_FILE, 0, 0, full);
    return x + w + 8;
  }
  wstring code;
  if (name == L"Bash" && input.contains("command") && input["command"].is_string()) code = ccOneLine(fromUtf8(input["command"].get<std::string>()), 200);
  else
    for (const char* k : {"pattern", "query", "url"})
      if (input.contains(k) && input[k].is_string() && !input[k].get<std::string>().empty()) { code = ccOneLine(fromUtf8(input[k].get<std::string>()), 160); break; }
  if (!code.empty()) {
    wstring s = ccEllipsize(code, avail - 10, 11.5f, false, false, true);
    float w = ccTextW(s, 11.5f, false, false, true);
    bd.fill(x, rowY + 2, w + 10, rowH - 4, T().fg, 0.12f, 4);
    bd.str(s, x + 5, ccLineTop(rowY, rowH, 11.5f), w + 4, rowH, 11.5f, T().fg, 1.f, false, false, true);
    return x + w + 18;
  }
  if (name == L"Task" && input.contains("description") && input["description"].is_string()) {
    wstring s = ccEllipsize(fromUtf8(input["description"].get<std::string>()), avail, 12.5f);
    bd.str(s, x, ccLineTop(rowY, rowH, 12.5f), ccTextW(s, 12.5f) + 2, rowH, 12.5f, T().fg);
    return x + ccTextW(s, 12.5f) + 8;
  }
  return x;
}
static void ccToolStat(CcBuild& bd, const wstring& name, const json& input, float rightX, float rowY, float rowH) {
  auto lines = [](const json& j, const char* k) {
    if (!j.is_object() || !j.contains(k) || !j[k].is_string()) return 0;
    const std::string& s = j[k].get_ref<const std::string&>();  // one string: begin() and end() of two copies crashed
    return s.empty() ? 0 : (int)std::count(s.begin(), s.end(), '\n') + 1;
  };
  int add = 0, del = 0;
  if (name == L"Write") add = lines(input, "content");
  else if (name == L"Edit") { add = lines(input, "new_string"); del = lines(input, "old_string"); }
  else if (name == L"MultiEdit" && input.contains("edits") && input["edits"].is_array()) for (auto& e : input["edits"]) { add += lines(e, "new_string"); del += lines(e, "old_string"); }
  else return;
  wstring a = L"+" + std::to_wstring(add), d = L"\x2212" + std::to_wstring(del);
  float wa = ccTextW(a, 11.5f), wd = ccTextW(d, 11.5f);
  float x = rightX - wa - 6 - wd;
  bd.str(a, x, ccLineTop(rowY, rowH, 11.5f), wa + 2, rowH, 11.5f, CC_GREEN);
  bd.str(d, x + wa + 6, ccLineTop(rowY, rowH, 11.5f), wd + 2, rowH, 11.5f, CC_RED);
}

/* ---------------------------------------------------------------- Markdown blocks of an answer */
static float ccMdLayout(CcBuild& bd, const vector<CcMd>& md, float x, float y, float w, DWORD col, float alpha, float size = 13.f, float lineH = 20.8f) {
  float cy = y, prevMb = 0;
  for (size_t i = 0; i < md.size(); i++) {
    const CcMd& m = md[i];
    float mt = 0, mb = 0;
    bool firstOfList = m.kind == 2 && (i == 0 || md[i - 1].kind != 2 || md[i - 1].lastOfList);
    if (m.kind == 0) mb = 10;
    else if (m.kind == 1) { mt = 12; mb = 6; }
    else if (m.kind == 2) { mt = firstOfList ? 4.f : 0.f; mb = m.lastOfList ? 10.f : 0.f; }
    else { mt = 8; mb = 8; }
    if (i > 0) cy += std::max(prevMb, mt);
    if (m.kind == 0 || m.kind == 1) {
      CcText* t = bd.mk(m.p.text, m.p.spans, w, m.kind == 1 ? size * 1.02f : size, lineH, m.kind == 1);
      CcPrim& p = bd.put(t, x, cy, col, alpha);
      bool hasLink = false;
      for (auto& sp : m.p.spans) if (sp.fl & 8) hasLink = true;
      if (hasLink) p.hit = HT_TEXT;
      cy += t->h;
    } else if (m.kind == 2) {
      CcText* t = bd.mk(m.p.text, m.p.spans, w - 22, size, lineH);
      CcPrim& p = bd.put(t, x + 22, cy, col, alpha);
      bool hasLink = false;
      for (auto& sp : m.p.spans) if (sp.fl & 8) hasLink = true;
      if (hasLink) p.hit = HT_TEXT;
      float mw = ccTextW(m.marker, size);
      bd.str(m.marker, x + 22 - 6 - mw, cy + (lineH - size * 1.33f) / 2, mw + 2, lineH, size, col, alpha * 0.9f);
      cy += t->h;
    } else {
      CcText* t = bd.mk(m.code, {}, w - 26, 12.5f, 18.75f, false, false, true, false);
      float h = t->h + 22;
      bd.fill(x, cy, w, h, T().input, 1, 8);
      bd.stroke(x, cy, w, h, T().border, 1, 8);
      bd.put(t, x + 13, cy + 11, col, alpha, t->h);
      cy += h;
    }
    prevMb = mb;
  }
  return cy - y;
}

/* ---------------------------------------------------------------- the layout of every kind of block */
static void ccMargins(CcBlock* b, float& mt, float& mb) {
  mt = mb = 12;
  if (b->kind == CB_TOOL) mt = mb = 4;
  if (b->kind == CB_WELCOME) mt = ch() * 0.02f;
}
static void ccAccordionHeader(CcBuild& bd, CcBlock* b, float W, float& y, bool tool) {
  float rowY = 3, rowH = 20;
  bd.hit(0, 0, W, rowH + 6, HT_TOGGLE);
  CcPrim& cv = bd.add(PK_CHEV, 0, rowY + 5, 10, 10);
  cv.open = b->open;
  cv.alpha = 0.55f;
  float x = 18;
  if (tool) {
    bd.dot(x + 3.5f, rowY + rowH / 2, 3.5f, b->resErr ? CC_REDDOT : CC_ORANGE);
    x += 7 + 8;
    wstring label = ccToolLabelOf(b->tname);
    float lw = ccTextW(label, 12.5f, true);
    bd.str(label, x, ccLineTop(rowY, rowH, 12.5f), lw + 2, rowH, 12.5f, b->resErr ? CC_RED : T().fg, 1.f, true);
    x += lw + 8;
    ccToolStat(bd, b->tname, b->input, W, rowY, rowH);
    x = ccToolDetail(bd, b->tname, b->input, x, rowY, rowH, W - 70);
  } else {
    wstring label = b->label.empty() ? L"Thinking" : b->label;
    CcPrim& p = bd.str(label, x, ccLineTop(rowY, rowH, 13), W - x, rowH, 13, T().fg, 0.6f, false, true);
    p.pulse = b->live;
  }
  y = rowY + rowH + 3;
}
static void ccLayout(CcChat* c, CcBlock* b, float W) {
  ccFreeLayout(b);
  b->prims = new vector<CcPrim>();
  b->texts = new vector<CcText*>();
  CcBuild bd{b, W, c, *b->prims, *b->texts};
  const Theme& t = T();
  float h = 0;
  switch (b->kind) {
    case CB_USER: {
      float iw = W - 26;
      CcText* tx = bd.mk(b->text, {}, iw, 13.f, 20.8f);
      float y = 9;
      float textH = tx->h;
      y += textH;
      float chipsY = y + 8;
      vector<std::pair<D2D1_RECT_F, int>> chips;
      float cx = 13, cy = chipsY, rowBottom = chipsY;
      for (size_t i = 0; i < b->atts.size(); i++) {
        float nw = ccTextW(b->atts[i].name, 12.f);
        float w = 8 + 13 + 6 + std::min(nw, iw - 40) + 8;
        if (cx + w > W - 13 && cx > 13) { cx = 13; cy += 30; }
        chips.push_back({rc(cx, cy, w, 24), (int)i});
        cx += w + 6;
        rowBottom = cy + 24;
      }
      float total = b->atts.empty() ? y + 9 : rowBottom + 9;
      bd.fill(0, 0, W, total, ccBoxCol(), 1, 8);
      bd.stroke(0, 0, W, total, t.border, 1, 8);
      bd.put(tx, 13, 9, t.fg);
      for (auto& ch : chips) {
        const CcAtt& a = b->atts[ch.second];
        bd.fill(ch.first.left, ch.first.top, ch.first.right - ch.first.left, 24, t.fg, 0.06f, 6);
        bd.stroke(ch.first.left, ch.first.top, ch.first.right - ch.first.left, 24, t.border, 1, 6);
        bd.icon(a.kind == 1 ? CI_IMAGE : CI_FILE, ch.first.left + 8, ch.first.top + 5.5f, 13, t.fg, 0.7f);
        bd.str(ccEllipsize(a.name, iw - 40, 12.f), ch.first.left + 27, ccLineTop(ch.first.top, 24, 12.f), ch.first.right - ch.first.left - 30, 20, 12.f, t.fg);
      }
      if (!b->uuid.empty()) {
        CcPrim& u = bd.add(PK_BTN, W - 6 - 26, 6, 26, 22);
        u.icon = CI_UNDO;
        u.hit = HT_UNDO;
        u.onlyHover = true;
        u.sm = true;
      }
      h = total;
      break;
    }
    case CB_AI: case CB_FAIL: {
      vector<CcMd> md = mdParse(b->text);
      h = ccMdLayout(bd, md, 0, 0, W, b->kind == CB_FAIL ? CC_RED : t.fg, 1.f);
      if (h < 21) h = 21;
      break;
    }
    case CB_TOOL: {
      float y;
      ccAccordionHeader(bd, b, W, y, true);
      h = y;
      if (b->open) {
        float bx = 18, bw = W - 18;
        float by = h + 4;
        float bh = ccToolBody(bd, b->tname, b->input, bx, by - 4, bw);
        by += bh - 4;
        if (!b->result.empty()) {
          bd.str(b->resErr ? L"Error" : L"Result", bx, by + 4, 120, 16, 11.f, t.fg, 0.55f);
          by += 20;
          by += ccPreBox(bd, bx, by, bw, b->result.substr(0, 4000)) + 4;
        }
        h = by + 2;
      }
      break;
    }
    case CB_THINK: {
      float y;
      ccAccordionHeader(bd, b, W, y, false);
      h = y;
      if (b->open) {
        float bx = 18, bw = W - 18 - 12;
        wstring txt = b->emptyThink ? L"The reasoning of this step is not shown." : b->text;
        CcText* tx = bd.mk(txt, {}, bw, 12.f, 18.6f, false, true);
        float vis = std::min(260.f, tx->h);
        bd.fill(bx, h + 4, 2, vis, t.fg, 0.2f);
        bd.put(tx, bx + 12, h + 4, t.fg, 0.65f, vis);
        h += 4 + vis + 6;
      }
      break;
    }
    case CB_ASK: {
      if (b->done) {
        float rowH = 24;
        bd.dot(4 + 3.5f, 4 + rowH / 2, 3.5f, b->ok ? CC_ORANGE : CC_REDDOT);
        bd.str(b->label, 4 + 7 + 8, ccLineTop(4, rowH, 12.5f), W - 30, rowH, 12.5f, t.fg, 0.7f);
        h = 4 + rowH + 4;
        break;
      }
      float pad = 12, iw = W - 2 - 2 * pad;
      float y = 10 + 1;
      // header
      float rowH = 24;
      bd.dot(pad + 1 + 3.5f, y + rowH / 2, 3.5f, CC_ORANGE);
      wstring label = b->askKind == 1 ? L"Plan" : b->askKind == 2 ? L"Question" : ccToolLabelOf(b->tname);
      float lw = ccTextW(label, 13.f, true);
      float x = pad + 1 + 7 + 8;
      if (b->askKind != 2) bd.str(label, x, ccLineTop(y, rowH, 13), lw + 2, rowH, 13, t.fg, 1.f, true);
      if (b->askKind == 0) ccToolDetail(bd, b->tname, b->input, x + lw + 8, y, rowH, W - pad);
      y += rowH + 8;
      float bodyTop = y;
      if (b->askKind == 0) {
        size_t from = bd.pr.size();
        float bh = ccToolBody(bd, b->tname, b->input, pad + 1, y - 4, iw);
        for (size_t k = from; k < bd.pr.size(); k++) bd.pr[k].clipY = y + 240;
        y += std::min(240.f, bh - 4);
      } else if (b->askKind == 1) {
        vector<CcMd> md = mdParse(ccJsonStr(b->input, "plan"));
        size_t from = bd.pr.size();
        float mh = ccMdLayout(bd, md, pad + 1, y, iw, t.fg, 1.f);
        for (size_t k = from; k < bd.pr.size(); k++) bd.pr[k].clipY = y + 240;
        y += std::min(240.f, mh);
      } else {
        const json& qs = b->input["questions"];
        for (size_t qi = 0; qi < qs.size(); qi++) {
          const json& q = qs[qi];
          wstring head = ccJsonStr(q, "header");
          if (!head.empty()) {
            wstring up = head;
            for (auto& ch : up) ch = (wchar_t)towupper(ch);
            bd.str(up, pad + 1, y, iw, 16, 11.f, t.fg, 0.55f);
            y += 17;
          }
          CcText* qt = bd.mk(ccJsonStr(q, "question"), {}, iw, 13.f, 20.8f, true);
          bd.put(qt, pad + 1, y + 2, t.fg);
          y += qt->h + 8;
          if (q.contains("options") && q["options"].is_array()) {
            for (size_t oi = 0; oi < q["options"].size(); oi++) {
              const json& o = q["options"][oi];
              wstring ol = ccJsonStr(o, "label"), od = ccJsonStr(o, "description");
              float oh = 12 + 16 + (od.empty() ? 0 : 15) + 2;
              bool on = qi < b->picked.size() && b->picked[qi].count((int)oi);
              if (on) bd.fill(pad + 1, y, iw, oh, CC_ORANGE, 0.14f, 8);
              bd.stroke(pad + 1, y, iw, oh, on ? CC_ORANGE : t.border, 1, 8);
              bd.str(ol, pad + 11, y + 6, iw - 20, 16, 12.5f, t.fg, 1.f, true);
              if (!od.empty()) bd.str(ccEllipsize(od, iw - 20, 11.5f), pad + 11, y + 22, iw - 20, 15, 11.5f, t.fg, 0.6f);
              CcPrim& p = bd.add(PK_HIT, pad + 1, y, iw, oh);
              p.hit = HT_QOPT;
              p.a = (int)qi;
              p.b = (int)oi;
              y += oh + 5;
            }
          }
          // "Something else..."
          CcPrim& fp = bd.add(PK_FILL, pad + 1, y + 1, iw, 28);
          fp.col = t.input;
          fp.radius = 7;
          fp.field = true;
          fp.a = (int)qi;
          fp.hit = HT_QOTHER;
          y += 36;
        }
        bodyTop = y;
      }
      float buttonsY = y + 8;
      float bx = pad + 1;
      if (b->askKind == 0) {
        bx += bd.btn(L"Allow", bx, buttonsY, true, false, HT_ASK, 0) + 8;
        bx += bd.btn(L"Always allow", bx, buttonsY, false, false, HT_ASK, 1) + 8;
        bd.btn(L"Deny", bx, buttonsY, false, false, HT_ASK, 2);
      } else if (b->askKind == 1) {
        float rowX = bx;
        auto place = [&](const wstring& s, bool primary, int id) {
          float w = ccTextW(s, 12.5f, primary) + 26;
          if (rowX + w > W - pad && rowX > bx) { rowX = bx; buttonsY += 34; }
          bd.btn(s, rowX, buttonsY, primary, false, HT_ASK, id);
          rowX += w + 8;
        };
        place(L"Yes, and edit files", true, 0);
        place(L"Yes, ask before each change", false, 1);
        place(L"No, keep planning", false, 2);
      } else {
        bx += bd.btn(L"Send answer", bx, buttonsY, true, false, HT_ASK, 0) + 8;
        bd.btn(L"Skip", bx, buttonsY, false, false, HT_ASK, 1);
      }
      (void)bodyTop;
      float total = buttonsY + 26 + 10 + 1;
      bd.fill(0, 0, W, total, CC_ORANGE, 0.07f, 10);
      // the fill goes first in the list so it ends up behind everything: move it to the front
      std::rotate(bd.pr.begin(), bd.pr.end() - 1, bd.pr.end());
      bd.stroke(0, 0, W, total, mixc(t.border, CC_ORANGE, 0.55f), 1, 10);
      h = total;
      break;
    }
    case CB_CHANGES: {
      float pad = 12, iw = W - 2 - 2 * pad;
      float y = 8 + 1;
      wstring head = b->reverted ? L"Changes reverted" : std::to_wstring(b->touched.size()) + L" file" + (b->touched.size() > 1 ? L"s" : L"") + L" changed";
      bd.str(head, pad + 1, ccLineTop(y, 22, 13), 200, 22, 13, t.fg, 1.f, true);
      if (!b->turnUuid.empty() && !b->reverted) {
        float rw = ccTextW(L"Revert", 12.f) + 20 + 19;
        bd.btn(L"Revert", W - pad - 1 - rw, y, false, true, HT_REVERT, 0, 0, CI_UNDO);
      }
      y += 22 + 4;
      for (size_t i = 0; i < b->touched.size(); i++) {
        const CcTouched& tf = b->touched[i];
        float rowH = 22;
        float cx = pad + 1;
        float dw = ccTextW(L"Diff", 12.f) + 20;
        wstring a = L"+" + std::to_wstring(tf.add), d = L"\x2212" + std::to_wstring(tf.del);
        float wa = ccTextW(a, 11.5f), wd = ccTextW(d, 11.5f);
        float right = W - pad - 1 - dw - 10;
        bd.str(d, right - wd, ccLineTop(y + 3, rowH, 11.5f), wd + 2, rowH, 11.5f, CC_RED);
        bd.str(a, right - wd - 6 - wa, ccLineTop(y + 3, rowH, 11.5f), wa + 2, rowH, 11.5f, CC_GREEN);
        float avail = right - wd - 6 - wa - 10 - cx;
        wstring sp = ccEllipsize(ccShort(c, tf.file), avail, 11.9f, false, false, true);
        bd.str(sp, cx, ccLineTop(y + 3, rowH, 11.9f), avail, rowH, 11.9f, t.fg, 0.75f, false, false, true);
        bd.hit(cx, y + 3, std::min(avail, ccTextW(sp, 11.9f, false, false, true)), rowH, HT_FILE, 0, 0, tf.file);
        bd.btn(L"Diff", W - pad - 1 - dw, y + 3, false, true, HT_DIFF, (int)i);
        y += rowH + 6;
      }
      float total = y + 8 - 1;
      bd.fill(0, 0, W, total, ccBoxCol(), 1, 10);
      std::rotate(bd.pr.begin(), bd.pr.end() - 1, bd.pr.end());
      bd.stroke(0, 0, W, total, t.border, 1, 10);
      h = total;
      break;
    }
    case CB_HINT: {
      CcText* tx = bd.mk(b->text, {}, W, 12.5f, 20.f, false, false, false, true, true);
      bd.put(tx, 0, 0, t.fg, 0.6f);
      h = tx->h;
      break;
    }
    case CB_TRUST: case CB_WARN: {
      CcText* tx = bd.mk(b->text, {}, W - 22, 12.f, 18.f);
      h = tx->h + 8;
      bd.fill(0, 0, W, h, CC_YELLOW, 0.07f);
      bd.fill(0, 0, 2, h, CC_YELLOW);
      bd.put(tx, 12, 4, t.fg, 0.85f);
      break;
    }
    case CB_AUTH: {
      if (b->done) {
        float rowH = 24;
        bd.dot(4 + 3.5f, 4 + rowH / 2, 3.5f, b->ok ? CC_ORANGE : CC_REDDOT);
        bd.str(b->label, 4 + 7 + 8, ccLineTop(4, rowH, 12.5f), W - 30, rowH, 12.5f, t.fg, 0.75f);
        h = 4 + rowH + 4;
        break;
      }
      float pad = 12, iw = W - 2 - 2 * pad;
      float y = 9;
      CcPrim& sp = bd.add(PK_SPIN, pad + 1, y + 4, 16, 16);
      sp.col = CC_ORANGE;
      float cw = ccTextW(L"Cancel", 12.f) + 20;
      CcText* lt = bd.mk(b->label, {}, iw - 16 - 8 - cw - 8, 12.5f, 20.f);
      bd.put(lt, pad + 1 + 16 + 8, y + 2, t.fg);
      bd.btn(L"Cancel", W - pad - 1 - cw, y, false, true, HT_AUTHX);
      y += std::max(24.f, lt->h + 4) + 2;
      if (!b->authLog.empty()) y += ccPreBox(bd, pad + 1, y + 6, iw, b->authLog, 120) + 6;
      float total = y + 9;
      bd.fill(0, 0, W, total, ccBoxCol(), 1, 10);
      std::rotate(bd.pr.begin(), bd.pr.end() - 1, bd.pr.end());
      bd.stroke(0, 0, W, total, t.border, 1, 10);
      h = total;
      break;
    }
    case CB_WELCOME: {
      ccLoadArt();
      static IDWriteTextFormat* georgia = nullptr;
      if (!georgia) {
        g_dw->CreateTextFormat(L"Georgia", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 21.f, L"en-us", &georgia);
        if (georgia) georgia->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
      }
      float nameW = georgia ? textWidth(L"Claude Code", georgia) : 120;
      float total = 24 + 8 + nameW, x0 = (W - total) / 2, y = 4;
      CcPrim& lg = bd.add(PK_ICON, x0, y, 24, 24);
      lg.icon = g_ccLogo.geo ? -1 : CI_SPARK;
      lg.col = CC_ORANGE;
      CcPrim& nm = bd.add(PK_STR, x0 + 32, y - 2, nameW + 4, 30);
      nm.s = L"Claude Code";
      nm.size = 21;
      nm.col = t.fg;
      nm.hs = L"georgia";
      y += 24 + ch() * 0.12f;
      float mw = 56;
      if (g_ccClawd.geo) {
        float mh = mw * g_ccClawd.vh / g_ccClawd.vw;
        CcPrim& m = bd.add(PK_ICON, (W - mw) / 2, y, mw, mh);
        m.icon = -2;
        m.col = CC_ORANGE;
        y += mh;
      } else {
        bd.icon(CI_STAR, (W - 46) / 2, y, 46, CC_ORANGE);
        y += 46;
      }
      y += 14;
      // "Press [Shift] [Tab] to change what Claude may do without asking"
      float fs = 12.5f;
      float kw1 = ccTextW(L"Shift", 11.f) + 12, kw2 = ccTextW(L"Tab", 11.f) + 12;
      float l1 = ccTextW(L"Press ", fs) + kw1 + 4 + kw2 + ccTextW(L" to change what Claude may do", fs);
      float x1 = (W - l1) / 2;
      float ty = ccLineTop(y, 20, fs);
      bd.str(L"Press ", x1, ty, 60, 20, fs, t.fg, 0.6f);
      float kx = x1 + ccTextW(L"Press ", fs);
      auto kbd = [&](const wstring& s, float w) {
        bd.stroke(kx, y + 1, w, 18, t.fg, 0.6f, 4);
        bd.fill(kx + 1, y + 18, w - 2, 1.5f, t.fg, 0.35f);
        bd.str(s, kx + 6, ccLineTop(y + 1, 18, 11.f), w, 18, 11.f, t.fg, 0.6f);
      };
      kbd(L"Shift", kw1);
      kx += kw1 + 4;
      kbd(L"Tab", kw2);
      kx += kw2;
      bd.str(L" to change what Claude may do", kx, ty, 220, 20, fs, t.fg, 0.6f);
      float l2 = ccTextW(L"without asking", fs);
      bd.str(L"without asking", (W - l2) / 2, ccLineTop(y + 20, 20, fs), l2 + 4, 20, fs, t.fg, 0.6f);
      y += 40;
      h = y;
      break;
    }
  }
  b->h = h;
  b->lastW = W;
  b->dirty = false;
}

/* ---------------------------------------------------------------- painting a block */
static bool g_ccAnim = false;  // something on screen is moving: the animation timer keeps running
static void ccDrawBtn(const CcPrim& p, const D2D1_RECT_F& r, bool hover) {
  const Theme& t = T();
  float w = r.right - r.left, h = r.bottom - r.top;
  if (p.type == PK_BTN && p.s.empty() && p.icon) {  // the rewind button of a user message
    fillR(r, ccBoxCol(), 1.f, 6);
    strokeR(r, t.border, 1.f, 6);
    ccDrawIcon(p.icon, rc(r.left + (w - 13) / 2, r.top + (h - 13) / 2, 13, 13), t.fg, hover ? 1.f : 0.75f);
    return;
  }
  if (p.primary) {
    fillR(r, mixc(CC_ORANGE, 0xffffff, hover ? 0.12f : 0.f), 1.f, 7);
    strokeR(r, CC_ORANGE, 1.f, 7);
  } else {
    if (hover) fillR(r, t.fg, 0.10f, 7);
    strokeR(r, t.border, 1.f, 7);
  }
  float fs = p.sm ? 12.f : 12.5f;
  float tw = ccTextW(p.s, fs, p.primary);
  float ix = r.left + (w - tw - (p.icon ? 19 : 0)) / 2;
  if (p.icon) { ccDrawIcon(p.icon, rc(ix, r.top + (h - 13) / 2, 13, 13), t.fg, 0.9f); ix += 19; }
  drawText(p.s.c_str(), (int)p.s.size(), ix, ccLineTop(r.top, h, fs), p.primary ? 0x1f1f1f : t.fg, ccFont(fs, p.primary, false, false), tw + 8, 20);
}
static void ccDrawBlock(CcChat* c, CcBlock* b, float ox, float oy, float vpTop, float vpBottom) {
  if (!b->prims) return;
  const Theme& t = T();
  float W = b->lastW;
  D2D1_RECT_F whole = rc(ox, oy, W, b->h);
  bool hovBlock = g_mx >= whole.left && g_mx < whole.right && g_my >= std::max(whole.top, vpTop) && g_my < std::min(whole.bottom, vpBottom);
  for (size_t i = 0; i < b->prims->size(); i++) {
    const CcPrim& p = (*b->prims)[i];
    D2D1_RECT_F r = D2D1::RectF(p.r.left + ox, p.r.top + oy, p.r.right + ox, p.r.bottom + oy);
    if (r.bottom < vpTop - 40 || r.top > vpBottom + 40) continue;
    if (p.onlyHover && !hovBlock) continue;
    bool hov = p.hit && inRect(r, g_mx, g_my) && g_my >= vpTop && g_my < vpBottom;
    bool clipped = p.clipY > 0;
    if (clipped) g_rt->PushAxisAlignedClip(D2D1::RectF(ox - 20, oy - 100000, ox + W + 20, oy + p.clipY), D2D1_ANTIALIAS_MODE_ALIASED);
    switch (p.type) {
      case PK_FILL: fillR(r, p.col, p.alpha, p.radius); break;
      case PK_STROKE: strokeR(r, p.col, p.alpha, p.radius); break;
      case PK_TEXT: {
        if (!p.text || !p.text->lay) break;
        float vis = r.bottom - r.top;
        bool clip = vis + 0.5f < p.text->h;
        if (clip) g_rt->PushAxisAlignedClip(D2D1::RectF(r.left, r.top, r.right + 400, r.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
        ccDrawText(*p.text, r.left, r.top, p.col, p.alpha);
        if (clip) g_rt->PopAxisAlignedClip();
        break;
      }
      case PK_STR: {
        float a = p.alpha;
        if (p.pulse) { a *= 0.35f + 0.65f * (0.5f + 0.5f * (float)cos(GetTickCount64() * 6.2832 / 1400.0)); g_ccAnim = true; }
        if (p.hs == L"georgia") {
          static IDWriteTextFormat* gf = nullptr;
          if (!gf) { g_dw->CreateTextFormat(L"Georgia", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 21.f, L"en-us", &gf); if (gf) gf->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP); }
          if (gf) { g_br->SetColor(Rgb(p.col, a)); g_rt->DrawText(p.s.c_str(), (UINT32)p.s.size(), gf, r, g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP); }
          break;
        }
        g_br->SetColor(Rgb(p.col, a));
        g_rt->DrawText(p.s.c_str(), (UINT32)p.s.size(), ccFont(p.size, p.bold, p.italic, p.mono), D2D1::RectF(r.left, r.top, r.right + 40, r.bottom), g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        break;
      }
      case PK_ICON:
        if (p.icon == -1) ccDrawArt(g_ccLogo, r.left, r.top, r.right - r.left, p.col);
        else if (p.icon == -2) ccDrawArt(g_ccClawd, r.left, r.top, r.right - r.left, p.col);
        else ccDrawIcon(p.icon, r, p.col, p.alpha);
        break;
      case PK_SPIN: {
        g_ccAnim = true;
        ccDrawIcon(CI_SPARK, r, p.col, 1.f, (float)(GetTickCount64() % 2400) * 360.f / 2400.f);
        break;
      }
      case PK_DOT: {
        g_br->SetColor(Rgb(p.col, p.alpha));
        g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F((r.left + r.right) / 2, (r.top + r.bottom) / 2), (r.right - r.left) / 2, (r.right - r.left) / 2), g_br);
        break;
      }
      case PK_CHEV: {
        float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
        g_br->SetColor(Rgb(t.fg, p.alpha));
        if (p.open) {
          g_rt->DrawLine(D2D1::Point2F(cx - 3.2f, cy - 1.4f), D2D1::Point2F(cx, cy + 2.2f), g_br, 1.5f);
          g_rt->DrawLine(D2D1::Point2F(cx + 3.2f, cy - 1.4f), D2D1::Point2F(cx, cy + 2.2f), g_br, 1.5f);
        } else {
          g_rt->DrawLine(D2D1::Point2F(cx - 1.4f, cy - 3.2f), D2D1::Point2F(cx + 2.2f, cy), g_br, 1.5f);
          g_rt->DrawLine(D2D1::Point2F(cx - 1.4f, cy + 3.2f), D2D1::Point2F(cx + 2.2f, cy), g_br, 1.5f);
        }
        break;
      }
      case PK_BTN: ccDrawBtn(p, r, hov); break;
      default: break;
    }
    if (p.field && b->kind == CB_ASK && p.a < (int)b->others.size()) {
      // the "Something else..." field of a question
      drawInput(b->others[p.a], D2D1::RectF(r.left, r.top, r.right, r.bottom), g_ccFocusBlock == b && g_ccFocusField == p.a, L"Something else\x2026");
    }
    if (clipped) g_rt->PopAxisAlignedClip();
    if (p.hit && r.bottom > vpTop && r.top < vpBottom) {
      D2D1_RECT_F hr = D2D1::RectF(r.left, std::max(r.top, vpTop), r.right, std::min(r.bottom, vpBottom));
      g_ccHits.push_back({hr, p.hit, p.a, p.b, b, p.hs, r.left, r.top});
      if (p.type == PK_TEXT) g_ccHits.back().a = (int)i;
    }
  }
}
