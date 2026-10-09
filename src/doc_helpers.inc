// doc_helpers.inc: helpers that only need the document model (brackets, code folding, symbols). Included by main.cpp.

/* ---------------------------------------------------------------- brackets */
static bool isOpenBr(wchar_t c) { return c == L'(' || c == L'[' || c == L'{'; }
static bool isCloseBr(wchar_t c) { return c == L')' || c == L']' || c == L'}'; }
static wchar_t brPartner(wchar_t c) {
  switch (c) {
    case L'(': return L')';
    case L'[': return L']';
    case L'{': return L'}';
    case L')': return L'(';
    case L']': return L'[';
    default: return L'{';
  }
}
// 1 for characters that are code (not inside a string or a comment)
static void codeMask(Doc& d, int l, vector<char>& m) {
  m.assign(d.lines[l].size(), 1);
  if (d.lang == L_PLAIN || d.lang == L_MD) return;
  vector<Span> sp;
  d.tokensOf(l, sp);
  for (auto& s : sp)
    if (s.kind == 2 || s.kind == 4)
      for (int i = s.a; i < s.b && i < (int)m.size(); i++) m[i] = 0;
}
static bool matchBracketAt(Doc& d, Pos at, Pos& out) {
  if (at.l < 0 || at.l >= (int)d.lines.size()) return false;
  const wstring& s0 = d.lines[at.l];
  if (at.c < 0 || at.c >= (int)s0.size()) return false;
  wchar_t c = s0[at.c];
  bool fwd = isOpenBr(c);
  if (!fwd && !isCloseBr(c)) return false;
  wchar_t other = brPartner(c);
  vector<char> mask;
  codeMask(d, at.l, mask);
  if (!mask[at.c]) return false;
  int depth = 0, l = at.l, i = at.c;
  long budget = 600000;
  while (true) {
    const wstring& s = d.lines[l];
    for (; fwd ? i < (int)s.size() : i >= 0; i += fwd ? 1 : -1) {
      if (--budget < 0) return false;
      if (!mask[i]) continue;
      if (s[i] == c) depth++;
      else if (s[i] == other) {
        depth--;
        if (depth == 0) { out = {l, i}; return true; }
      }
    }
    l += fwd ? 1 : -1;
    if (l < 0 || l >= (int)d.lines.size()) return false;
    codeMask(d, l, mask);
    i = fwd ? 0 : (int)d.lines[l].size() - 1;
  }
}
// nearest pair of brackets around [from, to]: `open` and `close` are the positions of the bracket characters
static bool enclosingBrackets(Doc& d, Pos from, Pos to, Pos& open, Pos& close) {
  int l = from.l, i = from.c - 1, depth = 0;
  long budget = 600000;
  vector<char> mask;
  codeMask(d, l, mask);
  while (true) {
    for (; i >= 0; i--) {
      if (--budget < 0) return false;
      if (i >= (int)mask.size() || !mask[i]) continue;
      wchar_t c = d.lines[l][i];
      if (isCloseBr(c)) depth++;
      else if (isOpenBr(c)) {
        if (depth > 0) { depth--; continue; }
        Pos cl;
        if (matchBracketAt(d, {l, i}, cl) && !(cl < to)) { open = {l, i}; close = cl; return true; }
      }
    }
    if (--l < 0) return false;
    codeMask(d, l, mask);
    i = (int)d.lines[l].size() - 1;
  }
}

