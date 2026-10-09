// files.inc: recent files/folders/projects, reopen closed file, new window, project files, reveal in side bar.
// Included by main.cpp after panel.inc.

static void testLog(const wstring& s);
/* ---------------------------------------------------------------- recent items */
static wstring recentPath() { return dataDir() + L"\\recent.json"; }
static json recentRead() {
  json j = parseJsonc(readFileText(recentPath()), json::object());
  if (!j.is_object()) j = json::object();
  for (const char* k : {"files", "folders", "projects"}) if (!j.contains(k) || !j[k].is_array()) j[k] = json::array();
  return j;
}
static void recentAdd(const char* kind, const wstring& path) {
  if (path.empty()) return;
  json j = recentRead();
  std::string p = toUtf8(path);
  json arr = json::array({p});
  for (auto& e : j[kind]) if (e.is_string() && _stricmp(e.get<std::string>().c_str(), p.c_str()) != 0 && arr.size() < 40) arr.push_back(e);
  j[kind] = arr;
  writeFileUtf8(recentPath(), j.dump());
}
static void recentClear() { writeFileUtf8(recentPath(), "{}"); }
static vector<wstring> recentList(const char* kind) {
  vector<wstring> r;
  json j = recentRead();
  for (auto& e : j[kind]) if (e.is_string()) { wstring p = fromUtf8(e.get<std::string>()); if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) r.push_back(p); }
  return r;
}
static wstring dirNameOf(const wstring& p) { size_t k = p.find_last_of(L"\\/"); return k == wstring::npos ? L"" : p.substr(0, k); }

/* ---------------------------------------------------------------- the folders of the project */
static void gitStatusRefresh();
static void foldersChanged() {
  gitStatusRefresh();
  for (auto* r : g_roots) recentAdd("folders", r->path);
  g_projScanned = false;
  g_sbVisible = true;
  computeLayout();
  redraw();
}
static void loadProject(const wstring& p) {
  json d = parseJsonc(readFileText(p), json::object());
  if (!d.is_object() || !d.contains("folders") || !d["folders"].is_array()) { flash(L"Not a valid project file"); return; }
  clearRoots();
  for (auto& f : d["folders"]) if (f.is_string()) addRoot(fromUtf8(f.get<std::string>()));
  foldersChanged();
  recentAdd("projects", p);
  flash(L"Project opened: " + baseName(p));
}
static wstring pickProjectFile(bool save) {
  wchar_t buf[MAX_PATH * 2] = {};
  if (save) wcsncpy_s(buf, L"project.scriba-project", MAX_PATH);
  OPENFILENAMEW o{sizeof(o)};
  o.hwndOwner = g_hwnd;
  o.lpstrFilter = L"Scriba project\0*.scriba-project\0All files\0*.*\0";
  o.lpstrFile = buf;
  o.nMaxFile = MAX_PATH * 2;
  o.lpstrDefExt = L"scriba-project";
  o.lpstrTitle = save ? L"Save project" : L"Open project";
  o.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST) : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);
  if (!(save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o))) return L"";
  return buf;
}
static void saveProject() {
  if (g_roots.empty()) { flash(L"Open a folder first: a project is a set of folders"); return; }
  wstring p = pickProjectFile(true);
  if (p.empty()) return;
  json d;
  d["version"] = 1;
  d["folders"] = json::array();
  for (auto* r : g_roots) d["folders"].push_back(toUtf8(r->path));
  writeFileUtf8(p, d.dump(1) + "\n");
  recentAdd("projects", p);
  flash(L"Project saved: " + baseName(p));
}
static void openProject() {
  wstring p = pickProjectFile(false);
  if (!p.empty()) loadProject(p);
}
static void addFolderDialog() {
  wstring p = pickFolder();
  if (p.empty()) return;
  addRoot(p);
  foldersChanged();
}
static void removeFolders() {
  clearRoots();
  computeLayout();
  redraw();
}
// opens the tree down to the file of the active tab and scrolls to it
static void revealActive() {
  Doc& d = D();
  if (d.path.empty()) { flash(L"This tab is not a file on disk"); return; }
  for (auto* rn : g_roots) {
    if (d.path.size() <= rn->path.size() || _wcsicmp(d.path.substr(0, rn->path.size()).c_str(), rn->path.c_str()) != 0) continue;
    FNode* cur = rn;
    wstring rest = d.path.substr(rn->path.size() + 1);
    while (!rest.empty()) {
      size_t k = rest.find(L'\\');
      wstring part = k == wstring::npos ? rest : rest.substr(0, k);
      rest = k == wstring::npos ? L"" : rest.substr(k + 1);
      cur->open = true;
      loadKids(cur);
      FNode* nx = nullptr;
      for (auto* c : cur->kids) if (_wcsicmp(c->name.c_str(), part.c_str()) == 0) { nx = c; break; }
      if (!nx) break;
      cur = nx;
    }
    vector<SbRow> rows;
    for (auto* r2 : g_roots) flatten(r2, 0, rows);
    for (size_t i = 0; i < rows.size(); i++)
      if (rows[i].n == cur) {
        g_sbVisible = true;
        g_sbView = 0;
        float viewH = ch() - MENU_H - STATUS_H - SB_HEAD;
        g_sbScroll = std::max(0.f, std::min(i * SB_ROW - viewH / 2, std::max(0.f, rows.size() * SB_ROW - viewH)));
        computeLayout();
        redraw();
        return;
      }
  }
  flash(L"The file is not inside one of the open folders");
}

