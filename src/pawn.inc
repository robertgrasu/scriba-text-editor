// pawn.inc: Pawn / AMX Mod X code analysis: functions, natives, forwards, #define macros, constants, enums and global variables,
// with the comment above each one as its documentation, and #include resolution across files (a port of src/pawnparse.js).
// Included by main.cpp before the AMX Mod X features.

struct PawnLocal { wstring name, sig; int line; };
struct PawnSym {
  wstring name, kind, sig, doc, file, parent;
  int line = 0, pos = 0, endLine = 0;
  vector<wstring> params;
  vector<PawnLocal> locals;
  bool entered = false;
};
struct PawnInclude { wstring name; bool angle; int line; };
struct PawnParsed { vector<PawnSym> symbols; vector<PawnInclude> includes; };

static const wchar_t* PAWN_IDENT = L"[A-Za-z_@][\\w@]*";
static const std::unordered_set<wstring> PAWN_KEYS = {
    L"if", L"else", L"for", L"while", L"do", L"switch", L"case", L"default", L"return", L"break", L"continue", L"sizeof", L"tagof", L"charsmax", L"new",
    L"static", L"const", L"public", L"stock", L"native", L"forward", L"enum", L"goto", L"sleep", L"exit", L"assert", L"state", L"operator", L"defined"};
static wstring wnorm(const wstring& s) {  // runs of white space become one space
  wstring r;
  bool sp = false;
  for (wchar_t c : s) {
    if (iswspace(c)) { sp = true; continue; }
    if (sp && !r.empty()) r += L' ';
    sp = false;
    r += c;
  }
  return r;
}
struct PawnComment { int line, endLine, kind; wstring text; };  // kind 0 line comment, 1 block
// comments become spaces (offsets and newlines are kept), and structural characters inside strings are neutralised
static wstring pawnClean(const wstring& text, vector<PawnComment>& comments) {
  size_t n = text.size();
  wstring out(n, L' ');
  int line = 1;
  size_t i = 0;
  auto blank = [&](size_t a, size_t b) { for (size_t k = a; k < b; k++) out[k] = text[k] == L'\n' ? L'\n' : L' '; };
  while (i < n) {
    wchar_t c = text[i], d = i + 1 < n ? text[i + 1] : 0;
    if (c == L'/' && d == L'/') {
      size_t j = text.find(L'\n', i);
      if (j == wstring::npos) j = n;
      comments.push_back({line, line, 0, text.substr(i + 2, j - i - 2)});
      blank(i, j);
      i = j;
    } else if (c == L'/' && d == L'*') {
      size_t j = text.find(L"*/", i + 2);
      j = j == wstring::npos ? n : j + 2;
      wstring body = text.substr(i, j - i);
      int nl = (int)std::count(body.begin(), body.end(), L'\n');
      size_t a = 0;
      while (a < body.size() && body[a] == L'/') a++;
      while (a < body.size() && body[a] == L'*') a++;
      size_t b = body.size();
      while (b > a && body[b - 1] == L'/') b--;
      while (b > a && body[b - 1] == L'*') b--;
      comments.push_back({line, line + nl, 1, body.substr(a, b > a ? b - a : 0)});
      blank(i, j);
      line += nl;
      i = j;
    } else if (c == L'"' || c == L'\'') {
      out[i] = c;
      size_t j = i + 1;
      while (j < n && text[j] != c && text[j] != L'\n') {
        if (text[j] == L'^' || text[j] == L'\\') { out[j] = L'_'; j++; if (j < n && text[j] != L'\n') { out[j] = L'_'; j++; } continue; }
        out[j] = wcschr(L"{}()[];,=", text[j]) ? L'_' : text[j];
        j++;
      }
      if (j < n && text[j] == c) { out[j] = c; j++; }
      i = j;
    } else {
      out[i] = c;
      if (c == L'\n') line++;
      i++;
    }
  }
  return out;
}
static vector<wstring> splitNl(const wstring& s) {
  vector<wstring> r;
  size_t p = 0;
  while (true) {
    size_t q = s.find(L'\n', p);
    if (q == wstring::npos) { r.push_back(s.substr(p)); break; }
    r.push_back(s.substr(p, q - p));
    p = q + 1;
  }
  return r;
}
static int braceDelta(const wstring& s) {
  int d = 0;
  for (wchar_t c : s) { if (c == L'{') d++; else if (c == L'}') d--; }
  return d;
}
// the (...) group that starts at the first "(" of s, plus the index after it
static bool parenGroup(const wstring& s, wstring& inner, size_t& end) {
  size_t a = s.find(L'(');
  if (a == wstring::npos) return false;
  int depth = 0;
  for (size_t i = a; i < s.size(); i++) {
    if (s[i] == L'(') depth++;
    else if (s[i] == L')') { depth--; if (depth == 0) { inner = s.substr(a + 1, i - a - 1); end = i + 1; return true; } }
  }
  return false;
}
static wstring tidyDoc(const PawnComment& c) {
  vector<wstring> ls = splitNl(c.text);
  wstring r;
  for (size_t i = 0; i < ls.size(); i++) {
    wstring l = ls[i];
    size_t k = 0;
    while (k < l.size() && iswspace(l[k])) k++;
    size_t m = k;
    while (m < l.size() && l[m] == L'*') m++;
    if (m > k) { if (m < l.size() && l[m] == L' ') m++; l = l.substr(m); }
    else l = l;
    if (blankLine(l)) l.clear();
    if (i) r += L"\n";
    r += l;
  }
  return trimW(r);
}

