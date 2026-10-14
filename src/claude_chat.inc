// claude_chat.inc: the Claude Code chat dock, logic part (conversations, the stream-json events, permission cards, sessions).
// Port of Electron's claude.js. Drawing and input are in claude_view.inc.

struct CcModel { const wchar_t *id, *name, *desc; };
static const CcModel CC_MODELS[] = {
    {L"", L"Default", L"Whatever your Claude Code uses (recommended)"},
    {L"opus", L"Opus 5.5", L"For complex work and everyday tasks"},
    {L"sonnet", L"Sonnet 5.5", L"Most efficient for simpler tasks"},
    {L"fable", L"Fable 5.1", L"For your toughest challenges"},
    {L"haiku", L"Haiku 5.5", L"Fastest for quick answers"},
    {L"claude-haiku-4-5", L"Haiku 4.5", L"Fastest for quick answers"},
    {L"claude-sonnet-5", L"Sonnet 5", L"Efficient for routine tasks"}};
static const wchar_t* CC_EFFORTS[] = {L"low", L"medium", L"high", L"xhigh", L"max"};
static const CcModel CC_MODES[] = {
    {L"acceptEdits", L"Edit files", L"Changes files without asking, asks for commands"},
    {L"manual", L"Ask", L"Asks before every change and command"},
    {L"auto", L"Auto", L"Claude decides what is safe to do"},
    {L"plan", L"Plan", L"Only proposes a plan, changes nothing"},
    {L"dontAsk", L"Read only", L"Anything that would need approval is refused"},
    {L"bypassPermissions", L"Everything", L"Also runs any command (use with care)"}};
static const wchar_t* CC_LOCAL_CMDS[][2] = {
    {L"new", L"Start a new conversation"}, {L"resume", L"Past conversations in this folder"}, {L"model", L"Switch the model and effort"},
    {L"mode", L"What Claude may do without asking"}, {L"context", L"Send the open file with the message"}, {L"stop", L"Stop what Claude is doing"},
    {L"account", L"Show the signed-in account"}, {L"login", L"Sign in or switch account"}, {L"logout", L"Sign out of Claude Code"}};
static const wchar_t* CC_VERBS[] = {L"Thinking", L"Concocting", L"Pondering", L"Brewing", L"Noodling", L"Mulling", L"Cooking", L"Musing", L"Tinkering", L"Weaving"};

// "claude-opus-5-5" -> "Opus 5.5"
static wstring ccFriendlyModel(const wstring& id) {
  if (id.compare(0, 7, L"claude-") != 0) return id;
  size_t p = 7, q = id.find(L'-', p);
  if (q == wstring::npos) return id;
  wstring fam = id.substr(p, q - p);
  size_t r = q + 1, e = r;
  while (e < id.size() && iswdigit(id[e])) e++;
  if (e == r || fam.empty()) return id;
  wstring major = id.substr(r, e - r), minor;
  if (e < id.size() && id[e] == L'-') {
    size_t m = e + 1, me = m;
    while (me < id.size() && iswdigit(id[me])) me++;
    if (me > m) minor = id.substr(m, me - m);
  }
  fam[0] = (wchar_t)towupper(fam[0]);
  return fam + L" " + major + (!minor.empty() && minor.size() < 3 ? L"." + minor : L"");
}

/* ---------------------------------------------------------------- data */
struct CcAtt {
  int kind = 0;  // 0 file, 1 image, 2 selection
  wstring path, name, media, file, text;
  std::string data;  // base64 of an image
  int from = 0, to = 0;
};
struct CcHunk { wstring tool; bool hasOld = false; wstring oldS, newS; };
struct CcTouched { wstring file; int add = 0, del = 0; vector<CcHunk> hunks; };
enum { CB_WELCOME, CB_USER, CB_AI, CB_TOOL, CB_THINK, CB_ASK, CB_CHANGES, CB_HINT, CB_FAIL, CB_TRUST, CB_WARN, CB_AUTH };
// the drawing list of a block (filled by claude_view.inc)
struct CcPrim;
struct CcBlock {
  int kind = CB_HINT;
  wstring text;                 // user text, AI markdown, hint/error text, thinking text
  vector<CcAtt> atts;           // attachments of a user message
  std::string uuid;             // user message id (needed to rewind files)
  wstring tname;                // tool name
  json input = json::object();  // tool input
  wstring result;
  bool resErr = false, open = false;
  bool live = false, emptyThink = false;
  wstring label;
  ULONGLONG t0 = 0;
  // cards that wait for an answer: 0 tool permission, 1 plan, 2 questions
  int askKind = 0;
  std::string rid;
  json sug = json::array();
  bool done = false, ok = false;
  vector<std::set<int>> picked;
  vector<LineEdit> others;
  // what a turn changed
  vector<CcTouched> touched;
  std::string turnUuid;
  bool reverted = false;
  // sign in / sign out
  wstring authId, authAction, authLog;
  // layout cache (claude_view.inc)
  float h = 0, lastW = -1;
  bool dirty = true;
  float innerScroll = 0;
  vector<CcPrim>* prims = nullptr;
  vector<CcText*>* texts = nullptr;
  ~CcBlock();
};
struct CcTurn {
  vector<CcTouched> touched;
  std::string msgId, uuid;
  std::set<std::string> streamed, thinkIds;
  CcBlock *think = nullptr, *bubble = nullptr;
  wstring text;
  bool shown = false;
  int ctx = 0;
};
// the multi-line text box of the composer
struct CcInput {
  wstring t;
  int c = 0, a = 0;
  float wantX = -1;
  vector<std::pair<wstring, std::pair<int, int>>> undo;
  bool hasSel() const { return a != c; }
};
struct CcWaiter { std::function<void(const json&)> cb; DWORD deadline; };
struct CcSuggestItem { wstring label, desc, value; };
struct CcChat {
  int convId = 0;
  bool attention = false;
  wstring trustShown;
  bool procOpen = false;
  wstring procKey, procMode, procModel;
  CcTurn* current = nullptr;
  wstring sessionId, cwd, title = L"Untitled";
  double cost = 0;
  vector<ClSession> sessions;
  wstring activeModel;
  vector<wstring> slashCommands;
  std::map<std::string, CcWaiter> waiters;
  vector<CcBlock*> userBubbles;
  std::map<std::string, CcBlock*> toolEls;
  vector<CcAtt> atts;
  vector<CcBlock*> blocks;
  CcBlock* working = nullptr;
  wstring verb;
  float scroll = 0, contentH = 0, viewH = 0;
  bool stick = true, hasBar = false;
  vector<float> tops;
  LineEdit input;
  float inScroll = 0;
  IDWriteTextLayout* inLay = nullptr;  // layout of the text box
  wstring inLayText;
  float inLayW = 0, inLayH = 0;
  // suggestions above the text box
  int sgKind = 0;  // 0 none, 1 slash, 2 file
  vector<CcSuggestItem> sgItems;
  int sgIndex = 0, sgFrom = 0, sgTo = 0, sgScroll = 0;
  wstring costText, costTip, usageText, usageTip;
  bool costBig = false, usageHot = false;
  int ctxTokens = 0;
  ~CcChat();
};

// a pop-up list (model, mode, history, account, plus) opened from the chat
struct CcPopItem {
  wstring title, desc;
  bool sel = false;
  int kind = 0;  // 0 option, 1 section title, 2 note, 3 effort dots, 4 account header
  int icon = 0;
  std::function<void()> run;
};
struct CcPop {
  int kind = 0;  // 0 closed, 1 open
  wstring name;
  vector<CcPopItem> items;
  D2D1_RECT_F anchor{};
  bool down = false;  // opens below the anchor (account, history) instead of above
  int hover = -1;
  float scroll = 0;
};
static CcPop g_ccPop;

