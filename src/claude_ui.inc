// claude_ui.inc: the Claude Code dock on the right of the window: tabs, header, messages, composer, pop-up lists, mouse and keyboard.

enum { HD_TAB = 100, HD_TABX, HD_ADD, HD_HIDE, HD_ACCOUNT, HD_HISTORY, HD_NEW, HD_CLOSECHAT, HD_PLUS, HD_SLASH, HD_MODEL, HD_MODE, HD_SEND, HD_CTXOFF, HD_ATTOFF, HD_INPUT, HD_SUG, HD_POP, HD_POPDOT, HD_COST, HD_USAGE };
static float ccLeft() {
  if (!g_ccOpen) return cw();
  float w = std::max(280.f, std::min(g_ccW, std::min(900.f, cw() * 0.7f)));
  return cw() - w;
}
static const float CC_TABS_H = 38, CC_HEAD_H = 32;
static std::map<wstring, D2D1_RECT_F> g_ccAnchors;
static bool g_ccDragW = false, g_ccInDrag = false;
static CcText g_ccInTxt;
static wstring g_ccInKey;
static D2D1_RECT_F g_ccInRect{};
static float g_ccInOx = 0, g_ccInOy = 0;

CcBlock::~CcBlock() { ccFreeLayout(this); }
CcChat::~CcChat() {
  for (auto* b : blocks) delete b;
  delete working;
  delete current;
}

/* ---------------------------------------------------------------- the pop-up lists */
static CcPopItem ccPopTitle(const wstring& s) { CcPopItem i; i.kind = 1; i.title = s; return i; }
static CcPopItem ccPopNote(const wstring& s) { CcPopItem i; i.kind = 2; i.title = s; return i; }
static CcPopItem ccPopOpt(const wstring& t, const wstring& d, bool sel, std::function<void()> run) {
  CcPopItem i;
  i.title = t;
  i.desc = d;
  i.sel = sel;
  i.run = std::move(run);
  return i;
}
static wstring ccCap(wstring s) { if (!s.empty()) s[0] = (wchar_t)towupper(s[0]); return s; }
static void ccAttachPath(CcChat* c, const wstring& p, bool image) {
  for (auto& a : c->atts) if (a.path == p) return;
  CcAtt a;
  a.path = p;
  a.name = baseName(p);
  std::string media, b64;
  if ((image || p.find_last_of(L'.') != wstring::npos) && clReadImage(p, media, b64)) {
    a.kind = 1;
    a.media = fromUtf8(media);
    a.data = b64;
  }
  c->atts.push_back(a);
}
static void ccBuildPopover(CcChat* c, const wstring& which, const json* acct) {
  CcPop& p = g_ccPop;
  p.items.clear();
  int cid = c->convId;
  auto live = [cid]() -> CcChat* { for (auto* x : g_ccChats) if (x->convId == cid) return x; return nullptr; };
  if (which == L"model") {
    p.items.push_back(ccPopTitle(L"Select a model"));
    wstring cur = ccModelPref();
    for (auto& m : CC_MODELS) {
      wstring id = m.id;
      p.items.push_back(ccPopOpt(m.name, m.desc, id == cur, [live, id] {
        g_ccPrefModel = id;
        g_ccModelSet = true;
        if (CcChat* cc = live()) cc->activeModel.clear();
        ccSaveStore();
        g_ccPop.kind = 0;
        g_ccFocusInput = true;
      }));
    }
    CcPopItem e;
    e.kind = 3;
    e.title = L"Effort";
    e.desc = ccCap(g_ccPrefEffort);
    p.items.push_back(e);
  } else if (which == L"mode") {
    p.items.push_back(ccPopTitle(L"What Claude may do"));
    wstring cur = ccModePref();
    for (auto& m : CC_MODES) {
      wstring id = m.id;
      p.items.push_back(ccPopOpt(m.name, m.desc, id == cur, [live, id] {
        g_ccPrefMode = id;
        g_ccModeSet = true;
        ccSaveStore();
        g_ccPop.kind = 0;
        if (CcChat* cc = live()) ccApplyMode(cc);
      }));
    }
  } else if (which == L"history") {
    p.items.push_back(ccPopTitle(L"Conversations in this folder"));
    ccLoadSessions(c);
    if (c->sessions.empty()) p.items.push_back(ccPopNote(L"No earlier conversations here."));
    for (auto& s : c->sessions) {
      FILETIME nowFt;
      GetSystemTimeAsFileTime(&nowFt);
      ULONGLONG nt = ((ULONGLONG)nowFt.dwHighDateTime << 32) | nowFt.dwLowDateTime;
      ULONGLONG secs = nt > s.mtime ? (nt - s.mtime) / 10000000ULL : 0;
      wchar_t b[48];
      if (secs < 3600) swprintf_s(b, L"%d min", (int)std::max<ULONGLONG>(1, (secs + 30) / 60));
      else if (secs < 86400) swprintf_s(b, L"%d h", (int)((secs + 1800) / 3600));
      else if (secs < 86400 * 7) swprintf_s(b, L"%d d", (int)((secs + 43200) / 86400));
      else {
        FILETIME lf;
        FileTimeToLocalFileTime((const FILETIME*)&s.mtime, &lf);
        SYSTEMTIME st;
        FileTimeToSystemTime(&lf, &st);
        if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, b, 48, nullptr)) b[0] = 0;
      }
      wstring id = s.id, title = s.title;
      p.items.push_back(ccPopOpt(s.title, b, s.id == c->sessionId, [id, title] { g_ccPop.kind = 0; ccOpenSessionTab(id, title); }));
    }
  } else if (which == L"account") {
    p.items.push_back(ccPopTitle(L"Account"));
    if (!acct) p.items.push_back(ccPopNote(L"Reading the account\x2026"));
    else if (acct->contains("error")) p.items.push_back(ccPopNote(fromUtf8(acct->value("error", ""))));
    else if (!acct->value("loggedIn", false)) p.items.push_back(ccPopNote(L"Not signed in to Claude Code."));
    else {
      CcPopItem a;
      a.kind = 4;
      a.title = fromUtf8(acct->value("email", "Signed in"));
      std::string sub = acct->value("subscriptionType", "");
      for (auto& ch : sub) ch = (char)tolower((unsigned char)ch);
      wstring plan = sub == "pro" ? L"Pro" : sub == "max" ? L"Max" : sub == "team" ? L"Team" : sub == "enterprise" ? L"Enterprise" : sub == "free" ? L"Free" : fromUtf8(sub);
      std::string am = acct->value("authMethod", "");
      wstring how = am == "claude.ai" ? L"Claude subscription" : am == "console" ? L"Anthropic Console" : fromUtf8(am);
      wstring org = fromUtf8(acct->value("orgName", ""));
      wstring d = how;
      if (!plan.empty()) d += (d.empty() ? L"" : L" \x00B7 ") + plan;
      if (!org.empty() && org != a.title + L"'s Organization") d += (d.empty() ? L"" : L" \x00B7 ") + org;
      a.desc = d;
      p.items.push_back(a);
    }
    p.items.push_back(ccPopTitle(L"Settings"));
    p.items.push_back(ccPopOpt(L"Switch account", L"Sign in with a Claude subscription", false, [live] { g_ccPop.kind = 0; if (CcChat* cc = live()) ccStartAuth(cc, L"login", false); }));
    p.items.push_back(ccPopOpt(L"Sign in with the Anthropic Console", L"Pay for API use instead of a subscription", false, [live] { g_ccPop.kind = 0; if (CcChat* cc = live()) ccStartAuth(cc, L"login", true); }));
    p.items.push_back(ccPopOpt(L"Log out", L"Forget the account on this computer", false, [live] { g_ccPop.kind = 0; if (CcChat* cc = live()) ccStartAuth(cc, L"logout", false); }));
  } else if (which == L"plus") {
    p.items.push_back(ccPopTitle(L"Add to the message"));
    p.items.push_back(ccPopOpt(L"Attach file\x2026", L"A file from disk", false, [live] {
      g_ccPop.kind = 0;
      CcChat* cc = live();
      if (!cc) return;
      for (auto& f : clPickFiles(false)) ccAttachPath(cc, f, false);
      g_ccFocusInput = true;
      redraw();
    }));
    p.items.push_back(ccPopOpt(L"Attach image\x2026", L"A picture Claude can look at", false, [live] {
      g_ccPop.kind = 0;
      CcChat* cc = live();
      if (!cc) return;
      for (auto& f : clPickFiles(true)) ccAttachPath(cc, f, true);
      g_ccFocusInput = true;
      redraw();
    }));
    p.items.push_back(ccPopOpt(g_ccContext ? L"Open file: on" : L"Include the open file", L"Send the open file and selection with the message", false, [] {
      g_ccPop.kind = 0;
      g_ccContext = true;
      ccSaveStore();
    }));
  }
}
static void ccOpenPopover(CcChat* c, const wstring& which) {
  CcPop& p = g_ccPop;
  p.kind = 1;
  p.name = which;
  p.hover = -1;
  p.scroll = 0;
  auto it = g_ccAnchors.find(which);
  if (it != g_ccAnchors.end()) p.anchor = it->second;
  p.down = which == L"account" || which == L"history";
  ccBuildPopover(c, which, nullptr);
  if (which == L"account") {
    int cid = c->convId;
    clAuthStatus(sStr("claude_path"), [cid](const json& st) {
      if (g_ccPop.kind != 1 || g_ccPop.name != L"account") return;
      for (auto* x : g_ccChats) if (x->convId == cid) { ccBuildPopover(x, L"account", &st); redraw(); }
    });
  }
  redraw();
}
static float ccPopItemH(const CcPopItem& i) {
  switch (i.kind) {
    case 1: return 26;
    case 2: return 36;
    case 3: return 40;
    case 4: return 48;
  }
  return i.desc.empty() ? 30.f : 44.f;
}

