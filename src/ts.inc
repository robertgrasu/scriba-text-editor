// ts.inc: syntax colouring with tree-sitter (MIT): a real parse tree of the document (kept up to date by edits, so typing re-parses only what changed)
// and the highlight queries of each grammar. The captures of the queries (@keyword, @string, @function ...) become the span kinds of the editor.
// Included by main.cpp after doc_helpers.inc.
#include "ts_queries.inc"
#include "ts_table.inc"

struct TsQueryPred { int type = 0; bool neg = false; int cap = -1, cap2 = -1; vector<wstring> strs; std::shared_ptr<std::wregex> re; };  // 1 eq, 2 match, 3 any-of
struct TsLoaded {
  bool tried = false;
  TSQuery* q = nullptr;
  vector<unsigned char> kind;                // capture index -> span kind
  vector<vector<TsQueryPred>> preds;         // per pattern
};
static TsLoaded g_tsLoaded[sizeof(TS_LANGS) / sizeof(TS_LANGS[0])];
static TSParser* g_tsParser = nullptr;
static TSQueryCursor* g_tsCursor = nullptr;
static wstring g_tsLog;  // what went wrong while loading the queries (shown by the test driver)

static int tsForPath(const wstring& p) {
  size_t d = p.find_last_of(L'.');
  if (d == wstring::npos) return -1;
  wstring e = p.substr(d);
  for (auto& c : e) c = (wchar_t)towlower(c);
  if (e == L".h") e = L".cpp";  // headers are C++ as often as C: the C++ grammar reads both
  for (size_t i = 0; i < sizeof(TS_LANGS) / sizeof(TS_LANGS[0]); i++)
    for (auto x : TS_LANGS[i].exts) if (e == x) return (int)i;
  return -1;
}
static int tsFindName(const wchar_t* name) {
  for (size_t i = 0; i < sizeof(TS_LANGS) / sizeof(TS_LANGS[0]); i++) if (wcscmp(TS_LANGS[i].name, name) == 0) return (int)i;
  return -1;
}
// the kind of span (colour) for the name of a capture
static unsigned char tsCaptureKind(const std::string& n) {
  auto is = [&](const char* p) { size_t l = strlen(p); return n.compare(0, l, p) == 0 && (n.size() == l || n[l] == '.'); };
  if (is("comment")) return 4;
  if (is("string.escape") || is("escape")) return 2;
  if (is("string") || is("character")) return 2;
  if (is("keyword") || is("conditional") || is("repeat") || is("include") || is("storageclass") || is("exception") || is("define") || is("preproc") || is("variable.builtin")) return 1;
  if (is("number") || is("float") || is("boolean") || is("constant") || is("constant.builtin")) return 3;
  if (is("function") || is("method") || is("macro") || is("function.call")) return 5;
  if (is("constructor") || is("type") || is("namespace") || is("module") || is("class")) return 6;
  if (is("tag.error")) return 12;
  if (is("tag.attribute") || is("attribute") || is("property") || is("field") || is("variable.member")) return 9;
  if (is("tag")) return 7;
  if (is("operator")) return 10;
  if (is("markup.heading") || is("text.title") || is("label")) return 8;
  if (is("markup.link") || is("text.uri") || is("link")) return 11;
  return 0;
}
static wstring tsFromUtf8(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
  return w;
}
static TsLoaded& tsLoad(int li) {
  TsLoaded& L = g_tsLoaded[li];
  if (L.tried) return L;
  L.tried = true;
  if (!g_tsParser) { g_tsParser = ts_parser_new(); g_tsCursor = ts_query_cursor_new(); }
  const TsLangDef& def = TS_LANGS[li];
  uint32_t errOff = 0;
  TSQueryError err = TSQueryErrorNone;
  TSQuery* q = ts_query_new(def.fn(), (const char*)def.q, (uint32_t)def.qn, &errOff, &err);
  if (!q) {
    g_tsLog += tsFromUtf8(def.id) + L": query error " + std::to_wstring((int)err) + L" at " + std::to_wstring(errOff) + L"\n";
    return L;
  }
  L.q = q;
  uint32_t nc = ts_query_capture_count(q);
  for (uint32_t i = 0; i < nc; i++) {
    uint32_t ln = 0;
    const char* nm = ts_query_capture_name_for_id(q, i, &ln);
    L.kind.push_back(tsCaptureKind(std::string(nm, ln)));
  }
  uint32_t np = ts_query_pattern_count(q);
  L.preds.resize(np);
  for (uint32_t p = 0; p < np; p++) {
    uint32_t sc = 0;
    const TSQueryPredicateStep* st = ts_query_predicates_for_pattern(q, p, &sc);
    uint32_t i = 0;
    while (i < sc) {
      uint32_t j = i;
      while (j < sc && st[j].type != TSQueryPredicateStepTypeDone) j++;
      if (j > i && st[i].type == TSQueryPredicateStepTypeString) {
        uint32_t ln = 0;
        const char* ov = ts_query_string_value_for_id(q, st[i].value_id, &ln);
        std::string op(ov, ln);
        TsQueryPred pr;
        bool neg = op.compare(0, 4, "not-") == 0;
        std::string base = neg ? op.substr(4) : op;
        if (base == "eq?") pr.type = 1;
        else if (base == "match?" || base == "lua-match?" || base == "vim-match?") pr.type = 2;
        else if (base == "any-of?") pr.type = 3;
        pr.neg = neg;
        if (pr.type) {
          for (uint32_t k = i + 1; k < j; k++) {
            if (st[k].type == TSQueryPredicateStepTypeCapture) { if (pr.cap < 0) pr.cap = (int)st[k].value_id; else pr.cap2 = (int)st[k].value_id; }
            else {
              uint32_t sl = 0;
              const char* sv = ts_query_string_value_for_id(q, st[k].value_id, &sl);
              pr.strs.push_back(tsFromUtf8(std::string(sv, sl)));
            }
          }
          if (pr.type == 2 && !pr.strs.empty()) {
            try { pr.re = std::make_shared<std::wregex>(pr.strs[0], std::regex_constants::ECMAScript); } catch (...) { pr.type = 0; }
          }
          if (pr.type) L.preds[p].push_back(pr);
        }
      }
      i = j + 1;
    }
  }
  return L;
}

