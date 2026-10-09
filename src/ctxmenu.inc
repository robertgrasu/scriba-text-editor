// context menus (right click): editor, tabs, side bar rows, notes list. Items and the popup are in popup.inc.
static CtxItem ctxCmd(int cmd, const wchar_t* label) {
  CtxItem i;
  wstring k = effectiveKeys(cmd);
  i.label = k.empty() ? wstring(label) : wstring(label) + L"\t" + k;
  i.enabled = cmdAvailable(cmd);
  i.run = [cmd] { exec(cmd); };
  return i;
}
/* ---- file operations used by the side bar and tab menus ---- */
static wstring dirOfPath(const wstring& p) {
  size_t k = p.find_last_of(L"\\/");
  return k == wstring::npos ? L"" : p.substr(0, k);
}
static FNode* findNodeIn(FNode* n, const wstring& p) {
  if (_wcsicmp(n->path.c_str(), p.c_str()) == 0) return n;
  if (!n->dir || !n->loaded) return nullptr;
  for (auto* k : n->kids) if (FNode* r = findNodeIn(k, p)) return r;
  return nullptr;
}
static FNode* findNode(const wstring& p) {
  for (auto* r : g_roots) if (FNode* n = findNodeIn(r, p)) return n;
  return nullptr;
}
// re-reads a folder, keeping the open state of the sub-folders that are still there
static void reloadNode(FNode* n) {
  if (!n || !n->dir) return;
  vector<FNode*> old = n->kids;
  n->kids.clear();
  n->loaded = false;
  loadKids(n);
  for (auto*& k : n->kids) {
    for (auto*& o : old) {
      if (o && o->dir == k->dir && _wcsicmp(o->name.c_str(), k->name.c_str()) == 0) {
        FNode* fresh = k;
        k = o;
        o = nullptr;
        delete fresh;
        if (k->dir && k->loaded) reloadNode(k);
        break;
      }
    }
  }
  for (auto* o : old) if (o) freeNode(o);
  g_projScanned = false;
}
static void ctxError(const wstring& msg) { appMsgBox(g_hwnd, msg.c_str(), L"Scriba", MB_OK | MB_ICONWARNING); }
static void ctxRevealInExplorer(const wstring& path) {
  wstring arg = L"/select,\"" + path + L"\"";
  ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
}
static bool ctxTrash(const wstring& path) {
  wstring from = path;
  from.push_back(L'\0');
  SHFILEOPSTRUCTW op{};
  op.hwnd = g_hwnd;
  op.wFunc = FO_DELETE;
  op.pFrom = from.c_str();
  op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
  return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
}
static void ctxNewEntry(const wstring& dir, bool isDir) {
  openPrompt(isDir ? L"New folder name:" : L"New file name:", L"", [dir, isDir](const wstring& name) {
    wstring nm = trimW(name);
    if (nm.empty()) return;
    wstring target = dir + L"\\" + nm;
    bool ok;
    if (isDir) ok = CreateDirectoryW(target.c_str(), nullptr) != 0;
    else {
      HANDLE h = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
      ok = h != INVALID_HANDLE_VALUE;
      if (ok) CloseHandle(h);
    }
    if (!ok) { ctxError(L"Could not create “" + nm + L"”."); return; }
    if (FNode* n = findNode(dir)) { n->open = true; reloadNode(n); }
    if (!isDir) openPath(target);
    redraw();
  });
}
// open tabs follow a file or folder that was renamed
static void ctxRetarget(const wstring& from, const wstring& to) {
  for (auto* d : g_docs) {
    if (d->path.empty()) continue;
    if (_wcsicmp(d->path.c_str(), from.c_str()) == 0) {
      d->path = to;
      d->name = baseName(to);
    } else if (d->path.size() > from.size() && d->path[from.size()] == L'\\' && _wcsicmp(d->path.substr(0, from.size()).c_str(), from.c_str()) == 0) {
      d->path = to + d->path.substr(from.size());
    }
  }
  updateTitle();
}
static void ctxRename(const wstring& path) {
  openPrompt(L"Rename to:", baseName(path), [path](const wstring& name) {
    wstring nm = trimW(name);
    if (nm.empty() || nm == baseName(path)) return;
    wstring to = dirOfPath(path) + L"\\" + nm;
    if (!MoveFileW(path.c_str(), to.c_str())) { ctxError(L"Could not rename: " + path); return; }
    ctxRetarget(path, to);
    if (FNode* par = findNode(dirOfPath(path))) reloadNode(par);
    redraw();
  });
}
static void ctxDelete(const wstring& path, bool isDir) {
  int r = appMsgBox(g_hwnd, (L"Delete “" + baseName(path) + L"”? It will be moved to the Recycle Bin.").c_str(), L"Scriba", MB_YESNO | MB_ICONQUESTION);
  if (r != IDYES) return;
  if (!ctxTrash(path)) { ctxError(L"Could not delete: " + path); return; }
  // the tabs of the deleted file (or of files inside the folder) are closed
  for (int i = (int)g_docs.size() - 1; i >= 0; i--) {
    Doc* d = g_docs[i];
    if (d->path.empty()) continue;
    bool hit = _wcsicmp(d->path.c_str(), path.c_str()) == 0 ||
               (isDir && d->path.size() > path.size() && d->path[path.size()] == L'\\' && _wcsicmp(d->path.substr(0, path.size()).c_str(), path.c_str()) == 0);
    if (hit) { d->dirty = false; closeDoc(i); }
  }
  if (FNode* par = findNode(dirOfPath(path))) reloadNode(par);
  redraw();
}
static void ctxRemoveRoot(FNode* root) {
  for (size_t i = 0; i < g_roots.size(); i++)
    if (g_roots[i] == root) {
      freeNode(root);
      g_roots.erase(g_roots.begin() + i);
      break;
    }
  g_root = g_roots.empty() ? nullptr : g_roots.front();
  g_projScanned = false;
  g_sbScroll = 0;
  foldersChanged();
  computeLayout();
  redraw();
}