/* ---------------------------------------------------------------- code folding */
static int indentWidth(const wstring& s, int tabW) {
  int w = 0;
  for (wchar_t c : s) {
    if (c == L' ') w++;
    else if (c == L'\t') w = (w / tabW + 1) * tabW;
    else break;
  }
  return w;
}
static bool blankLine(const wstring& s) {
  for (wchar_t c : s) if (c != L' ' && c != L'\t') return false;
  return true;
}
// the lines l+1..endLine can be hidden when line l is folded
static bool foldRangeOf(Doc& d, int l, int& endLine) {
  int n = (int)d.lines.size();
  if (l < 0 || l >= n - 1) return false;
  const wstring& s = d.lines[l];
  if (blankLine(s)) return false;
  if (d.lang == L_MD) {
    // a heading folds up to the next heading of the same or a higher level
    int lvl = 0;
    size_t k = 0;
    while (k < s.size() && s[k] == L' ') k++;
    while (k + lvl < s.size() && s[k + lvl] == L'#') lvl++;
    if (lvl == 0 || lvl > 6) return false;
    int e = l;
    for (int j = l + 1; j < n; j++) {
      const wstring& t = d.lines[j];
      int lv2 = 0;
      while (lv2 < (int)t.size() && t[lv2] == L'#') lv2++;
      if (lv2 > 0 && lv2 <= lvl && lv2 < (int)t.size() && t[lv2] == L' ') break;
      e = j;
    }
    while (e > l && blankLine(d.lines[e])) e--;
    if (e <= l) return false;
    endLine = e;
    return true;
  }
  if (isClike(d.lang) || d.lang == L_HTML || d.lang == L_SH) {
    // the last code character of the line is an opening bracket whose partner is on a later line
    vector<char> mask;
    codeMask(d, l, mask);
    int last = (int)s.size() - 1;
    while (last >= 0 && (s[last] == L' ' || s[last] == L'\t' || !mask[last])) last--;
    if (last >= 0 && isOpenBr(s[last])) {
      Pos m;
      if (matchBracketAt(d, {l, last}, m) && m.l > l + 1) { endLine = m.l - 1; return true; }
    }
    if (!isClike(d.lang)) {
      if (d.lang == L_SH) return false;
    } else return false;
  }
  // indentation based (Python, YAML, plain text, HTML)
  int ind = indentWidth(s, d.tabW);
  int e = l;
  for (int j = l + 1; j < n; j++) {
    const wstring& t = d.lines[j];
    if (blankLine(t)) continue;
    if (indentWidth(t, d.tabW) <= ind) break;
    e = j;
  }
  if (e <= l) return false;
  endLine = e;
  return true;
}
static void recomputeHidden(Doc& d) {
  d.hid.assign(d.lines.size(), 0);
  if (d.folded.empty()) return;
  vector<int> drop;
  for (int f : d.folded) {
    int e;
    if (f < 0 || f >= (int)d.lines.size() || !foldRangeOf(d, f, e)) { drop.push_back(f); continue; }
    for (int j = f + 1; j <= e && j < (int)d.lines.size(); j++) d.hid[j] = 1;
  }
  for (int f : drop) d.folded.erase(f);
}
// makes the line visible by opening every fold that hides it
static bool unfoldAround(Doc& d, int l) {
  if (d.folded.empty() || !d.isHidden(l)) return false;
  bool any = false;
  for (auto it = d.folded.begin(); it != d.folded.end();) {
    int e;
    if (*it < l && foldRangeOf(d, *it, e) && e >= l) { it = d.folded.erase(it); any = true; }
    else ++it;
  }
  if (any) { d.ver++; }
  return any;
}