static PawnParsed parsePawn(wstring text) {
  PawnParsed out;
  // CRLF -> LF
  { wstring t; for (size_t i = 0; i < text.size(); i++) { if (text[i] == L'\r') { if (i + 1 < text.size() && text[i + 1] == L'\n') continue; t += L'\n'; } else t += text[i]; } text = t; }
  vector<PawnComment> comments;
  wstring code = pawnClean(text, comments);
  vector<wstring> lines = splitNl(code), rawLines = splitNl(text);
  vector<int> offsets;
  { int o = 0; for (auto& l : rawLines) { offsets.push_back(o); o += (int)l.size() + 1; } }
  auto lineAt = [&](int off) {
    int lo = 0, hi = (int)offsets.size() - 1;
    while (lo < hi) { int m = (lo + hi + 1) / 2; if (offsets[m] <= off) lo = m; else hi = m - 1; }
    return lo + 1;
  };
  std::map<int, const PawnComment*> byEnd, byLine;
  for (auto& c : comments) { if (c.kind == 1) byEnd[c.endLine] = &c; else byLine[c.line] = &c; }
  auto docFor = [&](int line) {
    vector<wstring> parts;
    auto b = byEnd.find(line - 1);
    if (b != byEnd.end()) parts.push_back(tidyDoc(*b->second));
    else {
      for (int l = line - 1; l >= 1 && byLine.count(l) && blankLine(lines[l - 1]); l--) parts.insert(parts.begin(), trimW(byLine[l]->text));
    }
    if (parts.empty() && byLine.count(line) && !blankLine(lines[line - 1])) parts.push_back(trimW(byLine[line]->text));
    wstring r;
    for (size_t i = 0; i < parts.size(); i++) { if (i) r += L"\n"; r += parts[i]; }
    return trimW(r);
  };
  auto posOf = [&](int line, const wstring& name) {
    size_t i = rawLines[line - 1].find(name);
    return offsets[line - 1] + (i == wstring::npos ? 0 : (int)i);
  };
  wstring IDENT = PAWN_IDENT, TAG = L"(?:" + IDENT + L"\\s*:\\s*)?";
  static const std::wregex reMods(L"^\\s*((?:(?:public|stock|native|forward|static)\\s+)+)");
  // names declared by "new a, Float:b[3] = {...}, c" (the text after the modifiers)
  struct DeclName { wstring name, text; };
  std::wregex reDecl(L"^\\s*" + TAG + L"(" + IDENT + L")\\s*(\\[[^\\]]*\\])*\\s*(?:=\\s*(.*))?$");
  auto declNames = [&](wstring rest) {
    size_t semi = rest.find(L';');
    if (semi != wstring::npos) rest = rest.substr(0, semi);
    vector<wstring> parts;
    int d = 0;
    wstring cur;
    for (wchar_t ch : rest) {
      if (wcschr(L"([{", ch)) d++;
      else if (wcschr(L")]}", ch)) d--;
      if (ch == L',' && d == 0) { parts.push_back(cur); cur.clear(); } else cur += ch;
    }
    if (!trimW(cur).empty()) parts.push_back(cur);
    vector<DeclName> res;
    for (size_t i = 0; i < parts.size() && i < 40; i++) {
      std::wsmatch m;
      wstring p = wnorm(parts[i]);
      if (std::regex_match(p, m, reDecl)) res.push_back({m[1].str(), p.substr(0, 90)});
    }
    return res;
  };
  std::wregex reParam(L"(?:^|\\s|&)(?:const\\s+)?(?:" + IDENT + L"\\s*:\\s*)?&?(" + IDENT + L")\\s*(?:\\[[^\\]]*\\])*\\s*(?:=.*)?$");
  auto paramNames = [&](const wstring& params) {
    vector<wstring> res;
    int d = 0;
    wstring cur;
    auto flush = [&]() {
      wstring t = trimW(cur);
      std::wsmatch m;
      if (t != L"..." && std::regex_search(t, m, reParam)) res.push_back(m[1].str());
      cur.clear();
    };
    for (wchar_t ch : params) {
      if (wcschr(L"([{", ch)) d++;
      else if (wcschr(L")]}", ch)) d--;
      if (ch == L',' && d == 0) flush(); else cur += ch;
    }
    if (!trimW(cur).empty()) flush();
    return res;
  };

  int depth = 0;
  int curFn = -1;  // index of the function whose body we are in
  vector<bool> cond;  // #if stack: only the first branch of each is analysed (like AMXXEditorV4)
  std::wregex reDirective(L"^\\s*#\\s*(if|ifdef|ifndef|elseif|else|endif)\\b");
  std::wregex reInclude(L"^\\s*#\\s*(?:try)?include\\s*(?:<([^>]+)>|\"([^\"]+)\")");
  std::wregex reDefine(L"^\\s*#\\s*define\\s+(" + IDENT + L")(\\([^)]*\\))?\\s*(.*)$");
  std::wregex reEnum(L"^\\s*enum\\b\\s*(?:(" + IDENT + L")\\s*:\\s*)?(" + IDENT + L")?\\s*(\\([^)]*\\))?");
  std::wregex reEnumMember(L"^\\s*" + TAG + L"(" + IDENT + L")\\s*(\\[[^\\]]*\\])?\\s*(?:=\\s*(.+?))?\\s*$");
  std::wregex reFnMod(L"^\\s*(?:(?:public|stock|native|forward|static)\\s+)+(?:const\\s+)?" + TAG + L"(?:\\[[^\\]]*\\]\\s*)?(" + IDENT + L")\\s*\\(");
  std::wregex rePlain(L"^\\s*" + TAG + L"(" + IDENT + L")\\s*\\(");
  std::wregex reVarMods(L"^\\s*((?:(?:new|static|stock|public|const)\\s+)+)");
  std::wregex reLocalMods(L"^\\s*(?:\\{\\s*)?((?:(?:new|static|const|stock)\\s+)+)");
  std::wregex reNextDecl(L"^\\s*(#|public|stock|native|forward|new)\\b");
  auto& syms = out.symbols;
  for (size_t i = 0; i < lines.size(); i++) {
    const wstring& ln = lines[i];
    wstring t = trimW(ln);
    if (t.empty()) continue;
    int lineNo = (int)i + 1;
    std::wsmatch m;
    if (t[0] == L'#') {
      if (std::regex_search(ln, m, reDirective)) {
        wstring w = m[1].str();
        if (w == L"if" || w == L"ifdef" || w == L"ifndef") cond.push_back(false);
        else if (w == L"elseif" || w == L"else") { if (!cond.empty()) cond.back() = true; }
        else if (!cond.empty()) cond.pop_back();
        continue;
      }
    }
    bool skip = false;
    for (bool b : cond) if (b) skip = true;
    if (skip) continue;

    if (depth == 0 && t[0] == L'#') {
      wstring stmt = ln;
      size_t j = i;
      auto endsBackslash = [](const wstring& s) { size_t e = s.size(); while (e > 0 && iswspace(s[e - 1])) e--; return e > 0 && s[e - 1] == L'\\'; };
      while (endsBackslash(lines[j]) && j + 1 < lines.size()) {
        j++;
        size_t e = stmt.size();
        while (e > 0 && iswspace(stmt[e - 1])) e--;
        stmt = stmt.substr(0, e - 1) + L" " + lines[j];
      }
      if (std::regex_search(stmt, m, reInclude)) {
        bool angle = m[1].matched;
        out.includes.push_back({trimW(angle ? m[1].str() : m[2].str()), angle, lineNo});
      } else if (std::regex_search(stmt, m, reDefine)) {
        bool macro = m[2].matched && m[2].length() > 0;
        wstring value = wnorm(m[3].str());
        PawnSym s;
        s.name = m[1].str();
        s.kind = macro ? L"macro" : L"define";
        s.line = lineNo;
        s.pos = posOf(lineNo, s.name);
        s.sig = L"#define " + s.name + (macro ? m[2].str() : L"") + (value.empty() ? L"" : L" " + value.substr(0, 80));
        s.doc = docFor(lineNo);
        syms.push_back(s);
      }
      i = j;
      continue;
    }

    if (depth == 0) {
      // enum [Tag:] [Name] [(step)] { A, B = 2, Tag:C }
      if (std::regex_search(ln, m, reEnum) && trimW(ln).compare(0, 4, L"enum") == 0) {
        wstring tag = m[1].matched ? m[1].str() : L"", nm = m[2].matched ? m[2].str() : L"";
        size_t startOff = offsets[i] + ln.find(L"enum");
        size_t open = code.find(L'{', startOff), close = wstring::npos;
        if (open != wstring::npos) {
          int d = 0;
          for (size_t k = open; k < code.size(); k++) {
            if (code[k] == L'{') d++;
            else if (code[k] == L'}') { d--; if (d == 0) { close = k; break; } }
          }
        }
        if (open != wstring::npos && close != wstring::npos && close > open) {
          wstring ename = !nm.empty() ? nm : tag;
          if (!ename.empty()) {
            PawnSym s;
            s.name = ename;
            s.kind = L"enum";
            s.line = lineNo;
            s.pos = posOf(lineNo, ename);
            s.sig = trimW(L"enum " + (tag.empty() ? L"" : tag + L":") + nm);
            s.doc = docFor(lineNo);
            syms.push_back(s);
          }
          wstring body = code.substr(open + 1, close - open - 1);
          size_t pieceStart = 0;
          int paren = 0;
          vector<std::pair<size_t, wstring>> pieces;
          for (size_t k = 0; k <= body.size(); k++) {
            wchar_t ch = k < body.size() ? body[k] : 0;
            if (ch == L'(' || ch == L'[') paren++;
            else if (ch == L')' || ch == L']') paren--;
            if (k == body.size() || (ch == L',' && paren == 0)) { pieces.push_back({pieceStart, body.substr(pieceStart, k - pieceStart)}); pieceStart = k + 1; }
          }
          for (auto& pc : pieces) {
            wstring p = wnorm(pc.second);
            std::wsmatch mm;
            if (!std::regex_match(p, mm, reEnumMember)) continue;
            size_t lead = pc.second.size() - (pc.second.find_first_not_of(L" \t\r\n") == wstring::npos ? pc.second.size() : pc.second.size() - pc.second.find_first_not_of(L" \t\r\n"));
            (void)lead;
            size_t firstNon = pc.second.find_first_not_of(L" \t\r\n");
            int mLine = lineAt((int)(open + 1 + pc.first + (firstNon == wstring::npos ? 0 : firstNon)));
            PawnSym s;
            s.name = mm[1].str();
            s.kind = L"enumvalue";
            s.line = mLine;
            s.pos = posOf(mLine, s.name);
            s.sig = s.name + (mm[2].matched ? mm[2].str() : L"") + (mm[3].matched ? L" = " + wnorm(mm[3].str()) : L"");
            s.doc = docFor(mLine);
            s.parent = ename;
            syms.push_back(s);
          }
          i = lineAt((int)close) - 1;
          continue;
        }
      }

      // functions and variables: join a few lines until the parentheses balance
      wstring stmt = ln;
      size_t j = i;
      auto needsParen = [&]() { if (stmt.find(L'(') == wstring::npos) return false; wstring in; size_t e; return !parenGroup(stmt, in, e); };
      while (needsParen() && j + 1 < lines.size() && j - i < 12) { j++; stmt += L" " + lines[j]; }
      bool hasMods = std::regex_search(stmt, m, reMods);
      wstring modsText = hasMods ? m[1].str() : L"";
      bool haveFn = false;
      wstring fnName, fnParams, fnMods, fnHead;
      std::wsmatch fm;
      if (hasMods && std::regex_search(stmt, fm, reFnMod)) {
        wstring in;
        size_t e;
        bool g = parenGroup(stmt, in, e);
        haveFn = true;
        fnName = fm[1].str();
        fnParams = g ? wnorm(in) : L"";
        fnMods = wnorm(modsText);
        fnHead = stmt.substr(0, stmt.find(L'('));
      } else if (!hasMods && std::regex_search(stmt, fm, rePlain) && !PAWN_KEYS.count(fm[1].str())) {
        wstring in;
        size_t e;
        if (parenGroup(stmt, in, e)) {
          wstring rest = trimW(stmt.substr(e));
          // a definition has a body: "{" here or on the next line
          bool body = (!rest.empty() && rest[0] == L'{') || (rest.empty() && j + 1 < lines.size() && !trimW(lines[j + 1]).empty() && trimW(lines[j + 1])[0] == L'{');
          if (body) { haveFn = true; fnName = fm[1].str(); fnParams = wnorm(in); fnMods.clear(); fnHead = stmt.substr(0, stmt.find(L'(')); }
        }
      }
      if (haveFn) {
        auto has = [&](const wchar_t* w) { return std::regex_search(fnMods, std::wregex(wstring(L"\\b") + w + L"\\b")); };
        wstring kind = has(L"native") ? L"native" : has(L"forward") ? L"forward" : has(L"stock") ? L"stock" : has(L"public") ? L"public" : L"function";
        PawnSym s;
        s.name = fnName;
        s.kind = kind;
        s.line = lineNo;
        s.pos = posOf(lineNo, fnName);
        s.sig = wnorm(fnHead) + L"(" + fnParams + L")";
        s.doc = docFor(lineNo);
        s.params = paramNames(fnParams);
        s.endLine = (int)j + 1;
        syms.push_back(s);
        wstring joined;
        for (size_t q = i; q <= j; q++) joined += lines[q] + L"\n";
        depth += braceDelta(joined);
        curFn = (kind == L"native" || kind == L"forward") ? -1 : (int)syms.size() - 1;
        if (curFn >= 0) syms[curFn].entered = depth > 0;
        if (curFn >= 0 && depth == 0 && !(j + 1 < lines.size() && trimW(lines[j + 1]).compare(0, 1, L"{") == 0) && stmt.find(L'{') == wstring::npos) curFn = -1;
        i = j;
        continue;
      }

      // global variables / constants: new, const, static, stock const, public ...
      if (std::regex_search(ln, m, reVarMods)) {
        wstring head = ln.substr(0, ln.find(L'='));
        if (head.find(L'(') == wstring::npos) {
          wstring decl = ln;
          size_t k = i;
          while (decl.find(L';') == wstring::npos && k + 1 < lines.size() && k - i < 20 && !std::regex_search(lines[k + 1], reNextDecl)) { k++; decl += L" " + lines[k]; }
          wstring vmods = m[1].str();
          bool isConst = std::regex_search(vmods, std::wregex(L"\\bconst\\b"));
          for (auto& dn : declNames(decl.substr(m[0].length()))) {
            PawnSym s;
            s.name = dn.name;
            s.kind = isConst ? L"const" : L"var";
            s.line = lineNo;
            s.pos = posOf(lineNo, dn.name);
            s.sig = wnorm(vmods) + L" " + dn.text;
            s.doc = docFor(lineNo);
            syms.push_back(s);
          }
          wstring joined;
          for (size_t q = i; q <= k; q++) joined += lines[q] + L"\n";
          depth += braceDelta(joined);
          i = k;
          continue;
        }
      }
    }

    // inside a function body: remember its local variables
    if (depth > 0 && curFn >= 0) {
      if (std::regex_search(ln, m, reLocalMods)) {
        wstring decl = ln;
        size_t k = i;
        while (decl.find(L';') == wstring::npos && k + 1 < lines.size() && k - i < 6) { k++; decl += L" " + lines[k]; }
        for (auto& dn : declNames(decl.substr(m[0].length()))) syms[curFn].locals.push_back({dn.name, wnorm(m[1].str()) + L" " + dn.text, lineNo});
      }
    }
    depth = std::max(0, depth + braceDelta(ln));
    if (curFn >= 0 && depth > 0) syms[curFn].entered = true;
    if (curFn >= 0 && syms[curFn].entered && depth == 0) { syms[curFn].endLine = lineNo; curFn = -1; }
  }
  return out;
}

