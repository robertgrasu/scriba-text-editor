// git.inc: Git in the editor: bars in the gutter for lines added / changed / removed since HEAD, the branch in the status bar,
// next / previous change, revert a change, show the diff, changed files, and marks in the side bar. Uses git.exe.
// Included by main.cpp after files.inc.

#define WM_GIT_DOC (WM_APP + 4)
#define WM_GIT_STATUS (WM_APP + 5)

// runs git with the arguments in a folder; returns true when it exited with 0. The output is raw bytes.
static bool gitExec(const wstring& args, const wstring& cwd, std::string& out, DWORD timeoutMs = 8000) {
  out.clear();
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE rd, wr;
  if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = wr;
  si.hStdError = nul;
  si.hStdInput = nul;
  PROCESS_INFORMATION pi{};
  wstring cmd = L"git " + args;
  vector<wchar_t> buf(cmd.begin(), cmd.end());
  buf.push_back(0);
  BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
  CloseHandle(wr);
  if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
  if (!ok) { CloseHandle(rd); return false; }
  CloseHandle(pi.hThread);
  // read on this thread; a watchdog kills a git that hangs
  std::atomic<bool> done{false};
  std::thread dog([&]() {
    DWORD waited = 0;
    while (!done && waited < timeoutMs) { Sleep(50); waited += 50; }
    if (!done) TerminateProcess(pi.hProcess, 1);
  });
  char b[16384];
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(rd, b, sizeof(b), &got, nullptr) || got == 0) break;
    out.append(b, got);
    if (out.size() > 64 * 1024 * 1024) break;
  }
  WaitForSingleObject(pi.hProcess, 2000);
  done = true;
  dog.join();
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  CloseHandle(rd);
  return code == 0;
}
static wstring trimNl(std::string s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return fromUtf8(s);
}
static wstring fwdSlashes(wstring s) { for (auto& c : s) if (c == L'\\') c = L'/'; return s; }

struct GitRepo { wstring root, branch; };
static std::mutex g_gitMu;
static std::map<wstring, std::pair<DWORD, std::pair<bool, GitRepo>>> g_gitInfoCache;  // dir -> (time, (is repo, info))
static bool gitInfoOf(const wstring& dir, GitRepo& out) {
  {
    std::lock_guard<std::mutex> lk(g_gitMu);
    auto it = g_gitInfoCache.find(dir);
    if (it != g_gitInfoCache.end() && GetTickCount() - it->second.first < 2500) { out = it->second.second.second; return it->second.second.first; }
  }
  std::string o;
  GitRepo r;
  bool repo = false;
  if (gitExec(L"rev-parse --show-toplevel", dir, o)) {
    repo = true;
    r.root = trimNl(o);
    for (auto& c : r.root) if (c == L'/') c = L'\\';
    std::string b;
    gitExec(L"rev-parse --abbrev-ref HEAD", dir, b);
    r.branch = trimNl(b);
    if (r.branch.empty() || r.branch == L"HEAD") {
      std::string h;
      r.branch = gitExec(L"rev-parse --short HEAD", dir, h) ? L"detached " + trimNl(h) : L"no commits";
    }
  }
  std::lock_guard<std::mutex> lk(g_gitMu);
  g_gitInfoCache[dir] = {GetTickCount(), {repo, r}};
  out = r;
  return repo;
}
static wstring relFromRoot(const wstring& root, const wstring& file) {
  if (file.size() > root.size() && _wcsicmp(file.substr(0, root.size()).c_str(), root.c_str()) == 0) return fwdSlashes(file.substr(root.size() + 1));
  return fwdSlashes(file);
}

