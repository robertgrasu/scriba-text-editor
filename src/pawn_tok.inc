// pawn_tok.inc: the Pawn / AMX Mod X syntax colouring, following the AMXX-Pawn grammar of the AMXX-Editor package:
// directives (#include path, #define name, #if ...), strings with ^ escapes and %d placeholders, tags (Float:), function
// definitions and calls, keywords, storage words, operators and punctuation, and the constants the analysis knows (dynamic).
// Included by main.cpp before tokenize().

// span kinds of Pawn (0 plain, 2 string, 3 number, 4 comment as in every other language)
enum { AK_PRE = 13, AK_INCPATH, AK_PPCONST, AK_PPGROUP, AK_CONTROL, AK_OP, AK_BRACKET, AK_PAREN, AK_BRACE, AK_COMMA, AK_FNMOD, AK_VARTYPE, AK_TAG,
       AK_ESC, AK_CONSTVAR, AK_FNDEF, AK_FNCALL, AK_BOOL, AK_FLOAT, AK_INVALID };

static std::unordered_set<wstring> g_constSet;        // #define / const / enum names visible from the analysed document
static unsigned long long g_constUid = 0;             // the document they belong to
static const std::unordered_set<wstring>* g_pawnConsts = nullptr;
static const std::unordered_set<wstring> PAWN_CTRL = {L"sizeof", L"tagof", L"charsmax", L"break", L"case", L"continue", L"default", L"do", L"else", L"for", L"goto",
                                                      L"if", L"return", L"state", L"switch", L"while", L"assert", L"sleep"};
static const std::unordered_set<wstring> PAWN_VT = {L"new", L"enum", L"char", L"const", L"static"};
static const std::unordered_set<wstring> PAWN_FM = {L"public", L"stock", L"native", L"forward"};
static const std::unordered_set<wstring> PAWN_BOOL = {L"true", L"false", L"EOS", L"cellmin", L"cellmax", L"cellbits", L"__DATE__", L"__TIME__", L"__LINE__", L"__BINARY__", L"__FILE__"};
static const std::unordered_set<wstring> PAWN_DIRS = {L"if", L"elseif", L"else", L"endif", L"endinput", L"undef", L"endscript", L"error", L"file", L"line", L"emit", L"pragma", L"assert"};