/* ---------------------------------------------------------------- include resolution across files */
struct PawnFileCache { ULONGLONG mtime = 0; DWORD checked = 0; PawnParsed parsed; bool ok = false; };
static std::map<wstring, PawnFileCache> g_pawnFiles;
static std::map<wstring, std::pair<DWORD, bool>> g_pawnExists;
static vector<wstring> g_pawnDirs;  // the directories searched by #include <...>
static bool pawnHas(const wstring& p) {
  auto it = g_pawnExists.find(p);
  if (it != g_pawnExists.end() && GetTickCount() - it->second.first < 4000) return it->second.second;
  DWORD at = GetFileAttributesW(p.c_str());
  bool ok = at != INVALID_FILE_ATTRIBUTES && !(at & FILE_ATTRIBUTE_DIRECTORY);
  g_pawnExists[p] = {GetTickCount(), ok};
  return ok;
}
static const PawnParsed* pawnLoad(const wstring& p) {
  auto it = g_pawnFiles.find(p);
  if (it != g_pawnFiles.end() && GetTickCount() - it->second.checked < 4000) return it->second.ok ? &it->second.parsed : nullptr;
  ULONGLONG mt = fileTime(p);
  if (!mt) return nullptr;
  if (it != g_pawnFiles.end() && it->second.mtime == mt) { it->second.checked = GetTickCount(); return it->second.ok ? &it->second.parsed : nullptr; }
  bool big;
  std::string raw = readFileRaw(p, 8 * 1024 * 1024, big);
  PawnFileCache c;
  c.mtime = mt;
  c.checked = GetTickCount();
  c.ok = !big;
  if (c.ok) {
    c.parsed = parsePawn(decodeBytes(raw));
    for (auto& s : c.parsed.symbols) s.file = p;
  }
  g_pawnFiles[p] = c;
  return g_pawnFiles[p].ok ? &g_pawnFiles[p].parsed : nullptr;
}
static wstring pawnJoin(const wstring& a, const wstring& b) {
  wstring r = a;
  while (!r.empty() && (r.back() == L'\\' || r.back() == L'/')) r.pop_back();
  wstring bb = b;
  for (auto& c : bb) if (c == L'/') c = L'\\';
  return r + L"\\" + bb;
}
static wstring pawnResolve(const PawnInclude& inc, const wstring& fromDir) {
  wstring name = inc.name;
  for (auto& c : name) if (c == L'/') c = L'\\';
  size_t dot = name.find_last_of(L'.');
  bool hasExt = dot != wstring::npos && name.size() - dot <= 5 && name.find(L'\\', dot) == wstring::npos;
  vector<wstring> exts = hasExt ? vector<wstring>{L""} : vector<wstring>{L".inc", L".sma", L".inl"};
  vector<wstring> bases;
  if (inc.angle) { bases = g_pawnDirs; bases.push_back(fromDir); } else { bases.push_back(fromDir); for (auto& d : g_pawnDirs) bases.push_back(d); }
  for (auto& b : bases) {
    if (b.empty()) continue;
    for (auto& e : exts) { wstring p = pawnJoin(b, name + e); if (pawnHas(p)) return p; }
  }
  return L"";
}
struct PawnVisible { vector<PawnSym> local, included; vector<wstring> unresolved; vector<PawnInclude> includes; };
// everything visible from a document: its own symbols plus those of its includes (recursively)
static PawnVisible pawnCollect(const wstring& text, const wstring& filePath) {
  PawnVisible v;
  PawnParsed local = parsePawn(text);
  if (!filePath.empty()) for (auto& s : local.symbols) s.file = filePath;
  wstring fromDir = filePath.empty() ? L"" : dirNameOf(filePath);
  std::set<wstring> seen;
  if (!filePath.empty()) seen.insert(lowerPath(filePath));
  std::function<void(const vector<PawnInclude>&, const wstring&, int)> walk = [&](const vector<PawnInclude>& incs, const wstring& dir, int depth) {
    for (auto& inc : incs) {
      wstring p = pawnResolve(inc, dir);
      if (p.empty()) { v.unresolved.push_back(inc.name); continue; }
      wstring key = lowerPath(p);
      if (seen.count(key)) continue;
      seen.insert(key);
      const PawnParsed* parsed = pawnLoad(p);
      if (!parsed) continue;
      for (auto& s : parsed->symbols) v.included.push_back(s);
      if (depth < 8) walk(parsed->includes, dirNameOf(p), depth + 1);
    }
  };
  walk(local.includes, fromDir, 0);
  v.local = local.symbols;
  v.includes = local.includes;
  return v;
}
// every `name(` outside comments, strings and #preprocessor lines, with its column
struct PawnCall { wstring name; int line, col; };
static vector<PawnCall> pawnFindCalls(const wstring& text) {
  vector<PawnCall> out;
  vector<PawnComment> comments;
  wstring t = text;
  { wstring r; for (size_t i = 0; i < t.size(); i++) { if (t[i] == L'\r') { if (i + 1 < t.size() && t[i + 1] == L'\n') continue; r += L'\n'; } else r += t[i]; } t = r; }
  wstring code = pawnClean(t, comments);
  vector<wstring> lines = splitNl(code);
  for (size_t li = 0; li < lines.size(); li++) {
    const wstring& ln = lines[li];
    wstring tr = trimW(ln);
    if (tr.empty() || tr[0] == L'#' || ln.find(L'(') == wstring::npos) continue;
    size_t i = 0;
    while (i < ln.size()) {
      if (!isIdStart(ln[i])) { i++; continue; }
      size_t j = i;
      while (j < ln.size() && (isIdCh(ln[j]) || ln[j] == L'@')) j++;
      size_t k = j;
      while (k < ln.size() && iswspace(ln[k])) k++;
      if (k < ln.size() && ln[k] == L'(') out.push_back({ln.substr(i, j - i), (int)li, (int)i});
      i = j;
    }
  }
  return out;
}