/* ---- notes ---- */
static void ctxNoteRename(const wstring& id) {
  loadMeta();
  wstring cur = g_meta[id].title;
  if (cur.empty()) for (auto& n : g_notes) if (n.id == id) cur = n.title;
  openPrompt(L"Rename note to:", cur, [id](const wstring& name) {
    g_meta[id].title = trimW(name);
    saveMeta();
    for (auto* d : g_docs) if (d->isNote && d->noteId == id) refreshNoteTitle(*d);
    refreshNotes();
    redraw();
  });
}
static void ctxNotePin(const wstring& id) {
  loadMeta();
  g_meta[id].pinned = !g_meta[id].pinned;
  saveMeta();
  refreshNotes();
  redraw();
}
static void ctxNoteColor(const wstring& id, const wstring& color) {
  loadMeta();
  g_meta[id].color = color;
  saveMeta();
  refreshNotes();
  redraw();
}
static void ctxNoteDelete(const wstring& id) {
  wstring title = id;
  for (auto& n : g_notes) if (n.id == id) title = n.title;
  int r = appMsgBox(g_hwnd, (L"Delete “" + title + L"”? It will be moved to the Recycle Bin.").c_str(), L"Scriba", MB_YESNO | MB_ICONQUESTION);
  if (r != IDYES) return;
  for (int i = (int)g_docs.size() - 1; i >= 0; i--)
    if (g_docs[i]->isNote && g_docs[i]->noteId == id) { g_docs[i]->dirty = false; closeDoc(i); }
  ctxTrash(notesDir() + L"\\" + id);
  loadMeta();
  g_meta.erase(id);
  saveMeta();
  refreshNotes();
  redraw();
}
static vector<CtxItem> noteColorItems(const wstring& id, const wstring& current) {
  vector<CtxItem> v;
  CtxItem none = ctxItem(L"None", [id] { ctxNoteColor(id, L""); });
  none.checked = current.empty();
  v.push_back(none);
  for (auto& c : NOTE_PALETTE) {
    wstring col = c.name;
    CtxItem it = ctxItem(capFirst(col), [id, col] { ctxNoteColor(id, col); });
    it.checked = col == current;
    v.push_back(it);
  }
  return v;
}
static vector<CtxItem> noteMenu(const wstring& id, bool fromTab) {
  loadMeta();
  NoteMeta mt = g_meta[id];
  vector<CtxItem> m;
  if (!fromTab) {
    m.push_back(ctxItem(L"Open", [id] { openNoteDoc(id); redraw(); }));
    m.push_back(ctxSep());
  }
  m.push_back(ctxItem(mt.pinned ? L"Unpin" : L"Pin to top", [id] { ctxNotePin(id); }));
  m.push_back(ctxItem(L"Rename…", [id] { ctxNoteRename(id); }));
  CtxItem col;
  col.label = L"Set Color";
  col.sub = noteColorItems(id, mt.color);
  m.push_back(col);
  if (!fromTab) {
    m.push_back(ctxSep());
    m.push_back(ctxItem(L"Reveal in Explorer", [id] { ctxRevealInExplorer(notesDir() + L"\\" + id); }));
    m.push_back(ctxItem(L"Copy Path", [id] { clipSet(notesDir() + L"\\" + id); }));
    m.push_back(ctxSep());
    m.push_back(ctxItem(L"Delete…", [id] { ctxNoteDelete(id); }));
  }
  return m;
}

