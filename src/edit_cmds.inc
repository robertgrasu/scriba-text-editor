// edit_cmds.inc: editing commands of the Edit, Selection, View and Goto menus, the pickers, bookmarks, folding and
// word completion. Included by main.cpp before exec().

static void flash(const wstring& m) {
  g_statusMsg = m;
  g_statusMsgAt = GetTickCount();
  SetTimer(g_hwnd, 4, 3700, nullptr);
  redraw();
}

/* ---------------------------------------------------------------- pickers: syntax, indentation, line endings, encoding */
static void setSyntax(Doc& d, const wchar_t* name, Lang lang) {
  d.lang = lang;
  d.langName = name;
  d.tsLang = tsFindName(name);  // a grammar of tree-sitter, when there is one
  d.tsOff = false;
  if (d.tsTree.t) { ts_tree_delete(d.tsTree.t); d.tsTree.t = nullptr; }
  d.invalidate(0);
  if (lang != L_MD) d.vis = false;
  ensureVisible();
  redraw();
}
static void pickSyntax() {
  vector<PItem> all;
  for (auto& sx : SYNTAXES) {
    PItem it;
    it.label = sx.name;
    if (wcscmp(sx.name, D().langName) == 0) it.right = L"current";
    const wchar_t* nm = sx.name;
    Lang lg = sx.lang;
    it.run = [nm, lg] { setSyntax(D(), nm, lg); };
    all.push_back(it);
  }
  openPick(L"Select syntax", listProvider(all));
}
// the language menu of the status bar (as in Sublime): every language in alphabetical order, the current one ticked; it opens upward from the item
static void syntaxMenu(float x, float yBottom) {
  Doc& d = D();
  struct E { wstring name; Lang lang; };
  vector<E> list;
  for (auto& sx : SYNTAXES) list.push_back({sx.name, sx.lang});
  for (size_t i = 0; i < sizeof(TS_LANGS) / sizeof(TS_LANGS[0]); i++) {
    bool have = false;
    for (auto& e : list) if (e.name == TS_LANGS[i].name) have = true;
    if (!have) list.push_back({TS_LANGS[i].name, L_CLIKE});
  }
  std::sort(list.begin(), list.end(), [](const E& a, const E& b) { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
  vector<CtxItem> items;
  for (auto& e : list) {
    CtxItem it = ctxItem(e.name, nullptr);
    wstring nm = e.name;
    Lang lg = e.lang;
    // the name must live as long as the document: take the pointer from the static tables
    const wchar_t* keep = nullptr;
    for (auto& sx : SYNTAXES) if (nm == sx.name) keep = sx.name;
    for (size_t i = 0; !keep && i < sizeof(TS_LANGS) / sizeof(TS_LANGS[0]); i++) if (nm == TS_LANGS[i].name) keep = TS_LANGS[i].name;
    it.run = [keep, lg] { if (keep) setSyntax(D(), keep, lg); };
    it.checked = nm == d.langName;
    items.push_back(it);
  }
  ctxShow(items, (int)x, (int)yBottom, true);
}
static void clampCaret(Doc& d) {
  d.cur.l = std::min(d.cur.l, (int)d.lines.size() - 1);
  d.anc.l = std::min(d.anc.l, (int)d.lines.size() - 1);
  d.cur.c = std::min(d.cur.c, (int)d.lines[d.cur.l].size());
  d.anc.c = std::min(d.anc.c, (int)d.lines[d.anc.l].size());
}
static void convertIndent(bool spaces) {
  Doc& d = D();
  d.snapshot(0);
  for (auto& s : d.lines) {
    size_t k = 0;
    int w = 0;
    while (k < s.size() && (s[k] == L' ' || s[k] == L'\t')) { w = s[k] == L'\t' ? w + d.tabW - w % d.tabW : w + 1; k++; }
    if (!k) continue;
    wstring nw = spaces ? wstring(w, L' ') : wstring(w / d.tabW, L'\t') + wstring(w % d.tabW, L' ');
    s = nw + s.substr(k);
  }
  d.spaces = spaces;
  d.dirty = true;
  d.invalidate(0);
  clampCaret(d);
  edited();
}
static void pickIndent() {
  Doc& d = D();
  vector<PItem> all;
  auto add = [&](const wstring& label, const wstring& right, std::function<void()> fn) {
    PItem it;
    it.label = label;
    it.right = right;
    it.run = fn;
    all.push_back(it);
  };
  add(d.spaces ? L"Indent Using Tabs" : L"Indent Using Spaces", L"", [] { Doc& x = D(); x.spaces = !x.spaces; x.invalidate(0); redraw(); });
  for (int n : {1, 2, 3, 4, 5, 6, 8})
    add(L"Tab Width: " + std::to_wstring(n), n == d.tabW ? L"current" : L"", [n] { Doc& x = D(); x.tabW = n; x.invalidate(0); ensureVisible(); redraw(); });
  add(L"Guess Settings From Buffer", L"", [] { Doc& x = D(); guessIndent(x); x.invalidate(0); redraw(); });
  add(L"Convert Indentation to Spaces", L"", [] { convertIndent(true); });
  add(L"Convert Indentation to Tabs", L"", [] { convertIndent(false); });
  openPick(L"Indentation", listProvider(all));
}
static void pickEol() {
  Doc& d = D();
  vector<PItem> all;
  PItem a, b;
  a.label = L"Windows (CRLF)";
  a.right = d.crlf ? L"current" : L"";
  a.run = [] { D().crlf = true; D().dirty = true; updateTitle(); redraw(); };
  b.label = L"Unix (LF)";
  b.right = d.crlf ? L"" : L"current";
  b.run = [] { D().crlf = false; D().dirty = true; updateTitle(); redraw(); };
  all.push_back(a);
  all.push_back(b);
  openPick(L"Line endings", listProvider(all));
}
static bool doSave(Doc& d);
static void pickEncoding() {
  Doc& d = D();
  struct E { const wchar_t* name; int enc; bool bom; };
  static const E encs[] = {{L"UTF-8", 0, false}, {L"UTF-8 with BOM", 0, true}, {L"UTF-16 LE with BOM", 1, true}, {L"UTF-16 BE with BOM", 2, true}, {L"Western (Windows 1252)", 3, false}};
  vector<PItem> all;
  for (auto& e : encs) {
    PItem it;
    it.label = wstring(L"Save with Encoding: ") + e.name;
    it.right = (d.enc == e.enc && d.bom == e.bom) ? L"current" : L"";
    int enc = e.enc;
    bool bom = e.bom;
    it.run = [enc, bom] {
      Doc& x = D();
      x.enc = enc;
      x.bom = bom;
      x.dirty = true;
      if (!x.path.empty() && !x.isNote) doSave(x);
      updateTitle();
      redraw();
    };
    all.push_back(it);
  }
  openPick(L"Encoding", listProvider(all));
}

// the status bar items open small menus above themselves, like Sublime's
static void indentMenu(float x, float yBottom) {
  Doc& d = D();
  vector<CtxItem> items;
  CtxItem sp = ctxItem(L"Indent Using Spaces", [] { Doc& z = D(); z.spaces = !z.spaces; z.invalidate(0); redraw(); });
  sp.checked = d.spaces;
  items.push_back(sp);
  items.push_back(ctxSep());
  for (int n = 1; n <= 8; n++) {
    CtxItem it = ctxItem(L"Tab Width: " + std::to_wstring(n), [n] { Doc& z = D(); z.tabW = n; z.invalidate(0); ensureVisible(); redraw(); });
    it.checked = n == d.tabW;
    items.push_back(it);
  }
  items.push_back(ctxSep());
  items.push_back(ctxItem(L"Guess Settings From Buffer", [] { Doc& z = D(); guessIndent(z); z.invalidate(0); redraw(); }));
  items.push_back(ctxSep());
  items.push_back(ctxItem(L"Convert Indentation to Spaces", [] { convertIndent(true); }));
  items.push_back(ctxItem(L"Convert Indentation to Tabs", [] { convertIndent(false); }));
  ctxShow(items, (int)x, (int)yBottom, true);
}
static void eolMenu(float x, float yBottom) {
  Doc& d = D();
  vector<CtxItem> items;
  CtxItem a = ctxItem(L"Windows (CRLF)", [] { D().crlf = true; D().dirty = true; updateTitle(); redraw(); });
  CtxItem b = ctxItem(L"Unix (LF)", [] { D().crlf = false; D().dirty = true; updateTitle(); redraw(); });
  a.checked = d.crlf;
  b.checked = !d.crlf;
  items.push_back(a);
  items.push_back(b);
  ctxShow(items, (int)x, (int)yBottom, true);
}
static void encodingMenu(float x, float yBottom) {
  Doc& d = D();
  struct E { const wchar_t* name; int enc; bool bom; };
  static const E encs[] = {{L"UTF-8", 0, false}, {L"UTF-8 with BOM", 0, true}, {L"UTF-16 LE with BOM", 1, true}, {L"UTF-16 BE with BOM", 2, true}, {L"Western (Windows 1252)", 3, false}};
  vector<CtxItem> items;
  for (auto& e : encs) {
    int enc = e.enc;
    bool bom = e.bom;
    CtxItem it = ctxItem(L"Save with Encoding: " + wstring(e.name), [enc, bom] {
      Doc& z = D();
      z.enc = enc;
      z.bom = bom;
      z.dirty = true;
      if (!z.path.empty() && !z.isNote) doSave(z);
      updateTitle();
      redraw();
    });
    it.checked = d.enc == e.enc && d.bom == e.bom;
    items.push_back(it);
  }
  ctxShow(items, (int)x, (int)yBottom, true);
}

/* ---------------------------------------------------------------- bracket and selection commands */
static long offsetOf(Doc& d, Pos p) {
  long o = 0;
  for (int i = 0; i < p.l && i < (int)d.lines.size(); i++) o += (long)d.lines[i].size() + 1;
  return o + p.c;
}
static void cmdSelectBrackets() {
  Doc& d = D();
  Pos a = d.selA(), b = d.selB(), from = a, o, c;
  for (int guard = 0; guard < 60; guard++) {
    if (!enclosingBrackets(d, from, b, o, c)) { flash(L"No brackets around the cursor"); return; }
    Pos ca{o.l, o.c + 1}, cb = c;
    if (ca < a || b < cb) { d.anc = ca; d.cur = cb; moved(true); return; }
    Pos ia = o, ib{c.l, c.c + 1};
    if (ia < a || b < ib) { d.anc = ia; d.cur = ib; moved(true); return; }
    from = o;
  }
}
static void cmdExpandScope() {
  Doc& d = D();
  Pos a = d.selA(), b = d.selB();
  struct R { Pos a, b; };
  vector<R> cand;
  const wstring& s = d.lines[a.l];
  if (a.l == b.l) {
    int x = a.c, y = b.c;
    if (x == y) {
      while (x > 0 && isWordCh(s[x - 1])) x--;
      while (y < (int)s.size() && isWordCh(s[y])) y++;
      if (x != y) cand.push_back({{a.l, x}, {a.l, y}});
    } else {
      int x2 = a.c, y2 = b.c;
      while (x2 > 0 && isWordCh(s[x2 - 1])) x2--;
      while (y2 < (int)s.size() && isWordCh(s[y2])) y2++;
      cand.push_back({{a.l, x2}, {a.l, y2}});
    }
    vector<Span> sp;
    d.tokensOf(a.l, sp);
    for (auto& t : sp)
      if (t.kind == 2 && t.a <= a.c && t.b >= b.c) {
        cand.push_back({{a.l, t.a}, {a.l, t.b}});
        if (t.b - t.a >= 2) cand.push_back({{a.l, t.a + 1}, {a.l, t.b - 1}});
      }
    size_t k = 0;
    while (k < s.size() && (s[k] == L' ' || s[k] == L'\t')) k++;
    size_t e = s.size();
    while (e > k && (s[e - 1] == L' ' || s[e - 1] == L'\t')) e--;
    cand.push_back({{a.l, (int)k}, {a.l, (int)e}});
  }
  Pos from = a, o, c;
  for (int guard = 0; guard < 40; guard++) {
    if (!enclosingBrackets(d, from, b, o, c)) break;
    cand.push_back({{o.l, o.c + 1}, c});
    cand.push_back({o, {c.l, c.c + 1}});
    from = o;
  }
  cand.push_back({{0, 0}, {(int)d.lines.size() - 1, (int)d.lines.back().size()}});
  long sa = offsetOf(d, a), sb = offsetOf(d, b), best = -1;
  int bi = -1;
  for (size_t i = 0; i < cand.size(); i++) {
    long ca = offsetOf(d, cand[i].a), cb = offsetOf(d, cand[i].b);
    if (ca <= sa && sb <= cb && !(ca == sa && cb == sb)) {
      if (best < 0 || cb - ca < best) { best = cb - ca; bi = (int)i; }
    }
  }
  if (bi >= 0) { d.anc = cand[bi].a; d.cur = cand[bi].b; moved(true); }
}
static void cmdGotoBracket() {
  Doc& d = D();
  Pos c = d.cur, m;
  for (int pass = 0; pass < 2; pass++) {
    int col = pass == 0 ? c.c - 1 : c.c;
    if (col < 0 || col >= (int)d.lines[c.l].size()) continue;
    if (matchBracketAt(d, {c.l, col}, m)) {
      Pos at{c.l, col};
      d.cur = at < m ? Pos{m.l, m.c} : Pos{m.l, m.c + 1};
      if (at < m) d.cur = {m.l, m.c + 1}; else d.cur = m;
      moved(false);
      return;
    }
  }
  Pos o, cl;
  if (enclosingBrackets(d, c, c, o, cl)) { d.cur = cl; moved(false); }
}
static void cmdSkipOccurrence() {
  Doc& d = D();
  size_t before = d.extra.size();
  addNextOccurrence();
  if (d.extra.size() > before) d.extra.pop_back();  // the occurrence we just left is dropped
  redraw();
}

/* ---------------------------------------------------------------- line and text commands */
static void cmdDeleteEol() {
  multi([] {
    Doc& d = D();
    if (d.hasSel()) { d.snapshot(0); d.replaceRange(d.selA(), d.selB(), L""); return; }
    Pos p = d.cur, e = p.c < (int)d.lines[p.l].size() ? Pos{p.l, (int)d.lines[p.l].size()} : (p.l + 1 < (int)d.lines.size() ? Pos{p.l + 1, 0} : p);
    if (e == p) return;
    d.snapshot(0);
    d.replaceRange(p, e, L"");
  });
  edited();
}
static void cmdDeleteBol() {
  multi([] {
    Doc& d = D();
    if (d.hasSel()) { d.snapshot(0); d.replaceRange(d.selA(), d.selB(), L""); return; }
    Pos p = d.cur, s = p.c > 0 ? Pos{p.l, 0} : (p.l > 0 ? Pos{p.l - 1, (int)d.lines[p.l - 1].size()} : p);
    if (s == p) return;
    d.snapshot(0);
    d.replaceRange(s, p, L"");
  });
  edited();
}
static void cmdTranspose() {
  multi([] {
    Doc& d = D();
    if (d.hasSel()) return;
    wstring& s = d.lines[d.cur.l];
    int c = d.cur.c;
    if (s.size() < 2 || c == 0) return;
    d.snapshot(0);
    if (c >= (int)s.size()) { std::swap(s[s.size() - 1], s[s.size() - 2]); }
    else { std::swap(s[c - 1], s[c]); d.cur.c = c + 1; d.anc = d.cur; }
    d.dirty = true;
    d.invalidate(d.cur.l);
  });
  edited();
}
static void replaceSelectionsText(const std::function<wstring(const wstring&)>& fn, bool wordIfEmpty) {
  multi([&] {
    Doc& d = D();
    Pos a = d.selA(), b = d.selB();
    if (!d.hasSel()) {
      if (!wordIfEmpty) return;
      const wstring& s = d.lines[d.cur.l];
      int x = d.cur.c, y = d.cur.c;
      while (x > 0 && isWordCh(s[x - 1])) x--;
      while (y < (int)s.size() && isWordCh(s[y])) y++;
      if (x == y) return;
      a = {d.cur.l, x};
      b = {d.cur.l, y};
    }
    wstring t = d.textOf(a, b), r = fn(t);
    if (r == t) return;
    d.snapshot(0);
    d.replaceRange(a, b, r);
    // keep the converted text selected, like the Electron edition
    d.anc = a;
  });
  edited();
}
static void cmdCase(int mode) {
  replaceSelectionsText([mode](const wstring& t) {
    wstring r = t;
    if (mode == 0) for (auto& c : r) c = (wchar_t)towupper(c);
    else if (mode == 1) for (auto& c : r) c = (wchar_t)towlower(c);
    else {
      bool up = true;
      for (auto& c : r) {
        if (iswalpha(c)) { c = (wchar_t)(up ? towupper(c) : towlower(c)); up = false; }
        else if (!isIdCh(c) && c != L'\'') up = true;
      }
    }
    return r;
  }, true);
}
static void cmdBlockComment() {
  Doc& d = D();
  const wchar_t *o = nullptr, *c = nullptr;
  if (isClike(d.lang)) { o = L"/*"; c = L"*/"; }
  else if (d.lang == L_HTML || d.lang == L_MD) { o = L"<!--"; c = L"-->"; }
  if (!o) { exec(C_COMMENT); return; }
  size_t ol = wcslen(o), cl = wcslen(c);
  Pos a = d.selA(), b = d.selB();
  if (!d.hasSel()) {
    const wstring& s = d.lines[d.cur.l];
    size_t k = 0;
    while (k < s.size() && (s[k] == L' ' || s[k] == L'\t')) k++;
    a = {d.cur.l, (int)k};
    b = {d.cur.l, (int)s.size()};
    if (a.c >= b.c) return;
  }
  wstring t = d.textOf(a, b);
  d.snapshot(0);
  // already commented: take the markers off (inside the selection, or just around it)
  wstring tt = trimW(t);
  if (tt.size() >= ol + cl && tt.compare(0, ol, o) == 0 && tt.compare(tt.size() - cl, cl, c) == 0) {
    wstring inner = tt.substr(ol, tt.size() - ol - cl);
    if (!inner.empty() && inner[0] == L' ') inner.erase(0, 1);
    if (!inner.empty() && inner.back() == L' ') inner.pop_back();
    d.replaceRange(a, b, inner);
    d.anc = a;
  } else {
    const wstring& ls = d.lines[a.l];
    const wstring& le = d.lines[b.l];
    bool before = a.c >= (int)ol && ls.compare(a.c - ol, ol, o) == 0 || (a.c >= (int)ol + 1 && ls.compare(a.c - ol - 1, ol, o) == 0);
    bool after = b.c + (int)cl <= (int)le.size() && le.compare(b.c, cl, c) == 0;
    if (before && after) {
      int ba = a.c, bb = b.c;
      if (ba >= (int)ol + 1 && ls.compare(ba - ol - 1, ol, o) == 0 && ls[ba - 1] == L' ') ba = ba - (int)ol - 1; else ba = ba - (int)ol;
      if (bb + (int)cl + 1 <= (int)le.size() && le[bb] == L' ' && le.compare(bb + 1, cl, c) == 0) bb = bb + 1 + (int)cl; else bb = bb + (int)cl;
      wstring keep = t;
      d.replaceRange({a.l, ba}, {b.l, bb}, keep);
      d.anc = {a.l, ba};
    } else {
      d.replaceRange(a, b, wstring(o) + L" " + t + L" " + c);
      d.anc = a;
    }
  }
  edited();
}
// indentation by brackets for the C-like languages: one level per open bracket
static void cmdReindent() {
  Doc& d = D();
  if (!isClike(d.lang)) { flash(L"Reindent works in C-like languages"); return; }
  Pos a = d.selA(), b = d.selB();
  int l0 = a.l, l1 = (b.c == 0 && b.l > a.l) ? b.l - 1 : b.l;
  if (!d.hasSel() || l0 == l1) { l0 = 0; l1 = (int)d.lines.size() - 1; if (d.hasSel()) { l0 = a.l; l1 = b.l; } }
  d.snapshot(0);
  int depth = 0;
  vector<char> mask;
  wstring unit = indentUnit(d);
  for (int l = 0; l <= l1; l++) {
    wstring& s = d.lines[l];
    codeMask(d, l, mask);
    size_t k = 0;
    while (k < s.size() && (s[k] == L' ' || s[k] == L'\t')) k++;
    int lead = 0;
    for (size_t i = k; i < s.size() && isCloseBr(s[i]) && mask[i]; i++) lead++;
    // multi-line comments and strings keep their own indentation
    bool inCode = k >= s.size() || mask[k];
    if (l >= l0 && k < s.size() && inCode) {
      int lvl = std::max(0, depth - lead);
      wstring ind;
      for (int i = 0; i < lvl; i++) ind += unit;
      s = ind + s.substr(k);
      if (l == d.cur.l) d.cur.c = std::min((int)s.size(), std::max((int)ind.size(), d.cur.c - (int)k + (int)ind.size()));
    }
    // recompute the mask for the new text positions
    codeMask(d, l, mask);
    for (size_t i = 0; i < s.size(); i++) {
      if (!mask[i]) continue;
      if (isOpenBr(s[i])) depth++;
      else if (isCloseBr(s[i])) depth = std::max(0, depth - 1);
    }
  }
  d.anc = d.cur;
  d.dirty = true;
  d.invalidate(l0);
  clampCaret(d);
  edited();
}
static int g_ruler = 80;
static void cmdWrapParagraph() {
  Doc& d = D();
  Pos a = d.selA(), b = d.selB();
  int l0 = a.l, l1 = b.l;
  if (!d.hasSel()) {
    while (l0 > 0 && !blankLine(d.lines[l0 - 1])) l0--;
    while (l1 + 1 < (int)d.lines.size() && !blankLine(d.lines[l1 + 1])) l1++;
    if (blankLine(d.lines[d.cur.l])) return;
  }
  const wstring& first = d.lines[l0];
  size_t k = 0;
  while (k < first.size() && (first[k] == L' ' || first[k] == L'\t')) k++;
  wstring indent = first.substr(0, k);
  vector<wstring> words;
  for (int l = l0; l <= l1; l++) {
    const wstring& s = d.lines[l];
    size_t i = 0;
    while (i < s.size()) {
      while (i < s.size() && iswspace(s[i])) i++;
      size_t j = i;
      while (j < s.size() && !iswspace(s[j])) j++;
      if (j > i) words.push_back(s.substr(i, j - i));
      i = j;
    }
  }
  vector<wstring> out;
  wstring cur = indent;
  bool empty = true;
  auto len = [&](const wstring& s) { int n = 0; for (wchar_t c : s) n += c == L'\t' ? d.tabW : 1; return n; };
  for (auto& w : words) {
    if (!empty && len(cur) + 1 + (int)w.size() > g_ruler) { out.push_back(cur); cur = indent + w; }
    else { cur += (empty ? L"" : L" ") + w; }
    empty = false;
  }
  out.push_back(cur);
  d.snapshot(0);
  Pos pa{l0, 0}, pb{l1, (int)d.lines[l1].size()};
  wstring text;
  for (size_t i = 0; i < out.size(); i++) { if (i) text += L"\n"; text += out[i]; }
  d.replaceRange(pa, pb, text);
  edited();
}
static void cmdSortLines(bool ci) {
  Doc& d = D();
  d.snapshot(0);
  int a = 0, b = (int)d.lines.size() - 1;
  if (d.hasSel() && d.selA().l != d.selB().l) { a = d.selA().l; b = d.selB().l; }
  std::stable_sort(d.lines.begin() + a, d.lines.begin() + b + 1, [ci](const wstring& x, const wstring& y) {
    if (!ci) return x < y;
    return _wcsicmp(x.c_str(), y.c_str()) < 0;
  });
  d.dirty = true;
  d.invalidate(a);
  clampCaret(d);
  edited();
}
static void cmdPasteIndent() {
  wstring t = clipGet();
  if (t.empty()) return;
  Doc& d = D();
  // lines after the first lose their common indentation and take the indentation of the line we paste into
  vector<wstring> ls;
  size_t p = 0;
  while (true) {
    size_t q = t.find(L'\n', p);
    if (q == wstring::npos) { ls.push_back(t.substr(p)); break; }
    ls.push_back(t.substr(p, q - p));
    p = q + 1;
  }
  if (ls.size() > 1) {
    int minInd = 1 << 20;
    for (size_t i = 1; i < ls.size(); i++) if (!blankLine(ls[i])) minInd = std::min(minInd, (int)(ls[i].size() - ls[i].find_first_not_of(L" \t")));
    if (minInd == (1 << 20)) minInd = 0;
    const wstring& cl = d.lines[d.selA().l];
    size_t k = 0;
    while (k < cl.size() && (cl[k] == L' ' || cl[k] == L'\t')) k++;
    wstring base = cl.substr(0, std::min<size_t>(k, d.selA().c));
    t = ls[0];
    for (size_t i = 1; i < ls.size(); i++) {
      wstring rest = ls[i].size() >= (size_t)minInd ? ls[i].substr(minInd) : L"";
      t += L"\n" + (blankLine(rest) ? wstring() : base + rest);
    }
  }
  multi([&] { D().insertText(t, 0); });
  edited();
}

/* ---------------------------------------------------------------- bookmarks and folding */
static void bmToggle() {
  Doc& d = D();
  int l = d.cur.l;
  if (!d.marks.erase(l)) d.marks.insert(l);
  redraw();
}
static void bmGoto(int dir) {
  Doc& d = D();
  if (d.marks.empty()) { flash(L"No bookmarks in this file"); return; }
  int target = -1;
  if (dir > 0) {
    auto it = d.marks.upper_bound(d.cur.l);
    target = it == d.marks.end() ? *d.marks.begin() : *it;
  } else {
    auto it = d.marks.lower_bound(d.cur.l);
    target = it == d.marks.begin() ? *d.marks.rbegin() : *std::prev(it);
  }
  target = std::min(target, (int)d.lines.size() - 1);
  d.cur = d.anc = {target, 0};
  revealCenter();
  moved(false);
}
static void bmSelectAll() {
  Doc& d = D();
  if (d.marks.empty()) return;
  d.extra.clear();
  bool first = true;
  for (int l : d.marks) {
    if (l >= (int)d.lines.size()) continue;
    Pos p{l, (int)d.lines[l].size()};
    if (first) { d.cur = p; d.anc = {l, 0}; first = false; }
    else d.extra.push_back({p, {l, 0}});
  }
  moved(true);
}
static void bmClear() { D().marks.clear(); redraw(); }
static void foldToggleAt(int l) {
  Doc& d = D();
  int e;
  if (d.folded.count(l)) d.folded.erase(l);
  else if (foldRangeOf(d, l, e)) {
    d.folded.insert(l);
    if (d.cur.l > l && d.cur.l <= e) { d.cur = d.anc = {l, (int)d.lines[l].size()}; }
  } else return;
  d.ver++;
  ensureVisible();
  redraw();
}
static void cmdFold() {
  Doc& d = D();
  for (int l = d.cur.l; l >= 0; l--) {
    int e;
    if (!d.folded.count(l) && foldRangeOf(d, l, e) && e >= d.cur.l) { foldToggleAt(l); return; }
  }
}
static void cmdUnfold() {
  Doc& d = D();
  if (d.folded.erase(d.cur.l)) { d.ver++; redraw(); return; }
  // a fold that starts above and contains the cursor line cannot exist (the line would be hidden); open the nearest one below
  for (int l = d.cur.l - 1; l >= 0; l--) if (d.folded.count(l)) { d.folded.erase(l); d.ver++; redraw(); return; }
}
static void cmdFoldAll() {
  Doc& d = D();
  d.folded.clear();
  for (int l = 0; l < (int)d.lines.size(); l++) { int e; if (foldRangeOf(d, l, e)) d.folded.insert(l); }
  d.cur = d.anc = {std::min(d.cur.l, (int)d.lines.size() - 1), 0};
  for (int l : d.folded) { int e; if (foldRangeOf(d, l, e) && d.cur.l > l && d.cur.l <= e) d.cur = d.anc = {l, 0}; }
  d.ver++;
  ensureVisible();
  redraw();
}
static void cmdUnfoldAll() { D().folded.clear(); D().ver++; redraw(); }
// folds every block that is nested exactly `lvl` blocks deep (1 = the outermost ones)
static void cmdFoldLevel(int lvl) {
  Doc& d = D();
  d.folded.clear();
  vector<int> ends;  // last lines of the blocks around the current line
  for (int l = 0; l < (int)d.lines.size(); l++) {
    while (!ends.empty() && ends.back() < l) ends.pop_back();
    int e;
    if (!foldRangeOf(d, l, e)) continue;
    if ((int)ends.size() + 1 == lvl) d.folded.insert(l);
    ends.push_back(e);
  }
  for (int l : d.folded) { int e; if (foldRangeOf(d, l, e) && d.cur.l > l && d.cur.l <= e) d.cur = d.anc = {l, 0}; }
  d.ver++;
  ensureVisible();
  redraw();
}

/* ---------------------------------------------------------------- symbols (Goto Symbol) */
static vector<Symbol> g_symCache;
static const Doc* g_symDoc = nullptr;
static unsigned g_symVer = ~0u;
static void amxOutline(Doc& d, vector<Symbol>& out);  // amxx.inc
static vector<Symbol>& symbolsOf(Doc& d) {
  if (g_symDoc != &d || g_symVer != d.ver) {
    if (d.lang == L_PAWN) amxOutline(d, g_symCache); else collectSymbols(d, g_symCache);
    g_symDoc = &d;
    g_symVer = d.ver;
  }
  return g_symCache;
}
// places the caret has jumped from (Goto Line, definitions, search results), for Alt+- and Alt+Shift+-
struct JumpPt { Doc* doc; Pos p; };
static vector<JumpPt> g_jumps;
static int g_jumpIdx = 0;  // the entry the caret was last taken back to; == size() when it is at the newest place
static void jumpPush() {
  Doc& d = D();
  if (!g_jumps.empty() && g_jumpIdx > 0 && g_jumpIdx <= (int)g_jumps.size()) g_jumps.resize(g_jumpIdx);
  if (!g_jumps.empty() && g_jumps.back().doc == &d && g_jumps.back().p.l == d.cur.l) { g_jumpIdx = (int)g_jumps.size(); return; }
  g_jumps.push_back({&d, d.cur});
  if (g_jumps.size() > 100) g_jumps.erase(g_jumps.begin());
  g_jumpIdx = (int)g_jumps.size();
}
static void jumpTo(const JumpPt& j) {
  int idx = -1;
  for (int i = 0; i < (int)g_docs.size(); i++) if (g_docs[i] == j.doc) idx = i;
  if (idx < 0) { flash(L"That file is closed"); return; }
  if (idx != g_active) activateDoc(idx);
  Doc& d = D();
  d.cur = d.anc = {std::min(j.p.l, (int)d.lines.size() - 1), 0};
  d.cur.c = d.anc.c = std::min(j.p.c, (int)d.lines[d.cur.l].size());
  revealCenter();
  moved(false);
}
static void cmdJump(int dir) {
  if (dir < 0) {
    if (g_jumpIdx <= 0 || g_jumps.empty()) { flash(L"No earlier place to jump back to"); return; }
    if (g_jumpIdx >= (int)g_jumps.size()) { Doc& d = D(); g_jumps.push_back({&d, d.cur}); }
    g_jumpIdx--;
  } else {
    if (g_jumpIdx + 1 >= (int)g_jumps.size()) { flash(L"No later place to jump forward to"); return; }
    g_jumpIdx++;
  }
  jumpTo(g_jumps[g_jumpIdx]);
}
static void gotoLine(int l) {
  jumpPush();
  Doc& d = D();
  d.cur = d.anc = {std::max(0, std::min(l, (int)d.lines.size() - 1)), 0};
  d.wantV = 0;
  revealCenter();
  moved(false);
}
static void pickSymbol() {
  Doc& d = D();
  vector<Symbol> syms = symbolsOf(d);
  vector<PItem> all;
  for (auto& s : syms) {
    PItem it;
    it.label = wstring(s.level > 0 ? std::min(s.level, 3) * 0 : 0, L' ') + s.name;
    it.right = s.kind + L"  " + std::to_wstring(s.line + 1);
    int ln = s.line;
    it.run = [ln] { gotoLine(ln); };
    all.push_back(it);
  }
  if (all.empty()) { flash(L"No symbols in this file"); return; }
  int cur = 0;
  for (size_t i = 0; i < syms.size(); i++) if (syms[i].line <= d.cur.l) cur = (int)i;
  auto prov = listProvider(all);
  openPick(L"Go to symbol in " + d.name, prov, nullptr, [](const PItem& it) { (void)it; });
  g_psel = cur;
  g_pscroll = std::max(0, cur - 4);
}

/* ---------------------------------------------------------------- word completion */
struct CompItem { wstring label, detail, sig, doc, src; int boost = 0; std::function<void(Doc&)> apply; };  // `apply` replaces the default insertion
struct Completion { bool open = false; vector<CompItem> items; int sel = 0; int scroll = 0; int line = 0, col = 0; };
static Completion g_comp;
static vector<wstring> g_extraWords;  // names from include files and language servers
// extra suggestions of the language (AMX Mod X symbols, a language server): fills `out` for the word being typed
static std::function<void(Doc&, const wstring&, vector<CompItem>&)> g_compExtra;
static bool g_autoComplete = false;
static void compClose() { if (g_comp.open) { g_comp.open = false; redraw(); } }
static void compUpdate(bool force) {
  Doc& d = D();
  if (!d.extra.empty() || d.hasSel() || d.readOnly) { compClose(); return; }
  const wstring& s = d.lines[d.cur.l];
  int e = d.cur.c, b = e;
  while (b > 0 && isIdCh(s[b - 1])) b--;
  wstring pre = s.substr(b, e - b);
  if (pre.empty() || (!force && pre.size() < 2)) { compClose(); return; }
  wstring lp = lowerStr(pre);
  std::unordered_map<wstring, int> freq;
  size_t budget = 3000000;
  auto scan = [&](Doc& x, bool same) {
    for (int l = 0; l < (int)x.lines.size() && budget > 0; l++) {
      const wstring& t = x.lines[l];
      budget -= std::min(budget, t.size() + 1);
      size_t i = 0;
      while (i < t.size()) {
        if (!isIdStart(t[i]) && !iswdigit(t[i])) { i++; continue; }
        size_t j = i;
        while (j < t.size() && isIdCh(t[j])) j++;
        if (j - i >= 3 && !(same && l == d.cur.l && (int)i == b)) {
          wstring w = t.substr(i, j - i);
          if (lowerStr(w).compare(0, lp.size(), lp) == 0 && w != pre) freq[w] += 1;
        }
        i = j;
      }
    }
  };
  scan(d, true);
  for (auto* x : g_docs) if (x != &d && x->lines.size() < 60000) scan(*x, false);
  const std::unordered_set<wstring>* kws[] = {&KW_C, &TY_C, &KW_PY, &KW_SH, &KW_PAWN};
  int kidx = d.lang == L_PAWN ? 4 : isClike(d.lang) ? 0 : d.lang == L_PY ? 2 : d.lang == L_SH ? 3 : -1;
  vector<CompItem> cand;
  std::set<wstring> have;
  // from the language: its own symbols (documented) come first
  if (g_compExtra) {
    vector<CompItem> ex;
    g_compExtra(d, pre, ex);
    for (auto& it : ex) if (it.label != pre && lowerStr(it.label).compare(0, lp.size(), lp) == 0 && have.insert(it.label).second) cand.push_back(it);
  }
  for (auto& kv : freq) if (have.insert(kv.first).second) { CompItem it; it.label = kv.first; it.boost = kv.second; cand.push_back(it); }
  if (kidx >= 0) {
    for (auto& w : *kws[kidx]) if (w.size() >= 3 && lowerStr(w).compare(0, lp.size(), lp) == 0 && w != pre && have.insert(w).second) { CompItem it; it.label = w; it.detail = L"keyword"; it.boost = -5; cand.push_back(it); }
    if (kidx == 0) for (auto& w : TY_C) if (lowerStr(w).compare(0, lp.size(), lp) == 0 && w != pre && have.insert(w).second) { CompItem it; it.label = w; it.detail = L"type"; it.boost = -4; cand.push_back(it); }
  }
  for (auto& w : g_extraWords) if (w.size() >= 2 && lowerStr(w).compare(0, lp.size(), lp) == 0 && w != pre && have.insert(w).second) { CompItem it; it.label = w; it.boost = -1; cand.push_back(it); }
  std::stable_sort(cand.begin(), cand.end(), [&](const CompItem& x, const CompItem& y) {
    bool xe = x.label.compare(0, pre.size(), pre) == 0, ye = y.label.compare(0, pre.size(), pre) == 0;
    if (xe != ye) return xe;
    if (x.boost != y.boost) return x.boost > y.boost;
    if (x.label.size() != y.label.size()) return x.label.size() < y.label.size();
    return x.label < y.label;
  });
  if (cand.size() > 80) cand.resize(80);
  if (cand.empty()) { compClose(); return; }
  bool wasOpen = g_comp.open;
  g_comp.items = cand;
  g_comp.open = true;
  g_comp.line = d.cur.l;
  g_comp.col = b;
  if (!wasOpen) { g_comp.sel = 0; g_comp.scroll = 0; }
  g_comp.sel = std::min(g_comp.sel, (int)g_comp.items.size() - 1);
  redraw();
}
static void compAccept() {
  if (!g_comp.open || g_comp.items.empty()) return;
  Doc& d = D();
  CompItem item = g_comp.items[g_comp.sel];
  int cline = g_comp.line, ccol = g_comp.col;
  compClose();
  if (item.apply) { item.apply(d); return; }
  d.snapshot(0);
  d.replaceRange({cline, ccol}, d.cur, item.label);
  edited();
}
static bool compKey(WPARAM wp) {
  if (!g_comp.open) return false;
  int n = (int)g_comp.items.size();
  switch (wp) {
    case VK_DOWN: g_comp.sel = (g_comp.sel + 1) % n; break;
    case VK_UP: g_comp.sel = (g_comp.sel + n - 1) % n; break;
    case VK_NEXT: g_comp.sel = std::min(n - 1, g_comp.sel + 8); break;
    case VK_PRIOR: g_comp.sel = std::max(0, g_comp.sel - 8); break;
    case VK_RETURN: case VK_TAB: compAccept(); return true;
    case VK_ESCAPE: compClose(); return true;
    case VK_LEFT: case VK_RIGHT: case VK_HOME: case VK_END: compClose(); return false;
    default: return false;
  }
  if (g_comp.sel < g_comp.scroll) g_comp.scroll = g_comp.sel;
  if (g_comp.sel >= g_comp.scroll + 8) g_comp.scroll = g_comp.sel - 7;
  redraw();
  return true;
}

/* ---------------------------------------------------------------- which commands exist in this edition */
static bool g_distFree = false, g_statusOn = true;
static bool cmdAvailable(int cmd) {
  switch (cmd) {
    // not written yet: shown dimmed in the menus and left out of the command palette
#ifdef SCRIBA_FREE
    case C_LICENSE: return false;  // the free build has no license
#endif

  }
  return true;
}
static bool cmdChecked(int cmd) {
  switch (cmd) {
    case C_WRAP: return g_wrap;
    case C_MINIMAP: return g_mapVisible;
    case C_MDPREVIEW: return !g_docs.empty() && D().mdPrev;
    case C_SIDEBAR: return g_sbVisible;
    case C_STATUSBAR: return g_statusOn;
    case C_TOGGLEVISUAL: return !g_docs.empty() && D().vis;
    case C_VLINES: return g_vLines;
    case C_VSYMS: return g_vSyms;
    case C_LAYOUT2: return g_layout == 1;
    case C_LAYOUT3: return g_layout == 2;
    case C_MACROTOGGLE: return g_recording;
    case C_DISTFREE: return g_distFree;
    default: return false;
  }
}
