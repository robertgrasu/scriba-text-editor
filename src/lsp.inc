// lsp.inc: Language Server Protocol client (stdio JSON-RPC): errors as you type, completion, hover docs, go to definition,
// find references, rename, format. Servers are found in the project's node_modules\.bin or on the PATH. Included by main.cpp.

#define WM_LSP_MSG (WM_APP + 8)
#define WM_LSP_EXIT (WM_APP + 9)

struct LspDiag { int l0, c0, l1, c1, sev; wstring msg, source; };
struct LspConfig { wstring id, command; vector<wstring> args; };
struct LspServer {
  wstring key, command, id, root, syntax;
  vector<wstring> args;
  HANDLE hin = nullptr, hproc = nullptr, hjob = nullptr;
  bool ready = false, failed = false;
  json caps = json::object();
  std::map<int, std::function<void(const json&, bool)>> pending;  // id -> callback(result, ok)
  std::map<int, DWORD> pendingAt;
  wstring lastError;
};
struct LspMsg { wstring key; std::string body; };
static std::map<wstring, LspServer*> g_lspServers;
static std::set<wstring> g_lspMissing;  // commands known not to exist
static std::map<wstring, vector<LspDiag>> g_lspDiags;  // lower-case path -> diagnostics
static int g_lspNextId = 1;
static const DWORD LSP_TIMEOUT = 15000;

static std::string lspPathToUri(const wstring& p) {
  std::string u = toUtf8(p), out = "file:///";
  for (auto& c : u) if (c == '\\') c = '/';
  for (unsigned char c : u) {
    if (isalnum(c) || strchr("-_.!~*'()/:;@&=+$,", c)) out += (char)c;
    else { char b[8]; snprintf(b, sizeof(b), "%%%02X", c); out += b; }
  }
  return out;
}
static wstring lspUriToPath(const std::string& uri) {
  if (uri.compare(0, 7, "file://") != 0) return L"";
  std::string s = uri.substr(7), d;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size() + 0 && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) { d += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
    else d += s[i];
  }
  wstring p = fromUtf8(d);
  if (p.size() > 2 && p[0] == L'/' && p[2] == L':') p.erase(0, 1);
  for (auto& c : p) if (c == L'/') c = L'\\';
  return p;
}
static wstring lspKey(const wstring& p) { return lowerPath(p); }

/* ---------------------------------------------------------------- which server for which syntax */
static bool lspConfigFor(const wstring& syntax, LspConfig& out) {
  if (!sBool("lsp_enabled")) return false;
  std::string key = toUtf8(syntax);
  const json& user = g_settings["lsp_servers"];
  if (user.is_object() && user.contains(key)) {
    const json& u = user[key];
    if (u.is_null()) return false;
    if (u.is_object() && u.contains("command") && u["command"].is_string()) {
      out.command = fromUtf8(u["command"].get<std::string>());
      out.id = u.contains("id") && u["id"].is_string() ? fromUtf8(u["id"].get<std::string>()) : lowerStr(syntax);
      static const std::map<wstring, wstring> ids = {{L"JavaScript", L"javascript"}, {L"TypeScript", L"typescript"}, {L"Python", L"python"}, {L"C++", L"cpp"}, {L"C", L"c"}, {L"Rust", L"rust"}, {L"Go", L"go"}, {L"HTML", L"html"}, {L"CSS", L"css"}, {L"JSON", L"json"}, {L"Shell", L"shellscript"}, {L"PHP", L"php"}, {L"Java", L"java"}, {L"YAML", L"yaml"}};
      if (!(u.contains("id") && u["id"].is_string()) && ids.count(syntax)) out.id = ids.at(syntax);
      out.args.clear();
      if (u.contains("args") && u["args"].is_array()) for (auto& a : u["args"]) if (a.is_string()) out.args.push_back(fromUtf8(a.get<std::string>()));
      return true;
    }
  }
  struct Def { const wchar_t* syntax; const wchar_t* id; const wchar_t* command; vector<const wchar_t*> args; };
  static const Def defs[] = {
      {L"JavaScript", L"javascript", L"typescript-language-server", {L"--stdio"}}, {L"TypeScript", L"typescript", L"typescript-language-server", {L"--stdio"}},
      {L"Python", L"python", L"pyright-langserver", {L"--stdio"}}, {L"C++", L"cpp", L"clangd", {}}, {L"C", L"c", L"clangd", {}},
      {L"Rust", L"rust", L"rust-analyzer", {}}, {L"Go", L"go", L"gopls", {}}, {L"HTML", L"html", L"vscode-html-language-server", {L"--stdio"}},
      {L"CSS", L"css", L"vscode-css-language-server", {L"--stdio"}}, {L"JSON", L"json", L"vscode-json-language-server", {L"--stdio"}},
      {L"Shell", L"shellscript", L"bash-language-server", {L"start"}}, {L"PHP", L"php", L"intelephense", {L"--stdio"}},
      {L"Java", L"java", L"jdtls", {}}, {L"YAML", L"yaml", L"yaml-language-server", {L"--stdio"}}};
  for (auto& d : defs)
    if (syntax == d.syntax) { out.id = d.id; out.command = d.command; out.args.assign(d.args.begin(), d.args.end()); return true; }
  return false;
}
static wstring lspResolve(const wstring& command, const wstring& root) {
  const wchar_t* exts[] = {L".cmd", L".exe", L".bat", L""};
  if (!root.empty())
    for (auto e : exts) {
      wstring p = root + L"\\node_modules\\.bin\\" + command + e;
      if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
    }
  if (command.size() > 2 && (command[1] == L':' || command[0] == L'\\')) return GetFileAttributesW(command.c_str()) != INVALID_FILE_ATTRIBUTES ? command : L"";
  for (auto e : exts) {
    wchar_t buf[MAX_PATH];
    if (SearchPathW(nullptr, command.c_str(), e[0] ? e : nullptr, MAX_PATH, buf, nullptr)) return buf;
  }
  return L"";
}
static wstring lspRootFor(const wstring& path) {
  for (auto* r : g_roots) {
    wstring rp = r->path;
    while (!rp.empty() && (rp.back() == L'\\' || rp.back() == L'/')) rp.pop_back();
    if (path.size() > rp.size() && _wcsicmp(path.substr(0, rp.size()).c_str(), rp.c_str()) == 0 && path[rp.size()] == L'\\') return rp;
  }
  return dirNameOf(path);
}