/* ---------------------------------------------------------------- suggestions above the text box ("/" and "@") */
static void ccUpdateSuggest(CcChat* c) {
  const wstring& v = c->input.t;
  int pos = c->input.c;
  wstring before = v.substr(0, pos);
  c->sgItems.clear();
  c->sgKind = 0;
  c->sgIndex = 0;
  c->sgScroll = 0;
  auto isWord = [](wchar_t ch) { return iswalnum(ch) || wcschr(L"_:.-", ch) != nullptr; };
  if (!before.empty() && before[0] == L'/') {
    bool ok = true;
    for (size_t i = 1; i < before.size(); i++) if (!isWord(before[i])) ok = false;
    if (ok) {
      wstring q = lowerW(before.substr(1));
      std::set<wstring> names;
      for (auto& lc : CC_LOCAL_CMDS) if (wstring(lc[0]).compare(0, q.size(), q) == 0) { c->sgItems.push_back({L"/" + wstring(lc[0]), lc[1], L"/" + wstring(lc[0])}); names.insert(lc[0]); }
      for (auto& n : c->slashCommands) if (!names.count(n) && lowerW(n).find(q) != wstring::npos) { c->sgItems.push_back({L"/" + n, L"Claude Code command", L"/" + n}); names.insert(n); }
      if (!c->sgItems.empty()) { c->sgKind = 1; c->sgFrom = 0; c->sgTo = pos; }
      return;
    }
  }
  size_t at = before.rfind(L'@');
  if (at != wstring::npos && (at == 0 || iswspace(before[at - 1]))) {
    wstring q = before.substr(at + 1);
    bool ok = true;
    for (wchar_t ch : q) if (iswspace(ch) || ch == L'@') ok = false;
    if (ok) {
      if (!g_projScanned && g_root) scanProject();
      wstring lq = lowerW(q);
      vector<std::pair<int, wstring>> hits;
      for (auto& f : g_projFiles) {
        if (hits.size() >= 400) break;
        wstring lf = lowerW(f);
        size_t k = lf.find(lq);
        if (lq.empty() || k != wstring::npos) hits.push_back({(lf.compare(0, lq.size(), lq) == 0 ? 0 : 1) * 1000 + (int)f.size(), f});
      }
      std::stable_sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.first < b.first; });
      for (size_t i = 0; i < hits.size() && i < 40; i++) {
        wstring v2 = hits[i].second;
        for (auto& ch : v2) if (ch == L'\\') ch = L'/';
        c->sgItems.push_back({hits[i].second, L"", L"@" + v2});
      }
      if (!c->sgItems.empty()) { c->sgKind = 2; c->sgFrom = (int)at; c->sgTo = pos; }
    }
  }
}
static void ccAcceptSuggest(CcChat* c, int i) {
  if (!c->sgKind || i < 0 || i >= (int)c->sgItems.size()) return;
  const wstring v = c->input.t;
  wstring ins = c->sgItems[i].value + L" ";
  c->input.t = v.substr(0, c->sgFrom) + ins + v.substr(c->sgTo);
  c->input.c = c->input.a = c->sgFrom + (int)ins.size();
  c->sgKind = 0;
  c->sgItems.clear();
  g_ccFocusInput = true;
  redraw();
}