/* ---- the menus ---- */
static vector<CtxItem> editorMenu() {
  vector<CtxItem> m;
  Doc& d0 = D();
  if (d0.isNote && g_spellOn) {
    int a, b;
    wstring word;
    if (spellWordAt(d0, d0.cur.l, d0.cur.c, a, b, word)) {
      int ln = d0.cur.l;
      for (auto& sg : spellSuggest(word)) {
        wstring rep = sg;
        m.push_back(ctxItem(sg, [ln, a, b, rep] {
          Doc& dd = D();
          if (ln < (int)dd.lines.size() && b <= (int)dd.lines[ln].size()) { dd.snapshot(0); dd.replaceRange({ln, a}, {ln, b}, rep); edited(); }
        }));
      }
      m.push_back(ctxItem(L"Add “" + word + L"” to dictionary", [word] { spellAdd(word); redraw(); }));
      m.push_back(ctxSep());
    }
  }
  m.push_back(ctxCmd(C_CUT, L"Cut"));
  m.push_back(ctxCmd(C_COPY, L"Copy"));
  m.push_back(ctxCmd(C_PASTE, L"Paste"));
  m.push_back(ctxSep());
  m.push_back(ctxCmd(C_SELALL, L"Select All"));
  m.push_back(ctxSep());
  m.push_back(ctxCmd(C_COMMENT, L"Toggle Comment"));
  Doc& d = D();
  if (!d.path.empty()) {
    m.push_back(ctxSep());
    m.push_back(ctxCmd(C_REVEAL, L"Reveal in Side Bar"));
  }
  m.push_back(ctxSep());
  m.push_back(ctxCmd(C_CLAUDEADD, L"Add Selection to Claude Chat"));
  m.push_back(ctxCmd(C_CLAUDEASK, L"Ask Claude About This\x2026"));
  m.push_back(ctxCmd(C_CLAUDEEXPLAIN, L"Claude: Explain"));
  m.push_back(ctxCmd(C_CLAUDEFIX, L"Claude: Fix Problems"));
  m.push_back(ctxCmd(C_CLAUDETESTS, L"Claude: Write Tests"));
  m.push_back(ctxCmd(C_CLAUDEREFACTOR, L"Claude: Refactor"));
  return m;
}
static void closeTabsWhere(int keep, int mode) {  // mode 0: all others, 1: tabs to the right in the same group
  Doc* kd = g_docs[keep];
  vector<Doc*> todo;
  bool after = false;
  for (auto* d : g_docs) {
    if (d == kd) { after = true; continue; }
    if (mode == 0 || (d->group == kd->group && after)) todo.push_back(d);
  }
  for (auto* d : todo) {
    for (size_t i = 0; i < g_docs.size(); i++)
      if (g_docs[i] == d) {
        if (!closeDoc((int)i)) return;
        break;
      }
  }
}
static vector<CtxItem> tabMenu(int ti) {
  vector<CtxItem> m;
  Doc* d = g_docs[ti];
  Doc* keep = d;
  auto idx = [keep]() -> int {
    for (size_t i = 0; i < g_docs.size(); i++) if (g_docs[i] == keep) return (int)i;
    return -1;
  };
  bool hasRight = false, seen = false;
  for (auto* o : g_docs) {
    if (o == d) { seen = true; continue; }
    if (seen && o->group == d->group) hasRight = true;
  }
  m.push_back(ctxItem(L"Close", [idx] { int i = idx(); if (i >= 0) closeDoc(i); }));
  m.push_back(ctxItem(L"Close Others", [idx] { int i = idx(); if (i >= 0) closeTabsWhere(i, 0); }, g_docs.size() > 1));
  m.push_back(ctxItem(L"Close Tabs to the Right", [idx] { int i = idx(); if (i >= 0) closeTabsWhere(i, 1); }, hasRight));
  if (d->isNote) {
    m.push_back(ctxSep());
    for (auto& i : noteMenu(d->noteId, true)) m.push_back(i);
  } else if (!d->path.empty()) {
    wstring p = d->path;
    m.push_back(ctxSep());
    m.push_back(ctxItem(L"Copy File Path", [p] { clipSet(p); }));
    m.push_back(ctxItem(L"Reveal in Explorer", [p] { ctxRevealInExplorer(p); }));
    m.push_back(ctxItem(L"Reveal in Side Bar", [idx] { int i = idx(); if (i >= 0) { activateDoc(i); revealActive(); } }, !g_roots.empty()));
    m.push_back(ctxSep());
    m.push_back(ctxItem(L"New File…", [p] { ctxNewEntry(dirOfPath(p), false); }));
  }
  return m;
}
static vector<CtxItem> sidebarMenu(FNode* n) {
  vector<CtxItem> m;
  wstring path = n->path;
  bool isDir = n->dir;
  bool isRoot = false;
  for (auto* r : g_roots) if (r == n) isRoot = true;
  wstring parent = isDir ? path : dirOfPath(path);
  if (!isDir) {
    m.push_back(ctxItem(L"Open", [path] { openPath(path); redraw(); }));
    m.push_back(ctxSep());
  }
  m.push_back(ctxItem(L"New File…", [parent] { ctxNewEntry(parent, false); }));
  m.push_back(ctxItem(L"New Folder…", [parent] { ctxNewEntry(parent, true); }));
  m.push_back(ctxSep());
  m.push_back(ctxItem(L"Rename…", [path] { ctxRename(path); }, !isRoot));
  m.push_back(ctxItem(L"Delete", [path, isDir] { ctxDelete(path, isDir); }, !isRoot));
  m.push_back(ctxSep());
  m.push_back(ctxItem(L"Reveal in Explorer", [path] { ctxRevealInExplorer(path); }));
  m.push_back(ctxItem(L"Copy Path", [path] { clipSet(path); }));
  if (isDir) {
    m.push_back(ctxSep());
    m.push_back(ctxItem(L"Find in Folder…", [path] {
      fifOpenPanel(L"");
      g_fifWhere.set(path);
      redraw();
    }));
  }
  if (isRoot) {
    m.push_back(ctxItem(L"Remove Folder from Project", [n] { ctxRemoveRoot(n); }));
  }
  return m;
}