/* ---------------------------------------------------------------- JSON-RPC over pipes */
static void lspWrite(LspServer* s, const json& msg) {
  if (!s->hin) return;
  json m = msg;
  m["jsonrpc"] = "2.0";
  std::string body = m.dump(-1, ' ', false, json::error_handler_t::replace);
  std::string head = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
  DWORD wr;
  WriteFile(s->hin, head.data(), (DWORD)head.size(), &wr, nullptr);
  WriteFile(s->hin, body.data(), (DWORD)body.size(), &wr, nullptr);
}
static void lspRequest(LspServer* s, const char* method, const json& params, std::function<void(const json&, bool)> cb, DWORD timeoutMs = LSP_TIMEOUT) {
  int id = g_lspNextId++;
  s->pending[id] = cb;
  s->pendingAt[id] = GetTickCount() + timeoutMs;
  lspWrite(s, {{"id", id}, {"method", method}, {"params", params}});
}
static void lspNotify(LspServer* s, const char* method, const json& params) { lspWrite(s, {{"method", method}, {"params", params}}); }
static void lspReader(HANDLE h, wstring key, HWND hw) {
  std::string buf;
  char tmp[16384];
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(h, tmp, sizeof(tmp), &got, nullptr) || got == 0) break;
    buf.append(tmp, got);
    for (;;) {
      size_t sep = buf.find("\r\n\r\n");
      if (sep == std::string::npos) break;
      std::string head = buf.substr(0, sep);
      size_t cl = head.find("ontent-Length:");
      if (cl == std::string::npos) cl = head.find("ontent-length:");
      if (cl == std::string::npos) { buf.erase(0, sep + 4); continue; }
      size_t len = (size_t)atoll(head.c_str() + cl + 14);
      if (buf.size() < sep + 4 + len) break;
      PostMessageW(hw, WM_LSP_MSG, 0, (LPARAM) new LspMsg{key, buf.substr(sep + 4, len)});
      buf.erase(0, sep + 4 + len);
    }
  }
  PostMessageW(hw, WM_LSP_EXIT, 0, (LPARAM) new wstring(key));
}
static void lspDrain(HANDLE h) { char b[4096]; DWORD g; while (ReadFile(h, b, sizeof(b), &g, nullptr) && g) {} }
static LspServer* lspStartServer(const wstring& syntax, const wstring& path, const LspConfig& cfg) {
  wstring root = lspRootFor(path);
  wstring key = syntax + L"|" + cfg.command + L"|" + lowerPath(root);
  auto it = g_lspServers.find(key);
  if (it != g_lspServers.end()) return it->second->failed ? nullptr : it->second;
  wstring missKey = cfg.command + L"|" + lowerPath(root);
  if (g_lspMissing.count(missKey)) return nullptr;
  wstring exe = lspResolve(cfg.command, root);
  if (g_testing) testLog(L"lsp: resolve " + cfg.command + L" -> " + exe);
  if (exe.empty()) { g_lspMissing.insert(missKey); return nullptr; }
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE inR, inW, outR, outW, errR, errW;
  if (!CreatePipe(&inR, &inW, &sa, 0) || !CreatePipe(&outR, &outW, &sa, 0) || !CreatePipe(&errR, &errW, &sa, 0)) return nullptr;
  SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(errR, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = inR;
  si.hStdOutput = outW;
  si.hStdError = errW;
  wstring cmd;
  bool shell = exe.size() > 4 && (_wcsicmp(exe.c_str() + exe.size() - 4, L".cmd") == 0 || _wcsicmp(exe.c_str() + exe.size() - 4, L".bat") == 0);
  if (shell) cmd = L"cmd.exe /d /s /c \"\"" + exe + L"\"";
  else cmd = L"\"" + exe + L"\"";
  for (auto& a : cfg.args) cmd += L" " + a;
  if (shell) cmd += L"\"";
  vector<wchar_t> buf(cmd.begin(), cmd.end());
  buf.push_back(0);
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, root.c_str(), &si, &pi);
  CloseHandle(inR);
  CloseHandle(outW);
  CloseHandle(errW);
  if (g_testing) testLog(L"lsp: CreateProcess " + std::to_wstring(ok) + L" err " + std::to_wstring(GetLastError()) + L" cmd " + cmd);
  if (!ok) { CloseHandle(inW); CloseHandle(outR); CloseHandle(errR); g_lspMissing.insert(missKey); return nullptr; }
  auto* s = new LspServer();
  s->key = key;
  s->command = cfg.command;
  s->id = cfg.id;
  s->args = cfg.args;
  s->root = root;
  s->syntax = syntax;
  s->hin = inW;
  s->hproc = pi.hProcess;
  s->hjob = CreateJobObjectW(nullptr, nullptr);
  if (s->hjob) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
    li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(s->hjob, JobObjectExtendedLimitInformation, &li, sizeof(li));
    AssignProcessToJobObject(s->hjob, pi.hProcess);
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  g_lspServers[key] = s;
  HWND hw = g_hwnd;
  std::thread(lspReader, outR, key, hw).detach();
  std::thread(lspDrain, errR).detach();
  // the initialize handshake
  json init = {{"processId", nullptr}, {"clientInfo", {{"name", "Scriba"}}}, {"rootUri", lspPathToUri(root)}, {"rootPath", toUtf8(root)},
               {"workspaceFolders", json::array({{{"uri", lspPathToUri(root)}, {"name", toUtf8(baseName(root))}}})}};
  init["capabilities"] = {
      {"workspace", {{"workspaceFolders", true}, {"configuration", true}, {"applyEdit", true}, {"workspaceEdit", {{"documentChanges", false}}}}},
      {"textDocument", {{"synchronization", {{"didSave", true}, {"dynamicRegistration", false}}},
                        {"completion", {{"completionItem", {{"snippetSupport", true}, {"documentationFormat", json::array({"markdown", "plaintext"})}, {"insertReplaceSupport", false}}}, {"contextSupport", true}}},
                        {"hover", {{"contentFormat", json::array({"markdown", "plaintext"})}}},
                        {"definition", {{"linkSupport", false}}}, {"references", json::object()}, {"rename", {{"prepareSupport", false}}},
                        {"formatting", json::object()}, {"publishDiagnostics", {{"relatedInformation", false}}}}}};
  lspRequest(s, "initialize", init, [s](const json& res, bool ok) {
    if (!ok || !res.is_object()) { s->failed = true; return; }
    s->caps = res.contains("capabilities") ? res["capabilities"] : json::object();
    s->ready = true;
    lspNotify(s, "initialized", json::object());
    for (auto* d : g_docs) d->lspDue = GetTickCount();  // open the documents that were waiting
  }, 30000);
  return s;
}