static void ccOpenPopover(CcChat* c, const wstring& which);
static void ccStartAuth(CcChat* c, const wstring& action, bool consoleLogin);

static int g_ccConvSeq = 0;
static vector<CcChat*> g_ccChats;
static int g_ccActive = -1;
static bool g_ccRestored = false;

static wstring g_ccPrefModel, g_ccPrefMode;
static bool g_ccModelSet = false, g_ccModeSet = false, g_ccContext = true;
static wstring g_ccPrefEffort = L"medium";
static json g_ccStoreTabs = json::array();
static int g_ccStoreActive = 0;
static std::set<wstring> g_ccAuthIds;
static int g_ccAuthSeq = 0;
static json g_ccAuthInfo;  // `claude auth status`, read once
static bool g_ccAuthAsked = false;

static CcChat* ccCur() { return g_ccActive >= 0 && g_ccActive < (int)g_ccChats.size() ? g_ccChats[g_ccActive] : nullptr; }
static bool ccAlive(CcChat* c) { return std::find(g_ccChats.begin(), g_ccChats.end(), c) != g_ccChats.end(); }
static wstring ccModelPref() { return g_ccModelSet ? g_ccPrefModel : sStr("claude_model"); }
static wstring ccModePref() {
  if (g_ccModeSet) return g_ccPrefMode;
  wstring m = sStr("claude_permission_mode");
  return m.empty() ? L"acceptEdits" : m;
}
static wstring ccStorePath() { return dataDir() + L"\\claude.json"; }
static void ccSaveStore() {
  json j = {{"effort", toUtf8(g_ccPrefEffort)}, {"context", g_ccContext}, {"width", (int)g_ccW}};
  if (g_ccModelSet) j["model"] = toUtf8(g_ccPrefModel);
  if (g_ccModeSet) j["mode"] = toUtf8(g_ccPrefMode);
  json tabs = json::array();
  int act = 0;
  for (size_t i = 0; i < g_ccChats.size() && tabs.size() < 12; i++) {
    if (g_ccChats[i]->sessionId.empty()) continue;
    if ((int)i == g_ccActive) act = (int)tabs.size();
    tabs.push_back({{"sid", toUtf8(g_ccChats[i]->sessionId)}, {"title", toUtf8(g_ccChats[i]->title)}});
  }
  if (!g_ccRestored) { tabs = g_ccStoreTabs; act = g_ccStoreActive; }
  j["tabs"] = tabs;
  j["active"] = act;
  j["open"] = g_ccOpen;
  std::string s = j.dump(1);
  wstring tmp = ccStorePath() + L".tmp";
  HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD wr = 0;
  WriteFile(h, s.data(), (DWORD)s.size(), &wr, nullptr);
  CloseHandle(h);
  MoveFileExW(tmp.c_str(), ccStorePath().c_str(), MOVEFILE_REPLACE_EXISTING);
}
static void ccLoadStore() {
  vector<char> b;
  if (!readAll(ccStorePath(), b)) return;
  json j = json::parse(std::string(b.begin(), b.end()), nullptr, false);
  if (!j.is_object()) return;
  if (j.contains("model") && j["model"].is_string()) { g_ccPrefModel = fromUtf8(j["model"].get<std::string>()); g_ccModelSet = true; }
  if (j.contains("mode") && j["mode"].is_string()) { g_ccPrefMode = fromUtf8(j["mode"].get<std::string>()); g_ccModeSet = true; }
  if (j.contains("effort") && j["effort"].is_string()) g_ccPrefEffort = fromUtf8(j["effort"].get<std::string>());
  if (j.contains("context") && j["context"].is_boolean()) g_ccContext = j["context"].get<bool>();
  if (j.contains("width") && j["width"].is_number() && j["width"].get<int>() >= 280 && j["width"].get<int>() <= 900) g_ccW = (float)j["width"].get<int>();
  if (j.contains("tabs") && j["tabs"].is_array()) g_ccStoreTabs = j["tabs"];
  if (j.contains("active") && j["active"].is_number()) g_ccStoreActive = j["active"].get<int>();
  if (j.value("open", false)) g_ccOpen = true;
}

/* ---------------------------------------------------------------- timers: spinner, verbs, waiters */
static std::vector<std::pair<DWORD, std::function<void()>>> g_ccLater;
static void ccLater(DWORD delayMs, std::function<void()> fn) { g_ccLater.push_back({GetTickCount() + delayMs, fn}); }
static void ccInvalidateAll(CcChat* c) {
  for (auto* b : c->blocks) b->dirty = true;
  if (c->working) c->working->dirty = true;
}
static void ccTouch(CcChat*, CcBlock* b) {
  if (b) b->dirty = true;
  redraw();
}