/* ---------------------------------------------------------------- painting */
static DWORD ccMix(float a) { return mixc(T().panel, T().fg, a); }
static void ccDrawIconBtn(const D2D1_RECT_F& r, int icon, bool hover, float alpha = 0.6f) {
  if (hover) fillR(r, T().fg, 0.10f, 6);
  float s = icon == CI_CLOSE ? 14.f : 16.f;
  ccDrawIcon(icon, rc((r.left + r.right - s) / 2, (r.top + r.bottom - s) / 2, s, s), T().fg, hover ? 1.f : alpha);
}
static wstring ccTail(const wstring& s, float w, float size) {
  if (ccTextW(s, size) <= w) return s;
  size_t k = 1;
  while (k < s.size() && ccTextW(L"\x2026" + s.substr(s.size() - k - 1), size) <= w) k++;
  return L"\x2026" + s.substr(s.size() - k);
}
static void drawClaude() {
  g_ccHits.clear();
  g_ccAnim = false;
  if (!g_ccOpen) return;
  const Theme& t = T();
  CcChat* c = ccCur();
  float x0 = ccLeft(), W = cw() - x0, top = MENU_H, bot = ch() - STATUS_H;
  auto hit = [&](const D2D1_RECT_F& r, int k, int a = 0) { g_ccHits.push_back({r, k, a, 0, nullptr, L"", 0, 0}); };
  auto hov = [&](const D2D1_RECT_F& r) { return inRect(r, g_mx, g_my); };
  fillRect(x0, top, W, bot - top, t.panel);
  fillRect(x0, top, 1, bot - top, t.border);
  g_rt->PushAxisAlignedClip(D2D1::RectF(x0, top, cw(), bot), D2D1_ANTIALIAS_MODE_ALIASED);

  /* tabs of the conversations */
  {
    float ty = top + 5, th = 28, x = x0 + 6;
    float avail = W - 12 - 26 - 4 - 26 - 4;
    float tw = g_ccChats.empty() ? 160.f : std::max(56.f, std::min(160.f, (avail - (g_ccChats.size() - 1) * 2) / g_ccChats.size()));
    for (size_t i = 0; i < g_ccChats.size(); i++) {
      CcChat* cc = g_ccChats[i];
      D2D1_RECT_F r = rc(x, ty, tw, th);
      bool on = (int)i == g_ccActive, h = hov(r);
      if (on) { fillR(r, ccBoxCol(), 1.f, 7); strokeR(r, t.border, 1.f, 7); }
      else if (h) fillR(r, t.fg, 0.08f, 7);
      float alpha = on || h ? 1.f : 0.62f;
      D2D1_RECT_F ic = rc(x + 8, ty + 6, 16, 16);
      if (ccBusy(cc)) g_ccAnim = true;
      ccDrawIcon(CI_SPARK, ic, CC_ORANGE, alpha, ccBusy(cc) ? (float)(GetTickCount64() % 2400) * 360.f / 2400.f : 0.f);
      D2D1_RECT_F xr = rc(x + tw - 24, ty + 4, 20, 20);
      float tx = x + 30, tmax = tw - 30 - 26;
      wstring title = ccEllipsize(cc->title.empty() ? L"Untitled" : cc->title, tmax, 12.5f);
      g_br->SetColor(Rgb(t.fg, alpha));
      g_rt->DrawText(title.c_str(), (UINT32)title.size(), ccFont(12.5f, false, false, false), D2D1::RectF(tx, ccLineTop(ty, th, 12.5f), tx + tmax + 4, ty + th), g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
      if (cc->attention || (!on && cc->current == nullptr && std::any_of(cc->blocks.begin(), cc->blocks.end(), [](CcBlock* b) { return b->kind == CB_ASK && !b->done; }))) {
        g_br->SetColor(Rgb(CC_YELLOW));
        g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + tw - 30, ty + th / 2), 3.5f, 3.5f), g_br);
      }
      hit(r, HD_TAB, (int)i);
      if (on || h) {
        if (hov(xr)) fillR(xr, t.fg, 0.14f, 5);
        ccDrawIcon(CI_CLOSE, rc(xr.left + 3, xr.top + 3, 14, 14), t.fg, hov(xr) ? 1.f : 0.6f);
        hit(xr, HD_TABX, (int)i);
      }
      x += tw + 2;
    }
    D2D1_RECT_F ad = rc(x + 2, ty + 1, 26, 26);
    ccDrawIconBtn(ad, CI_PLUS, hov(ad));
    hit(ad, HD_ADD);
    D2D1_RECT_F hd = rc(x0 + W - 6 - 26, ty + 1, 26, 26);
    ccDrawIconBtn(hd, CI_CLOSE, hov(hd));
    hit(hd, HD_HIDE);
    fillRect(x0 + 1, top + CC_TABS_H - 1, W - 1, 1, t.border);
  }
  if (!c) { g_rt->PopAxisAlignedClip(); return; }

  /* header of the conversation: usage, account, history, new, close */
  float hy = top + CC_TABS_H;
  {
    float x = x0 + W - 8;
    struct B { int icon, kind; const wchar_t* name; };
    B bs[] = {{CI_CLOSE, HD_CLOSECHAT, L""}, {CI_CHAT, HD_NEW, L""}, {CI_HISTORY, HD_HISTORY, L"history"}, {CI_USER, HD_ACCOUNT, L"account"}};
    for (auto& b : bs) {
      D2D1_RECT_F r = rc(x - 28, hy + 2, 28, 28);
      ccDrawIconBtn(r, b.icon, hov(r));
      hit(r, b.kind);
      if (b.name[0]) g_ccAnchors[b.name] = r;
      x -= 28;
    }
    if (!c->usageText.empty()) {
      float w = ccTextW(c->usageText, 11.f) + 12;
      D2D1_RECT_F r = rc(x - w - 6, hy + 6, w, 20);
      DWORD col = c->usageHot ? CC_YELLOW : t.fg;
      strokeR(r, col, c->usageHot ? 1.f : 0.5f, 10);
      drawText(c->usageText.c_str(), (int)c->usageText.size(), r.left + 6, ccLineTop(r.top, 20, 11.f), col, ccFont(11.f, false, false, false), w, 20);
      hit(r, HD_USAGE);
    }
  }

  /* composer geometry */
  float pad = 12, boxX = x0 + pad, boxW = W - 2 * pad;
  struct Chip { wstring name; int kind, idx; };
  vector<Chip> chips;
  {
    CcContext cx;
    if (g_ccContext && ccGetContext(cx)) {
      wstring nm = baseName(cx.file);
      if (!cx.selection.empty()) nm += L" \x00B7 lines " + std::to_wstring(cx.fromLine) + (cx.toLine != cx.fromLine ? L"-" + std::to_wstring(cx.toLine) : L"");
      chips.push_back({nm, 0, 0});
    }
    for (size_t i = 0; i < c->atts.size(); i++) chips.push_back({c->atts[i].name, c->atts[i].kind == 1 ? 2 : 1, (int)i});
  }
  float inW = boxW - 20;
  wstring key = std::to_wstring(c->convId) + L"|" + std::to_wstring((int)inW) + L"|" + c->input.t;
  if (key != g_ccInKey) {
    g_ccInKey = key;
    g_ccInTxt.text = c->input.t.empty() ? L" " : c->input.t;
    g_ccInTxt.spans.clear();
    ccBuild(g_ccInTxt, inW, 13.5f, 20.25f);
  }
  float inH = std::max(24.f, std::min(160.f, g_ccInTxt.h + 4));
  // the chips wrap into rows
  vector<D2D1_RECT_F> chipR;
  float chipsH = 0;
  {
    float cx = boxX + 10, cy = 0;
    int row = 0;
    for (auto& ch : chips) {
      float w = 8 + 13 + 6 + std::min(ccTextW(ch.name, 12.f), inW - 70) + 4 + 18 + 2;
      if (cx + w > boxX + boxW - 10 && cx > boxX + 10) { cx = boxX + 10; row++; }
      chipR.push_back(rc(cx, (float)row, w, 24));
      cx += w + 6;
    }
    int rows = chips.empty() ? 0 : row + 1;
    chipsH = rows ? rows * 24 + (rows - 1) * 6 + 8 : 0;
  }
  float cwdH = 6 + 15;
  float boxH = 8 + chipsH + inH + 8 + 28 + 6;
  float compH = 8 + boxH + cwdH + 8;
  float compTop = bot - compH;
  float boxY = compTop + 8;

  /* messages */
  {
    float my0 = hy + CC_HEAD_H, my1 = compTop;
    float innerW = W - 32;
    float y = 12, prevMb = 0;
    c->tops.assign(c->blocks.size(), 0);
    for (size_t i = 0; i < c->blocks.size(); i++) {
      CcBlock* b = c->blocks[i];
      if (b->dirty || !b->prims || std::fabs(b->lastW - innerW) > 0.5f) ccLayout(c, b, innerW);
      float mt, mb;
      ccMargins(b, mt, mb);
      if (i > 0) y += std::max(prevMb, mt);
      c->tops[i] = y;
      y += b->h;
      prevMb = mb;
    }
    float workTop = 0;
    if (c->working) {
      y += std::max(prevMb, 12.f);
      workTop = y;
      y += 24;
      prevMb = 12;
    }
    c->contentH = y + 16;
    c->viewH = my1 - my0;
    float maxS = std::max(0.f, c->contentH - c->viewH);
    if (c->stick) c->scroll = maxS;
    c->scroll = std::max(0.f, std::min(c->scroll, maxS));
    g_rt->PushAxisAlignedClip(D2D1::RectF(x0 + 1, my0, cw(), my1), D2D1_ANTIALIAS_MODE_ALIASED);
    for (size_t i = 0; i < c->blocks.size(); i++) {
      CcBlock* b = c->blocks[i];
      float by = my0 + c->tops[i] - c->scroll;
      if (by + b->h < my0 - 10 || by > my1 + 10) continue;
      ccDrawBlock(c, b, x0 + 16, by, my0, my1);
    }
    if (c->working) {
      float by = my0 + workTop - c->scroll;
      g_ccAnim = true;
      ccDrawIcon(CI_SPARK, rc(x0 + 16, by + 4, 16, 16), CC_ORANGE, 1.f, (float)(GetTickCount64() % 2400) * 360.f / 2400.f);
      wstring v = c->verb.empty() ? L"Thinking" : c->verb;
      v += L"\x2026";
      drawText(v.c_str(), (int)v.size(), x0 + 16 + 24, ccLineTop(by, 24, 13.f), mixc(t.panel, t.fg, 0.7f), ccFont(13.f, false, false, false), 300, 24);
    }
    if (c->contentH > c->viewH) {
      float th = std::max(30.f, c->viewH * c->viewH / c->contentH), ty = my0 + (c->viewH - th) * (c->scroll / maxS);
      fillR(rc(x0 + W - 7, ty, 4, th), t.gutterFg, 0.35f, 2);
    }
    g_rt->PopAxisAlignedClip();
  }

  /* composer */
  fillRect(x0 + 1, compTop, W - 1, compH, t.panel);
  {
    D2D1_RECT_F box = rc(boxX, boxY, boxW, boxH);
    bool focus = g_ccFocusInput;
    fillR(box, ccBoxCol(), 1.f, 10);
    strokeR(box, focus ? CC_ORANGE : t.border, 1.f, 10);
    float y = boxY + 8;
    for (size_t i = 0; i < chips.size(); i++) {
      D2D1_RECT_F r = chipR[i];
      r.top += y;
      r.bottom += y;
      fillR(r, t.fg, 0.06f, 6);
      strokeR(r, t.border, 1.f, 6);
      ccDrawIcon(chips[i].kind == 2 ? CI_IMAGE : CI_FILE, rc(r.left + 8, r.top + 5.5f, 13, 13), t.fg, 0.7f);
      wstring nm = ccEllipsize(chips[i].name, r.right - r.left - 8 - 13 - 6 - 4 - 20, 12.f);
      drawText(nm.c_str(), (int)nm.size(), r.left + 27, ccLineTop(r.top, 24, 12.f), t.fg, ccFont(12.f, false, false, false), r.right - r.left, 24);
      D2D1_RECT_F xr = rc(r.right - 22, r.top + 3, 18, 18);
      if (hov(xr)) fillR(xr, t.fg, 0.14f, 4);
      ccDrawIcon(CI_CLOSE, rc(xr.left + 3, xr.top + 3, 12, 12), t.fg, hov(xr) ? 1.f : 0.5f);
      hit(xr, chips[i].kind == 0 ? HD_CTXOFF : HD_ATTOFF, chips[i].idx);
    }
    y += chipsH;
    // text box
    D2D1_RECT_F ir = rc(boxX + 10, y, inW, inH);
    g_ccInRect = ir;
    hit(rc(boxX, y - 2, boxW, inH + 4), HD_INPUT);
    LineEdit& in = c->input;
    in.c = std::max(0, std::min((int)in.t.size(), in.c));
    in.a = std::max(0, std::min((int)in.t.size(), in.a));
    float caretX = 0, caretY = 0;
    if (g_ccInTxt.lay) {
      float cx2, cy2;
      DWRITE_HIT_TEST_METRICS m{};
      g_ccInTxt.lay->HitTestTextPosition((UINT32)std::min<size_t>(in.c, g_ccInTxt.text.size()), FALSE, &cx2, &cy2, &m);
      caretX = cx2;
      caretY = cy2;
    }
    float maxIn = std::max(0.f, g_ccInTxt.h + 4 - inH);
    if (caretY < c->inScroll) c->inScroll = caretY;
    if (caretY + 21 > c->inScroll + inH) c->inScroll = caretY + 21 - inH;
    c->inScroll = std::max(0.f, std::min(c->inScroll, maxIn));
    g_ccInOx = ir.left;
    g_ccInOy = ir.top + 2 - c->inScroll;
    g_rt->PushAxisAlignedClip(D2D1::RectF(ir.left - 2, ir.top, ir.right + 2, ir.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
    if (in.t.empty()) {
      const wchar_t* ph = L"Ask Claude\x2026  (/ for commands, @ for files, Shift+Tab changes mode)";
      drawText(ph, (int)wcslen(ph), ir.left, g_ccInOy + 1, t.fg, uiFont(13.5f), inW + 4, 24);
      g_br->SetColor(Rgb(t.panel, 0.6f));
    } else {
      if (in.a != in.c)
        for (auto& r : ccRangeRects(g_ccInTxt, std::min(in.a, in.c), std::max(in.a, in.c))) fillRect(g_ccInOx + r.left, g_ccInOy + r.top, std::max(3.f, r.right - r.left), r.bottom - r.top, t.sel);
      ccDrawText(g_ccInTxt, g_ccInOx, g_ccInOy, t.fg);
    }
    if (focus && g_caretOn) fillRect(g_ccInOx + caretX, g_ccInOy + caretY + 1, 1.5f, 19, t.caret);
    g_rt->PopAxisAlignedClip();
    y += inH + 8;
    // tools row
    float ty = y;
    float x = boxX + 8;
    D2D1_RECT_F pl = rc(x, ty + 1, 26, 26);
    ccDrawIconBtn(pl, CI_PLUS, hov(pl));
    hit(pl, HD_PLUS);
    g_ccAnchors[L"plus"] = pl;
    x += 28;
    D2D1_RECT_F sl = rc(x, ty + 1, 26, 26);
    ccDrawIconBtn(sl, CI_SLASH, hov(sl));
    hit(sl, HD_SLASH);
    x += 28;
    auto chip = [&](float cx, const wstring& a, const wstring& b, int icon, DWORD col, int kind, const wchar_t* anchor, bool leftAligned) {
      float wa = ccTextW(a, 12.f), wb = b.empty() ? 0 : ccTextW(b, 12.f);
      float w = 14 + (icon ? 18 : 0) + wa + (b.empty() ? 0 : 5 + wb);
      D2D1_RECT_F r = rc(leftAligned ? cx : cx - w, ty + 2, w, 24);
      bool h = hov(r);
      if (h) fillR(r, t.fg, 0.10f, 6);
      float tx = r.left + 7;
      float al = h ? 1.f : 0.75f;
      if (icon) { ccDrawIcon(icon, rc(tx, r.top + 5.5f, 13, 13), col, al); tx += 18; }
      g_br->SetColor(Rgb(col, al));
      g_rt->DrawText(a.c_str(), (UINT32)a.size(), ccFont(12.f, false, false, false), D2D1::RectF(tx, ccLineTop(r.top, 24, 12.f), tx + wa + 6, r.bottom), g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
      if (!b.empty()) {
        g_br->SetColor(Rgb(col, 0.55f * al));
        g_rt->DrawText(b.c_str(), (UINT32)b.size(), ccFont(12.f, false, false, false), D2D1::RectF(tx + wa + 5, ccLineTop(r.top, 24, 12.f), tx + wa + wb + 11, r.bottom), g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
      }
      hit(r, kind);
      if (anchor) g_ccAnchors[anchor] = r;
      return r;
    };
    chip(x, ccModelLabel(c), ccCap(g_ccPrefEffort), 0, t.fg, HD_MODEL, L"model", true);
    // right side: send, mode, cost
    D2D1_RECT_F sd = rc(boxX + boxW - 8 - 26, ty + 1, 26, 26);
    bool busy = ccBusy(c);
    bool can = busy || !trimW(c->input.t).empty() || !c->atts.empty();
    g_br->SetColor(Rgb(CC_ORANGE, can ? (hov(sd) ? 1.f : 0.95f) : 0.35f));
    g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F((sd.left + sd.right) / 2, (sd.top + sd.bottom) / 2), 13, 13), g_br);
    ccDrawIcon(busy ? CI_STOP : CI_SEND, rc(sd.left + 5, sd.top + 5, 16, 16), 0x1f1f1f, can ? 1.f : 0.7f);
    if (can) hit(sd, HD_SEND);
    wstring modeName = ccModeName(ccModePref());
    if (modeName.empty()) modeName = ccModePref();
    D2D1_RECT_F mr = chip(sd.left - 4, modeName, L"", CI_BOLT, ccModePref() == L"plan" ? 0x6fb7e8 : t.fg, HD_MODE, L"mode", false);
    if (!c->costText.empty()) {
      float cw2 = ccTextW(c->costText, 11.5f);
      D2D1_RECT_F cr = rc(mr.left - 8 - cw2, ty + 2, cw2, 24);
      drawText(c->costText.c_str(), (int)c->costText.size(), cr.left, ccLineTop(cr.top, 24, 11.5f), c->costBig ? CC_YELLOW : t.fg, ccFont(11.5f, false, false, false), cw2 + 4, 24);
      if (!c->costBig) {
        // drawText has no alpha: dim it by painting the panel colour back over it
        fillR(cr, t.panel, 0.55f);
        fillR(cr, ccBoxCol(), 0.55f);
        drawText(c->costText.c_str(), (int)c->costText.size(), cr.left, ccLineTop(cr.top, 24, 11.5f), mixc(ccBoxCol(), t.fg, 0.45f), ccFont(11.5f, false, false, false), cw2 + 4, 24);
      }
      hit(cr, HD_COST);
    }
    // the folder
    wstring cwdText = c->cwd.empty() ? ccGetCwd() : c->cwd;
    bool warn = cwdText.empty();
    wstring cs = warn ? L"No folder open: Claude works in your user folder" : ccTail(cwdText, boxW - 8, 11.f);
    g_br->SetColor(Rgb(warn ? CC_YELLOW : t.fg, warn ? 0.9f : 0.45f));
    g_rt->DrawText(cs.c_str(), (UINT32)cs.size(), ccFont(11.f, false, false, false), D2D1::RectF(boxX + 4, boxY + boxH + 6, boxX + boxW, boxY + boxH + 22), g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
  }

  /* suggestions above the text box */
  if (c->sgKind && !c->sgItems.empty() && g_ccFocusInput) {
    int n = (int)c->sgItems.size(), vis = std::min(n, 9);
    float h = vis * 26 + 8, w = boxW;
    float sy = compTop + 6 - h;
    if (c->sgIndex < c->sgScroll) c->sgScroll = c->sgIndex;
    if (c->sgIndex >= c->sgScroll + vis) c->sgScroll = c->sgIndex - vis + 1;
    fillR(D2D1::RectF(boxX + 2, sy + 3, boxX + w + 2, sy + h + 3), 0x000000, 0.25f, 10);
    fillR(rc(boxX, sy, w, h), t.menu, 1.f, 10);
    strokeR(rc(boxX, sy, w, h), t.border, 1.f, 10);
    for (int i = 0; i < vis; i++) {
      int idx = c->sgScroll + i;
      if (idx >= n) break;
      D2D1_RECT_F r = rc(boxX + 4, sy + 4 + i * 26, w - 8, 26);
      if (idx == c->sgIndex || hov(r)) fillR(r, t.menuHi, 1.f, 6);
      float dw = c->sgItems[idx].desc.empty() ? 0.f : std::min(ccTextW(c->sgItems[idx].desc, 11.5f), (w - 8) * 0.5f);
      wstring lb = ccEllipsize(c->sgItems[idx].label, w - 8 - 18 - dw - 12, 12.5f);
      drawText(lb.c_str(), (int)lb.size(), r.left + 9, ccLineTop(r.top, 26, 12.5f), t.chromeFg, ccFont(12.5f, true, false, false), w, 26);
      if (dw > 0) {
        wstring ds = ccEllipsize(c->sgItems[idx].desc, dw, 11.5f);
        drawText(ds.c_str(), (int)ds.size(), r.right - 9 - dw, ccLineTop(r.top, 26, 11.5f), t.gutterFg, ccFont(11.5f, false, false, false), dw + 4, 26);
      }
      hit(r, HD_SUG, idx);
    }
  }

  /* pop-up list */
  if (g_ccPop.kind == 1) {
    CcPop& p = g_ccPop;
    float pw = std::min(340.f, W - 16), ph = 12;
    for (auto& it : p.items) ph += ccPopItemH(it);
    float maxH = (bot - top) * 0.6f;
    float visH = std::min(ph, maxH);
    float px = p.down ? x0 + W - 10 - pw : std::max(x0 + 8, std::min(p.anchor.left, x0 + W - pw - 8));
    float py = p.down ? p.anchor.bottom + 4 : p.anchor.top - 6 - visH;
    py = std::max(top + 4, py);
    p.scroll = std::max(0.f, std::min(p.scroll, std::max(0.f, ph - visH)));
    fillR(D2D1::RectF(px + 2, py + 4, px + pw + 2, py + visH + 4), 0x000000, 0.3f, 10);
    fillR(rc(px, py, pw, visH), t.menu, 1.f, 10);
    strokeR(rc(px, py, pw, visH), t.border, 1.f, 10);
    hit(rc(px, py, pw, visH), HD_POP, -1);
    g_rt->PushAxisAlignedClip(D2D1::RectF(px, py + 1, px + pw, py + visH - 1), D2D1_ANTIALIAS_MODE_ALIASED);
    float y = py + 6 - p.scroll;
    for (size_t i = 0; i < p.items.size(); i++) {
      CcPopItem& it = p.items[i];
      float h = ccPopItemH(it);
      D2D1_RECT_F r = rc(px + 6, y, pw - 12, h);
      if (it.kind == 1) {
        drawText(it.title.c_str(), (int)it.title.size(), px + 16, y + 5, t.fg, ccFont(11.5f, false, false, false), pw, 20);
        g_br->SetColor(Rgb(t.menu, 0.45f));
        g_rt->FillRectangle(rc(px + 16, y + 5, pw - 20, 20), g_br);
        drawText(it.title.c_str(), (int)it.title.size(), px + 16, y + 5, mixc(t.menu, t.fg, 0.55f), ccFont(11.5f, false, false, false), pw, 20);
      } else if (it.kind == 2) {
        wstring s = ccEllipsize(it.title, pw - 36, 13.f);
        drawText(s.c_str(), (int)s.size(), px + 16, y + 8, mixc(t.menu, t.fg, 0.6f), uiFont(13.f), pw, 20);
      } else if (it.kind == 4) {
        wstring a = ccEllipsize(it.title, pw - 36, 13.f, true);
        drawText(a.c_str(), (int)a.size(), px + 16, y + 6, t.fg, ccFont(13.f, true, false, false), pw, 20);
        wstring d = ccEllipsize(it.desc, pw - 36, 11.5f);
        drawText(d.c_str(), (int)d.size(), px + 16, y + 26, mixc(t.menu, t.fg, 0.6f), uiFont(11.5f), pw, 18);
      } else if (it.kind == 3) {
        fillRect(px + 10, y + 1, pw - 20, 1, t.border);
        wstring lb = it.title;
        drawText(lb.c_str(), (int)lb.size(), px + 16, y + 11, t.fg, uiFont(13.f), 100, 20);
        float lw = ccTextW(lb, 13.f);
        drawText(it.desc.c_str(), (int)it.desc.size(), px + 16 + lw + 6, y + 11, mixc(t.menu, t.fg, 0.6f), uiFont(13.f), 100, 20);
        float dx = px + pw - 18;
        for (int k = 4; k >= 0; k--) {
          bool on = g_ccPrefEffort == CC_EFFORTS[k];
          float rad = on ? 6.5f : 4.5f;
          g_br->SetColor(on ? Rgb(CC_ORANGE) : Rgb(t.fg, 0.3f));
          g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dx - 6.5f, y + 20), rad, rad), g_br);
          hit(rc(dx - 14, y + 8, 16, 24), HD_POPDOT, k);
          dx -= 17;
        }
      } else {
        bool h = hov(r) && g_my >= py && g_my < py + visH;
        if (h) fillR(r, t.menuHi, 1.f, 7);
        else if (it.sel) fillR(r, t.fg, 0.08f, 7);
        float tw = pw - 12 - 20 - (it.sel ? 22 : 0);
        if (it.desc.empty()) {
          wstring s = ccEllipsize(it.title, tw, 13.f, true);
          drawText(s.c_str(), (int)s.size(), px + 16, y + 5, t.chromeFg, ccFont(13.f, true, false, false), pw, 22);
        } else {
          bool rightDesc = p.name == L"history";
          wstring s = ccEllipsize(it.title, rightDesc ? tw - 50 : tw, 13.f, true);
          drawText(s.c_str(), (int)s.size(), px + 16, y + 5, t.chromeFg, ccFont(13.f, true, false, false), pw, 22);
          wstring d = ccEllipsize(it.desc, tw, 11.5f);
          drawText(d.c_str(), (int)d.size(), px + 16, y + 23, mixc(t.menu, t.fg, 0.6f), uiFont(11.5f), pw, 18);
        }
        if (it.sel) ccDrawIcon(CI_CHECK, rc(px + pw - 12 - 20, y + (it.desc.empty() ? 8 : 15), 14, 14), t.fg, 0.9f);
        if (it.run) hit(r, HD_POP, (int)i);
      }
      y += h;
    }
    g_rt->PopAxisAlignedClip();
  }
  g_rt->PopAxisAlignedClip();
}

