#include <shlwapi.h>
// claude_proc.inc: the part of the Claude Code chat that talks to the `claude` program (no drawing here): finding it, one long-lived
// process per conversation speaking stream-json both ways, `claude auth ...`, and the past conversations in ~/.claude/projects.

#define WM_CLAUDE_EV (WM_APP + 10)      // wp = conversation id, lp = std::string* with one JSON object
#define WM_CLAUDE_AUTH (WM_APP + 11)    // lp = ClAuthMsg*
#define WM_CLAUDE_AUTHST (WM_APP + 12)  // lp = ClAuthSt*

struct ClProc {
  int conv = 0;
  HANDLE hin = nullptr, hproc = nullptr, hjob = nullptr;
  bool killed = false;
  int seq = 0;
};
struct ClAuthMsg { wstring id; std::string text; bool exited = false; int code = 0; };
struct ClAuthSt { int token = 0; std::string out; };
static std::map<int, ClProc*> g_clProcs;
static std::map<int, std::function<void(const json&)>> g_clAuthCbs;
static int g_clAuthToken = 0;

static bool clFileExists(const wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static wstring clHome() {
  wchar_t b[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(L"USERPROFILE", b, MAX_PATH);
  return n ? wstring(b, n) : L"";
}
// the newest Claude Code extension folder of VS Code that has the given sub path
static wstring clVsCodeFile(const wstring& sub) {
  wstring root = clHome() + L"\\.vscode\\extensions";
  vector<wstring> names;
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((root + L"\\anthropic.claude-code-*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return L"";
  do names.push_back(fd.cFileName); while (FindNextFileW(h, &fd));
  FindClose(h);
  std::sort(names.begin(), names.end(), [](const wstring& a, const wstring& b) { return StrCmpLogicalW(a.c_str(), b.c_str()) > 0; });
  for (auto& n : names) {
    wstring p = root + L"\\" + n + L"\\" + sub;
    if (clFileExists(p)) return p;
  }
  return L"";
}
static wstring clFind(const wstring& custom) {
  if (!custom.empty() && clFileExists(custom)) return custom;
  for (const wchar_t* e : {L".exe", L".cmd", L""}) {
    wchar_t buf[MAX_PATH];
    if (SearchPathW(nullptr, L"claude", e[0] ? e : nullptr, MAX_PATH, buf, nullptr) && clFileExists(buf)) return buf;
  }
  wstring home = clHome();
  vector<wstring> tries = {home + L"\\.local\\bin\\claude.exe", home + L"\\.local\\bin\\claude", home + L"\\.claude\\local\\claude"};
  wchar_t ad[MAX_PATH];
  if (GetEnvironmentVariableW(L"APPDATA", ad, MAX_PATH)) tries.push_back(wstring(ad) + L"\\npm\\claude.cmd");
  for (auto& p : tries) if (clFileExists(p)) return p;
  return clVsCodeFile(L"resources\\native-binary\\claude.exe");
}
static bool clIsShellScript(const wstring& exe) {
  return exe.size() > 4 && (_wcsicmp(exe.c_str() + exe.size() - 4, L".cmd") == 0 || _wcsicmp(exe.c_str() + exe.size() - 4, L".bat") == 0);
}
static wstring clQuote(const wstring& a) {
  if (!a.empty() && a.find_first_of(L" \t\"") == wstring::npos) return a;
  wstring o = L"\"";
  for (wchar_t c : a) { if (c == L'"') o += L'\\'; o += c; }
  return o + L"\"";
}
static wstring clCommandLine(const wstring& exe, const vector<wstring>& args) {
  wstring cmd;
  bool shell = clIsShellScript(exe);
  if (shell) cmd = L"cmd.exe /d /s /c \"\"" + exe + L"\"";
  else cmd = L"\"" + exe + L"\"";
  for (auto& a : args) cmd += L" " + clQuote(a);
  if (shell) cmd += L"\"";
  return cmd;
}

/* ---------------------------------------------------------------- the conversation process */
static void clPost(int conv, const json& j) {
  PostMessageW(g_hwnd, WM_CLAUDE_EV, (WPARAM)conv, (LPARAM) new std::string(j.dump(-1, ' ', false, json::error_handler_t::replace)));
}
static void clStdoutThread(HANDLE h, int conv, HANDLE hproc, ClProc* pr, HWND hw) {
  std::string buf;
  char tmp[16384];
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(h, tmp, sizeof(tmp), &got, nullptr) || got == 0) break;
    buf.append(tmp, got);
    size_t nl;
    while ((nl = buf.find('\n')) != std::string::npos) {
      std::string line = buf.substr(0, nl);
      buf.erase(0, nl + 1);
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
      if (line.empty()) continue;
      PostMessageW(hw, WM_CLAUDE_EV, (WPARAM)conv, (LPARAM) new std::string(line));
    }
  }
  if (!buf.empty()) PostMessageW(hw, WM_CLAUDE_EV, (WPARAM)conv, (LPARAM) new std::string(buf));
  CloseHandle(h);
  WaitForSingleObject(hproc, 5000);
  DWORD code = 0;
  GetExitCodeProcess(hproc, &code);
  if (code == STILL_ACTIVE) code = (DWORD)-1;
  json ev = {{"type", "exit"}, {"code", (int)code}, {"killed", pr->killed}};
  PostMessageW(hw, WM_CLAUDE_EV, (WPARAM)conv, (LPARAM) new std::string(ev.dump()));
}
static void clStderrThread(HANDLE h, int conv, HWND hw) {
  char tmp[4096];
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(h, tmp, sizeof(tmp), &got, nullptr) || got == 0) break;
    json ev = {{"type", "stderr"}, {"text", std::string(tmp, got)}};
    PostMessageW(hw, WM_CLAUDE_EV, (WPARAM)conv, (LPARAM) new std::string(ev.dump(-1, ' ', false, json::error_handler_t::replace)));
  }
  CloseHandle(h);
}
struct ClOpen {
  int conv = 0;
  wstring cwd, sessionId, permissionMode, model, effort, claudePath;
  vector<wstring> allowedTools;
};
static bool clValidSession(const wstring& s) {
  if (s.size() < 8 || s.size() > 64) return false;
  for (wchar_t c : s) if (!(iswalnum(c) || c == L'_' || c == L'-')) return false;
  return true;
}
// starts a process; on failure err says why
static bool clOpen(const ClOpen& o, wstring& err) {
  wstring exe = clFind(o.claudePath);
  if (exe.empty()) {
    err = L"Claude Code was not found. Install it (npm install -g @anthropic-ai/claude-code) or set \"claude_path\" in the settings.";
    return false;
  }
  wstring cwd = o.cwd;
  if (cwd.empty() || GetFileAttributesW(cwd.c_str()) == INVALID_FILE_ATTRIBUTES) cwd = clHome();
  vector<wstring> args = {L"-p", L"--input-format", L"stream-json", L"--output-format", L"stream-json", L"--verbose", L"--include-partial-messages",
                          L"--replay-user-messages", L"--permission-prompt-tool", L"stdio"};
  if (clValidSession(o.sessionId)) { args.push_back(L"--resume"); args.push_back(o.sessionId); }
  for (const wchar_t* m : {L"acceptEdits", L"auto", L"plan", L"manual", L"dontAsk", L"bypassPermissions"})
    if (o.permissionMode == m) { args.push_back(L"--permission-mode"); args.push_back(o.permissionMode); }
  if (!o.model.empty() && o.model.size() <= 60) {
    bool ok = true;
    for (wchar_t c : o.model) if (!(iswalnum(c) || wcschr(L".-_[]", c))) ok = false;
    if (ok) { args.push_back(L"--model"); args.push_back(o.model); }
  }
  for (const wchar_t* e : {L"low", L"medium", L"high", L"xhigh", L"max"})
    if (o.effort == e) { args.push_back(L"--effort"); args.push_back(o.effort); }
  if (!o.allowedTools.empty()) {
    wstring list;
    for (auto& t : o.allowedTools) { if (t.empty()) continue; if (!list.empty()) list += L","; list += t; }
    if (!list.empty()) { args.push_back(L"--allowedTools"); args.push_back(list); }
  }
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE inR, inW, outR, outW, errR, errW;
  if (!CreatePipe(&inR, &inW, &sa, 0) || !CreatePipe(&outR, &outW, &sa, 0) || !CreatePipe(&errR, &errW, &sa, 0)) { err = L"Could not create the pipes."; return false; }
  SetHandleInformation(inW, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(errR, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = inR;
  si.hStdOutput = outW;
  si.hStdError = errW;
  wstring cmdLine = clCommandLine(exe, args);
  vector<wchar_t> cl(cmdLine.begin(), cmdLine.end());
  cl.push_back(0);
  SetEnvironmentVariableW(L"CLAUDE_CODE_ENABLE_SDK_FILE_CHECKPOINTING", L"1");
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(nullptr, cl.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, cwd.c_str(), &si, &pi);
  CloseHandle(inR);
  CloseHandle(outW);
  CloseHandle(errW);
  if (!ok) {
    CloseHandle(inW);
    CloseHandle(outR);
    CloseHandle(errR);
    err = L"Could not start " + exe;
    logLine(err + L" (windows error " + std::to_wstring(GetLastError()) + L")");
    return false;
  }
  auto* pr = new ClProc();
  pr->conv = o.conv;
  pr->hin = inW;
  pr->hproc = pi.hProcess;
  pr->hjob = CreateJobObjectW(nullptr, nullptr);
  if (pr->hjob) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
    li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(pr->hjob, JobObjectExtendedLimitInformation, &li, sizeof(li));
    AssignProcessToJobObject(pr->hjob, pi.hProcess);
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  g_clProcs[o.conv] = pr;
  HWND hw = g_hwnd;
  std::thread(clStdoutThread, outR, o.conv, pi.hProcess, pr, hw).detach();
  std::thread(clStderrThread, errR, o.conv, hw).detach();
  return true;
}
static bool clWrite(int conv, const json& obj) {
  auto it = g_clProcs.find(conv);
  if (it == g_clProcs.end() || !it->second->hin) return false;
  std::string s = obj.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
  DWORD wr = 0;
  return WriteFile(it->second->hin, s.data(), (DWORD)s.size(), &wr, nullptr) && wr == s.size();
}
// a control request (interrupt, set_model, set_permission_mode, rewind_files, ...); returns its request id or ""
static std::string clControl(int conv, const json& request) {
  auto it = g_clProcs.find(conv);
  if (it == g_clProcs.end() || !request.contains("subtype")) return "";
  std::string rid = "ctl-" + std::to_string(++it->second->seq);
  return clWrite(conv, {{"type", "control_request"}, {"request_id", rid}, {"request", request}}) ? rid : "";
}
static bool clRespond(int conv, const std::string& rid, const json& response, bool isError) {
  json body = isError ? json{{"subtype", "error"}, {"request_id", rid}, {"error", response.is_string() ? response.get<std::string>() : std::string("error")}}
                      : json{{"subtype", "success"}, {"request_id", rid}, {"response", response}};
  return clWrite(conv, {{"type", "control_response"}, {"response", body}});
}
static void clClose(int conv) {
  auto it = g_clProcs.find(conv);
  if (it == g_clProcs.end()) return;
  it->second->killed = true;
  if (it->second->hin) { CloseHandle(it->second->hin); it->second->hin = nullptr; }
  if (it->second->hjob) TerminateJobObject(it->second->hjob, 1);
  else TerminateProcess(it->second->hproc, 1);
}
// the exit event arrived: the process record is not needed any more
static void clForget(int conv) {
  auto it = g_clProcs.find(conv);
  if (it == g_clProcs.end()) return;
  ClProc* p = it->second;
  g_clProcs.erase(it);
  if (p->hin) CloseHandle(p->hin);
  if (p->hjob) CloseHandle(p->hjob);
  if (p->hproc) CloseHandle(p->hproc);
  // the stdout thread may still hold the pointer for a moment; it only reads `killed` before posting the exit message
  delete p;
}
static void clShutdownAll() {
  for (auto& kv : g_clProcs) {
    kv.second->killed = true;
    if (kv.second->hjob) TerminateJobObject(kv.second->hjob, 1);
  }
}

/* ---------------------------------------------------------------- claude auth status / login / logout */
static void clRunCapture(wstring cmd, int token, HWND hw) {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE outR, outW;
  auto* res = new ClAuthSt();
  res->token = token;
  if (CreatePipe(&outR, &outW, &sa, 0)) {
    SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = outW;
    si.hStdError = outW;
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    si.hStdInput = nul;
    vector<wchar_t> cl(cmd.begin(), cmd.end());
    cl.push_back(0);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cl.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
      CloseHandle(outW);
      CloseHandle(pi.hThread);
      char tmp[4096];
      DWORD got;
      while (ReadFile(outR, tmp, sizeof(tmp), &got, nullptr) && got) res->out.append(tmp, got);
      WaitForSingleObject(pi.hProcess, 20000);
      CloseHandle(pi.hProcess);
    } else CloseHandle(outW);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    CloseHandle(outR);
  }
  PostMessageW(hw, WM_CLAUDE_AUTHST, 0, (LPARAM)res);
}
static void clAuthStatus(const wstring& custom, std::function<void(const json&)> cb) {
  wstring exe = clFind(custom);
  if (exe.empty()) { cb({{"error", "Claude Code was not found"}}); return; }
  int token = ++g_clAuthToken;
  g_clAuthCbs[token] = cb;
  std::thread(clRunCapture, clCommandLine(exe, {L"auth", L"status"}), token, g_hwnd).detach();
}
static std::map<wstring, HANDLE> g_clAuthJobs;  // run id -> job object of `claude auth login|logout`
static void clAuthReader(HANDLE h, wstring id, HANDLE hproc, HWND hw) {
  char tmp[2048];
  DWORD got;
  while (ReadFile(h, tmp, sizeof(tmp), &got, nullptr) && got) PostMessageW(hw, WM_CLAUDE_AUTH, 0, (LPARAM) new ClAuthMsg{id, std::string(tmp, got), false, 0});
  CloseHandle(h);
  WaitForSingleObject(hproc, 5000);
  DWORD code = 0;
  GetExitCodeProcess(hproc, &code);
  CloseHandle(hproc);
  PostMessageW(hw, WM_CLAUDE_AUTH, 0, (LPARAM) new ClAuthMsg{id, "", true, (int)code});
}
static bool clAuthRun(const wstring& id, const wstring& action, bool consoleLogin, const wstring& custom, wstring& err) {
  wstring exe = clFind(custom);
  if (exe.empty()) { err = L"Claude Code was not found"; return false; }
  vector<wstring> args = {L"auth", action};
  if (action == L"login" && consoleLogin) args.push_back(L"--console");
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE outR, outW;
  if (!CreatePipe(&outR, &outW, &sa, 0)) { err = L"Could not create a pipe"; return false; }
  SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = outW;
  si.hStdError = outW;
  HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
  si.hStdInput = nul;
  wstring cmd = clCommandLine(exe, args);
  vector<wchar_t> cl(cmd.begin(), cmd.end());
  cl.push_back(0);
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(nullptr, cl.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);
  CloseHandle(outW);
  if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
  if (!ok) { CloseHandle(outR); err = L"Could not start " + exe; return false; }
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
    li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));
    AssignProcessToJobObject(job, pi.hProcess);
    g_clAuthJobs[id] = job;
  }
  ResumeThread(pi.hThread);
  CloseHandle(pi.hThread);
  std::thread(clAuthReader, outR, id, pi.hProcess, g_hwnd).detach();
  return true;
}
static void clAuthKill(const wstring& id) {
  auto it = g_clAuthJobs.find(id);
  if (it == g_clAuthJobs.end()) return;
  TerminateJobObject(it->second, 1);
}
static void clAuthForget(const wstring& id) {
  auto it = g_clAuthJobs.find(id);
  if (it == g_clAuthJobs.end()) return;
  CloseHandle(it->second);
  g_clAuthJobs.erase(it);
}