/* ---------------------------------------------------------------- line diff (Myers) */
static bool myersOps(const vector<wstring>& a, const vector<wstring>& b, int maxD, std::string& ops) {
  int n = (int)a.size(), m = (int)b.size();
  int mx = std::min(n + m, maxD), off = mx + 1;
  vector<int> v(2 * mx + 3, 0);
  vector<vector<int>> trace;
  for (int d = 0; d <= mx; d++) {
    trace.push_back(v);
    for (int k = -d; k <= d; k += 2) {
      int x = (k == -d || (k != d && v[off + k - 1] < v[off + k + 1])) ? v[off + k + 1] : v[off + k - 1] + 1;
      int y = x - k;
      while (x < n && y < m && a[x] == b[y]) { x++; y++; }
      v[off + k] = x;
      if (x >= n && y >= m) {
        std::string r;
        int cx = n, cy = m;
        for (int dd = d; dd >= 0; dd--) {
          const vector<int>& vv = trace[dd];
          int kk = cx - cy;
          int pk = (kk == -dd || (kk != dd && vv[off + kk - 1] < vv[off + kk + 1])) ? kk + 1 : kk - 1;
          int px = vv[off + pk], py = px - pk;
          while (cx > px && cy > py) { r.push_back('='); cx--; cy--; }
          if (dd > 0) { if (cx == px) { r.push_back('+'); cy--; } else { r.push_back('-'); cx--; } }
        }
        std::reverse(r.begin(), r.end());
        ops = r;
        return true;
      }
    }
  }
  return false;
}
// hunks with 1-based lines of the new file, `to` exclusive
static vector<GitHunk> diffHunks(const vector<wstring>& oldL, const vector<wstring>& newL) {
  size_t s = 0;
  while (s < oldL.size() && s < newL.size() && oldL[s] == newL[s]) s++;
  size_t eo = oldL.size(), en = newL.size();
  while (eo > s && en > s && oldL[eo - 1] == newL[en - 1]) { eo--; en--; }
  vector<wstring> a(oldL.begin() + s, oldL.begin() + eo), b(newL.begin() + s, newL.begin() + en);
  vector<GitHunk> hunks;
  if (a.empty() && b.empty()) return hunks;
  if (a.empty()) { hunks.push_back({1, (int)s + 1, (int)(s + 1 + b.size()), {}}); return hunks; }
  if (b.empty()) { hunks.push_back({3, (int)s + 1, (int)s + 1, a}); return hunks; }
  std::string ops;
  if ((double)a.size() * b.size() > 4e7 || !myersOps(a, b, 1500, ops)) { hunks.push_back({2, (int)s + 1, (int)(s + 1 + b.size()), a}); return hunks; }
  size_t i = 0, j = 0;
  for (size_t p = 0; p < ops.size();) {
    if (ops[p] == '=') { i++; j++; p++; continue; }
    int dc = 0, ac = 0;
    size_t i0 = i, j0 = j;
    while (p < ops.size() && ops[p] != '=') { if (ops[p] == '-') { dc++; i++; } else { ac++; j++; } p++; }
    int from = (int)(s + j0) + 1;
    if (dc && ac) hunks.push_back({2, from, from + ac, vector<wstring>(a.begin() + i0, a.begin() + i0 + dc)});
    else if (ac) hunks.push_back({1, from, from + ac, {}});
    else hunks.push_back({3, from, from, vector<wstring>(a.begin() + i0, a.begin() + i0 + dc)});
  }
  return hunks;
}
static vector<wstring> splitLines(const wstring& t) {
  vector<wstring> r;
  size_t p = 0;
  while (true) {
    size_t q = t.find(L'\n', p);
    if (q == wstring::npos) { r.push_back(t.substr(p)); break; }
    wstring ln = t.substr(p, q - p);
    if (!ln.empty() && ln.back() == L'\r') ln.pop_back();
    r.push_back(ln);
    p = q + 1;
  }
  return r;
}