/* ---------------------------------------------------------------- documents */
static wstring docText(const Doc& d) {
  wstring t;
  for (size_t i = 0; i < d.lines.size(); i++) { if (i) t += L"\n"; t += d.lines[i]; }
  return t;
}
static LspServer* lspServerOf(const Doc& d) {
  if (d.lspSrv.empty()) return nullptr;
  auto it = g_lspServers.find(d.lspSrv);
  return it == g_lspServers.end() || it->second->failed || !it->second->ready ? nullptr : it->second;
}
static void lspOpenDoc(Doc& d) {
  if (d.path.empty() || d.isNote || !d.lspSrv.empty()) return;
  LspConfig cfg;
  if (!lspConfigFor(d.langName, cfg)) return;
  LspServer* s = lspStartServer(d.langName, d.path, cfg);
  if (!s || !s->ready) return;
  d.lspSrv = s->key;
  d.lspUri = lspPathToUri(d.path);
  d.lspVersion = 1;
  d.lspSentVer = d.ver;
  lspNotify(s, "textDocument/didOpen", {{"textDocument", {{"uri", d.lspUri}, {"languageId", toUtf8(s->id)}, {"version", 1}, {"text", toUtf8(docText(d))}}}});
}
static void lspFlush(Doc& d) {
  LspServer* s = lspServerOf(d);
  if (!s || d.lspSentVer == d.ver) return;
  d.lspSentVer = d.ver;
  d.lspVersion++;
  lspNotify(s, "textDocument/didChange", {{"textDocument", {{"uri", d.lspUri}, {"version", d.lspVersion}}}, {"contentChanges", json::array({{{"text", toUtf8(docText(d))}}})}});
}
static void lspDocClosedImpl(Doc& d) {
  LspServer* s = lspServerOf(d);
  if (s) lspNotify(s, "textDocument/didClose", {{"textDocument", {{"uri", d.lspUri}}}});
  if (!d.path.empty()) g_lspDiags.erase(lspKey(d.path));
}
static void lspDocSavedImpl(Doc& d) {
  LspServer* s = lspServerOf(d);
  if (!s) { d.lspDue = GetTickCount(); return; }
  lspFlush(d);
  lspNotify(s, "textDocument/didSave", {{"textDocument", {{"uri", d.lspUri}}}, {"text", toUtf8(docText(d))}});
}
static wstring lspStatusText(Doc& d) {
  LspServer* s = lspServerOf(d);
  if (!s) return L"";
  int e = 0, w = 0;
  auto it = g_lspDiags.find(lspKey(d.path));
  if (it != g_lspDiags.end()) for (auto& x : it->second) { if (x.sev == 1) e++; else if (x.sev == 2) w++; }
  wstring r = s->command;
  if (e) r += L"  ✕ " + std::to_wstring(e);
  if (w) r += L"  ▲ " + std::to_wstring(w);
  if (!e && !w) r += L"  ✓";
  return r;
}
// timer: keeps the server in sync with the visible documents
static void testLog(const wstring& s);
static void lspTick() {
  DWORD now = GetTickCount();
  for (auto& kv : g_lspServers)
    for (auto it = kv.second->pending.begin(); it != kv.second->pending.end();) {
      auto at = kv.second->pendingAt.find(it->first);
      if (at != kv.second->pendingAt.end() && now > at->second) {
        auto cb = it->second;
        kv.second->pendingAt.erase(at);
        it = kv.second->pending.erase(it);
        if (cb) cb(json(), false);
      } else ++it;
    }
  for (int g = 0; g < paneCount(); g++) {
    int idx = g == g_curGroup ? g_active : g_groupActive[g];
    if (idx < 0 || idx >= (int)g_docs.size()) continue;
    Doc& d = *g_docs[idx];
    if (d.path.empty() || d.isNote || !sBool("lsp_enabled")) continue;
    if (d.lspSrv.empty()) { if (now >= d.lspDue) { d.lspDue = now + 1500; lspOpenDoc(d); } }
    else if (d.lspSentVer != d.ver && now - d.changedAt > 280) lspFlush(d);
  }
  Doc& a = D();
  wstring st = a.path.empty() ? L"" : lspStatusText(a);
  if (st != g_lspStatus) { g_lspStatus = st; redraw(); }
}