/* ---------------------------------------------------------------- past conversations of a folder */
// Claude Code keeps them in ~/.claude/projects/<folder path with every other character as "-">; the drive letter's case varies
static wstring clProjectDir(const wstring& cwd) {
  if (cwd.empty()) return L"";
  wstring enc;
  for (wchar_t c : cwd) enc += iswalnum(c) && c < 128 ? (wchar_t)towlower(c) : L'-';
  wstring root = clHome() + L"\\.claude\\projects";
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((root + L"\\*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return L"";
  wstring hit;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
    wstring n = fd.cFileName, ln = n;
    for (auto& c : ln) c = (wchar_t)towlower(c);
    if (ln == enc) { hit = root + L"\\" + n; break; }
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return hit;
}
static std::string clMsgText(const json& msg) {
  if (!msg.is_object() || !msg.contains("content")) return "";
  const json& c = msg["content"];
  if (c.is_string()) return c.get<std::string>();
  std::string out;
  if (c.is_array())
    for (auto& b : c)
      if (b.is_object() && b.value("type", "") == "text" && b.contains("text") && b["text"].is_string()) { if (!out.empty()) out += "\n"; out += b["text"].get<std::string>(); }
  return out;
}
static std::string clTrim(std::string s) {
  size_t a = 0;
  while (a < s.size() && isspace((unsigned char)s[a])) a++;
  size_t b = s.size();
  while (b > a && isspace((unsigned char)s[b - 1])) b--;
  return s.substr(a, b - a);
}
static std::string clOneLine(const std::string& s, size_t max) {
  std::string o;
  bool sp = false;
  for (unsigned char c : s) {
    if (isspace(c)) { sp = true; continue; }
    if (sp && !o.empty()) o += ' ';
    sp = false;
    o += (char)c;
  }
  // do not cut inside a UTF-8 sequence
  if (o.size() > max) {
    size_t k = max;
    while (k > 0 && (o[k] & 0xC0) == 0x80) k--;
    o.resize(k);
  }
  return o;
}
struct ClSession { wstring id, title; ULONGLONG mtime = 0; };
static vector<ClSession> clSessions(const wstring& cwd) {
  vector<ClSession> out;
  wstring dir = clProjectDir(cwd);
  if (dir.empty()) return out;
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((dir + L"\\*.jsonl").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    wstring path = dir + L"\\" + fd.cFileName;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) continue;
    std::string head(96 * 1024, '\0');
    DWORD got = 0;
    ReadFile(f, head.data(), (DWORD)head.size(), &got, nullptr);
    CloseHandle(f);
    head.resize(got);
    std::string title;
    size_t p = 0;
    while (p < head.size() && title.empty()) {
      size_t q = head.find('\n', p);
      if (q == std::string::npos) q = head.size();
      std::string line = head.substr(p, q - p);
      p = q + 1;
      if (line.empty() || line[0] != '{') continue;
      json j = json::parse(line, nullptr, false);
      if (!j.is_object() || j.value("type", "") != "user" || j.value("isMeta", false) || j.value("isSidechain", false)) continue;
      std::string t = clTrim(clMsgText(j.value("message", json::object())));
      if (!t.empty() && t[0] != '<' && t.compare(0, 15, "[Editor context") != 0) { title = clOneLine(t, 90); break; }
      if (t.compare(0, 15, "[Editor context") == 0) {
        size_t k = t.find("\n\n");
        if (k != std::string::npos) {
          std::string m = clTrim(t.substr(k + 2));
          if (!m.empty()) { title = clOneLine(m, 90); break; }
        }
      }
    }
    if (title.empty()) continue;
    ClSession s;
    s.id = wstring(fd.cFileName).substr(0, wcslen(fd.cFileName) - 6);
    s.title = fromUtf8(title);
    s.mtime = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
    out.push_back(s);
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  std::sort(out.begin(), out.end(), [](const ClSession& a, const ClSession& b) { return a.mtime > b.mtime; });
  if (out.size() > 60) out.resize(60);
  return out;
}
struct ClHistBlock { bool tool = false; std::string text, name; json input; };
struct ClHistMsg { bool user = false; std::string text, uuid; vector<ClHistBlock> blocks; };
// what the editor put in front of a typed message (open file, selections, attachments) is not shown again
static std::string clStripContext(std::string t) {
  for (int k = 0; k < 6; k++) {
    std::string before = t;
    t = clTrim(t);
    if (t.compare(0, 16, "[Editor context]") == 0) {
      size_t e = t.find('\n');
      std::string rest = e == std::string::npos ? "" : t.substr(e + 1);
      if (rest.compare(0, 13, "Selected text") == 0) {
        size_t a = rest.find("```\n"), b = a == std::string::npos ? a : rest.find("\n```\n\n", a + 4);
        rest = b == std::string::npos ? rest : rest.substr(b + 5);
      } else if (!rest.empty() && rest[0] == '\n') rest.erase(0, 1);
      t = rest;
    }
    if (t.compare(0, 41, "Attached files (read them if needed):\n- ") == 0) {
      size_t e = t.find("\n\n");
      if (e != std::string::npos) t = t.substr(e + 2);
    }
    if (t.compare(0, 18, "Selected code from") == 0) {
      size_t a = t.find("```\n"), b = a == std::string::npos ? a : t.find("\n```\n\n", a + 4);
      if (b != std::string::npos) t = t.substr(b + 5);
    }
    t = clTrim(t);
    if (t == clTrim(before)) break;
  }
  return t;
}
static vector<ClHistMsg> clHistory(const wstring& cwd, const wstring& id) {
  vector<ClHistMsg> out;
  wstring dir = clProjectDir(cwd);
  if (dir.empty() || !clValidSession(id)) return out;
  vector<char> raw;
  if (!readAll(dir + L"\\" + id + L".jsonl", raw)) return out;
  size_t p = 0;
  std::string text(raw.begin(), raw.end());
  while (p < text.size()) {
    size_t q = text.find('\n', p);
    if (q == std::string::npos) q = text.size();
    std::string line = text.substr(p, q - p);
    p = q + 1;
    if (line.empty() || line[0] != '{') continue;
    json j = json::parse(line, nullptr, false);
    if (!j.is_object()) continue;
    std::string type = j.value("type", "");
    if ((type != "user" && type != "assistant") || j.value("isSidechain", false) || j.value("isMeta", false) || !j.contains("message")) continue;
    const json& msg = j["message"];
    if (type == "user") {
      std::string t = clStripContext(clTrim(clMsgText(msg)));
      if (t.empty() || t[0] == '<') continue;
      ClHistMsg m;
      m.user = true;
      m.text = t;
      m.uuid = j.value("uuid", "");
      out.push_back(m);
    } else if (msg.contains("content") && msg["content"].is_array()) {
      ClHistMsg m;
      for (auto& b : msg["content"]) {
        if (!b.is_object()) continue;
        std::string bt = b.value("type", "");
        if (bt == "text" && b.contains("text") && b["text"].is_string() && !clTrim(b["text"].get<std::string>()).empty()) {
          ClHistBlock hb;
          hb.text = b["text"].get<std::string>();
          m.blocks.push_back(hb);
        } else if (bt == "tool_use") {
          ClHistBlock hb;
          hb.tool = true;
          hb.name = b.value("name", "");
          hb.input = b.contains("input") ? b["input"] : json::object();
          m.blocks.push_back(hb);
        }
      }
      if (!m.blocks.empty()) out.push_back(m);
    }
  }
  if (out.size() > 400) out.erase(out.begin(), out.end() - 400);
  return out;
}

/* ---------------------------------------------------------------- small helpers for attachments */
static std::string clBase64(const unsigned char* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve((n + 2) / 3 * 4);
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    o += T[v >> 18 & 63];
    o += T[v >> 12 & 63];
    o += i + 1 < n ? T[v >> 6 & 63] : '=';
    o += i + 2 < n ? T[v & 63] : '=';
  }
  return o;
}
// an image file as base64 plus its media type; false when it is not a supported picture or is bigger than 8 MB
static bool clReadImage(const wstring& path, std::string& media, std::string& b64) {
  size_t dot = path.find_last_of(L'.');
  if (dot == wstring::npos) return false;
  wstring ext = lowerW(path.substr(dot + 1));
  if (ext == L"png") media = "image/png";
  else if (ext == L"jpg" || ext == L"jpeg") media = "image/jpeg";
  else if (ext == L"gif") media = "image/gif";
  else if (ext == L"webp") media = "image/webp";
  else return false;
  vector<char> b;
  if (!readAll(path, b) || b.size() > 8u * 1024 * 1024) return false;
  b64 = clBase64((const unsigned char*)b.data(), b.size());
  return true;
}
static vector<wstring> clPickFiles(bool images) {
  vector<wstring> out;
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg)))) return out;
  DWORD o = 0;
  dlg->GetOptions(&o);
  dlg->SetOptions(o | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM);
  COMDLG_FILTERSPEC img[] = {{L"Images", L"*.png;*.jpg;*.jpeg;*.gif;*.webp;*.bmp"}};
  COMDLG_FILTERSPEC all[] = {{L"All files", L"*.*"}};
  if (images) dlg->SetFileTypes(1, img);
  else dlg->SetFileTypes(1, all);
  if (SUCCEEDED(dlg->Show(g_hwnd))) {
    IShellItemArray* arr = nullptr;
    if (SUCCEEDED(dlg->GetResults(&arr))) {
      DWORD n = 0;
      arr->GetCount(&n);
      for (DWORD i = 0; i < n; i++) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(arr->GetItemAt(i, &it))) {
          PWSTR p = nullptr;
          if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out.push_back(p); CoTaskMemFree(p); }
          it->Release();
        }
      }
      arr->Release();
    }
  }
  dlg->Release();
  return out;
}