/* ---------------------------------------------------------------- background refresh of a document */
struct GitDocJob { unsigned long long uid; unsigned ver; wstring path; vector<wstring> lines; bool force; };
struct GitDocResult { unsigned long long uid; unsigned ver; bool repo = false, tracked = false; wstring branch; vector<GitHunk> hunks; };
static std::map<wstring, std::pair<DWORD, std::pair<bool, wstring>>> g_gitHeadCache;
static void gitDocWorker(GitDocJob job, HWND hw) {
  auto* r = new GitDocResult();
  r->uid = job.uid;
  r->ver = job.ver;
  size_t k = job.path.find_last_of(L"\\/");
  wstring dir = k == wstring::npos ? L"." : job.path.substr(0, k);
  GitRepo info;
  if (gitInfoOf(dir, info)) {
    r->repo = true;
    r->branch = info.branch;
    bool tracked = false;
    wstring head;
    bool cached = false;
    {
      std::lock_guard<std::mutex> lk(g_gitMu);
      auto it = g_gitHeadCache.find(job.path);
      if (!job.force && it != g_gitHeadCache.end() && GetTickCount() - it->second.first < 4000) { tracked = it->second.second.first; head = it->second.second.second; cached = true; }
    }
    if (!cached) {
      std::string o;
      wstring rel = relFromRoot(info.root, job.path);
      if (gitExec(L"show HEAD:" + rel, info.root, o, 15000)) { tracked = true; head = decodeBytes(o); }
      std::lock_guard<std::mutex> lk(g_gitMu);
      g_gitHeadCache[job.path] = {GetTickCount(), {tracked, head}};
    }
    r->tracked = tracked;
    if (tracked) {
      for (auto& c : head) (void)c;
      r->hunks = diffHunks(splitLines(head), job.lines);
    }
  }
  PostMessageW(hw, WM_GIT_DOC, 0, (LPARAM)r);
}
// turns the hunks into the per-line marks the gutter draws
static void gitApplyHunks(Doc& d, const vector<GitHunk>& hunks) {
  d.gitHunks = hunks;
  d.gitMark.assign(d.lines.size(), 0);
  for (auto& h : hunks) {
    if (h.kind == 3) { int l = std::min(std::max(h.from, 1), (int)d.lines.size()) - 1; if (!d.gitMark[l]) d.gitMark[l] = 3; }
    else for (int n = h.from; n < h.to && n <= (int)d.lines.size(); n++) d.gitMark[n - 1] = (char)h.kind;
  }
}
static void gitDocDone(GitDocResult* r) {
  Doc* target = nullptr;
  for (auto* d : g_docs) if (d->uid == r->uid) target = d;
  if (target) {
    target->gitBusy = false;
    target->gitAt = GetTickCount();
    if (target->ver == r->ver) {
      gitApplyHunks(*target, r->hunks);
      target->gitBranch = r->repo ? r->branch + (r->tracked ? L"" : L"  (new file)") : L"";
      target->gitChanges = (int)r->hunks.size();
    }
    if (target == &D()) { g_gitBranch = target->gitBranch; g_gitChanges = target->gitChanges; }
    redraw();
  }
  delete r;
}
static void gitStartJob(Doc& d, bool force) {
  if (d.gitBusy || d.path.empty() || d.isNote) return;
  d.gitBusy = true;
  d.gitVerSeen = d.ver;
  GitDocJob job{d.uid, d.ver, d.path, d.lines, force};
  HWND hw = g_hwnd;
  std::thread([job, hw]() mutable { gitDocWorker(std::move(job), hw); }).detach();
}
// timer: schedules a refresh a moment after the text of a visible document changed
static void gitTick() {
  if (!sBool("git_integration")) {
    if (!g_gitBranch.empty()) { g_gitBranch.clear(); g_gitChanges = 0; redraw(); }
    return;
  }
  DWORD now = GetTickCount();
  for (int g = 0; g < paneCount(); g++) {
    int idx = g == g_curGroup ? g_active : g_groupActive[g];
    if (idx < 0 || idx >= (int)g_docs.size()) continue;
    Doc& d = *g_docs[idx];
    if (d.path.empty() || d.isNote) { if (g == g_curGroup && !g_gitBranch.empty()) { g_gitBranch.clear(); redraw(); } continue; }
    if (d.ver != d.gitVerSeenTimer) { d.gitVerSeenTimer = d.ver; d.gitDue = now + (d.gitAt ? 450 : 120); }
    if (d.gitDue && now >= d.gitDue && !d.gitBusy) { d.gitDue = 0; gitStartJob(d, false); }
    else if (!d.gitBusy && !d.gitDue && now - d.gitAt > 5000) gitStartJob(d, false);
    if (g == g_curGroup && g_gitBranch != d.gitBranch) { g_gitBranch = d.gitBranch; g_gitChanges = d.gitChanges; redraw(); }
  }
}