/* ---------------------------------------------------------------- the editor around the chat */
static wstring ccGetCwd() {
  if (!g_roots.empty()) return g_roots.front()->path;
  if (!g_docs.empty() && g_active >= 0 && g_active < (int)g_docs.size() && !D().path.empty()) return dirNameOf(D().path);
  return L"";
}
struct CcContext { wstring file, selection; int line = 0, fromLine = 0, toLine = 0; };
static bool ccGetContext(CcContext& out) {
  if (g_docs.empty() || g_active < 0 || g_active >= (int)g_docs.size()) return false;
  Doc& d = D();
  if (d.path.empty() || d.isNote) return false;
  out.file = d.path;
  out.line = d.cur.l + 1;
  if (d.hasSel()) {
    out.selection = d.selectedText();
    out.fromLine = d.selA().l + 1;
    out.toLine = d.selB().l + 1;
  }
  return true;
}
static void ccAfterChange() {
  checkExternalChanges();
  for (auto* d : g_docs) { d->gitVerSeenTimer = ~0u; d->gitAt = 0; }
  gitStatusRefresh();
}
static wstring ccAbsPath(CcChat* c, const wstring& p) {
  if ((p.size() > 2 && p[1] == L':') || (!p.empty() && (p[0] == L'\\' || p[0] == L'/'))) return p;
  wstring base = c->cwd;
  while (!base.empty() && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
  wstring r = base + L"\\" + p;
  for (auto& ch : r) if (ch == L'/') ch = L'\\';
  return r;
}
static wstring ccShort(CcChat* c, const wstring& p0) {
  wstring p = p0;
  for (auto& ch : p) if (ch == L'/') ch = L'\\';
  wstring base = c->cwd;
  while (!base.empty() && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
  base += L"\\";
  if (p.size() > base.size() && _wcsnicmp(p.c_str(), base.c_str(), base.size()) == 0) return p.substr(base.size());
  return p;
}
static wstring ccModelLabel(CcChat* c) {
  wstring pm = ccModelPref();
  for (auto& m : CC_MODELS) if (pm == m.id) {
    if (pm.empty() && !c->activeModel.empty()) return ccFriendlyModel(c->activeModel);
    return m.name;
  }
  return ccFriendlyModel(pm);
}
static const wchar_t* ccModeName(const wstring& id) {
  for (auto& m : CC_MODES) if (id == m.id) return m.name;
  return L"";
}
static bool ccBusy(CcChat* c) { return c->current != nullptr; }

/* ---------------------------------------------------------------- blocks */
static CcBlock* ccAdd(CcChat* c, int kind, const wstring& text = L"") {
  auto* b = new CcBlock();
  b->kind = kind;
  b->text = text;
  c->blocks.push_back(b);
  redraw();
  return b;
}
static void ccPersist() {
  ccLater(400, [] { ccSaveStore(); });
}
static void ccSetTitle(CcChat* c, const wstring& t) {
  c->title = t;
  ccPersist();
  redraw();
}
static void ccClearBlocks(CcChat* c) {
  for (auto* b : c->blocks) delete b;
  c->blocks.clear();
  c->userBubbles.clear();
  c->toolEls.clear();
  if (c->current) { c->current->bubble = nullptr; c->current->think = nullptr; }
  c->scroll = 0;
  c->stick = true;
}
static void ccShowWelcome(CcChat* c) {
  if (!c->blocks.empty()) return;
  ccAdd(c, CB_WELCOME);
}
static void ccStartWorking(CcChat* c) {
  if (!c->working) { c->working = new CcBlock(); c->working->kind = CB_HINT; }
  c->verb = CC_VERBS[rand() % (sizeof(CC_VERBS) / sizeof(CC_VERBS[0]))];
  redraw();
}
static void ccStopWorking(CcChat* c) {
  delete c->working;
  c->working = nullptr;
  redraw();
}
static std::string ccResultText(const json& c) {
  if (c.is_string()) return c.get<std::string>();
  std::string o;
  if (c.is_array())
    for (auto& x : c)
      if (x.is_object() && x.contains("text") && x["text"].is_string()) { if (!o.empty()) o += "\n"; o += x["text"].get<std::string>(); }
  return o;
}
static CcBlock* ccAddUser(CcChat* c, const wstring& text, const vector<CcAtt>& atts) {
  CcBlock* b = ccAdd(c, CB_USER, text);
  b->atts = atts;
  return b;
}
static CcBlock* ccAddTool(CcChat* c, const wstring& name, const json& input, const std::string& id) {
  CcBlock* b = ccAdd(c, CB_TOOL);
  b->tname = name;
  b->input = input.is_object() ? input : json::object();
  if (name == L"TodoWrite") b->open = true;
  if (!id.empty()) c->toolEls[id] = b;
  return b;
}
static CcBlock* ccAddThink(CcChat* c, const wstring& text, bool streaming) {
  CcBlock* b = ccAdd(c, CB_THINK, text);
  b->live = streaming;
  b->label = streaming ? L"Thinking\x2026" : L"Thinking";
  b->t0 = GetTickCount64();
  return b;
}

/* ---------------------------------------------------------------- usage and cost texts */
static void ccReadAuth(std::function<void(const json&)> cb) {
  if (g_ccAuthAsked) { cb(g_ccAuthInfo); return; }
  g_ccAuthAsked = true;
  clAuthStatus(sStr("claude_path"), [cb](const json& r) {
    g_ccAuthInfo = r.is_object() && !r.contains("error") ? r : json();
    cb(g_ccAuthInfo);
  });
}
static void ccRenderCost(CcChat* c) {
  int ctx = c->ctxTokens;
  wchar_t kb[24] = L"";
  if (ctx) {
    if (ctx >= 1000) swprintf_s(kb, L"%dk", (int)std::lround(ctx / 1000.0));
    else swprintf_s(kb, L"%d", ctx);
  }
  int cid = c->convId;
  ccReadAuth([cid, ctx, kb = wstring(kb)](const json& info) {
    CcChat* cc = nullptr;
    for (auto* x : g_ccChats) if (x->convId == cid) cc = x;
    if (!cc) return;
    bool sub = info.is_object() && info.value("authMethod", "") == "claude.ai";
    wchar_t cost[32];
    swprintf_s(cost, L"%.2f", cc->cost);
    cc->costText = wstring(sub ? L"\x2248 $" : L"$") + cost + (kb.empty() ? L"" : L" \x00B7 " + kb);
    cc->costTip = sub ? wstring(L"About $") + cost + L" if these tokens were paid on the API. With your Claude subscription (" +
                            (info.contains("subscriptionType") && info["subscriptionType"].is_string() ? fromUtf8(info["subscriptionType"].get<std::string>()) : L"plan") +
                            L") nothing is charged per token: it counts toward the usage limits of the plan."
                      : wstring(L"Cost of this conversation on the API: $") + cost + L".";
    if (!kb.empty()) cc->costTip += L"\nContext of the last answer: " + kb + L" tokens. A long context uses the limits faster; /compact shrinks it, a new tab starts fresh.";
    cc->costBig = ctx > 150000;
    redraw();
  });
}
static void ccShowUsage(CcChat* c, const json& info) {
  if (!info.is_object()) return;
  double best = -1;
  wstring bestName, lines;
  if (info.contains("unifiedWindows") && info["unifiedWindows"].is_object()) {
    for (auto it = info["unifiedWindows"].begin(); it != info["unifiedWindows"].end(); ++it) {
      if (!it.value().is_object() || !it.value().contains("utilization") || !it.value()["utilization"].is_number()) continue;
      double u = it.value()["utilization"].get<double>();
      std::string k = it.key();
      wstring nm = k == "five_hour" ? L"5 hours" : k == "seven_day" ? L"week" : k == "seven_day_opus" ? L"week (Opus)" : k == "seven_day_sonnet" ? L"week (Sonnet)" : fromUtf8(k);
      if (!lines.empty()) lines += L"\n";
      lines += nm + L": " + std::to_wstring((int)std::lround(u * 100)) + L"% used";
      if (u > best) { best = u; bestName = nm; }
    }
  }
  if (best < 0 && info.contains("utilization") && info["utilization"].is_number()) {
    best = info["utilization"].get<double>();
    bestName = fromUtf8(info.value("rateLimitType", ""));
    for (auto& ch : bestName) if (ch == L'_') ch = L' ';
  }
  if (best < 0) return;
  c->usageText = std::to_wstring((int)std::lround(best * 100)) + L"%";
  c->usageHot = best >= 0.8;
  c->usageTip = lines.empty() ? bestName + L": " + std::to_wstring((int)std::lround(best * 100)) + L"% used" : lines;
  if (info.contains("resetsAt") && info["resetsAt"].is_number()) {
    time_t t = (time_t)info["resetsAt"].get<double>();
    struct tm tmv;
    localtime_s(&tmv, &t);
    wchar_t buf[64];
    wcsftime(buf, 64, L"%x %X", &tmv);
    c->usageTip += wstring(L"\nResets ") + buf;
  }
  redraw();
}

/* ---------------------------------------------------------------- sessions */
static void ccLoadSessions(CcChat* c) {
  c->cwd = ccGetCwd();
  c->sessions = c->cwd.empty() ? vector<ClSession>() : clSessions(c->cwd);
}
static void ccResetProc(CcChat* c) {
  bool had = c->procOpen;
  c->procOpen = false;
  int old = c->convId;
  c->convId = ++g_ccConvSeq;
  if (had) clClose(old);
  c->waiters.clear();
}
static void ccInterrupt(CcChat* c) {
  if (!c->current) return;
  int id = c->convId;
  clControl(id, {{"subtype", "interrupt"}});
  // if it does not stop by itself, the process is ended; the conversation continues from its session
  ccLater(4000, [c, id] { if (ccAlive(c) && c->current && c->convId == id) ccResetProc(c); });
}
static void ccRenderHistory(CcChat* c, const vector<ClHistMsg>& list) {
  ccClearBlocks(c);
  for (auto& m : list) {
    if (m.user) {
      CcBlock* b = ccAddUser(c, fromUtf8(m.text), {});
      b->uuid = m.uuid;
    } else {
      for (auto& hb : m.blocks) {
        if (!hb.tool) ccAdd(c, CB_AI, fromUtf8(hb.text));
        else ccAddTool(c, fromUtf8(hb.name), hb.input, "");
      }
    }
  }
  c->stick = true;
}
static void ccOpenSession(CcChat* c, const wstring& id, const wstring& title) {
  if (ccBusy(c)) return;
  ccResetProc(c);
  c->sessionId = id;
  c->cost = 0;
  c->costText.clear();
  c->userBubbles.clear();
  ccSetTitle(c, title.empty() ? L"Untitled" : title);
  if (id.empty()) { ccClearBlocks(c); ccShowWelcome(c); return; }
  ccRenderHistory(c, clHistory(c->cwd, id));
}
static void ccNewChat(CcChat* c) {
  if (ccBusy(c)) ccInterrupt(c);
  ccResetProc(c);
  c->sessionId.clear();
  c->slashCommands.clear();
  ccClearBlocks(c);
  c->costText.clear();
  c->cost = 0;
  ccSetTitle(c, L"Untitled");
  ccShowWelcome(c);
}

/* ---------------------------------------------------------------- talking to the process */
static wstring ccProcKey(CcChat* c) {
  json k = json::array({toUtf8(ccGetCwd()), toUtf8(g_ccPrefEffort), g_settings["claude_allowed_tools"], toUtf8(sStr("claude_path"))});
  return fromUtf8(k.dump());
}
static bool ccEnsureProc(CcChat* c) {
  c->cwd = ccGetCwd();
  if (c->procOpen && c->procKey != ccProcKey(c)) ccResetProc(c);
  if (c->procOpen) return true;
  ClOpen o;
  o.conv = c->convId;
  o.cwd = c->cwd;
  o.sessionId = c->sessionId;
  o.permissionMode = ccModePref();
  o.model = ccModelPref();
  o.effort = g_ccPrefEffort;
  o.claudePath = sStr("claude_path");
  o.allowedTools = sList("claude_allowed_tools");
  wstring err;
  if (!clOpen(o, err)) {
    ccAdd(c, CB_FAIL, err);
    return false;
  }
  c->procOpen = true;
  c->procKey = ccProcKey(c);
  c->procMode = ccModePref();
  c->procModel = ccModelPref();
  return true;
}
static void ccControl(CcChat* c, const json& req, DWORD timeoutMs, std::function<void(const json&)> cb) {
  std::string rid = clControl(c->convId, req);
  if (rid.empty()) { cb({{"error", "Claude is not running"}}); return; }
  c->waiters[rid] = {cb, GetTickCount() + timeoutMs};
}
static void ccApplyMode(CcChat* c, std::function<void()> then = nullptr) {
  if (!c->procOpen || c->procMode == ccModePref()) { if (then) then(); return; }
  c->procMode = ccModePref();
  ccControl(c, {{"subtype", "set_permission_mode"}, {"mode", toUtf8(c->procMode)}}, 8000, [then](const json&) { if (then) then(); });
}
static void ccRefreshAfterTurn(CcChat* c, const vector<CcTouched>& files);

// the message that is sent: editor context, attached files, then what was typed
static void ccSend(CcChat* c, wstring text, bool noContext = false) {
  text = trimW(text);
  if ((text.empty() && c->atts.empty()) || ccBusy(c)) return;
  g_ccPop.kind = 0;
  c->sgKind = 0;
  // commands handled by the editor itself
  if (text.size() > 1 && text[0] == L'/' && c->atts.empty()) {
    wstring k = text.substr(1);
    bool word = !k.empty();
    for (wchar_t ch : k) if (!(iswalnum(ch) || ch == L'_')) word = false;
    if (word) {
      static const wchar_t* local[] = {L"new", L"resume", L"model", L"mode", L"context", L"stop", L"login", L"logout", L"account"};
      for (auto l : local)
        if (k == l) {
          c->input = LineEdit();
          if (k == L"login") ccStartAuth(c, L"login", false);
          else if (k == L"logout") ccStartAuth(c, L"logout", false);
          else if (k == L"account") ccOpenPopover(c, L"account");
          else if (k == L"stop") ccInterrupt(c);
          else if (k == L"new") ccNewChat(c);
          else if (k == L"resume") ccOpenPopover(c, L"history");
          else if (k == L"model") ccOpenPopover(c, L"model");
          else if (k == L"mode") ccOpenPopover(c, L"mode");
          else if (k == L"context") { g_ccContext = !g_ccContext; ccSaveStore(); flash(g_ccContext ? L"Claude gets the open file" : L"Open file not sent"); }
          redraw();
          return;
        }
    }
  }
  if (!c->blocks.empty() && c->blocks[0]->kind == CB_WELCOME) { delete c->blocks[0]; c->blocks.erase(c->blocks.begin()); }
  c->cwd = ccGetCwd();
  wstring prompt = text.empty() ? L"Look at the attached files." : text;
  bool isSlash = text.size() > 1 && text[0] == L'/' && (iswalnum(text[1]) || wcschr(L"_:.-", text[1]));
  vector<const CcAtt*> files, sels;
  for (auto& a : c->atts) { if (a.kind == 0) files.push_back(&a); else if (a.kind == 2) sels.push_back(&a); }
  if (!files.empty() && !isSlash) {
    wstring l = L"Attached files (read them if needed):\n";
    for (auto* a : files) l += L"- " + a->path + L"\n";
    prompt = l + L"\n" + prompt;
  }
  if (!sels.empty() && !isSlash) {
    wstring l;
    for (size_t i = 0; i < sels.size(); i++) {
      auto* a = sels[i];
      if (i) l += L"\n\n";
      l += L"Selected code from " + a->file + L" (lines " + std::to_wstring(a->from) + L"-" + std::to_wstring(a->to) + L"):\n```\n" + a->text + L"\n```";
    }
    prompt = l + L"\n\n" + prompt;
  }
  CcContext cx;
  if (g_ccContext && !noContext && !isSlash && ccGetContext(cx)) {
    wstring l = L"[Editor context] Open file: " + cx.file + (cx.line ? L" (cursor on line " + std::to_wstring(cx.line) + L")" : L"");
    if (!cx.selection.empty()) {
      wstring sel = cx.selection.substr(0, 6000);
      l += L"\nSelected text" + (cx.fromLine ? L" (lines " + std::to_wstring(cx.fromLine) + L"-" + std::to_wstring(cx.toLine) + L")" : wstring()) + L":\n```\n" + sel + L"\n```";
    }
    prompt = l + L"\n\n" + prompt;
  }
  json content = json::array();
  for (auto& a : c->atts)
    if (a.kind == 1 && !a.data.empty()) content.push_back({{"type", "image"}, {"source", {{"type", "base64"}, {"media_type", toUtf8(a.media)}, {"data", a.data}}}});
  content.push_back({{"type", "text"}, {"text", toUtf8(prompt)}});
  if (c->title == L"Untitled" || c->sessionId.empty()) {
    wstring t = text.empty() ? L"Attachments" : text;
    for (auto& ch : t) if (iswspace(ch)) ch = L' ';
    ccSetTitle(c, t.substr(0, 60));
  }
  CcBlock* bubble = ccAddUser(c, text.empty() ? L"(attachments)" : text, c->atts);
  c->userBubbles.push_back(bubble);
  c->input = LineEdit();
  c->atts.clear();
  delete c->current;
  c->current = new CcTurn();
  ccStartWorking(c);
  c->stick = true;
  if (!ccEnsureProc(c)) { delete c->current; c->current = nullptr; ccStopWorking(c); return; }
  int cid = c->convId;
  ccApplyMode(c, [c, cid, content] {
    if (!ccAlive(c) || c->convId != cid) return;
    auto sendNow = [c, cid, content] {
      if (!ccAlive(c) || c->convId != cid) return;
      bool ok = clWrite(c->convId, {{"type", "user"}, {"message", {{"role", "user"}, {"content", content}}}, {"parent_tool_use_id", nullptr}});
      if (!ok) {
        logLine(L"claude: could not send the message to the process");
        ccAdd(c, CB_FAIL, L"Could not send the message to Claude Code.");
        delete c->current;
        c->current = nullptr;
        ccStopWorking(c);
      }
    };
    if (c->procModel != ccModelPref()) {
      c->procModel = ccModelPref();
      json req = {{"subtype", "set_model"}};
      if (!c->procModel.empty()) req["model"] = toUtf8(c->procModel);
      ccControl(c, req, 8000, [sendNow](const json&) { sendNow(); });
    } else sendNow();
  });
}
// a question put to Claude from the editor (commands, context menu)
static void ccAsk(CcChat* c, const wstring& text, bool noContext = false) {
  if (ccBusy(c)) { flash(L"Claude is still working on the last message"); return; }
  ccSend(c, text, noContext);
}

/* ---------------------------------------------------------------- cards that wait for an answer */
static void ccAnswered(CcBlock* b, const wstring& label, bool ok) {
  b->done = true;
  b->ok = ok;
  b->label = label;
  b->dirty = true;
  redraw();
}
static const wchar_t* ccToolLabel(const wstring& n) {
  static const std::map<wstring, const wchar_t*> m = {
      {L"Read", L"Read"}, {L"Edit", L"Edit"}, {L"Write", L"Write"}, {L"MultiEdit", L"Edit"}, {L"NotebookEdit", L"Edit notebook"}, {L"Glob", L"Find files"},
      {L"Grep", L"Search"}, {L"Bash", L"Run"}, {L"WebFetch", L"Fetch"}, {L"WebSearch", L"Search the web"}, {L"Task", L"Agent"}, {L"TodoWrite", L"Plan"},
      {L"ExitPlanMode", L"Plan"}, {L"AskUserQuestion", L"Question"}, {L"BashOutput", L"Output"}, {L"KillShell", L"Stop command"}};
  auto it = m.find(n);
  return it == m.end() ? nullptr : it->second;
}
static wstring ccToolLabelOf(const wstring& n) {
  const wchar_t* l = ccToolLabel(n);
  return l ? wstring(l) : n;
}
static void ccPermissionCard(CcChat* c, const json& ev) {
  if (ccCur() != c) c->attention = true;
  json r = ev.value("request", json::object());
  wstring name = fromUtf8(r.value("tool_name", ""));
  CcBlock* b = ccAdd(c, CB_ASK);
  b->tname = name;
  b->input = r.contains("input") && r["input"].is_object() ? r["input"] : json::object();
  b->rid = ev.value("request_id", "");
  b->sug = r.contains("permission_suggestions") && r["permission_suggestions"].is_array() ? r["permission_suggestions"] : json::array();
  b->askKind = name == L"ExitPlanMode" ? 1 : name == L"AskUserQuestion" ? 2 : 0;
  if (b->askKind == 2) {
    size_t n = b->input.contains("questions") && b->input["questions"].is_array() ? b->input["questions"].size() : 0;
    b->picked.assign(n, {});
    b->others.assign(n, LineEdit());
  }
  c->stick = true;
}
static void ccRespond(CcChat* c, const std::string& rid, const json& resp, bool isErr = false) { clRespond(c->convId, rid, resp, isErr); }
// button n of a card: tool: 0 Allow, 1 Always allow, 2 Deny; plan: 0 edit, 1 ask, 2 no; question: 0 send, 1 skip
static void ccAskButton(CcChat* c, CcBlock* b, int n) {
  if (b->done) return;
  if (b->askKind == 0) {
    wstring label = ccToolLabelOf(b->tname);
    if (n == 2) {
      ccRespond(c, b->rid, {{"behavior", "deny"}, {"message", "The user denied this action."}});
      ccAnswered(b, L"Denied " + label, false);
    } else {
      json resp = {{"behavior", "allow"}, {"updatedInput", b->input}};
      if (n == 1) {
        if (!b->sug.empty()) resp["updatedPermissions"] = b->sug;
        else {
          std::string rule = toUtf8(b->tname);
          if (b->tname == L"Bash" && b->input.contains("command") && b->input["command"].is_string()) {
            std::string cmd = b->input["command"].get<std::string>(), two;
            int words = 0;
            size_t i = 0;
            while (i < cmd.size() && words < 2) {
              while (i < cmd.size() && isspace((unsigned char)cmd[i])) i++;
              size_t s = i;
              while (i < cmd.size() && !isspace((unsigned char)cmd[i])) i++;
              if (i > s) { if (!two.empty()) two += " "; two += cmd.substr(s, i - s); words++; }
            }
            rule = "Bash(" + two + " *)";
          }
          json list = g_settings["claude_allowed_tools"].is_array() ? g_settings["claude_allowed_tools"] : json::array();
          bool have = false;
          for (auto& x : list) if (x.is_string() && x.get<std::string>() == rule) have = true;
          if (!have) { list.push_back(rule); setSetting("claude_allowed_tools", list); }
        }
        // a mode suggestion changes the mode of the running session: show it
        for (auto& s : b->sug)
          if (s.is_object() && s.value("type", "") == "setMode") {
            wstring m = fromUtf8(s.value("mode", ""));
            for (auto& md : CC_MODES) if (m == md.id) { g_ccPrefMode = m; g_ccModeSet = true; c->procMode = m; ccSaveStore(); }
          }
      }
      ccRespond(c, b->rid, resp);
      ccAnswered(b, L"Allowed " + label, true);
    }
  } else if (b->askKind == 1) {
    if (n == 2) {
      ccRespond(c, b->rid, {{"behavior", "deny"}, {"message", "The user wants to keep planning. Ask what to change in the plan."}});
      ccAnswered(b, L"Plan not approved", false);
      return;
    }
    g_ccPrefMode = n == 0 ? L"acceptEdits" : L"manual";
    g_ccModeSet = true;
    ccSaveStore();
    ccRespond(c, b->rid, {{"behavior", "allow"}, {"updatedInput", b->input}});
    ccAnswered(b, L"Plan approved", true);
    if (c->procOpen) {
      c->procMode = g_ccPrefMode;
      ccControl(c, {{"subtype", "set_permission_mode"}, {"mode", toUtf8(g_ccPrefMode)}}, 8000, [](const json&) {});
    }
  } else {
    if (n == 1) {
      ccRespond(c, b->rid, {{"behavior", "deny"}, {"message", "The user chose not to answer."}});
      ccAnswered(b, L"Question skipped", false);
      return;
    }
    json answers = json::object();
    const json& qs = b->input["questions"];
    for (size_t qi = 0; qi < qs.size(); qi++) {
      std::string joined;
      auto add = [&](const std::string& s) { if (!joined.empty()) joined += ", "; joined += s; };
      if (qi < b->picked.size() && qs[qi].contains("options") && qs[qi]["options"].is_array())
        for (int oi : b->picked[qi]) if (oi < (int)qs[qi]["options"].size()) add(qs[qi]["options"][oi].value("label", ""));
      if (qi < b->others.size()) { wstring o = trimW(b->others[qi].t); if (!o.empty()) add(toUtf8(o)); }
      answers[qs[qi].value("question", "")] = joined;
    }
    json upd = b->input;
    upd["answers"] = answers;
    ccRespond(c, b->rid, {{"behavior", "allow"}, {"updatedInput", upd}});
    ccAnswered(b, L"Answered", true);
  }
}

/* ---------------------------------------------------------------- what a turn changed */
static void ccNoteTouch(CcChat* c, const wstring& name, const json& input) {
  if (!c->current) return;
  std::string f0;
  if (input.is_object()) {
    if (input.contains("file_path") && input["file_path"].is_string()) f0 = input["file_path"].get<std::string>();
    else if (input.contains("notebook_path") && input["notebook_path"].is_string()) f0 = input["notebook_path"].get<std::string>();
  }
  if (f0.empty() || !(name == L"Edit" || name == L"Write" || name == L"MultiEdit" || name == L"NotebookEdit")) return;
  wstring f = ccAbsPath(c, fromUtf8(f0));
  CcTouched* info = nullptr;
  for (auto& t : c->current->touched) if (_wcsicmp(t.file.c_str(), f.c_str()) == 0) info = &t;
  if (!info) { c->current->touched.push_back({f, 0, 0, {}}); info = &c->current->touched.back(); }
  auto lines = [](const std::string& s) { return s.empty() ? 0 : (int)std::count(s.begin(), s.end(), '\n') + 1; };
  auto str = [&](const json& j, const char* k) { return j.contains(k) && j[k].is_string() ? j[k].get<std::string>() : std::string(); };
  if (name == L"Write") { info->add += lines(str(input, "content")); info->hunks.push_back({L"Write", false, L"", fromUtf8(str(input, "content"))}); }
  else if (name == L"Edit") {
    info->add += lines(str(input, "new_string"));
    info->del += lines(str(input, "old_string"));
    info->hunks.push_back({L"Edit", true, fromUtf8(str(input, "old_string")), fromUtf8(str(input, "new_string"))});
  } else if (name == L"MultiEdit" && input.contains("edits") && input["edits"].is_array()) {
    for (auto& e : input["edits"]) {
      info->add += lines(str(e, "new_string"));
      info->del += lines(str(e, "old_string"));
      info->hunks.push_back({L"Edit", true, fromUtf8(str(e, "old_string")), fromUtf8(str(e, "new_string"))});
    }
  }
}
static wstring ccHunksToDiff(const wstring& file, const CcTouched& info) {
  wstring out = L"--- " + file + L"\n+++ " + file + L"\n";
  for (auto& h : info.hunks) {
    out += L"@@ " + h.tool + L" @@\n";
    if (h.hasOld && !h.oldS.empty()) for (auto& l : mdSplitLines(h.oldS)) out += L"-" + l + L"\n";
    for (auto& l : mdSplitLines(h.newS)) out += L"+" + l + L"\n";
  }
  return out;
}
static void ccChangesCard(CcChat* c, CcTurn* turn) {
  if (turn->touched.empty()) return;
  CcBlock* b = ccAdd(c, CB_CHANGES);
  b->touched = turn->touched;
  b->turnUuid = turn->uuid;
}
static void ccRewind(CcChat* c, const std::string& uuid, std::function<void(bool, wstring)> done) {
  ccControl(c, {{"subtype", "rewind_files"}, {"user_message_id", uuid}}, 30000, [done](const json& r) {
    bool ok = r.is_object() && !r.contains("error") && r.value("subtype", "") != "error";
    if (ok && r.contains("response") && r["response"].is_object() && r["response"].value("subtype", "") == "error") ok = false;
    wstring why;
    if (!ok) {
      if (r.contains("error") && r["error"].is_string()) why = fromUtf8(r["error"].get<std::string>());
      else if (r.contains("response") && r["response"].is_object() && r["response"].contains("error") && r["response"]["error"].is_string()) why = fromUtf8(r["response"]["error"].get<std::string>());
      else why = L"not available";
    }
    done(ok, why);
  });
}

/* ---------------------------------------------------------------- sign in / sign out */
static void ccStartAuth(CcChat* c, const wstring& action, bool consoleLogin) {
  wstring id = L"auth" + std::to_wstring(++g_ccAuthSeq) + L"-" + std::to_wstring(c->convId);
  g_ccAuthIds.insert(id);
  if (!c->blocks.empty() && c->blocks[0]->kind == CB_WELCOME) { delete c->blocks[0]; c->blocks.erase(c->blocks.begin()); }
  CcBlock* b = ccAdd(c, CB_AUTH);
  b->authId = id;
  b->authAction = action;
  b->label = action == L"logout" ? L"Signing out\x2026" : consoleLogin ? L"Sign in with the Anthropic Console: finish in the browser window that opens\x2026" : L"Sign in: finish in the browser window that opens\x2026";
  wstring err;
  if (!clAuthRun(id, action, consoleLogin, sStr("claude_path"), err)) {
    b->kind = CB_FAIL;
    b->text = err;
    g_ccAuthIds.erase(id);
  }
  c->stick = true;
  redraw();
}
static void ccAuthMessage(ClAuthMsg* m) {
  CcBlock* b = nullptr;
  CcChat* owner = nullptr;
  for (auto* c : g_ccChats) for (auto* x : c->blocks) if (x->kind == CB_AUTH && x->authId == m->id) { b = x; owner = c; }
  if (!b) return;
  if (!m->text.empty()) {
    b->authLog += fromUtf8(m->text);
    if (b->authLog.size() > 2000) b->authLog = b->authLog.substr(b->authLog.size() - 2000);
    b->dirty = true;
  }
  if (m->exited) {
    g_ccAuthIds.erase(m->id);
    clAuthForget(m->id);
    bool ok = m->code == 0;
    b->done = true;
    b->ok = ok;
    b->label = ok ? (b->authAction == L"logout" ? L"Signed out." : L"Signed in.") : (b->authAction == L"logout" ? L"Could not sign out." : L"Sign-in did not finish.");
    b->dirty = true;
    if (ok) {
      // the running processes hold the old account: start fresh on the next message
      for (auto* c : g_ccChats) ccResetProc(c);
      g_ccAuthAsked = false;
      g_ccAuthInfo = json();
      flash(b->authAction == L"logout" ? L"Signed out of Claude Code" : L"Signed in to Claude Code");
    }
    (void)owner;
  }
  redraw();
}

/* ---------------------------------------------------------------- events from the process */
static void ccOnEvent(CcChat* c, const json& ev) {
  std::string type = ev.value("type", "");
  if (type == "system" && ev.contains("session_id") && ev["session_id"].is_string()) { c->sessionId = fromUtf8(ev["session_id"].get<std::string>()); ccPersist(); }
  if (type == "system" && ev.value("subtype", "") == "init") {
    if (ev.contains("model") && ev["model"].is_string()) { c->activeModel = fromUtf8(ev["model"].get<std::string>()); redraw(); }
    if (ev.contains("slash_commands") && ev["slash_commands"].is_array()) {
      c->slashCommands.clear();
      for (auto& s : ev["slash_commands"]) if (s.is_string()) { wstring n = fromUtf8(s.get<std::string>()); if (!n.empty() && n[0] == L'/') n.erase(0, 1); c->slashCommands.push_back(n); }
    }
  }
  if (type == "rate_limit_event") { if (ev.contains("rate_limit_info")) ccShowUsage(c, ev["rate_limit_info"]); return; }
  if (type == "control_response") {
    json r = ev.value("response", json::object());
    std::string rid = r.value("request_id", "");
    auto it = c->waiters.find(rid);
    if (it != c->waiters.end()) {
      auto cb = it->second.cb;
      c->waiters.erase(it);
      cb(r);
    }
    return;
  }
  if (type == "control_request") {
    json r = ev.value("request", json::object());
    if (r.value("subtype", "") == "can_use_tool") ccPermissionCard(c, ev);
    else ccRespond(c, ev.value("request_id", ""), "Not supported by this editor", true);
    return;
  }
  if (type == "exit") {
    CcTurn* was = c->current;
    c->procOpen = false;
    ccStopWorking(c);
    if (was) {
      if (was->think) { was->think->live = false; was->think->dirty = true; }
      if (ev.value("killed", false)) ccAdd(c, CB_HINT, L"Stopped.");
      else if (!was->shown) {
        int code = ev.value("code", 0);
        ccAdd(c, CB_FAIL, L"Claude Code ended" + (code ? L" with code " + std::to_wstring(code) : wstring()) + L" before answering. Check the path and your sign-in under Settings > Claude Code.");
      }
      c->current = nullptr;
      vector<CcTouched> files = was->touched;
      delete was;
      ccAfterChange();
    }
    ccLoadSessions(c);
    return;
  }
  if (type == "stderr") {
    wstring t = trimW(fromUtf8(ev.value("text", "")));
    if (t.empty()) return;
    wstring lt = lowerW(t);
    if (lt.find(L"has not been trusted") != wstring::npos || lt.find(L"trust dialog") != wstring::npos) {
      if (c->trustShown == c->cwd) return;
      c->trustShown = c->cwd;
      ccAdd(c, CB_TRUST,
            L"Claude Code has not been asked to trust this folder yet, so the permission rules of the project (.claude/settings.json) are ignored. Claude still works, but asks more often. "
            L"To trust it, use Tools > Claude Code: Trust This Folder (then start a new conversation), or run claude once in this folder from a terminal and accept the question.");
      return;
    }
    logLine(L"claude stderr: " + t.substr(0, 400));
    ccAdd(c, CB_WARN, t.substr(0, 600));
    return;
  }
  if (!c->current) {
    if (type == "result" && ev.contains("session_id") && ev["session_id"].is_string()) c->sessionId = fromUtf8(ev["session_id"].get<std::string>());
    return;
  }
  CcTurn* cur = c->current;
  bool toolResult = false;
  if (ev.contains("message") && ev["message"].is_object() && ev["message"].contains("content") && ev["message"]["content"].is_array())
    for (auto& b : ev["message"]["content"]) if (b.is_object() && b.value("type", "") == "tool_result") toolResult = true;
  if (type == "user" && ev.contains("uuid") && ev["uuid"].is_string() && ev.contains("message") && !toolResult) {
    // the echo of our message carries the id used to rewind files
    if (!c->userBubbles.empty()) { c->userBubbles.front()->uuid = ev["uuid"].get<std::string>(); c->userBubbles.erase(c->userBubbles.begin()); }
    if (cur->uuid.empty()) cur->uuid = ev["uuid"].get<std::string>();
    return;
  }
  if (type == "stream_event" && ev.contains("event") && ev["event"].is_object()) {
    const json& e = ev["event"];
    std::string et = e.value("type", "");
    if (et == "message_start") {
      cur->msgId = e.contains("message") && e["message"].is_object() ? e["message"].value("id", "") : "";
      if (e.contains("message") && e["message"].is_object() && e["message"].contains("model") && e["message"]["model"].is_string()) c->activeModel = fromUtf8(e["message"]["model"].get<std::string>());
      cur->bubble = nullptr;
      cur->text.clear();
    } else if (et == "content_block_start" && e.contains("content_block") && e["content_block"].is_object()) {
      std::string t = e["content_block"].value("type", "");
      if (t == "thinking" || t == "redacted_thinking") {
        cur->think = ccAddThink(c, L"", true);
        cur->thinkIds.insert(cur->msgId);
        cur->bubble = nullptr;
      }
    } else if (et == "content_block_delta" && e.contains("delta") && e["delta"].is_object()) {
      const json& d = e["delta"];
      std::string dt = d.value("type", "");
      if (dt == "text_delta") {
        if (!cur->bubble || cur->bubble->kind != CB_AI) { cur->bubble = ccAdd(c, CB_AI); cur->text.clear(); }
        cur->text += fromUtf8(d.value("text", ""));
        cur->streamed.insert(cur->msgId);
        cur->shown = true;
        cur->bubble->text = cur->text;
        cur->bubble->dirty = true;
        redraw();
      } else if (dt == "thinking_delta" && cur->think) {
        cur->think->text += fromUtf8(d.value("thinking", ""));
        cur->think->dirty = true;
        redraw();
      }
    } else if (et == "content_block_stop" && cur->think) {
      long secs = std::max(1L, (long)std::lround((GetTickCount64() - cur->think->t0) / 1000.0));
      cur->think->live = false;
      cur->think->label = L"Thought for " + std::to_wstring(secs) + L"s";
      cur->think->dirty = true;
      cur->think = nullptr;
      redraw();
    }
  } else if (type == "assistant" && ev.contains("message") && ev["message"].is_object()) {
    const json& m = ev["message"];
    std::string mid = m.value("id", "");
    if (m.contains("model") && m["model"].is_string()) { wstring mm = fromUtf8(m["model"].get<std::string>()); if (mm != c->activeModel) c->activeModel = mm; }
    if (m.contains("usage") && m["usage"].is_object() && !(ev.contains("parent_tool_use_id") && ev["parent_tool_use_id"].is_string())) {
      const json& u = m["usage"];
      int tok = u.value("input_tokens", 0) + u.value("cache_read_input_tokens", 0) + u.value("cache_creation_input_tokens", 0);
      if (tok) cur->ctx = tok;
    }
    if (m.contains("content") && m["content"].is_array()) {
      for (auto& b : m["content"]) {
        if (!b.is_object()) continue;
        std::string bt = b.value("type", "");
        if (bt == "thinking" && !cur->thinkIds.count(mid)) {
          std::string th = b.value("thinking", "");
          CcBlock* el = ccAddThink(c, fromUtf8(th), false);
          if (th.empty()) el->emptyThink = true;
        } else if (bt == "text" && !b.value("text", "").empty() && !cur->streamed.count(mid)) {
          cur->bubble = ccAdd(c, CB_AI, fromUtf8(b.value("text", "")));
          cur->shown = true;
        } else if (bt == "tool_use") {
          wstring nm = fromUtf8(b.value("name", ""));
          if (nm != L"ExitPlanMode" && nm != L"AskUserQuestion") ccAddTool(c, nm, b.contains("input") ? b["input"] : json::object(), b.value("id", ""));
          ccNoteTouch(c, nm, b.contains("input") ? b["input"] : json::object());
          cur->bubble = nullptr;
          cur->shown = true;
        }
      }
    }
  } else if (type == "user" && ev.contains("message") && ev["message"].is_object() && ev["message"].contains("content") && ev["message"]["content"].is_array()) {
    for (auto& b : ev["message"]["content"]) {
      if (!b.is_object() || b.value("type", "") != "tool_result") continue;
      std::string tr = clTrim(b.contains("content") ? ccResultText(b["content"]) : "");
      bool isErr = b.value("is_error", false);
      auto it = c->toolEls.find(b.value("tool_use_id", ""));
      if (it != c->toolEls.end()) {
        CcBlock* el = it->second;
        if (!tr.empty()) { std::string cut = tr.substr(0, 4000); el->result = fromUtf8(cut); }
        el->resErr = isErr;
        if (isErr) el->open = true;
        el->dirty = true;
      } else if (isErr && !tr.empty()) {
        CcBlock* e = ccAdd(c, CB_FAIL, fromUtf8(tr.substr(0, 300)));
        (void)e;
      }
      redraw();
    }
    ccLater(250, [] { ccAfterChange(); });
  } else if (type == "result") {
    if (ev.contains("total_cost_usd") && ev["total_cost_usd"].is_number()) c->cost = ev["total_cost_usd"].get<double>();
    c->ctxTokens = cur->ctx;
    ccRenderCost(c);
    if (ev.contains("session_id") && ev["session_id"].is_string()) { c->sessionId = fromUtf8(ev["session_id"].get<std::string>()); ccPersist(); }
    if (ev.value("is_error", false)) {
      std::string r = ev.contains("result") && ev["result"].is_string() ? ev["result"].get<std::string>() : ev.value("subtype", "Claude reported an error");
      ccAdd(c, CB_FAIL, fromUtf8(r));
    } else if (!cur->shown && ev.contains("result") && ev["result"].is_string() && !ev["result"].get<std::string>().empty()) ccAdd(c, CB_AI, fromUtf8(ev["result"].get<std::string>()));
    CcTurn* turn = cur;
    c->current = nullptr;
    ccStopWorking(c);
    if (turn->think) { turn->think->live = false; turn->think->dirty = true; }
    ccChangesCard(c, turn);
    delete turn;
    ccAfterChange();
    ccLoadSessions(c);
  }
  redraw();
}
static void ccHandleEvent(int conv, const std::string& line) {
  json ev = json::parse(line, nullptr, false);
  if (!ev.is_object()) ev = {{"type", "raw"}, {"text", line}};
  bool isExit = ev.value("type", "") == "exit";
  CcChat* owner = nullptr;
  for (auto* c : g_ccChats) if (c->convId == conv) owner = c;
  if (owner) ccOnEvent(owner, ev);
  if (isExit) clForget(conv);
}
static void ccTick() {
  DWORD now = GetTickCount();
  for (size_t i = 0; i < g_ccLater.size();) {
    if ((int)(g_ccLater[i].first - now) <= 0) {
      auto fn = g_ccLater[i].second;
      g_ccLater.erase(g_ccLater.begin() + i);
      fn();
    } else i++;
  }
  for (auto* c : g_ccChats) {
    for (auto it = c->waiters.begin(); it != c->waiters.end();) {
      if ((int)(it->second.deadline - now) <= 0) {
        auto cb = it->second.cb;
        it = c->waiters.erase(it);
        cb({{"error", "No answer"}});
      } else ++it;
    }
    if (c->working && now % 2200 < 260) c->verb = CC_VERBS[rand() % (sizeof(CC_VERBS) / sizeof(CC_VERBS[0]))];
  }
}

/* ---------------------------------------------------------------- the dock: one chat per tab */
static CcChat* ccAddChat(const wstring& sessionId = L"", const wstring& title = L"") {
  if (sessionId.empty())
    for (size_t i = 0; i < g_ccChats.size(); i++)
      if (g_ccChats[i]->blocks.size() <= 1 && !ccBusy(g_ccChats[i]) && g_ccChats[i]->sessionId.empty() && (g_ccChats[i]->blocks.empty() || g_ccChats[i]->blocks[0]->kind == CB_WELCOME)) {
        g_ccActive = (int)i;
        return g_ccChats[i];
      }
  auto* c = new CcChat();
  c->convId = ++g_ccConvSeq;
  c->cwd = ccGetCwd();
  g_ccChats.push_back(c);
  g_ccActive = (int)g_ccChats.size() - 1;
  ccLoadSessions(c);
  if (!sessionId.empty()) ccOpenSession(c, sessionId, title);
  else ccShowWelcome(c);
  return c;
}
static void ccActivate(int i) {
  if (i < 0 || i >= (int)g_ccChats.size()) return;
  g_ccActive = i;
  g_ccChats[i]->attention = false;
  g_ccPop.kind = 0;
  redraw();
}
static void ccHide();
static void ccCloseTab(int i) {
  if (i < 0 || i >= (int)g_ccChats.size()) return;
  CcChat* c = g_ccChats[i];
  ccResetProc(c);
  clClose(c->convId);
  g_ccChats.erase(g_ccChats.begin() + i);
  delete c;
  if (g_ccChats.empty()) { g_ccActive = -1; ccHide(); return; }
  g_ccActive = std::min(i, (int)g_ccChats.size() - 1);
  ccSaveStore();
  redraw();
}
static void ccOpenSessionTab(const wstring& id, const wstring& title) {
  for (size_t i = 0; i < g_ccChats.size(); i++) if (g_ccChats[i]->sessionId == id) { ccActivate((int)i); return; }
  ccAddChat(id, title);
}
static void computeLayout();
static void ccShow() {
  g_ccOpen = true;
  if (!g_ccRestored) {
    g_ccRestored = true;
    bool any = false;
    if (g_ccStoreTabs.is_array())
      for (auto& t : g_ccStoreTabs) {
        if (!t.is_object() || !t.contains("sid") || !t["sid"].is_string()) continue;
        ccAddChat(fromUtf8(t["sid"].get<std::string>()), fromUtf8(t.value("title", "")));
        any = true;
      }
    if (any) g_ccActive = std::max(0, std::min((int)g_ccChats.size() - 1, g_ccStoreActive));
    else ccAddChat();
  }
  g_ccFocusInput = true; g_panelFocus = 0; g_findFocus = false; g_nqFocus = false;
  computeLayout();
  ccSaveStore();
  redraw();
}
static void ccHide() {
  g_ccOpen = false;
  g_ccFocusInput = false;
  g_ccPop.kind = 0;
  computeLayout();
  ccSaveStore();
  redraw();
}
static void ccAddSelection(const wstring& file, int from, int to, const wstring& text) {
  ccShow();
  CcChat* c = ccCur();
  if (!c) return;
  CcAtt a;
  a.kind = 2;
  a.file = file;
  a.from = from;
  a.to = to;
  a.text = text.substr(0, 20000);
  a.name = baseName(file) + L":" + std::to_wstring(from) + (to != from ? L"-" + std::to_wstring(to) : L"");
  c->atts.push_back(a);
  g_ccFocusInput = true;
  redraw();
}
static void ccShutdown() {
  ccSaveStore();
  clShutdownAll();
}

// Claude Code keeps "this folder is trusted" in ~/.claude.json (projects -> <folder> -> hasTrustDialogAccepted). Scriba sets it for one folder,
// and only after the user has said yes: a trusted folder may run the hooks and commands of its .claude/settings.json.
static void ccTrustFolder() {
  CcChat* c = ccCur();
  wstring cwd = c ? c->cwd : L"";
  if (cwd.empty() && g_root) cwd = g_root->path;
  if (cwd.empty() && !g_docs.empty() && !D().path.empty()) { size_t k = D().path.find_last_of(L"\\/"); if (k != wstring::npos) cwd = D().path.substr(0, k); }
  if (cwd.empty()) { flash(L"Open a folder or a file first"); return; }
  wchar_t home[MAX_PATH];
  if (!GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH)) { flash(L"Cannot find the user folder"); return; }
  wstring cfg = wstring(home) + L"\\.claude.json";
  wstring ask = L"Trust this folder for Claude Code?\n\n" + cwd +
                L"\n\nClaude Code will then follow the permission rules, hooks and commands in the .claude folder of this project. Say yes only for folders you wrote or know. Scriba writes the flag into " + cfg + L" (a copy is kept next to it).";
  if (appMsgBox(g_hwnd, ask.c_str(), L"Trust this folder", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
  using oj = nlohmann::ordered_json;
  oj root = oj::object();
  vector<char> raw;
  if (readAll(cfg, raw) && !raw.empty()) {
    root = oj::parse(raw.begin(), raw.end(), nullptr, false);
    if (!root.is_object()) { flash(L"~/.claude.json is not in the expected format; nothing changed"); return; }
    CopyFileW(cfg.c_str(), (cfg + L".scriba-backup").c_str(), FALSE);
  }
  auto norm = [](std::string s) { for (auto& ch : s) { if (ch == '\\') ch = '/'; ch = (char)tolower((unsigned char)ch); } while (s.size() > 1 && s.back() == '/') s.pop_back(); return s; };
  std::string want = norm(toUtf8(cwd)), key;
  if (!root.contains("projects") || !root["projects"].is_object()) root["projects"] = oj::object();
  for (auto it = root["projects"].begin(); it != root["projects"].end(); ++it) if (norm(it.key()) == want) { key = it.key(); break; }
  if (key.empty()) {  // new entry, written the way Claude Code writes its keys: forward slashes, drive letter in lower case
    key = toUtf8(cwd);
    for (auto& ch : key) if (ch == '\\') ch = '/';
    if (key.size() > 1 && key[1] == ':') key[0] = (char)tolower((unsigned char)key[0]);
    root["projects"][key] = oj::object();
  }
  root["projects"][key]["hasTrustDialogAccepted"] = true;
  std::string out = root.dump(2);
  wstring tmp = cfg + L".scriba-tmp";
  HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  DWORD wr = 0;
  bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, out.data(), (DWORD)out.size(), &wr, nullptr) && wr == out.size();
  if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  if (!ok || !MoveFileExW(tmp.c_str(), cfg.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(tmp.c_str()); logLine(L"could not write " + cfg); flash(L"Could not write ~/.claude.json"); return; }
  flash(L"Folder trusted. Start a new Claude conversation for it to apply.");
}
