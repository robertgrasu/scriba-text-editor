// The Notes view of the side bar. Same list modes, sorts, filters, colours and tags as Electron's notes.js.
struct NRow { bool head = false; wstring key; int count = 0; bool closed = false; int note = -1; float y = 0, h = 0; };
struct NChip { int kind; wstring key; D2D1_RECT_F r; };  // kind 0 colour dot, 1 #tag, 2 "clear"
struct NTagHit { D2D1_RECT_F r; wstring tag; };
static wstring g_fTag, g_fColor;
static std::set<wstring> g_nCollapsed;
static vector<NRow> g_nrows;
static vector<NChip> g_nchips;
static vector<NTagHit> g_ntagHits;
static float g_nContentH = 0;

static const wchar_t* NOTE_VIEWS[] = {L"List with previews", L"Compact titles", L"Grouped by tag", L"Grouped by color"};
static const wchar_t* NOTE_SORTS[] = {L"Newest first", L"Oldest first", L"Title A-Z", L"Date created"};
static const float NF_TOP = 102;  // the filter chips start here (below the search box), relative to the menu bar

static vector<int> notesVisible() {
  vector<int> v;
  for (size_t i = 0; i < g_notes.size(); i++) {
    const NoteInfo& n = g_notes[i];
    if (!g_fTag.empty() && std::find(n.tags.begin(), n.tags.end(), g_fTag) == n.tags.end()) continue;
    if (!g_fColor.empty() && n.color != g_fColor) continue;
    v.push_back((int)i);
  }
  return v;
}
static wstring capFirst(wstring s) {
  if (!s.empty()) s[0] = (wchar_t)towupper(s[0]);
  return s;
}
static float notesListTop() { return MENU_H + NOTE_TOP + g_nFiltH; }
// chips and rows for the current notes, filters and width
static void notesLayout() {
  float w = g_sbW;
  g_nchips.clear();
  std::map<wstring, int> tags;
  vector<wstring> colors;
  for (auto& n : g_notes) {
    for (auto& t : n.tags) tags[t]++;
    if (!n.color.empty() && std::find(colors.begin(), colors.end(), n.color) == colors.end()) colors.push_back(n.color);
  }
  bool any = !tags.empty() || !colors.empty() || !g_fTag.empty() || !g_fColor.empty();
  g_nFiltH = 0;
  if (any) {
    float x = 10, y = MENU_H + NF_TOP, maxX = w - 10;
    auto place = [&](int kind, const wstring& key, float cw, float chh) {
      if (x + cw > maxX && x > 10) { x = 10; y += 23; }
      g_nchips.push_back({kind, key, rc(x, y + (18 - chh) / 2, cw, chh)});
      x += cw + 5;
    };
    for (auto& c : colors) place(0, c, 17, 17);
    vector<std::pair<wstring, int>> tl(tags.begin(), tags.end());
    std::stable_sort(tl.begin(), tl.end(), [](auto& a, auto& b) { return a.second > b.second; });
    if (tl.size() > 14) tl.resize(14);
    for (auto& t : tl) place(1, t.first, textWidth(L"#" + t.first, g_uiSmall) + 16, 18);
    if (!g_fTag.empty() || !g_fColor.empty()) place(2, L"", textWidth(L"clear", g_uiSmall) + 10, 18);
    g_nFiltH = (y + 23) - (MENU_H + NF_TOP) - 8;
  }
  // rows
  g_nrows.clear();
  vector<int> vis = notesVisible();
  float y = 0;
  auto addNote = [&](int idx, bool compact) {
    NRow r;
    r.note = idx;
    r.y = y;
    r.h = compact ? 28.f : (g_notes[idx].tags.empty() ? 44.f : 63.f);
    y += r.h;
    g_nrows.push_back(r);
  };
  if (g_noteMode >= 2) {
    std::map<wstring, vector<int>> groups;
    for (int i : vis) {
      const NoteInfo& n = g_notes[i];
      wstring key = g_noteMode == 2 ? (n.tags.empty() ? L"No tag" : L"#" + n.tags[0]) : (n.color.empty() ? L"No color" : capFirst(n.color));
      groups[key].push_back(i);
    }
    vector<wstring> keys;
    for (auto& g : groups) keys.push_back(g.first);
    std::stable_sort(keys.begin(), keys.end(), [](const wstring& a, const wstring& b) {
      bool na = a.compare(0, 3, L"No ") == 0, nb = b.compare(0, 3, L"No ") == 0;
      if (na != nb) return nb;
      return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    for (auto& k : keys) {
      NRow h;
      h.head = true;
      h.key = k;
      h.count = (int)groups[k].size();
      h.closed = g_nCollapsed.count(k) != 0;
      h.y = y;
      h.h = 26;
      y += h.h;
      g_nrows.push_back(h);
      if (!h.closed) for (int i : groups[k]) addNote(i, false);
    }
  } else {
    for (int i : vis) addNote(i, g_noteMode == 1);
  }
  g_nContentH = y + 10;
}
static int notesRowAt(float y) {
  float rel = y - notesListTop() + g_nbScroll;
  for (size_t i = 0; i < g_nrows.size(); i++) if (rel >= g_nrows[i].y && rel < g_nrows[i].y + g_nrows[i].h) return (int)i;
  return -1;
}
static float notesMaxScroll() { return std::max(0.f, g_nContentH - (ch() - STATUS_H - notesListTop())); }
static wstring noteAgo(ULONGLONG mtime) {
  FILETIME nowFt;
  GetSystemTimeAsFileTime(&nowFt);
  ULONGLONG nt = ((ULONGLONG)nowFt.dwHighDateTime << 32) | nowFt.dwLowDateTime;
  ULONGLONG secs = nt > mtime ? (nt - mtime) / 10000000ULL : 0;
  wchar_t ds[48];
  if (secs < 60) return L"now";
  if (secs < 3600) swprintf_s(ds, L"%dm", (int)(secs / 60));
  else if (secs < 86400) swprintf_s(ds, L"%dh", (int)(secs / 3600));
  else if (secs < 86400 * 30) swprintf_s(ds, L"%dd", (int)(secs / 86400));
  else {
    FILETIME lf;
    FileTimeToLocalFileTime((const FILETIME*)&mtime, &lf);
    SYSTEMTIME st;
    FileTimeToSystemTime(&lf, &st);
    if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, ds, 48, nullptr)) swprintf_s(ds, L"%02d.%02d.%04d", st.wDay, st.wMonth, st.wYear);
  }
  return ds;
}
static void drawPinIcon(float x, float y, DWORD col, float a) {
  g_br->SetColor(Rgb(col, a));
  g_rt->FillRoundedRectangle(D2D1::RoundedRect(rc(x + 2.2f, y, 4.6f, 4.4f), 0.8f, 0.8f), g_br);
  g_rt->DrawLine(D2D1::Point2F(x + 0.8f, y + 6.2f), D2D1::Point2F(x + 8.2f, y + 6.2f), g_br, 1.6f);
  g_rt->DrawLine(D2D1::Point2F(x + 4.5f, y + 6.6f), D2D1::Point2F(x + 4.5f, y + 11), g_br, 1.1f);
}
static void drawNotesView(float top, float w, float h) {
  const Theme& t = T();
  DWORD head = mixc(t.sidebar, t.fg, 0.45f);
  vector<int> vis = notesVisible();
  notesLayout();
  g_nbScroll = std::max(0.f, std::min(g_nbScroll, notesMaxScroll()));
  {
    drawText(L"NOTES", 5, 12, top + 44, head, g_uiSmall, 150, 18);
    if (!g_notes.empty()) {
      wchar_t cb[32];
      int cn = vis.size() == g_notes.size() ? swprintf_s(cb, L"%d", (int)g_notes.size()) : swprintf_s(cb, L"%d/%d", (int)vis.size(), (int)g_notes.size());
      drawText(cb, cn, 12 + textWidth(L"NOTES", g_uiSmall) + 8, top + 44, mixc(t.sidebar, t.fg, 0.3f), g_uiSmall, 80, 18);
    }
    float cy = top + 53;
    for (float bxc : {w - 20.f, w - 50.f, w - 78.f}) {
      D2D1_RECT_F hb = rc(bxc - 12, top + 41, 24, 24);
      if (inRect(hb, g_mx, g_my)) fillR(hb, t.fg, 0.1f, 6);
    }
    g_br->SetColor(Rgb(head));
    float cx = w - 20;
    g_rt->DrawLine(D2D1::Point2F(cx - 4.5f, cy), D2D1::Point2F(cx + 4.5f, cy), g_br, 1.5f);
    g_rt->DrawLine(D2D1::Point2F(cx, cy - 4.5f), D2D1::Point2F(cx, cy + 4.5f), g_br, 1.5f);
    // sort: two arrows, down on the left and up on the right
    float ix = w - 50;
    g_rt->DrawLine(D2D1::Point2F(ix - 3, cy - 5), D2D1::Point2F(ix - 3, cy + 5), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(ix - 5.2f, cy + 2.8f), D2D1::Point2F(ix - 3, cy + 5), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(ix - 0.8f, cy + 2.8f), D2D1::Point2F(ix - 3, cy + 5), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(ix + 3, cy + 5), D2D1::Point2F(ix + 3, cy - 5), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(ix + 0.8f, cy - 2.8f), D2D1::Point2F(ix + 3, cy - 5), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(ix + 5.2f, cy - 2.8f), D2D1::Point2F(ix + 3, cy - 5), g_br, 1.3f);
    // view: four lines, the last one short
    float jx = w - 78;
    g_rt->DrawLine(D2D1::Point2F(jx - 5.5f, cy - 4.5f), D2D1::Point2F(jx + 5.5f, cy - 4.5f), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(jx - 5.5f, cy - 1.5f), D2D1::Point2F(jx + 5.5f, cy - 1.5f), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(jx - 5.5f, cy + 1.5f), D2D1::Point2F(jx + 5.5f, cy + 1.5f), g_br, 1.3f);
    g_rt->DrawLine(D2D1::Point2F(jx - 5.5f, cy + 4.5f), D2D1::Point2F(jx + 1.5f, cy + 4.5f), g_br, 1.3f);
  }
  drawInput(g_nq, rc(10, top + 68, w - 20, 30), g_nqFocus, L"Search notes or #tags");
  // filter chips
  for (auto& c : g_nchips) {
    bool hov = inRect(c.r, g_mx, g_my);
    if (c.kind == 0) {
      float cx = (c.r.left + c.r.right) / 2, cy = (c.r.top + c.r.bottom) / 2;
      g_br->SetColor(Rgb(noteColorRgb(c.key, t.accent)));
      g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 5.5f, 5.5f), g_br);
      if (g_fColor == c.key) {
        g_br->SetColor(Rgb(t.fg));
        g_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 7.f, 7.f), g_br, 2.f);
      }
    } else if (c.kind == 1) {
      bool on = g_fTag == c.key;
      fillR(c.r, on ? t.accent : t.fg, on ? 0.32f : 0.09f, 9);
      wstring s = L"#" + c.key;
      drawText(s.c_str(), (int)s.size(), c.r.left + 8, c.r.top + 1, on || hov ? t.fg : sbFg(), g_uiSmall, c.r.right - c.r.left, 18);
    } else {
      drawText(L"clear", 5, c.r.left + 5, c.r.top + 1, t.accent, g_uiSmall, c.r.right - c.r.left, 18);
    }
  }
  float listTop = top + NOTE_TOP + g_nFiltH;
  g_rt->PushAxisAlignedClip(D2D1::RectF(0, listTop, w - 1, top + h), D2D1_ANTIALIAS_MODE_ALIASED);
  g_ntagHits.clear();
  DWORD dim = mixc(t.sidebar, t.fg, 0.42f);
  for (size_t i = 0; i < g_nrows.size(); i++) {
    const NRow& r = g_nrows[i];
    float ry = listTop + r.y - g_nbScroll;
    if (ry + r.h < listTop || ry > top + h) continue;
    if (r.head) {
      float cx = 20, cy = ry + 14;
      g_br->SetColor(Rgb(head, 0.8f));
      D2D1_POINT_2F p1 = r.closed ? D2D1::Point2F(cx - 1.5f, cy - 3.5f) : D2D1::Point2F(cx - 3.5f, cy - 1.5f);
      D2D1_POINT_2F p2 = r.closed ? D2D1::Point2F(cx + 2.5f, cy) : D2D1::Point2F(cx, cy + 2.5f);
      D2D1_POINT_2F p3 = r.closed ? D2D1::Point2F(cx - 1.5f, cy + 3.5f) : D2D1::Point2F(cx + 3.5f, cy - 1.5f);
      g_rt->DrawLine(p1, p2, g_br, 1.3f);
      g_rt->DrawLine(p3, p2, g_br, 1.3f);
      wstring label = r.key;
      for (auto& c : label) c = (wchar_t)towupper(c);
      drawText(label.c_str(), (int)label.size(), 32, ry + 6, head, g_uiSmall, w - 90, 18);
      wchar_t cb[16];
      int cn = swprintf_s(cb, L"%d", r.count);
      float cw2 = textWidth(cb, g_uiSmall);
      drawText(cb, cn, w - cw2 - 16, ry + 6, mixc(t.sidebar, t.fg, 0.3f), g_uiSmall, cw2 + 4, 18);
      continue;
    }
    const NoteInfo& n = g_notes[r.note];
    bool open = g_active >= 0 && g_active < (int)g_docs.size() && g_docs[g_active]->isNote && g_docs[g_active]->noteId == n.id;
    D2D1_RECT_F rr = rc(6, ry, w - 13, r.h);
    if (open) fillR(rr, sbHi(), 1.f, 7);
    else if ((int)i == g_noteHover) fillR(rr, sbHi(), 1.f, 7);
    bool compact = r.h < 30;
    float lineY = ry + (compact ? 5.f : 6.f);
    wstring ago = noteAgo(n.mtime);
    float dw = textWidth(ago, g_uiSmall);
    drawText(ago.c_str(), (int)ago.size(), w - dw - 18, lineY + 1, dim, g_uiSmall, dw + 4, 18);
    float tx0 = 16;
    if (!n.color.empty()) {
      g_br->SetColor(Rgb(noteColorRgb(n.color, t.accent)));
      g_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(tx0 + 4, lineY + 10), 4.f, 4.f), g_br);
      tx0 += 13;
    }
    float tw = w - dw - 44 - (tx0 - 16) - (n.pinned ? 14 : 0);
    drawText(n.title.c_str(), (int)n.title.size(), tx0, lineY, t.fg, g_ui, tw, 20);
    if (n.pinned) drawPinIcon(std::min(tx0 + textWidth(n.title, g_ui) + 4, tx0 + tw), lineY + 4, t.accent, 0.65f);
    if (!compact) {
      const wstring& sub = n.snippet.empty() ? n.preview : n.snippet;
      drawText(sub.c_str(), (int)sub.size(), 16, ry + 25, dim, g_uiSmall, w - 34, 18);
      float tx = 16;
      for (size_t k = 0; k < n.tags.size() && k < 3; k++) {
        wstring s = L"#" + n.tags[k];
        float cw2 = textWidth(s, g_uiSmall) + 12;
        D2D1_RECT_F cr = rc(tx, ry + 43, cw2, 16);
        bool hv = inRect(cr, g_mx, g_my);
        fillR(cr, hv ? t.accent : t.fg, hv ? 0.28f : 0.09f, 8);
        drawText(s.c_str(), (int)s.size(), tx + 6, ry + 43, hv ? t.fg : sbFg(), g_uiSmall, cw2, 16);
        g_ntagHits.push_back({cr, n.tags[k]});
        tx += cw2 + 5;
      }
    }
  }
  if (g_nrows.empty()) {
    wstring m = g_nq.t.empty() && g_fTag.empty() && g_fColor.empty() ? L"No notes yet. Press " + effectiveKeys(C_NEWNOTE) + L" to start one." : L"No notes match.";
    drawText(m.c_str(), (int)m.size(), 14, listTop + 10, mixc(t.sidebar, t.fg, 0.55f), g_uiSmall, w - 28, 40);
  }
  g_rt->PopAxisAlignedClip();
  float vh = top + h - listTop;
  if (g_nContentH > vh) {
    float th = std::max(30.f, vh * vh / g_nContentH), ty = listTop + (vh - th) * (g_nbScroll / (g_nContentH - vh));
    fillR(rc(w - 7, ty, 4, th), t.gutterFg, 0.35f, 2);
  }
}
static void notesSetMode(int m) {
  g_noteMode = m;
  g_nbScroll = 0;
  redraw();
}
static void notesSetSort(int s) {
  g_noteSort = s;
  refreshNotes();
  redraw();
}
// a click in the notes view (below the Files | Notes | Outline pills)
static bool notesMouse(float x, float y) {
  float top = MENU_H;
  notesLayout();
  if (y < top + 66) {
    if (y >= top + 41 && y < top + 65) {
      float s = 1.f / g_dpiScale;
      if (x >= g_sbW - 32) newNote();
      else if (x >= g_sbW - 64) {
        vector<CtxItem> m;
        for (int i = 0; i < 4; i++) { CtxItem it = ctxItem(NOTE_SORTS[i], [i] { notesSetSort(i); }); it.checked = g_noteSort == i; m.push_back(it); }
        ctxShow(m, (int)((g_sbW - 62) * s), (int)((top + 67) * s));
      } else if (x >= g_sbW - 92) {
        vector<CtxItem> m;
        for (int i = 0; i < 4; i++) { CtxItem it = ctxItem(NOTE_VIEWS[i], [i] { notesSetMode(i); }); it.checked = g_noteMode == i; m.push_back(it); }
        ctxShow(m, (int)((g_sbW - 90) * s), (int)((top + 67) * s));
      }
    }
    return true;
  }
  if (y < top + NOTE_TOP - 4) { g_nqFocus = true; caretReset(); redraw(); return true; }
  for (auto& c : g_nchips) {
    if (!inRect(c.r, x, y)) continue;
    if (c.kind == 0) g_fColor = g_fColor == c.key ? L"" : c.key;
    else if (c.kind == 1) g_fTag = g_fTag == c.key ? L"" : c.key;
    else g_fTag.clear(), g_fColor.clear();
    g_nbScroll = 0;
    redraw();
    return true;
  }
  if (y < notesListTop()) return true;
  for (auto& th : g_ntagHits) if (inRect(th.r, x, y)) { g_fTag = th.tag; g_nbScroll = 0; redraw(); return true; }
  int i = notesRowAt(y);
  if (i < 0) return true;
  const NRow& r = g_nrows[i];
  if (r.head) {
    if (g_nCollapsed.count(r.key)) g_nCollapsed.erase(r.key);
    else g_nCollapsed.insert(r.key);
  } else openNoteDoc(g_notes[r.note].id);
  redraw();
  return true;
}
// Enter in the search box opens the first note shown
static void notesOpenFirst() {
  notesLayout();
  for (auto& r : g_nrows) if (!r.head) { openNoteDoc(g_notes[r.note].id); return; }
}
static void notesClearSearch() {
  g_fTag.clear();
  g_fColor.clear();
  g_nq.set(L"");
  g_nbScroll = 0;
  refreshNotes();
}