/* ---------------------------------------------------------------- commands */
static void gitGoto(Doc& d, const GitHunk& h) {
  int n = std::min(std::max(h.from, 1), (int)d.lines.size());
  d.cur = d.anc = {n - 1, 0};
  revealCenter();
  moved(false);
}
static void gitNextChange(int dir) {
  Doc& d = D();
  if (d.gitHunks.empty()) { flash(L"No changes since the last commit"); return; }
  int cur = d.cur.l + 1;
  const GitHunk* pick = nullptr;
  if (dir > 0) { for (auto& h : d.gitHunks) if (h.from > cur) { pick = &h; break; } if (!pick) pick = &d.gitHunks.front(); }
  else { for (int i = (int)d.gitHunks.size() - 1; i >= 0; i--) if (d.gitHunks[i].from < cur) { pick = &d.gitHunks[i]; break; } if (!pick) pick = &d.gitHunks.back(); }
  gitGoto(d, *pick);
}
static void gitRevertChange() {
  Doc& d = D();
  int cur = d.cur.l + 1;
  const GitHunk* h = nullptr;
  for (auto& x : d.gitHunks) if (x.kind == 3 ? x.from == cur : (cur >= x.from && cur < x.to)) { h = &x; break; }
  if (!h) { flash(L"The cursor is not on a changed line"); return; }
  GitHunk hh = *h;
  wstring text;
  for (size_t i = 0; i < hh.old.size(); i++) { if (i) text += L"\n"; text += hh.old[i]; }
  d.snapshot(0);
  int n = (int)d.lines.size();
  if (hh.kind == 1) {
    int a = hh.from - 1, b = hh.to - 2;  // 0-based first and last added line
    if (a > 0) d.replaceRange({a - 1, (int)d.lines[a - 1].size()}, {b, (int)d.lines[b].size()}, L"");
    else if (b + 1 < n) d.replaceRange({a, 0}, {b + 1, 0}, L"");
    else d.replaceRange({a, 0}, {b, (int)d.lines[b].size()}, L"");
  } else if (hh.kind == 2) {
    d.replaceRange({hh.from - 1, 0}, {hh.to - 2, (int)d.lines[hh.to - 2].size()}, text);
  } else {
    if (hh.from > n) d.replaceRange({n - 1, (int)d.lines[n - 1].size()}, {n - 1, (int)d.lines[n - 1].size()}, L"\n" + text);
    else d.replaceRange({hh.from - 1, 0}, {hh.from - 1, 0}, text + L"\n");
  }
  d.cur = d.anc = {std::min(d.cur.l, (int)d.lines.size() - 1), 0};
  d.gitDue = GetTickCount() + 50;
  flash(L"Change reverted");
  edited();
}
static void openText(const wstring& text, const wstring& name, Lang lang, const wchar_t* langName) {
  Doc* d = new Doc();
  d->lines = splitLines(text);
  d->name = name;
  d->lang = lang;
  d->langName = langName;
  d->readOnly = true;
  d->dirty = false;
  d->invalidate(0);
  addDoc(d);
}
static void gitShowDiff() {
  Doc& d = D();
  if (d.path.empty()) { flash(L"Open a file in a Git repository first"); return; }
  if (d.dirty && !d.isNote) doSave(d);
  size_t k = d.path.find_last_of(L"\\/");
  GitRepo info;
  if (!gitInfoOf(d.path.substr(0, k), info)) { flash(L"Not a Git repository"); return; }
  std::string o;
  gitExec(L"diff HEAD -- " + relFromRoot(info.root, d.path), info.root, o, 15000);
  if (o.empty()) { flash(L"No changes since the last commit"); return; }
  wstring nm = d.name;
  openText(decodeBytes(o), L"Diff", L_DIFF, L"Diff");
  D().name = L"Diff";
  (void)nm;
}
struct GitStatusEntry { wstring path; char x, y; };
static bool gitStatusOf(const wstring& where, wstring& root, vector<GitStatusEntry>& files) {
  wstring dir = where;
  DWORD at = GetFileAttributesW(where.c_str());
  if (at == INVALID_FILE_ATTRIBUTES || !(at & FILE_ATTRIBUTE_DIRECTORY)) { size_t k = where.find_last_of(L"\\/"); dir = k == wstring::npos ? L"." : where.substr(0, k); }
  GitRepo info;
  if (!gitInfoOf(dir, info)) return false;
  root = info.root;
  std::string o;
  gitExec(L"status --porcelain=v1 -z --untracked-files=normal", root, o, 15000);
  size_t p = 0;
  vector<std::string> parts;
  while (p <= o.size()) { size_t q = o.find('\0', p); if (q == std::string::npos) q = o.size(); parts.push_back(o.substr(p, q - p)); p = q + 1; }
  for (size_t i = 0; i < parts.size(); i++) {
    const std::string& s = parts[i];
    if (s.size() < 4) continue;
    char x = s[0], y = s[1];
    if (x == 'R' || x == 'C') i++;
    wstring rel = fromUtf8(s.substr(3));
    for (auto& c : rel) if (c == L'/') c = L'\\';
    files.push_back({root + L"\\" + rel, x, y});
  }
  return true;
}
static const wchar_t* gitKindName(const GitStatusEntry& f) {
  return f.x == '?' ? L"new" : f.x == 'A' ? L"added" : (f.x == 'D' || f.y == 'D') ? L"deleted" : f.x == 'R' ? L"renamed" : L"modified";
}
static void gitChangedFiles() {
  Doc& d = D();
  wstring where = !d.path.empty() ? d.path : g_root ? g_root->path : L"";
  if (where.empty()) { flash(L"Open a file or a folder first"); return; }
  wstring root;
  vector<GitStatusEntry> files;
  if (!gitStatusOf(where, root, files)) { flash(L"Not a Git repository"); return; }
  if (files.empty()) { flash(L"Working tree clean"); return; }
  vector<PItem> items;
  for (auto& f : files) {
    PItem it;
    it.label = f.path.substr(root.size() + 1);
    it.right = gitKindName(f);
    wstring p = f.path;
    bool deleted = f.x == 'D' || f.y == 'D';
    it.run = [p, deleted] { if (!deleted) openPath(p); };
    items.push_back(it);
  }
  openPick(L"Changed files", listProvider(items));
}