/* ---------------------------------------------------------------- incoming messages */
static wstring lspPlainDoc(const json& c) {
  if (c.is_null()) return L"";
  if (c.is_string()) return fromUtf8(c.get<std::string>());
  if (c.is_array()) { wstring r; for (auto& x : c) { wstring p = lspPlainDoc(x); if (p.empty()) continue; if (!r.empty()) r += L"\n\n"; r += p; } return r; }
  if (c.is_object() && c.contains("value") && c["value"].is_string()) {
    wstring v = fromUtf8(c["value"].get<std::string>());
    // code fences and links become plain text
    wstring o;
    size_t i = 0;
    while (i < v.size()) {
      if (v.compare(i, 3, L"```") == 0) { size_t e = v.find(L'\n', i); i = e == wstring::npos ? v.size() : e + 1; continue; }
      if (v[i] == L'[') {
        size_t b = v.find(L']', i), p = b == wstring::npos ? b : v.find(L'(', b);
        if (b != wstring::npos && p == b + 1) { size_t q = v.find(L')', p); if (q != wstring::npos) { o += v.substr(i + 1, b - i - 1); i = q + 1; continue; } }
      }
      if (v[i] == L'\\' && i + 1 < v.size() && wcschr(L"\\`*_{}[]()#+-.!", v[i + 1])) { o += v[i + 1]; i += 2; continue; }
      o += v[i++];
    }
    return trimW(o);
  }
  return L"";
}
static void lspApplyWorkspaceEdit(const json& edit, int& n);
static void lspHandle(LspServer* s, const json& m) {
  bool hasId = m.contains("id") && !m["id"].is_null();
  if (hasId && !m.contains("method")) {  // a response
    int id = m["id"].is_number() ? (int)m["id"].get<double>() : -1;
    auto it = s->pending.find(id);
    if (m.contains("error") && m["error"].is_object() && m["error"].contains("message")) s->lastError = fromUtf8(m["error"]["message"].dump()).substr(0, 200);
    if (it != s->pending.end()) {
      auto cb = it->second;
      s->pending.erase(it);
      s->pendingAt.erase(id);
      bool ok = !m.contains("error");
      if (cb) cb(ok && m.contains("result") ? m["result"] : json(), ok);
    }
    return;
  }
  std::string method = m.contains("method") && m["method"].is_string() ? m["method"].get<std::string>() : "";
  if (hasId && !method.empty()) {  // a request of the server
    json result = nullptr;
    if (method == "workspace/configuration") { result = json::array(); if (m.contains("params") && m["params"].contains("items")) for (size_t i = 0; i < m["params"]["items"].size(); i++) result.push_back(nullptr); }
    else if (method == "workspace/workspaceFolders") result = json::array({{{"uri", lspPathToUri(s->root)}, {"name", toUtf8(baseName(s->root))}}});
    else if (method == "workspace/applyEdit") { int n = 0; if (m["params"].contains("edit")) lspApplyWorkspaceEdit(m["params"]["edit"], n); result = {{"applied", true}}; }
    lspWrite(s, {{"id", m["id"]}, {"result", result}});
    return;
  }
  if (method == "textDocument/publishDiagnostics" && m.contains("params")) {
    const json& p = m["params"];
    wstring path = lspUriToPath(p.value("uri", ""));
    vector<LspDiag> list;
    if (p.contains("diagnostics") && p["diagnostics"].is_array())
      for (auto& d : p["diagnostics"]) {
        LspDiag x;
        x.l0 = d["range"]["start"].value("line", 0);
        x.c0 = d["range"]["start"].value("character", 0);
        x.l1 = d["range"]["end"].value("line", 0);
        x.c1 = d["range"]["end"].value("character", 0);
        x.sev = d.value("severity", 1);
        x.msg = d.contains("message") && d["message"].is_string() ? fromUtf8(d["message"].get<std::string>()) : L"";
        x.source = d.contains("source") && d["source"].is_string() ? fromUtf8(d["source"].get<std::string>()) : L"";
        list.push_back(x);
      }
    g_lspDiags[lspKey(path)] = list;
    redraw();
  } else if (method == "window/showMessage" && m.contains("params") && m["params"].value("type", 4) <= 2) {
    std::string t = m["params"].value("message", "");
    flash(fromUtf8(t).substr(0, 160));
  }
}
static void lspMessage(LspMsg* m) {
  auto it = g_lspServers.find(m->key);
  if (it != g_lspServers.end()) {
    json j = json::parse(m->body, nullptr, false);
    if (!j.is_discarded()) lspHandle(it->second, j);
  }
  delete m;
}
static void lspExited(wstring* key) {
  auto it = g_lspServers.find(*key);
  if (it != g_lspServers.end()) {
    LspServer* s = it->second;
    for (auto* d : g_docs) if (d->lspSrv == s->key) { d->lspSrv.clear(); g_lspDiags.erase(lspKey(d->path)); d->lspDue = GetTickCount() + 3000; }
    g_lspServers.erase(it);
    if (s->hin) CloseHandle(s->hin);
    if (s->hproc) CloseHandle(s->hproc);
    if (s->hjob) CloseHandle(s->hjob);
    delete s;
    g_lspStatus.clear();
    redraw();
  }
  delete key;
}
static void lspRestart() {
  for (auto& kv : g_lspServers) { if (kv.second->hjob) TerminateJobObject(kv.second->hjob, 0); }
  g_lspMissing.clear();
  for (auto* d : g_docs) { d->lspDue = GetTickCount() + 500; }
}