/* ---------------------------------------------------------------- symbols (Goto Symbol, Outline) */
struct Symbol { int line; wstring name, kind; int level = 0; };
static bool startsWithWord(const wstring& s, size_t at, const wchar_t* w) {
  size_t n = wcslen(w);
  return s.compare(at, n, w) == 0 && (at + n >= s.size() || !isIdCh(s[at + n]));
}
static wstring trimW(const wstring& s) {
  size_t a = 0, b = s.size();
  while (a < b && iswspace(s[a])) a++;
  while (b > a && iswspace(s[b - 1])) b--;
  return s.substr(a, b - a);
}
static void collectSymbols(Doc& d, vector<Symbol>& out) {
  out.clear();
  int n = (int)d.lines.size();
  bool inFence = false;
  static const wchar_t* notFn[] = {L"if", L"for", L"while", L"switch", L"return", L"else", L"catch", L"sizeof", L"do", L"case", L"new", L"delete", L"throw", L"defined", L"foreach"};
  for (int l = 0; l < n; l++) {
    const wstring& s = d.lines[l];
    if (s.empty() || s.size() > 400) continue;
    size_t k = 0;
    while (k < s.size() && (s[k] == L' ' || s[k] == L'\t')) k++;
    if (k >= s.size()) continue;
    if (d.lang == L_MD) {
      if (s.compare(k, 3, L"```") == 0 || s.compare(k, 3, L"~~~") == 0) { inFence = !inFence; continue; }
      if (inFence || k > 3 || s[k] != L'#') continue;
      int lvl = 0;
      while (k + lvl < s.size() && s[k + lvl] == L'#') lvl++;
      if (lvl > 6 || k + lvl >= s.size() || s[k + lvl] != L' ') continue;
      wstring t = trimW(s.substr(k + lvl));
      while (!t.empty() && t.back() == L'#') t.pop_back();
      t = trimW(t);
      if (!t.empty()) out.push_back({l, t, L"heading", lvl});
      continue;
    }
    if (d.lang == L_PY) {
      for (const wchar_t* kw : {L"def", L"class", L"async def"}) {
        size_t m = wcslen(kw);
        if (s.compare(k, m, kw) == 0 && k + m < s.size() && s[k + m] == L' ') {
          size_t a = k + m;
          while (a < s.size() && s[a] == L' ') a++;
          size_t b = a;
          while (b < s.size() && isIdCh(s[b])) b++;
          if (b > a) out.push_back({l, s.substr(a, b - a), kw[0] == L'c' ? L"class" : L"function", (int)(k / 4)});
          break;
        }
      }
      continue;
    }
    if (isClike(d.lang) || d.lang == L_SH || d.lang == L_HTML) {
      if (d.lang == L_HTML) continue;
      if (s.compare(k, 2, L"//") == 0 || s[k] == L'*' || s[k] == L'/') continue;
      // keywords that introduce a named type
      bool done = false;
      for (const wchar_t* kw : {L"class", L"struct", L"enum", L"interface", L"namespace", L"trait", L"impl", L"union", L"mod", L"type"}) {
        size_t off = k;
        // skip simple modifiers
        for (const wchar_t* mod : {L"export ", L"public ", L"private ", L"static ", L"abstract ", L"final ", L"pub ", L"default "})
          if (s.compare(off, wcslen(mod), mod) == 0) off += wcslen(mod);
        if (startsWithWord(s, off, kw)) {
          size_t a = off + wcslen(kw);
          while (a < s.size() && s[a] == L' ') a++;
          size_t b = a;
          while (b < s.size() && (isIdCh(s[b]) || s[b] == L':')) b++;
          if (b > a && s.find(L';') == wstring::npos) { out.push_back({l, s.substr(a, b - a), kw, 0}); done = true; }
          break;
        }
      }
      if (done) continue;
      if (s[k] == L'#') {
        if (startsWithWord(s, k + 1, L"define") || s.compare(k, 7, L"#define") == 0) {
          size_t a = k + 7;
          while (a < s.size() && s[a] == L' ') a++;
          size_t b = a;
          while (b < s.size() && isIdCh(s[b])) b++;
          if (b > a) out.push_back({l, s.substr(a, b - a), L"define", 0});
        }
        continue;
      }
      // functions: [modifiers/type] name ( ... ) at brace depth zero (indent at most 1 level)
      size_t paren = s.find(L'(', k);
      if (paren == wstring::npos || paren == k) continue;
      size_t e = paren;
      while (e > k && s[e - 1] == L' ') e--;
      size_t b = e;
      while (b > k && (isIdCh(s[b - 1]) || s[b - 1] == L':' || s[b - 1] == L'~')) b--;
      if (b == e) continue;
      wstring name = s.substr(b, e - b);
      bool bad = false;
      for (auto w : notFn) if (name == w) bad = true;
      if (bad || iswdigit(name[0])) continue;
      // a call or a statement ends with ';' or has an operator before the name
      wstring head = trimW(s.substr(k, b - k));
      if (!head.empty()) {
        wchar_t lc = head.back();
        if (lc == L'=' || lc == L'.' || lc == L'(' || lc == L',' || lc == L'+' || lc == L'-' || lc == L'!' || lc == L'&' || lc == L'|' || lc == L'?' || lc == L':' && head.size() > 1 && head[head.size() - 2] != L':') continue;
        if (head.compare(0, 6, L"return") == 0) continue;
      }
      // the line (or the next ones) must open a body or be a Pawn/AMXX prototype
      bool pawn = head.find(L"public") == 0 || head.find(L"stock") == 0 || head.find(L"native") == 0 || head.find(L"forward") == 0 || head.find(L"static") == 0;
      bool jsfn = startsWithWord(s, k, L"function") || head.find(L"function") != wstring::npos;
      bool body = false;
      if (s.find(L';', paren) == wstring::npos) {
        if (s.find(L'{', paren) != wstring::npos) body = true;
        else if (l + 1 < n) { const wstring& nx = d.lines[l + 1]; size_t q = 0; while (q < nx.size() && (nx[q] == L' ' || nx[q] == L'\t')) q++; if (q < nx.size() && nx[q] == L'{') body = true; }
        if (s.find(L')', paren) == wstring::npos && !pawn && !jsfn) body = body && false;
      }
      if (!body && !pawn && !jsfn) continue;
      if (pawn && s.find(L';') != wstring::npos && head.find(L"native") != 0 && head.find(L"forward") != 0) continue;
      if (!pawn && !jsfn && head.empty() && k > 0 && s.find(L'{') == wstring::npos && body == false) continue;
      out.push_back({l, name, pawn && head.find(L"native") == 0 ? L"native" : L"function", k > 0 ? 1 : 0});
      continue;
    }
    if (d.lang == L_PLAIN) continue;
  }
}