/* ---------------------------------------------------------------- marks in the side bar */
static std::atomic<bool> g_gitStatusBusy{false};
struct GitStatusResult { std::map<wstring, char> files; std::set<wstring> dirs; };
static void gitStatusRefresh() {
  if (!sBool("git_integration") || g_roots.empty() || g_gitStatusBusy.exchange(true)) {
    if (!sBool("git_integration") && !g_gitStatusMap.empty()) { g_gitStatusMap.clear(); g_gitDirSet.clear(); redraw(); }
    return;
  }
  vector<wstring> roots;
  for (auto* r : g_roots) roots.push_back(r->path);
  HWND hw = g_hwnd;
  std::thread([roots, hw]() {
    auto* res = new GitStatusResult();
    for (auto& r : roots) {
      wstring root;
      vector<GitStatusEntry> files;
      if (!gitStatusOf(r, root, files)) continue;
      for (auto& f : files) {
        wstring abs = lowerPath(f.path);
        char code = f.x == '?' ? '?' : f.x == 'A' ? 'A' : (f.x == 'D' || f.y == 'D') ? 'D' : f.x == 'R' ? 'R' : 'M';
        res->files[abs] = code;
        wstring dpath = abs;
        while (true) {
          size_t k = dpath.find_last_of(L'\\');
          if (k == wstring::npos || k < 3) break;
          dpath = dpath.substr(0, k);
          if (dpath.size() + 1 < lowerPath(root).size()) break;
          res->dirs.insert(dpath);
        }
      }
    }
    PostMessageW(hw, WM_GIT_STATUS, 0, (LPARAM)res);
  }).detach();
}
static void gitStatusDone(GitStatusResult* r) {
  g_gitStatusMap = r->files;
  g_gitDirSet = r->dirs;
  g_gitStatusBusy = false;
  delete r;
  redraw();
}