/* ---------------------------------------------------------------- requests on the active document */
static json lspPos(const Doc& d, Pos p) { return {{"line", p.l}, {"character", p.c}}; }
static Pos lspToPos(const Doc& d, const json& p) {
  int l = std::max(0, std::min((int)d.lines.size() - 1, p.value("line", 0)));
  return {l, std::max(0, std::min((int)d.lines[l].size(), p.value("character", 0)))};
}
static void lspNeedServer(Doc& d, const std::function<void(LspServer*)>& fn, const wchar_t* capability) {
  LspServer* s = lspServerOf(d);
  if (!s) { flash(L"No language server for this file"); return; }
  if (capability && (!s->caps.contains(toUtf8(capability)) || s->caps[toUtf8(capability)].is_null() || s->caps[toUtf8(capability)] == false)) { flash(L"This language server does not provide that"); return; }
  lspFlush(d);
  fn(s);
}
struct LspLoc { wstring path; int line, col; };
static vector<LspLoc> lspLocations(const json& res) {
  vector<LspLoc> out;
  auto one = [&](const json& l) {
    std::string uri = l.contains("uri") ? l["uri"].get<std::string>() : l.contains("targetUri") ? l["targetUri"].get<std::string>() : "";
    const json* r = l.contains("range") ? &l["range"] : l.contains("targetSelectionRange") ? &l["targetSelectionRange"] : l.contains("targetRange") ? &l["targetRange"] : nullptr;
    if (uri.empty() || !r) return;
    out.push_back({lspUriToPath(uri), (*r)["start"].value("line", 0) + 1, (*r)["start"].value("character", 0)});
  };
  if (res.is_array()) for (auto& l : res) one(l); else if (res.is_object()) one(res);
  return out;
}
static void lspShowLocations(const vector<LspLoc>& locs) {
  if (locs.empty()) { flash(L"Nothing found"); return; }
  if (locs.size() == 1) { openAt(locs[0].path, locs[0].line, locs[0].col, 0); return; }
  vector<PItem> items;
  for (auto& l : locs) {
    PItem it;
    it.label = baseName(l.path) + L":" + std::to_wstring(l.line);
    it.right = l.path;
    LspLoc c = l;
    it.run = [c] { openAt(c.path, c.line, c.col, 0); };
    items.push_back(it);
  }
  openPick(L"Locations", listProvider(items));
}
static bool lspGotoDefinition() {
  Doc& d = D();
  LspServer* s = lspServerOf(d);
  if (!s || !s->caps.contains("definitionProvider")) return false;
  lspFlush(d);
  lspRequest(s, "textDocument/definition", {{"textDocument", {{"uri", d.lspUri}}}, {"position", lspPos(d, d.cur)}}, [](const json& res, bool ok) {
    vector<LspLoc> locs = ok ? lspLocations(res) : vector<LspLoc>();
    if (locs.empty()) flash(L"No definition found"); else lspShowLocations(locs);
  });
  return true;
}
static void lspFindReferences() {
  Doc& d = D();
  lspNeedServer(d, [&](LspServer* s) {
    lspRequest(s, "textDocument/references", {{"textDocument", {{"uri", d.lspUri}}}, {"position", lspPos(d, d.cur)}, {"context", {{"includeDeclaration", true}}}}, [](const json& res, bool ok) {
      vector<LspLoc> locs = ok ? lspLocations(res) : vector<LspLoc>();
      if (locs.empty()) flash(L"No references found"); else lspShowLocations(locs);
    }, 20000);
  }, L"referencesProvider");
}
struct LspTextEdit { Pos a, b; wstring text; };
static void lspApplyEditsToDoc(Doc& d, vector<LspTextEdit> edits) {
  std::sort(edits.begin(), edits.end(), [](const LspTextEdit& x, const LspTextEdit& y) { return y.a < x.a; });  // from the end, so positions stay valid
  d.snapshot(0);
  g_noSnap = true;
  for (auto& e : edits) d.replaceRange(e.a, e.b, e.text);
  g_noSnap = false;
}
static vector<LspTextEdit> lspParseEdits(Doc& d, const json& list) {
  vector<LspTextEdit> out;
  if (!list.is_array()) return out;
  for (auto& e : list)
    if (e.contains("range") && e.contains("newText")) out.push_back({lspToPos(d, e["range"]["start"]), lspToPos(d, e["range"]["end"]), fromUtf8(e["newText"].get<std::string>())});
  return out;
}
static void lspApplyWorkspaceEdit(const json& edit, int& n) {
  std::map<std::string, json> per;
  if (edit.contains("changes") && edit["changes"].is_object()) for (auto it = edit["changes"].begin(); it != edit["changes"].end(); ++it) per[it.key()] = it.value();
  if (edit.contains("documentChanges") && edit["documentChanges"].is_array())
    for (auto& dc : edit["documentChanges"])
      if (dc.contains("textDocument") && dc.contains("edits")) { json& arr = per[dc["textDocument"].value("uri", "")]; if (!arr.is_array()) arr = json::array(); for (auto& e : dc["edits"]) arr.push_back(e); }
  for (auto& kv : per) {
    wstring p = lspUriToPath(kv.first);
    if (p.empty()) continue;
    Doc* open = nullptr;
    for (auto* d : g_docs) if (!d->path.empty() && _wcsicmp(d->path.c_str(), p.c_str()) == 0) open = d;
    if (open) { lspApplyEditsToDoc(*open, lspParseEdits(*open, kv.second)); n += (int)kv.second.size(); if (open == &D()) edited(); continue; }
    Doc tmp;  // a file that is not open: edit it on disk
    if (!loadFile(p, tmp)) continue;
    lspApplyEditsToDoc(tmp, lspParseEdits(tmp, kv.second));
    if (saveFile(tmp, p)) n += (int)kv.second.size();
  }
}
static void lspRename() {
  Doc& d = D();
  LspServer* s0 = lspServerOf(d);
  if (!s0) { flash(L"No language server for renaming here"); return; }
  wstring w;
  int a, b;
  wordAt(d, d.cur.l, d.cur.c, w, a, b);
  openPrompt(L"Rename symbol to:", w, [w](const wstring& name) {
    if (name.empty() || name == w) return;
    Doc& dd = D();
    lspNeedServer(dd, [&](LspServer* s) {
      lspRequest(s, "textDocument/rename", {{"textDocument", {{"uri", dd.lspUri}}}, {"position", lspPos(dd, dd.cur)}, {"newName", toUtf8(name)}}, [](const json& res, bool ok) {
        if (!ok || !res.is_object()) { flash(L"The server could not rename this"); return; }
        int n = 0;
        lspApplyWorkspaceEdit(res, n);
        flash(L"Renamed: " + std::to_wstring(n) + L" change" + (n == 1 ? L"" : L"s"));
      }, 20000);
    }, L"renameProvider");
  });
}
static void lspFormat() {
  Doc& d = D();
  lspNeedServer(d, [&](LspServer* s) {
    unsigned long long uid = d.uid;
    lspRequest(s, "textDocument/formatting", {{"textDocument", {{"uri", d.lspUri}}}, {"options", {{"tabSize", d.tabW}, {"insertSpaces", d.spaces}}}}, [uid](const json& res, bool ok) {
      for (auto* dd : g_docs) if (dd->uid == uid) {
        if (!ok || !res.is_array() || res.empty()) { flash(L"Nothing to format"); return; }
        lspApplyEditsToDoc(*dd, lspParseEdits(*dd, res));
        dd->cur = dd->anc = {std::min(dd->cur.l, (int)dd->lines.size() - 1), 0};
        flash(L"Formatted");
        if (dd == &D()) edited();
      }
    }, 20000);
  }, L"documentFormattingProvider");
}
static void lspShowProblems() {
  Doc& d = D();
  auto it = g_lspDiags.find(lspKey(d.path));
  if (it == g_lspDiags.end() || it->second.empty()) { flash(lspServerOf(d) ? L"No problems found in this file" : L"No language server is running for this file"); return; }
  vector<LspDiag> list = it->second;
  std::sort(list.begin(), list.end(), [](const LspDiag& a, const LspDiag& b) { return a.l0 < b.l0; });
  vector<PItem> items;
  for (auto& x : list) {
    PItem pi;
    wstring first = x.msg.substr(0, x.msg.find(L'\n'));
    pi.label = first.substr(0, 110);
    pi.right = wstring(x.sev == 2 ? L"warning" : x.sev > 2 ? L"info" : L"error") + L"  line " + std::to_wstring(x.l0 + 1);
    pi.line = x.l0;
    items.push_back(pi);
  }
  openPick(L"Problems in this file", listProvider(items));
}
static void lspShowStatus() {
  vector<PItem> items;
  for (auto& kv : g_lspServers) {
    PItem it;
    it.label = kv.second->command + L"  (" + kv.second->root + L")  " + (kv.second->failed ? L"failed: " + (kv.second->lastError.empty() ? wstring(L"no answer") : kv.second->lastError) : kv.second->ready ? L"running" : L"starting");
    items.push_back(it);
  }
  for (auto& m : g_lspMissing) { PItem it; it.label = m.substr(0, m.find(L'|')) + L"  not installed"; items.push_back(it); }
  if (items.empty()) { flash(L"No language server has been started. Open a JavaScript, Python, C++, Rust or Go file with its server installed."); return; }
  openPick(L"Language servers", listProvider(items));
}