// the "Show tabs" button of a tab bar: every tab of the group, the active one checked
static void showTabList(int gi) {
  const VP& p = g_paneRect[gi];
  vector<CtxItem> m;
  for (size_t i = 0; i < g_docs.size(); i++) {
    Doc* d = g_docs[i];
    if (d->group != gi) continue;
    CtxItem it = ctxItem(d->name + (d->dirty && !d->isNote ? L" \x2022" : L""), [d] {
      for (size_t k = 0; k < g_docs.size(); k++) if (g_docs[k] == d) { activateDoc((int)k); break; }
    });
    it.checked = (int)i == g_active;
    m.push_back(it);
  }
  float s = 1.f / g_dpiScale;
  ctxShow(m, (int)((p.right - 240) * s), (int)((p.top + TAB_H) * s));
}
// x, y in DIPs (as the mouse code uses them), px/py the raw client position for the popup
static bool contextMenuAt(float x, float y, int px, int py) {
  if (g_palOpen || g_suOpen || g_openMenu >= 0) return false;
  if (y < MENU_H) return false;
  // tabs
  int ti = tabAt(x, y);
  if (ti >= 0) {
    ctxShow(tabMenu(ti), px, py);
    return true;
  }
  // side bar
  if (g_sbVisible && x < g_sbW && y < ch() - STATUS_H) {
    if (g_sbView == 0 && g_root && y >= MENU_H + SB_HEAD) {
      vector<SbRow> rows;
      for (auto* rn : g_roots) flatten(rn, 0, rows);
      int i = (int)std::floor((y - MENU_H - SB_HEAD + g_sbScroll) / SB_ROW);
      if (i >= 0 && i < (int)rows.size()) {
        g_sbHover = i;
        redraw();
        ctxShow(sidebarMenu(rows[i].n), px, py);
        return true;
      }
    } else if (g_sbView == 1 && y >= notesListTop()) {
      notesLayout();
      int i = notesRowAt(y);
      if (i >= 0 && !g_nrows[i].head) {
        wstring id = g_notes[g_nrows[i].note].id;
        ctxShow(noteMenu(id, false), px, py);
        return true;
      }
    }
    return true;
  }
  // editor text
  bool inText = y >= edTop() && y < edTop() + edH() && x >= L0() + gutterW() && x < mapX();
  if (inText) {
    Doc& d = D();
    Pos p = hit(x, y);
    Pos a = d.selA(), b = d.selB();
    bool inSel = d.hasSel() && !(p.l < a.l || (p.l == a.l && p.c < a.c) || p.l > b.l || (p.l == b.l && p.c > b.c));
    if (!inSel) {
      d.extra.clear();
      d.cur = d.anc = p;
      moved(false);
    }
    ctxShow(editorMenu(), px, py);
    return true;
  }
  return false;
}