/* ---------------------------------------------------------------- the parse tree of a document */
struct TsInputCtx { Doc* d; };
static const char* tsRead(void* payload, uint32_t byteIndex, TSPoint pos, uint32_t* bytesRead) {
  static const wchar_t nl = L'\n';
  Doc& d = *((TsInputCtx*)payload)->d;
  (void)byteIndex;
  uint32_t row = pos.row, col = pos.column / 2;
  if (row >= d.lines.size()) { *bytesRead = 0; return ""; }
  const wstring& s = d.lines[row];
  if (col < s.size()) { *bytesRead = (uint32_t)((s.size() - col) * 2); return (const char*)(s.data() + col); }
  if (row + 1 < d.lines.size() && col == s.size()) { *bytesRead = 2; return (const char*)&nl; }
  *bytesRead = 0;
  return "";
}
static const wchar_t* tsNameOf(int lang) { return TS_LANGS[lang].name; }
static bool tsUsable(const Doc& d) {
  if (d.tsLang < 0 || d.tsOff) return false;
  return d.lines.size() <= 150000;
}
static void tsEnsure(Doc& d) {
  if (d.tsVerParsed == d.ver && d.tsTree.t) return;
  TsLoaded& L = tsLoad(d.tsLang);
  if (!L.q) { d.tsOff = true; return; }
  ts_parser_set_language(g_tsParser, TS_LANGS[d.tsLang].fn());
  TsInputCtx ctx{&d};
  TSInput in{&ctx, tsRead, TSInputEncodingUTF16LE, nullptr};
  TSTree* t = ts_parser_parse(g_tsParser, d.tsTree.t, in);
  ts_parser_reset(g_tsParser);
  if (d.tsTree.t) ts_tree_delete(d.tsTree.t);
  d.tsTree.t = t;
  d.tsVerParsed = d.tsSyncVer = d.ver;
  d.tsSpans.assign(d.lines.size(), {});
  d.tsOk.assign(d.lines.size(), 0);
}
// called by Doc::replaceRange before the text changes: the tree learns what was replaced, so the next parse reuses what is left
static void tsEditPre(Doc& d, Pos a, Pos b, const wstring& text) {
  if (!d.tsTree.t) return;
  if (d.tsSyncVer != d.ver) { ts_tree_delete(d.tsTree.t); d.tsTree.t = nullptr; return; }  // the tree is not the text any more
  uint32_t off = 0;
  for (int i = 0; i < a.l && i < (int)d.lines.size(); i++) off += (uint32_t)d.lines[i].size() + 1;
  uint32_t startChar = off + a.c;
  uint32_t oldEnd = startChar;
  {
    uint32_t o2 = 0;
    for (int i = 0; i < b.l && i < (int)d.lines.size(); i++) o2 += (uint32_t)d.lines[i].size() + 1;
    oldEnd = o2 + b.c;
  }
  uint32_t nl = 0, lastLen = 0;
  for (wchar_t c : text) { if (c == L'\n') { nl++; lastLen = 0; } else lastLen++; }
  TSInputEdit e;
  e.start_byte = startChar * 2;
  e.old_end_byte = oldEnd * 2;
  e.new_end_byte = (startChar + (uint32_t)text.size()) * 2;
  e.start_point = {(uint32_t)a.l, (uint32_t)a.c * 2};
  e.old_end_point = {(uint32_t)b.l, (uint32_t)b.c * 2};
  e.new_end_point = nl ? TSPoint{(uint32_t)a.l + nl, lastLen * 2} : TSPoint{(uint32_t)a.l, (uint32_t)(a.c + text.size()) * 2};
  ts_tree_edit(d.tsTree.t, &e);
  d.tsKeep = true;
}
static wstring tsNodeText(const Doc& d, TSNode n) {
  TSPoint a = ts_node_start_point(n), b = ts_node_end_point(n);
  if (a.row != b.row || a.row >= d.lines.size()) return L"";
  const wstring& s = d.lines[a.row];
  size_t c0 = a.column / 2, c1 = b.column / 2;
  return c1 <= s.size() && c0 <= c1 ? s.substr(c0, c1 - c0) : L"";
}
static bool tsPredsOk(const Doc& d, const TsLoaded& L, const TSQueryMatch& m) {
  if (m.pattern_index >= L.preds.size()) return true;
  for (auto& pr : L.preds[m.pattern_index]) {
    auto capText = [&](int id, wstring& out) {
      for (uint16_t i = 0; i < m.capture_count; i++) if ((int)m.captures[i].index == id) { out = tsNodeText(d, m.captures[i].node); return true; }
      return false;
    };
    wstring t;
    if (!capText(pr.cap, t)) continue;
    bool ok = true;
    if (pr.type == 1) {
      wstring other = pr.cap2 >= 0 ? L"" : (pr.strs.empty() ? L"" : pr.strs[0]);
      if (pr.cap2 >= 0) capText(pr.cap2, other);
      ok = t == other;
    } else if (pr.type == 2) ok = pr.re && std::regex_search(t, *pr.re);
    else if (pr.type == 3) { ok = false; for (auto& s : pr.strs) if (s == t) ok = true; }
    if (pr.neg) ok = !ok;
    if (!ok) return false;
  }
  return true;
}
// colours of the lines [l0, l1): one pass of the highlight query over that part of the tree
static void tsHighlightBlock(Doc& d, int l0, int l1) {
  TsLoaded& L = tsLoad(d.tsLang);
  if (!L.q || !d.tsTree.t) return;
  l1 = std::min(l1, (int)d.lines.size());
  int n = l1 - l0;
  vector<vector<unsigned char>> kind(n);
  vector<vector<int>> prio(n);
  for (int i = 0; i < n; i++) { kind[i].assign(d.lines[l0 + i].size(), 0); prio[i].assign(d.lines[l0 + i].size(), -1); }
  ts_query_cursor_set_point_range(g_tsCursor, {(uint32_t)l0, 0}, {(uint32_t)l1, 0});
  ts_query_cursor_exec(g_tsCursor, L.q, ts_tree_root_node(d.tsTree.t));
  TSQueryMatch m;
  while (ts_query_cursor_next_match(g_tsCursor, &m)) {
    if (!tsPredsOk(d, L, m)) continue;
    for (uint16_t ci = 0; ci < m.capture_count; ci++) {
      unsigned char k = L.kind[m.captures[ci].index];
      TSNode nd = m.captures[ci].node;
      TSPoint a = ts_node_start_point(nd), b = ts_node_end_point(nd);
      int pi = (int)m.pattern_index;
      for (int row = (int)a.row; row <= (int)b.row; row++) {
        if (row < l0 || row >= l1) continue;
        int r = row - l0, size = (int)kind[r].size();
        int c0 = row == (int)a.row ? (int)a.column / 2 : 0, c1 = row == (int)b.row ? (int)b.column / 2 : size;
        c1 = std::min(c1, size);
        for (int c = c0; c < c1; c++) if (pi >= prio[r][c]) { prio[r][c] = pi; kind[r][c] = k; }
      }
    }
  }
  for (int i = 0; i < n; i++) {
    vector<Span>& out = d.tsSpans[l0 + i];
    out.clear();
    const vector<unsigned char>& kk = kind[i];
    int c = 0, sz = (int)kk.size();
    while (c < sz) {
      int e = c + 1;
      while (e < sz && kk[e] == kk[c]) e++;
      if (kk[c]) out.push_back({c, e, kk[c]});
      c = e;
    }
    d.tsOk[l0 + i] = 1;
  }
}
// the spans of a line; false when the document has no tree-sitter grammar (the line is then coloured by the simple tokenizer)
static bool tsSpansOf(Doc& d, int line, vector<Span>& out) {
  if (!tsUsable(d)) return false;
  tsEnsure(d);
  if (d.tsOff || !d.tsTree.t || line < 0 || line >= (int)d.tsOk.size()) return false;
  if (!d.tsOk[line]) {
    int a = line - line % 48;
    tsHighlightBlock(d, a, a + 48);
  }
  out = d.tsSpans[line];
  return true;
}