/* ---------------------------------------------------------------- completion and hover from the server */
// the message of a diagnostic under the mouse
static bool lspDiagTip(Doc& d, int line, int col, vector<TipLine>& out, int& c0) {
  if (d.path.empty()) return false;
  auto it = g_lspDiags.find(lspKey(d.path));
  if (it == g_lspDiags.end()) return false;
  bool any = false;
  for (auto& x : it->second) {
    bool inside = (line > x.l0 || (line == x.l0 && col >= x.c0)) && (line < x.l1 || (line == x.l1 && col < x.c1));
    if (!inside) continue;
    out.push_back({(x.source.empty() ? L"" : x.source + L": ") + x.msg, 3});
    if (!any) c0 = x.l0 == line ? x.c0 : 0;
    any = true;
  }
  return any;
}
struct LspCompCache { unsigned long long uid = 0; int line = -1, wordStart = -1; vector<CompItem> items; bool pending = false; };
static LspCompCache g_lspComp;
static wstring lspSnippetToBody(const wstring& s) {
  // ${1|a,b|} -> ${1:a}; $0 -> ${}; $1 -> ${1}
  wstring o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == L'$' && i + 1 < s.size()) {
      if (s[i + 1] == L'{') {
        size_t e = s.find(L'}', i);
        if (e == wstring::npos) { o += s[i]; continue; }
        wstring in = s.substr(i + 2, e - i - 2);
        size_t bar = in.find(L'|');
        if (bar != wstring::npos) { size_t k = 0; while (k < in.size() && iswdigit(in[k])) k++; size_t c = in.find(L',', bar); wstring first = in.substr(bar + 1, (c == wstring::npos ? in.size() - 1 : c) - bar - 1); o += L"${" + in.substr(0, k) + L":" + first + L"}"; }
        else if (in == L"0") o += L"${}";
        else o += L"${" + in + L"}";
        i = e;
        continue;
      }
      if (iswdigit(s[i + 1])) {
        size_t k = i + 1;
        while (k < s.size() && iswdigit(s[k])) k++;
        wstring num = s.substr(i + 1, k - i - 1);
        o += num == L"0" ? L"${}" : L"${" + num + L"}";
        i = k - 1;
        continue;
      }
    }
    o += s[i];
  }
  return o;
}
static void snipInsertBody(Doc& d, Pos a, Pos b, const wstring& body);  // extras.inc
static void lspCompletionExtra(Doc& d, const wstring& pre, vector<CompItem>& out) {
  LspServer* s = lspServerOf(d);
  if (!s || !s->caps.contains("completionProvider")) return;
  int wordStart = d.cur.c - (int)pre.size();
  if (g_lspComp.uid == d.uid && g_lspComp.line == d.cur.l && g_lspComp.wordStart == wordStart) {
    for (auto& it : g_lspComp.items) out.push_back(it);
    return;
  }
  if (g_lspComp.pending) return;
  g_lspComp = LspCompCache();
  g_lspComp.uid = d.uid;
  g_lspComp.line = d.cur.l;
  g_lspComp.wordStart = wordStart;
  g_lspComp.pending = true;
  lspFlush(d);
  unsigned long long uid = d.uid;
  int line = d.cur.l;
  lspRequest(s, "textDocument/completion", {{"textDocument", {{"uri", d.lspUri}}}, {"position", lspPos(d, d.cur)}, {"context", {{"triggerKind", 1}}}}, [uid, line, wordStart](const json& res, bool ok) {
    g_lspComp.pending = false;
    Doc& cd = D();
    if (!ok || cd.uid != uid || cd.cur.l != line) return;
    const json* arr = res.is_array() ? &res : (res.is_object() && res.contains("items") ? &res["items"] : nullptr);
    if (!arr || !arr->is_array()) return;
    for (auto& it : *arr) {
      if (g_lspComp.items.size() >= 400) break;
      CompItem ci;
      ci.label = fromUtf8(it.value("label", ""));
      ci.detail = it.contains("detail") && it["detail"].is_string() ? fromUtf8(it["detail"].get<std::string>()) : L"";
      ci.sig = ci.detail;
      if (it.contains("documentation")) ci.doc = lspPlainDoc(it["documentation"]);
      ci.boost = it.contains("sortText") && it["sortText"].is_string() && !it["sortText"].get<std::string>().empty() ? 100 - (unsigned char)it["sortText"].get<std::string>()[0] / 3 : 0;
      wstring insert = it.contains("textEdit") && it["textEdit"].is_object() && it["textEdit"].contains("newText") ? fromUtf8(it["textEdit"]["newText"].get<std::string>()) : it.contains("insertText") && it["insertText"].is_string() ? fromUtf8(it["insertText"].get<std::string>()) : ci.label;
      bool snippet = it.value("insertTextFormat", 1) == 2;
      json range = it.contains("textEdit") && it["textEdit"].is_object() ? (it["textEdit"].contains("range") ? it["textEdit"]["range"] : it["textEdit"].contains("replace") ? it["textEdit"]["replace"] : json()) : json();
      json extra = it.contains("additionalTextEdits") ? it["additionalTextEdits"] : json();
      ci.apply = [insert, snippet, range, extra, wordStart, line](Doc& dd) {
        Pos a{line, wordStart}, b = dd.cur;
        if (range.is_object() && range.contains("start")) { a = lspToPos(dd, range["start"]); b = lspToPos(dd, range["end"]); }
        vector<LspTextEdit> more = extra.is_array() ? lspParseEdits(dd, extra) : vector<LspTextEdit>();
        dd.snapshot(0);
        if (snippet) snipInsertBody(dd, a, b, lspSnippetToBody(insert)); else dd.replaceRange(a, b, insert);
        if (!more.empty()) lspApplyEditsToDoc(dd, more);
        edited();
      };
      g_lspComp.items.push_back(ci);
    }
    compUpdate(true);
  }, 8000);
}
// hover: documentation of the symbol under the mouse
static void lspHover(Doc& d, int line, int col, std::function<void(const wstring&)> done) {
  LspServer* s = lspServerOf(d);
  if (!s || !s->caps.contains("hoverProvider")) return;
  lspFlush(d);
  lspRequest(s, "textDocument/hover", {{"textDocument", {{"uri", d.lspUri}}}, {"position", {{"line", line}, {"character", col}}}}, [done](const json& res, bool ok) {
    if (!ok || !res.is_object() || !res.contains("contents")) return;
    wstring text = lspPlainDoc(res["contents"]);
    if (!text.empty()) done(text.size() > 1400 ? text.substr(0, 1400) + L"…" : text);
  }, 5000);
}