/* ---------------------------------------------------------------- recent files and closed tabs */
static void pickRecent() {
  vector<PItem> all;
  for (auto& p : recentList("files")) {
    PItem it;
    it.label = baseName(p);
    it.right = dirNameOf(p);
    wstring pp = p;
    it.run = [pp] { openPath(pp); };
    all.push_back(it);
  }
  for (auto& p : recentList("projects")) {
    PItem it;
    wstring nm = baseName(p);
    size_t e = nm.rfind(L".scriba-project");
    if (e != wstring::npos) nm.erase(e);
    it.label = nm;
    it.right = L"project  " + dirNameOf(p);
    wstring pp = p;
    it.run = [pp] { loadProject(pp); };
    all.push_back(it);
  }
  for (auto& p : recentList("folders")) {
    PItem it;
    it.label = baseName(p).empty() ? p : baseName(p);
    it.right = L"folder  " + p;
    wstring pp = p;
    it.run = [pp] { addRoot(pp); foldersChanged(); };
    all.push_back(it);
  }
  if (all.empty()) { flash(L"Nothing opened recently yet"); return; }
  openPick(L"Open recent", listProvider(all));
}
struct ClosedTab { wstring path, noteId, text; Pos cur; bool vis = false; };
static vector<ClosedTab> g_closed;
static void pushClosedDoc(Doc& d) {
  bool blank = d.lines.size() == 1 && d.lines[0].empty();
  if (d.path.empty() && !d.isNote && blank) return;
  if (d.isNote && blank) return;
  ClosedTab c;
  c.path = d.path;
  c.noteId = d.noteId;
  c.cur = d.cur;
  c.vis = d.vis;
  if (d.path.empty() && !d.isNote) { for (size_t i = 0; i < d.lines.size(); i++) { if (i) c.text += L"\n"; c.text += d.lines[i]; } }
  g_closed.push_back(c);
  if (g_closed.size() > 30) g_closed.erase(g_closed.begin());
}
static void reopenClosed() {
  if (g_closed.empty()) { flash(L"No closed files to reopen"); return; }
  if (g_testing) testLog(L"reopen: begin");
  ClosedTab c = g_closed.back();
  g_closed.pop_back();
  if (!c.noteId.empty()) openNoteDoc(c.noteId);
  else if (!c.path.empty()) { if (g_testing) testLog(L"reopen: openPath " + c.path); openPath(c.path); if (g_testing) testLog(L"reopen: opened"); }
  else {
    Doc* d = new Doc();
    d->lines.clear();
    size_t p = 0;
    while (true) {
      size_t q = c.text.find(L'\n', p);
      if (q == wstring::npos) { d->lines.push_back(c.text.substr(p)); break; }
      d->lines.push_back(c.text.substr(p, q - p));
      p = q + 1;
    }
    d->dirty = true;
    addDoc(d);
  }
  Doc& d = D();
  if (g_testing) testLog(L"reopen: active " + std::to_wstring(g_active) + L" of " + std::to_wstring(g_docs.size()) + L" lines " + std::to_wstring(d.lines.size()) + L" cur " + std::to_wstring(c.cur.l) + L"," + std::to_wstring(c.cur.c));
  d.cur = d.anc = {std::min(c.cur.l, (int)d.lines.size() - 1), 0};
  d.cur.c = d.anc.c = std::min(c.cur.c, (int)d.lines[d.cur.l].size());
  moved(false);
  if (g_testing) testLog(L"reopen: done");
}