/* ---------------------------------------------------------------- mouse */
static CcBlock* ccBlockOk(CcChat* c, CcBlock* b) {
  if (!c || !b) return nullptr;
  return std::find(c->blocks.begin(), c->blocks.end(), b) != c->blocks.end() ? b : nullptr;
}
static void ccInputClick(CcChat* c, float x, float y, bool extend) {
  if (!g_ccInTxt.lay) return;
  BOOL trailing, inside;
  DWRITE_HIT_TEST_METRICS m{};
  g_ccInTxt.lay->HitTestPoint(x - g_ccInOx, y - g_ccInOy, &trailing, &inside, &m);
  int pos = (int)m.textPosition + (trailing ? 1 : 0);
  pos = std::max(0, std::min((int)c->input.t.size(), pos));
  c->input.c = pos;
  if (!extend) c->input.a = pos;
  caretReset();
}
static bool ccMouseDown(float x, float y) {
  if (!g_ccOpen) return false;
  float x0 = ccLeft();
  if (x < x0 - 3) {
    g_ccFocusInput = false;
    g_ccFocusBlock = nullptr;
    g_ccPop.kind = 0;
    return false;
  }
  if (x < x0 + 3 && g_ccPop.kind == 0) { g_ccDragW = true; SetCapture(g_hwnd); return true; }
  CcChat* c = ccCur();
  // a click outside an open list closes it; the click itself still counts
  if (g_ccPop.kind == 1) {
    bool inside = false;
    for (int i = (int)g_ccHits.size() - 1; i >= 0; i--)
      if (g_ccHits[i].kind == HD_POP && g_ccHits[i].a == -1 && inRect(g_ccHits[i].r, x, y)) inside = true;
    if (!inside) {
      bool onAnchor = false;
      for (auto& kv : g_ccAnchors) if (inRect(kv.second, x, y)) onAnchor = true;
      g_ccPop.kind = 0;
      redraw();
      if (onAnchor) return true;
    }
  }
  for (int i = (int)g_ccHits.size() - 1; i >= 0; i--) {
    CcHit h = g_ccHits[i];
    if (!inRect(h.r, x, y)) continue;
    switch (h.kind) {
      case HD_TAB: ccActivate(h.a); g_ccFocusInput = true; break;
      case HD_TABX: ccCloseTab(h.a); break;
      case HD_ADD: ccAddChat(); ccActivate(g_ccActive); g_ccFocusInput = true; break;
      case HD_HIDE: ccHide(); break;
      case HD_ACCOUNT: if (c) ccOpenPopover(c, L"account"); break;
      case HD_HISTORY: if (c) ccOpenPopover(c, L"history"); break;
      case HD_NEW: ccAddChat(); ccActivate(g_ccActive); g_ccFocusInput = true; break;
      case HD_CLOSECHAT: ccCloseTab(g_ccActive); break;
      case HD_PLUS: if (c) ccOpenPopover(c, L"plus"); break;
      case HD_SLASH:
        if (c) { c->input.set(L"/"); g_ccFocusInput = true; ccUpdateSuggest(c); }
        break;
      case HD_MODEL: if (c) ccOpenPopover(c, L"model"); break;
      case HD_MODE: if (c) ccOpenPopover(c, L"mode"); break;
      case HD_SEND: if (c) { if (ccBusy(c)) ccInterrupt(c); else ccSend(c, c->input.t); } break;
      case HD_CTXOFF: g_ccContext = false; ccSaveStore(); break;
      case HD_ATTOFF: if (c && h.a < (int)c->atts.size()) c->atts.erase(c->atts.begin() + h.a); break;
      case HD_INPUT:
        g_ccFocusInput = true; g_panelFocus = 0; g_findFocus = false; g_nqFocus = false;
        g_ccFocusBlock = nullptr;
        if (c) { ccInputClick(c, x, y, GetKeyState(VK_SHIFT) & 0x8000); g_ccInDrag = true; SetCapture(g_hwnd); }
        break;
      case HD_SUG: if (c) ccAcceptSuggest(c, h.a); break;
      case HD_POP:
        if (h.a >= 0 && h.a < (int)g_ccPop.items.size() && g_ccPop.items[h.a].run) { auto run = g_ccPop.items[h.a].run; run(); }
        break;
      case HD_POPDOT:
        g_ccPrefEffort = CC_EFFORTS[h.a];
        ccSaveStore();
        if (c) ccBuildPopover(c, L"model", nullptr);
        break;
      // blocks of the chat
      case HT_TOGGLE: if (ccBlockOk(c, h.blk)) { h.blk->open = !h.blk->open; h.blk->dirty = true; } break;
      case HT_FILE: if (c) openPath(h.s); break;
      case HT_TEXT: {
        CcBlock* b = ccBlockOk(c, h.blk);
        if (b && b->prims && h.a >= 0 && h.a < (int)b->prims->size() && (*b->prims)[h.a].text) {
          wstring url = ccLinkAt(*(*b->prims)[h.a].text, h.ox, h.oy, x, y);
          if (!url.empty()) ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        break;
      }
      case HT_ASK: if (ccBlockOk(c, h.blk)) ccAskButton(c, h.blk, h.a); break;
      case HT_QOPT: {
        CcBlock* b = ccBlockOk(c, h.blk);
        if (!b || h.a >= (int)b->picked.size()) break;
        bool multi = b->input["questions"][h.a].value("multiSelect", false);
        auto& s = b->picked[h.a];
        if (!multi) { bool had = s.count(h.b) != 0; s.clear(); if (!had) s.insert(h.b); }
        else if (s.count(h.b)) s.erase(h.b);
        else s.insert(h.b);
        break;
      }
      case HT_QOTHER:
        if (ccBlockOk(c, h.blk)) { g_ccFocusBlock = h.blk; g_ccFocusField = h.a; g_ccFocusInput = false; caretReset(); }
        break;
      case HT_DIFF: {
        CcBlock* b = ccBlockOk(c, h.blk);
        if (b && h.a < (int)b->touched.size()) {
          wstring sh = ccShort(c, b->touched[h.a].file);
          for (auto& ch : sh) if (ch == L'\\') ch = L'/';
          openText(ccHunksToDiff(sh, b->touched[h.a]), L"Claude: " + sh, L_DIFF, L"Diff");
        }
        break;
      }
      case HT_REVERT: {
        CcBlock* b = ccBlockOk(c, h.blk);
        if (!b) break;
        if (appMsgBox(g_hwnd, L"Restore the files Claude changed in this turn to how they were before it?", L"Scriba", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
        int cid = c->convId;
        ccRewind(c, b->turnUuid, [b, c, cid](bool ok, wstring why) {
          if (!ccAlive(c) || !ccBlockOk(c, b)) return;
          if (ok) { b->reverted = true; b->dirty = true; ccAfterChange(); flash(L"Claude\x2019s changes were reverted"); }
          else flash(L"Could not revert: " + why);
          redraw();
        });
        break;
      }
      case HT_UNDO: {
        CcBlock* b = ccBlockOk(c, h.blk);
        if (!b || b->uuid.empty()) { flash(L"This message cannot be rewound (no checkpoint)"); break; }
        if (appMsgBox(g_hwnd, L"Restore the files to how they were before this message?", L"Scriba", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
        if (!c->procOpen && !ccEnsureProc(c)) break;
        std::string uuid = b->uuid;
        ccRewind(c, uuid, [](bool ok, wstring why) {
          if (ok) { ccAfterChange(); flash(L"Files restored"); }
          else flash(L"Could not restore: " + why);
        });
        break;
      }
      case HT_AUTHX: if (ccBlockOk(c, h.blk)) clAuthKill(h.blk->authId); break;
      default: continue;
    }
    redraw();
    return true;
  }
  // somewhere else in the dock: the messages
  g_ccFocusBlock = nullptr;
  redraw();
  return true;
}
static bool ccMouseMove(float x, float y) {
  if (g_ccDragW) {
    g_ccW = std::max(280.f, std::min(std::min(900.f, cw() * 0.7f), cw() - x));
    computeLayout();
    redraw();
    return true;
  }
  if (g_ccInDrag) {
    CcChat* c = ccCur();
    if (c) { ccInputClick(c, x, y, true); redraw(); }
    return true;
  }
  return false;
}
static void ccMouseUp() {
  if (g_ccDragW) ccSaveStore();
  g_ccDragW = false;
  g_ccInDrag = false;
}
static bool ccWheel(int delta) {
  CcChat* c = ccCur();
  if (!c) return false;
  if (g_ccPop.kind == 1) { g_ccPop.scroll -= delta / 120.f * 40; redraw(); return true; }
  float maxS = std::max(0.f, c->contentH - c->viewH);
  c->scroll = std::max(0.f, std::min(maxS, c->scroll - delta / 120.f * 60));
  c->stick = c->scroll >= maxS - 4;
  redraw();
  return true;
}

/* ---------------------------------------------------------------- keyboard */
static void ccInsertText(CcChat* c, const wstring& s) {
  c->input.insert(s);
  caretReset();
  ccUpdateSuggest(c);
}
static int ccLineStart(const wstring& t, int p) { while (p > 0 && t[p - 1] != L'\n') p--; return p; }
static int ccLineEnd(const wstring& t, int p) { while (p < (int)t.size() && t[p] != L'\n') p++; return p; }
static bool ccKey(WPARAM wp, bool ctrl, bool shift, bool alt) {
  if (!g_ccOpen) return false;
  if (g_panelFocus || g_findFocus || g_nqFocus) g_ccFocusInput = false;
  CcChat* c = ccCur();
  if (!c) return false;
  if (g_ccFocusBlock && !ccBlockOk(c, g_ccFocusBlock)) g_ccFocusBlock = nullptr;
  if (g_ccFocusBlock && g_ccFocusField >= 0 && g_ccFocusField < (int)g_ccFocusBlock->others.size()) {
    LineEdit& e = g_ccFocusBlock->others[g_ccFocusField];
    if (wp == VK_ESCAPE || wp == VK_RETURN) { g_ccFocusBlock = nullptr; redraw(); return true; }
    if (ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V') return false;
    if (alt) return false;
    editKey(e, wp, ctrl, shift);
    caretReset();
    redraw();
    return wp != VK_UP && wp != VK_DOWN && wp != VK_PRIOR && wp != VK_NEXT && wp != VK_F5;
  }
  if (g_ccPop.kind == 1 && wp == VK_ESCAPE) { g_ccPop.kind = 0; redraw(); return true; }
  if (!g_ccFocusInput) return false;
  LineEdit& in = c->input;
  if (c->sgKind && !c->sgItems.empty()) {
    int n = (int)std::min<size_t>(40, c->sgItems.size());
    if (wp == VK_DOWN) { c->sgIndex = (c->sgIndex + 1) % n; redraw(); return true; }
    if (wp == VK_UP) { c->sgIndex = (c->sgIndex + n - 1) % n; redraw(); return true; }
    if (wp == VK_TAB || (wp == VK_RETURN && !shift)) { ccAcceptSuggest(c, c->sgIndex); return true; }
    if (wp == VK_ESCAPE) { c->sgKind = 0; redraw(); return true; }
  }
  if (wp == VK_RETURN && !shift && !ctrl) { ccSend(c, in.t); redraw(); return true; }
  if (wp == VK_RETURN && shift) { ccInsertText(c, L"\n"); redraw(); return true; }
  if (wp == VK_TAB && shift) {
    static const wchar_t* order[] = {L"acceptEdits", L"plan", L"auto", L"manual"};
    wstring cur = ccModePref();
    int k = 0;
    for (int i = 0; i < 4; i++) if (cur == order[i]) k = i;
    g_ccPrefMode = order[(k + 1) % 4];
    g_ccModeSet = true;
    ccSaveStore();
    ccApplyMode(c);
    flash(wstring(L"Claude: ") + ccModeName(g_ccPrefMode));
    redraw();
    return true;
  }
  if (wp == VK_ESCAPE) {
    if (ccBusy(c)) ccInterrupt(c);
    else ccHide();
    redraw();
    return true;
  }
  if (alt) return false;
  if (ctrl && wp != 'A' && wp != 'C' && wp != 'X' && wp != 'V' && wp != VK_LEFT && wp != VK_RIGHT && wp != VK_BACK && wp != VK_DELETE) return false;
  bool changed = false;
  int n = (int)in.t.size();
  auto mv = [&](int p) { in.c = std::max(0, std::min(n, p)); if (!shift) in.a = in.c; };
  if (wp == 'V' && ctrl) {
    wstring p = clipGet();
    wstring o;
    for (wchar_t ch : p) if (ch != L'\r') o += ch;
    if (!o.empty()) { in.insert(o); changed = true; }
  } else if (wp == VK_HOME) mv(ccLineStart(in.t, in.c));
  else if (wp == VK_END) mv(ccLineEnd(in.t, in.c));
  else if (wp == VK_UP || wp == VK_DOWN) {
    int ls = ccLineStart(in.t, in.c), col = in.c - ls;
    if (wp == VK_UP) { if (ls == 0) mv(0); else { int ps = ccLineStart(in.t, ls - 1); mv(std::min(ps + col, ls - 1)); } }
    else { int le = ccLineEnd(in.t, in.c); if (le >= n) mv(n); else { int ne = ccLineEnd(in.t, le + 1); mv(std::min(le + 1 + col, ne)); } }
  } else if (wp == VK_PRIOR || wp == VK_NEXT) {
    wp == VK_PRIOR ? ccWheel(120 * 5) : ccWheel(-120 * 5);
    return true;
  } else changed = editKey(in, wp, ctrl, shift);
  caretReset();
  if (changed || wp == VK_LEFT || wp == VK_RIGHT) ccUpdateSuggest(c);
  redraw();
  return wp != VK_F5 && wp != VK_F3 && !(wp >= VK_F1 && wp <= VK_F12);
}
static bool ccChar(wchar_t ch) {
  if (!g_ccOpen) return false;
  if (g_panelFocus || g_findFocus || g_nqFocus) g_ccFocusInput = false;
  CcChat* c = ccCur();
  if (!c) return false;
  if (g_ccFocusBlock && ccBlockOk(c, g_ccFocusBlock) && g_ccFocusField >= 0 && g_ccFocusField < (int)g_ccFocusBlock->others.size()) {
    if (ch >= 32) { g_ccFocusBlock->others[g_ccFocusField].insert(wstring(1, ch)); caretReset(); redraw(); }
    return true;
  }
  if (!g_ccFocusInput) return false;
  if (ch >= 32 && ch != 127) ccInsertText(c, wstring(1, ch));
  redraw();
  return true;
}

/* ---------------------------------------------------------------- commands and the timer */
static void ccCommand(int cmd) {
  auto selectionText = [](wstring& file, int& from, int& to, wstring& text) {
    if (g_docs.empty() || D().path.empty() || D().isNote) return false;
    Doc& d = D();
    file = d.path;
    if (d.hasSel()) { from = d.selA().l + 1; to = d.selB().l + 1; text = d.selectedText(); }
    else { from = to = d.cur.l + 1; text = d.lines[d.cur.l]; }
    return !trimW(text).empty();
  };
  switch (cmd) {
    case C_CLAUDE: if (g_ccOpen) ccHide(); else ccShow(); break;
    case C_CLAUDENEW: ccShow(); ccAddChat(); ccActivate(g_ccActive); break;
    case C_CLAUDEASK: ccShow(); break;
    case C_CLAUDESTOP: if (CcChat* c = ccCur()) ccInterrupt(c); break;
    case C_CLAUDEADD: {
      wstring f, text;
      int a, b;
      if (!selectionText(f, a, b, text)) { flash(L"Open a saved file and select some code first"); break; }
      ccAddSelection(f, a, b, text);
      break;
    }
    case C_CLAUDEEXPLAIN: case C_CLAUDEFIX: case C_CLAUDETESTS: case C_CLAUDEREFACTOR: case C_CLAUDEDOC: {
      ccShow();
      CcChat* c = ccCur();
      if (!c) break;
      const wchar_t* q = cmd == C_CLAUDEEXPLAIN ? L"Explain this code clearly and briefly."
                       : cmd == C_CLAUDEFIX ? L"Look for bugs or risky code here and fix what is clearly wrong."
                       : cmd == C_CLAUDETESTS ? L"Write tests for this code."
                       : cmd == C_CLAUDEREFACTOR ? L"Refactor this code to be simpler and clearer without changing what it does."
                                                 : L"Add clear comments or documentation to this code, where it is not obvious.";
      ccAsk(c, q);
      break;
    }
  }
  redraw();
}
static void ccTimer() {
  ccTick();
  if (!g_ccOpen) return;
  bool any = g_ccAnim;
  for (auto* c : g_ccChats) if (ccBusy(c)) any = true;
  if (any) redraw();
}