static void tokenizePawn(const wstring& s, int& state, vector<Span>& out) {
  const int n = (int)s.size();
  int i = 0;
  auto push = [&](int a, int b, int k) { if (b > a) out.push_back({a, b, k}); };
  auto isHex = [](wchar_t c) { return iswdigit(c) || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'); };
  int mode = state & 15, depth = state >> 4;  // mode: 0 code, 1 inside /* */, 5 after a "\" of a #define, 6 inside a string that goes on; depth: open (
  bool define = mode == 5;  // a #define line, or the line after a "\" continuation
  auto fin = [&](int md) { state = md | (std::min(depth, 15) << 4); };
  bool cont = false, inStr = mode == 6;
  // a string: ^ escapes and %d placeholders are coloured apart; it goes on in the next line after a "\" at the end of the line
  auto scanString = [&](int j, int seg) -> int {
    while (j < n && s[j] != L'"') {
      if ((s[j] == L'^' || s[j] == L'\\') && j + 1 < n) {
        int len = 2;
        if (s[j] == L'^') {
          wchar_t e = s[j + 1];
          if (e >= L'0' && e <= L'3') { while (len < 4 && j + len < n && iswdigit(s[j + len])) len++; }
          else if (e == L'x') { while (len < 4 && j + len < n && isHex(s[j + len])) len++; }
          else if (e == L'u') { while (len < 6 && j + len < n && isHex(s[j + len])) len++; }
        } else if (!wcschr(L"drywR", s[j + 1])) { j++; continue; }
        push(seg, j, 2);
        push(j, j + len, AK_ESC);
        j += len;
        seg = j;
        continue;
      }
      if (s[j] == L'%') {
        int k = j + 1;
        if (k < n && s[k] == L'-') k++;
        if (k < n && s[k] == L'*') k++; else while (k < n && iswdigit(s[k])) k++;
        if (k < n && s[k] == L'.') { k++; if (k < n && s[k] == L'*') k++; else while (k < n && iswdigit(s[k])) k++; }
        if (k < n && wcschr(L"cbdiufXxasLNn%", s[k])) {
          push(seg, j, 2);
          push(j, k + 1, AK_ESC);
          j = k + 1;
          seg = j;
          continue;
        }
      }
      j++;
    }
    int e = j < n ? j + 1 : n;
    push(seg, e, 2);
    if (j >= n && n > 0 && s[n - 1] == L'\\') cont = true;
    return e;
  };
  if (mode == 6) i = scanString(0, 0);
  if (mode == 1) {  // inside a /* */ comment
    size_t e = s.find(L"*/");
    if (e == wstring::npos) { push(0, n, 4); fin(1); return; }
    push(0, (int)e + 2, 4);
    i = (int)e + 2;
  }
  int first = 0;
  while (first < n && (s[first] == L' ' || s[first] == L'\t')) first++;
  if (i == 0 && !define && !inStr && mode != 1 && first < n && s[first] == L'#') {
    int j = first + 1;
    while (j < n && (s[j] == L' ' || s[j] == L'\t')) j++;
    int w0 = j;
    while (j < n && iswalpha(s[j])) j++;
    wstring word = s.substr(w0, j - w0);
    push(first, j, AK_PRE);
    int p = j;
    while (p < n && (s[p] == L' ' || s[p] == L'\t')) p++;
    if (word == L"include" || word == L"tryinclude") {
      int e = p;
      if (p < n && s[p] == L'"') { size_t q = s.find(L'"', p + 1); e = q == wstring::npos ? n : (int)q + 1; }
      else if (p < n && s[p] == L'<') { size_t q = s.find(L'>', p + 1); e = q == wstring::npos ? n : (int)q + 1; }
      else while (e < n && s[e] != L' ' && s[e] != L'\t' && !(s[e] == L'/' && e + 1 < n && s[e + 1] == L'/')) e++;
      push(p, e, e > p ? AK_INCPATH : AK_INVALID);
      i = e;
    } else if (word == L"define") {
      int e = p;
      if (e < n && (iswalpha(s[e]) || s[e] == L'_')) while (e < n && s[e] != L' ' && s[e] != L'\t') e++;
      push(p, e, AK_PRE);
      i = e;
      define = true;
    } else if (PAWN_DIRS.count(word)) {
      int e = p;
      while (e < n && !(s[e] == L'/' && e + 1 < n && (s[e + 1] == L'/' || s[e + 1] == L'*'))) e++;
      while (e > p && (s[e - 1] == L' ' || s[e - 1] == L'\t')) e--;
      push(p, e, AK_PPCONST);
      i = e;
    } else {
      push(j, n, AK_INVALID);
      i = n;
    }
  }
  while (i < n) {
    wchar_t c = s[i];
    if (c == L' ' || c == L'\t') { i++; continue; }
    if (c == L'/' && i + 1 < n && s[i + 1] == L'/') { push(i, n, 4); i = n; break; }
    if (c == L'/' && i + 1 < n && s[i + 1] == L'*') {
      size_t e = s.find(L"*/", i + 2);
      if (e == wstring::npos) { push(i, n, 4); fin(1); return; }
      push(i, (int)e + 2, 4);
      i = (int)e + 2;
      continue;
    }
    if (c == L'"') { i = scanString(i + 1, i); continue; }
    if (c == L'\'') {  // character: 'a' or '^n'
      if (i + 3 < n && s[i + 1] == L'^' && s[i + 3] == L'\'') { push(i, i + 4, 2); i += 4; continue; }
      if (i + 2 < n && s[i + 2] == L'\'') { push(i, i + 3, 2); i += 3; continue; }
      i++;
      continue;
    }
    if (iswdigit(c)) {
      int j = i;
      while (j < n && iswdigit(s[j])) j++;
      if (j + 2 < n && s[j] == L'.' && s[j + 1] == L'.' && iswdigit(s[j + 2])) {  // 1..5 (case range)
        int k = j + 2;
        while (k < n && iswdigit(s[k])) k++;
        push(i, j, 3); push(j, j + 2, AK_OP); push(j + 2, k, 3);
        i = k;
        continue;
      }
      if (c == L'0' && j == i + 1 && j < n && (s[j] == L'x' || s[j] == L'X')) {
        j++;
        while (j < n && isHex(s[j])) j++;
        push(i, j, 3); i = j; continue;
      }
      if (j + 1 < n && s[j] == L'.' && iswdigit(s[j + 1])) {
        j++;
        while (j < n && iswdigit(s[j])) j++;
        push(i, j, AK_FLOAT); i = j; continue;
      }
      while (j < n && iswalnum(s[j])) j++;
      push(i, j, 3); i = j;
      continue;
    }
    if (iswalpha(c) || c == L'_') {
      int j = i + 1;
      while (j < n && (iswalnum(s[j]) || s[j] == L'_')) j++;
      wstring w = s.substr(i, j - i);
      int p = j;
      while (p < n && (s[p] == L' ' || s[p] == L'\t')) p++;
      bool callNext = p < n && s[p] == L'(';
      if (j + 1 < n && s[j] == L':' && s[j + 1] == L':') { push(i, j + 2, 3); i = j + 2; continue; }
      if (w == L"enum") {  // enum Name / enum _:name
        push(i, j, AK_CONTROL);
        if (p < n && (iswalpha(s[p]) || s[p] == L'_')) {
          int e = p;
          while (e < n && (iswalnum(s[e]) || s[e] == L'_')) e++;
          push(p, e, AK_TAG);
          j = e;
        }
        i = j;
        continue;
      }
      if (PAWN_CTRL.count(w)) { push(i, j, AK_CONTROL); i = j; continue; }
      if (PAWN_VT.count(w)) { push(i, j, AK_VARTYPE); i = j; continue; }
      if (PAWN_BOOL.count(w)) { push(i, j, AK_BOOL); i = j; continue; }
      if (PAWN_FM.count(w)) {
        bool lead = true;  // only blanks and ";" before it: this can be a function definition
        for (int k = 0; k < i; k++) if (s[k] != L' ' && s[k] != L'\t' && s[k] != L';') lead = false;
        push(i, j, AK_FNMOD);
        i = j;
        if (lead) {  // [Tag:] name (
          int q = p, tagEnd = -1;
          while (q < n && (iswalnum(s[q]) || s[q] == L'_')) q++;
          if (q > p && q < n && s[q] == L':' && !(q + 1 < n && s[q + 1] == L':')) { tagEnd = q + 1; }
          int nm = tagEnd >= 0 ? tagEnd : p;
          while (nm < n && (s[nm] == L' ' || s[nm] == L'\t')) nm++;
          int ne = nm;
          while (ne < n && (iswalnum(s[ne]) || s[ne] == L'_')) ne++;
          int pp = ne;
          while (pp < n && (s[pp] == L' ' || s[pp] == L'\t')) pp++;
          if (ne > nm && pp < n && s[pp] == L'(') {
            if (tagEnd >= 0) push(p, tagEnd, AK_TAG);
            push(nm, ne, AK_FNDEF);
            push(pp, pp + 1, AK_PAREN);
            depth++;
            i = pp + 1;
          }
        }
        continue;
      }
      if (j < n && s[j] == L':' && !(j + 1 < n && s[j + 1] == L':') && w != L"default") {  // tag: Float: bool: any:
        int q = i - 1;
        while (q >= 0 && (s[q] == L' ' || s[q] == L'\t')) q--;
        bool afterCase = q >= 3 && s.compare(q - 3, 4, L"case") == 0 && (q < 4 || !(iswalnum(s[q - 4]) || s[q - 4] == L'_'));
        bool ternary = s.find(L'?') != wstring::npos && (int)s.find(L'?') < i;
        if (!afterCase && !ternary) { push(i, j + 1, AK_TAG); i = j + 1; continue; }
      }
      if (callNext) {
        bool def = false;  // `name(` at the start of the line (after an optional word and tag): a definition
        if (i == 0) def = true;
        else if (!iswspace(s[0])) {
          def = true;
          for (int k = 0; k < i; k++) if (!(iswalnum(s[k]) || s[k] == L'_' || s[k] == L' ' || s[k] == L'\t' || s[k] == L':')) def = false;
          for (auto& sp : out) if (sp.kind != AK_TAG && sp.kind != 0) def = false;
        }
        push(i, j, def ? AK_FNDEF : AK_FNCALL);
        push(p, p + 1, AK_PAREN);
        depth++;
        i = p + 1;
        continue;
      }
      if (g_pawnConsts && g_pawnConsts->count(w)) push(i, j, AK_CONSTVAR);
      i = j;
      continue;
    }
    if (c == L'%' && i + 1 < n && iswdigit(s[i + 1])) { push(i, i + 2, AK_PPGROUP); i += 2; continue; }
    if (c == L'(') { depth++; push(i, i + 1, AK_PAREN); i++; continue; }
    if (c == L')') { if (depth > 0) { depth--; push(i, i + 1, AK_PAREN); } else push(i, i + 1, AK_INVALID); i++; continue; }
    if (c == L'[' || c == L']') { push(i, i + 1, AK_BRACKET); i++; continue; }
    if (c == L'{' || c == L'}') { push(i, i + 1, AK_BRACE); i++; continue; }
    if (c == L',' || c == L';' || c == L'.') { push(i, i + 1, AK_COMMA); i++; continue; }
    if (wcschr(L"=<>!+-*/%&|^~?:", c)) {
      int j = i + 1;
      while (j < n && wcschr(L"=<>!+-*/%&|^~?:", s[j]) && !(s[j] == L'/' && j + 1 < n && (s[j + 1] == L'/' || s[j + 1] == L'*'))) j++;
      push(i, j, AK_OP);
      i = j;
      continue;
    }
    i++;
  }
  size_t e = s.find_last_not_of(L" \t");
  fin(cont ? 6 : (define && e != wstring::npos && s[e] == L'\\') ? 5 : 0);  // a string or the #define goes on in the next line
}