/* ---------------------------------------------------------------- changes on disk and auto save */
static bool g_askingReload = false;
// reloads the tabs whose file changed on disk; asks first when the tab has unsaved changes
static void checkExternalChanges() {
  if (g_askingReload || g_docs.empty()) return;
  for (size_t i = 0; i < g_docs.size(); i++) {
    Doc& d = *g_docs[i];
    if (d.path.empty() || d.isNote || d.readOnly) continue;
    ULONGLONG ft = fileTime(d.path);
    if (!ft || !d.diskTime || ft == d.diskTime || d.askedTime == ft) continue;
    if (!d.dirty) {
      reloadDocFromDisk(d);
      flash(L"Reloaded " + d.name);
    } else {
      if (GetForegroundWindow() != g_hwnd && !g_testing) continue;
      d.askedTime = ft;
      g_askingReload = true;
      int r = appMsgBox(g_hwnd, (L"“" + d.name + L"” was modified on disk.\nReload it and discard your changes?").c_str(), L"Scriba", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
      g_askingReload = false;
      if (r == IDYES) reloadDocFromDisk(d);
      else d.diskTime = ft;
    }
    if (i >= g_docs.size()) break;
  }
  updateTitle();
  redraw();
}
// files can save themselves (Preferences: Auto save); never over a file that changed on disk in the meantime
static void autoSaveDoc(Doc& d) {
  if (!d.dirty || d.path.empty() || d.isNote || d.readOnly) return;
  ULONGLONG ft = fileTime(d.path);
  if (ft && d.diskTime && ft != d.diskTime) return;
  doSave(d);
  updateTitle();
  redraw();
}
static void fileWatchTick() {
  DWORD now = GetTickCount();
  for (auto* d : g_docs) {
    if (d->ver != d->autoVer) { d->autoVer = d->ver; d->changedAt = now; }
    if (g_autoSave == 1 && d->dirty && !d->path.empty() && !d->isNote && now - d->changedAt >= (DWORD)g_autoSaveDelay * 1000) autoSaveDoc(*d);
  }
  static int n = 0;
  if (++n % 2 == 0) checkExternalChanges();
}

/* ---------------------------------------------------------------- more windows: another process with its own session */
static HANDLE g_instMutex = nullptr;
static void testLog(const wstring& s);
static void claimInstance() {
  for (int i = 0; i < 12; i++) {
    wstring name = L"Local\\ScribaCpp.Instance." + std::to_wstring(i);
    HANDLE m = CreateMutexW(nullptr, TRUE, name.c_str());
    if (m && GetLastError() != ERROR_ALREADY_EXISTS) { g_instMutex = m; g_instance = i; return; }
    if (m) CloseHandle(m);
  }
}
static void newWindowWith(const wstring& file);
static void newWindow() { newWindowWith(L""); }
static void newWindowWith(const wstring& file) {
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  wstring cmd = L"\"" + wstring(exe) + L"\"";
  if (!file.empty()) cmd += L" \"" + file + L"\"";
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  vector<wchar_t> buf(cmd.begin(), cmd.end());
  buf.push_back(0);
  if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
  else flash(L"Could not start another window");
}