/* ---- dragging a tab: reorder inside a group or move it to the other group ---- */
static Doc* g_dragDoc = nullptr;
static float g_dragX0 = 0, g_dragY0 = 0;
static bool g_dragTab = false;
static int g_dropGroup = -1;
static Doc* g_dropBefore = nullptr;  // the tab the dragged one goes in front of; null = the end of the group
static D2D1_RECT_F g_dropMark{0, 0, 0, 0};
static int g_dropWinTest = 0;  // test driver: 1 = never treat a drop as outside the window
static void tabDragStart(Doc* d, float x, float y) {
  g_dragDoc = d;
  g_dragX0 = x;
  g_dragY0 = y;
  g_dragTab = false;
  g_dropGroup = -1;
  SetCapture(g_hwnd);
}
static void tabDragMove(float x, float y) {
  if (!g_dragDoc) return;
  if (!g_dragTab && std::fabs(x - g_dragX0) + std::fabs(y - g_dragY0) < 8) return;
  g_dragTab = true;
  g_dropGroup = -1;
  g_dropBefore = nullptr;
  for (int gi = 0; gi < paneCount(); gi++) {
    const VP& p = g_paneRect[gi];
    if (y < p.top || y >= p.top + TAB_H || x < p.left || x >= p.right) continue;
    VP saved = g_vp;
    g_vp = p;
    auto rects = tabRects(gi);
    g_vp = saved;
    g_dropGroup = gi;
    float mx = rects.empty() ? p.left + 52 : rects.back().x + rects.back().w;
    for (auto& r : rects) {
      if (x < r.x + r.w / 2) { g_dropBefore = g_docs[r.idx]; mx = r.x - 1; break; }
      mx = r.x + r.w;
    }
    g_dropMark = rc(mx, p.top + 4, 2, TAB_H - 8);
    break;
  }
  redraw();
}
static void tabDragEnd() {
  Doc* d = g_dragDoc;
  bool moved = g_dragTab;
  int grp = g_dropGroup;
  Doc* before = g_dropBefore;
  g_dragDoc = nullptr;
  g_dragTab = false;
  g_dropGroup = -1;
  ReleaseCapture();
  if (d && moved) {  // dropped outside the window: the file opens in a window of its own
    POINT pt;
    RECT wr;
    GetCursorPos(&pt);
    GetWindowRect(g_hwnd, &wr);
    if (!PtInRect(&wr, pt) && !g_testing) {
      if (d->path.empty() || d->isNote) { flash(L"Only files on disk can move to another window"); redraw(); return; }
      if (d->dirty) { flash(L"Save the file first, then drag it to another window"); redraw(); return; }
      newWindowWith(d->path);
      size_t k = std::find(g_docs.begin(), g_docs.end(), d) - g_docs.begin();
      if (k < g_docs.size()) closeDoc((int)k);
      return;
    }
  }
  if (!d || !moved || grp < 0 || before == d) { redraw(); return; }
  size_t from = std::find(g_docs.begin(), g_docs.end(), d) - g_docs.begin();
  if (from >= g_docs.size()) return;
  Doc* activeDoc = g_docs[g_active];
  g_docs.erase(g_docs.begin() + from);
  size_t at;
  if (before) at = std::find(g_docs.begin(), g_docs.end(), before) - g_docs.begin();
  else {
    at = 0;
    for (size_t i = 0; i < g_docs.size(); i++) if (g_docs[i]->group == grp) at = i + 1;
    if (at == 0 && !g_docs.empty() && grp != g_docs[0]->group) at = g_docs.size();
  }
  d->group = grp;
  g_docs.insert(g_docs.begin() + std::min(at, g_docs.size()), d);
  (void)activeDoc;
  g_active = (int)(std::find(g_docs.begin(), g_docs.end(), d) - g_docs.begin());
  g_curGroup = grp;
  g_groupActive[grp] = g_active;
  for (int g = 0; g < 2; g++) {
    if (g == grp) continue;
    int idx = g_groupActive[g];
    bool ok = false;
    if (idx >= 0 && idx < (int)g_docs.size() && g_docs[idx]->group == g) ok = true;
    if (!ok) g_groupActive[g] = -1;
  }
  normalizeGroups(g_active);
  computeLayout();
  updateTitle();
  redraw();
}
static void drawTabDrop() {
  if (!g_dragTab || g_dropGroup < 0) return;
  fillR(g_dropMark, T().accent, 1.f, 1);
}
