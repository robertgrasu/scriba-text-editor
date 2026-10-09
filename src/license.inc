// license.inc: the license of Scriba. Nothing works without one: the editor stays on the license page until a key is activated.
// A key is activated on a computer: the editor sends the key and a hash of the computer to the license server (license_server/, Flask) and gets
// back a token signed with the private key of the server (ECDSA P-256). The editor checks the signature with the public key built into it,
// that the token is for this computer and that it did not expire; it asks for a new token every few days (so a revoked license stops working),
// and keeps working without a connection until the token it has runs out.
// Included by main.cpp after amxx_unpack.inc.
#include <bcrypt.h>
#include <winhttp.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")
#include "license_pub.inc"

#define WM_LICENSE (WM_APP + 15)
enum { LS_NONE, LS_OK, LS_EXPIRED, LS_REVOKED, LS_TAMPER };
struct LicInfo { int state = LS_NONE; wstring name, lic; long long exp = 0, until = 0, iat = 0; int maxm = 0; };
static LicInfo g_lic;
static std::string g_licKey, g_licToken, g_licMachine;
static long long g_licSeen = 0, g_licLastTry = 0;
static bool g_licOpen = false, g_licBusy = false, g_licBypass = false, g_licMsgErr = false, g_licManual = false;
static wstring g_licMsg;
static LineEdit g_licEdit;
static vector<D2D1_RECT_F> g_licRects;
static int g_licKind[8];

/* ---------------------------------------------------------------- the computer, the signature */
static void licSha256(const void* data, size_t n, unsigned char out[32]) {
  BCRYPT_ALG_HANDLE h = nullptr;
  memset(out, 0, 32);
  if (BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return;
  BCryptHash(h, nullptr, 0, (PUCHAR)data, (ULONG)n, out, 32);
  BCryptCloseAlgorithmProvider(h, 0);
}
static std::string licMachineId() {
  std::string guid;
  HKEY k;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
    wchar_t b[128] = {};
    DWORD sz = sizeof(b), ty = 0;
    if (RegQueryValueExW(k, L"MachineGuid", nullptr, &ty, (LPBYTE)b, &sz) == ERROR_SUCCESS) guid = toUtf8(b);
    RegCloseKey(k);
  }
  DWORD serial = 0;
  wchar_t sysdir[MAX_PATH] = L"C:\\";
  GetSystemDirectoryW(sysdir, MAX_PATH);
  wchar_t root[4] = {sysdir[0], L':', L'\\', 0};
  GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
  std::string src = "scriba-v1|" + guid + "|" + std::to_string(serial);
  unsigned char h[32];
  licSha256(src.data(), src.size(), h);
  static const char* hx = "0123456789abcdef";
  std::string o;
  for (int i = 0; i < 32; i++) { o += hx[h[i] >> 4]; o += hx[h[i] & 15]; }
  return o;
}
static bool licVerifySig(const std::string& signed_, const unsigned char sig[64]) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_KEY_HANDLE key = nullptr;
  bool ok = false;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) == 0) {
    struct { BCRYPT_ECCKEY_BLOB h; unsigned char x[32], y[32]; } blob;
    blob.h.dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    blob.h.cbKey = 32;
    memcpy(blob.x, LIC_PUB_X, 32);
    memcpy(blob.y, LIC_PUB_Y, 32);
    if (BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, (PUCHAR)&blob, sizeof(blob), 0) == 0) {
      unsigned char hash[32];
      licSha256(signed_.data(), signed_.size(), hash);
      ok = BCryptVerifySignature(key, nullptr, hash, 32, (PUCHAR)sig, 64, 0) == 0;
      BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
  }
  return ok;
}
static long long licNow() { return (long long)time(nullptr); }
// checks a token: signature, computer, dates. Fills `out` on success.
static int licCheckToken(const std::string& tok, LicInfo& out) {
  size_t dot = tok.find('.');
  if (dot == std::string::npos) return LS_NONE;
  std::string body = tok.substr(0, dot), sig = hudUnB64(tok.substr(dot + 1));
  if (sig.size() != 64 || !licVerifySig(body, (const unsigned char*)sig.data())) return LS_TAMPER;
  json j = json::parse(hudUnB64(body), nullptr, false);
  if (!j.is_object() || j.value("m", "") != g_licMachine) return LS_TAMPER;
  out.name = fromUtf8(j.value("name", ""));
  out.lic = fromUtf8(j.value("lic", ""));
  out.exp = j.value("exp", 0LL);
  out.until = j.value("until", 0LL);
  out.iat = j.value("iat", 0LL);
  out.maxm = j.value("max", 0);
  long long now = licNow();
  if (out.until && now > out.until) return LS_EXPIRED;
  if (now > out.exp) return LS_EXPIRED;
  if (now + 86400 * 2 < out.iat) return LS_TAMPER;  // the clock was put back before the day the token was made
  return LS_OK;
}

/* ---------------------------------------------------------------- what is kept on disk */
static wstring licFile() { return userDir() + L"\\license.dat"; }
static void licSave() {
  json j = {{"key", g_licKey}, {"token", g_licToken}, {"seen", g_licSeen}};
  std::string s = j.dump();
  HANDLE h = CreateFileW(licFile().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD w;
  WriteFile(h, s.data(), (DWORD)s.size(), &w, nullptr);
  CloseHandle(h);
}
static void licLoad() {
  vector<char> b;
  if (!readAll(licFile(), b)) return;
  json j = json::parse(std::string(b.begin(), b.end()), nullptr, false);
  if (!j.is_object()) return;
  g_licKey = j.value("key", "");
  g_licToken = j.value("token", "");
  g_licSeen = j.value("seen", 0LL);
}
static std::string licServer() {
  wstring s = sStr("license_server");
  return s.empty() ? "http://127.0.0.1:5057" : toUtf8(s);
}

/* ---------------------------------------------------------------- talking to the server */
struct LicResult { int op = 0; int status = 0; std::string body; };
static void licWorker(int op, std::string key, std::string server, HWND hw) {
  auto* r = new LicResult();
  r->op = op;
  json req = {{"key", key}, {"machine", g_licMachine}, {"version", "0.1"}};
  wchar_t cn[64] = L"";
  DWORD n = 64;
  GetComputerNameW(cn, &n);
  req["label"] = toUtf8(cn);
  std::string body = req.dump();
  const char* path = op == 1 ? "/api/activate" : op == 2 ? "/api/refresh" : "/api/deactivate";
  wstring url = fromUtf8(server + path);
  URL_COMPONENTS uc{};
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256] = L"", up[256] = L"";
  uc.lpszHostName = host; uc.dwHostNameLength = 256;
  uc.lpszUrlPath = up; uc.dwUrlPathLength = 256;
  if (WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
    HINTERNET s = WinHttpOpen(L"Scriba/0.1", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : nullptr;
    HINTERNET q = c ? WinHttpOpenRequest(c, L"POST", up, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (q) {
      WinHttpSetTimeouts(q, 4000, 4000, 8000, 8000);
      if (WinHttpSendRequest(q, L"Content-Type: application/json", (DWORD)-1L, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) && WinHttpReceiveResponse(q, nullptr)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(q, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
        r->status = (int)code;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(q, &avail) && avail) {
          std::string chunk(avail, 0);
          DWORD got = 0;
          if (!WinHttpReadData(q, &chunk[0], avail, &got) || !got) break;
          r->body.append(chunk, 0, got);
          if (r->body.size() > 65536) break;
        }
      }
      WinHttpCloseHandle(q);
    }
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
  }
  PostMessageW(hw, WM_LICENSE, 0, (LPARAM)r);
}
static void licRequest(int op, const std::string& key) {
  if (g_licBusy) return;
  g_licBusy = true;
  g_licLastTry = licNow();
  if (op == 1) { g_licMsg = L"Activating\x2026"; g_licMsgErr = false; }
  HWND hw = g_hwnd;
  std::string server = licServer();
  std::thread([op, key, server, hw]() { licWorker(op, key, server, hw); }).detach();
  redraw();
}
static void licOpen();
static wstring licDate(long long t);
static void licDone(LicResult* r) {
  g_licBusy = false;
  json j = json::parse(r->body, nullptr, false);
  bool isObj = j.is_object();
  std::string tok = isObj ? j.value("token", "") : "";
  if (r->op == 3) {  // this computer was released
    if (r->status == 200) {
      g_licToken.clear(); g_licKey.clear(); g_lic = LicInfo(); licSave();
      g_licMsg = L"This computer was deactivated. The license can be used on another one.";
      g_licMsgErr = false;
      licOpen();
    } else { g_licMsg = isObj ? fromUtf8(j.value("message", "The server refused.")) : L"Could not reach the license server."; g_licMsgErr = true; }
  } else if (r->status == 200 && !tok.empty()) {
    LicInfo li;
    int st = licCheckToken(tok, li);
    if (st == LS_OK) {
      g_licToken = tok;
      g_lic = li;
      g_lic.state = LS_OK;
      g_licSeen = licNow();
      if (r->op == 1) { g_licKey = toUtf8(trimW(g_licEdit.t)); }
      licSave();
      if (r->op == 1) { g_licMsg = L"Activated. Thank you!"; g_licMsgErr = false; g_licOpen = false; g_licEdit.set(L""); }
      else {
        if (g_licManual) { g_licMsg = L"Checked with the server: the license is valid."; g_licMsgErr = false; }
        else { g_licMsg.clear(); g_licMsgErr = false; }
        if (g_licOpen && licLocked()) g_licOpen = false;
      }
      g_licManual = false;  // a new token: the editor opens again if it was locked
    } else { g_licMsg = L"The answer of the server could not be verified."; g_licMsgErr = true; }
  } else if (r->status == 0) {  // no answer
    if (g_licManual) { g_licMsg = L"Could not reach the license server. The license keeps working until " + licDate(g_lic.exp) + L"."; g_licMsgErr = true; g_licManual = false; }
    if (r->op == 1) { g_licMsg = L"Could not reach the license server (" + fromUtf8(licServer()) + L")."; g_licMsgErr = true; }
    // a refresh that does not get an answer changes nothing: the token it has keeps working until it runs out
  } else {  // the server said no: revoked, expired, not activated on this computer ...
    wstring m = isObj ? fromUtf8(j.value("message", "The license was refused.")) : L"The license was refused.";
    g_licManual = false;
    g_licMsg = m;
    g_licMsgErr = true;
    if (r->op == 2) {
      g_licToken.clear();
      g_lic = LicInfo();
      g_lic.state = LS_REVOKED;
      licSave();
      g_licOpen = true;
    }
  }
  delete r;
  redraw();
}

/* ---------------------------------------------------------------- state, checks */
static bool licLocked() { return !g_licBypass && g_lic.state != LS_OK; }
// the stored token is checked again now (a second, separate check of the same thing)
static void licRecheck() {
  if (g_licBypass) return;
  if (g_lic.state == LS_OK) {
    LicInfo li;
    int st = licCheckToken(g_licToken, li);
    if (st != LS_OK) { g_lic = LicInfo(); g_lic.state = st; g_licOpen = true; redraw(); }
  }
}
static void licOpen() { g_licOpen = true; g_licEdit.set(L""); if (g_lic.state == LS_OK) g_licMsg.clear(); caretReset(); redraw(); }
static void licInit() {
  g_licMachine = licMachineId();
  wchar_t v[8];
#ifdef SCRIBA_TEST_BYPASS
  g_licBypass = g_testing && !GetEnvironmentVariableW(L"SCRIBA_LIC_REAL", v, 8);  // the scripted tests run without a license server (compiled out of a release: cmake -DSCRIBA_TEST_BYPASS=OFF)
#else
  (void)v;
#endif
  licLoad();
  long long now = licNow();
  g_lic = LicInfo();
  if (!g_licToken.empty()) {
    LicInfo li;
    int st = licCheckToken(g_licToken, li);
    bool clockBack = st == LS_OK && g_licSeen && now + 86400 < g_licSeen;  // the clock was put back since the last run
    if (clockBack) st = LS_TAMPER;
    g_lic = li;
    g_lic.state = st;
    if (st == LS_TAMPER) { g_licMsg = clockBack ? L"The clock of the computer is wrong. Connect to the internet to check the license." : L"The license file of this computer is not valid. Connect to the internet to activate it again."; g_licMsgErr = true; }
  }
  if (g_lic.state == LS_OK && now > g_licSeen) { g_licSeen = now; licSave(); }
  if (!g_licKey.empty() && !g_licToken.empty()) licRequest(2, g_licKey);  // a new token (and the news if the license was revoked)
  else if (!g_licKey.empty() && g_lic.state != LS_OK) licRequest(2, g_licKey);
  if (licLocked()) g_licOpen = true;
}
// every minute: the check again, and a new token when the one it has is more than half used
static void licTick() {
  if (g_licBypass) return;
  licRecheck();
  long long now = licNow();
  if (g_lic.state == LS_OK && !g_licBusy && !g_licKey.empty() && now - g_licLastTry > 900) licRequest(2, g_licKey);  // every 15 minutes: a revoked license stops working within that time
  if (g_lic.state == LS_OK && now > g_licSeen + 3600) { g_licSeen = now; licSave(); }
}
// commands that work while the editor is locked
static bool licBlock(int cmd) {
  if (g_licBypass || g_lic.state == LS_OK) return false;
  return cmd != C_LICENSE && cmd != C_EXIT && cmd != C_ABOUT;
}

/* ---------------------------------------------------------------- the page */
static wstring licDate(long long t) {
  struct tm tmv;
  time_t tt = (time_t)t;
  localtime_s(&tmv, &tt);
  wchar_t b[32];
  wcsftime(b, 32, L"%Y-%m-%d", &tmv);
  return b;
}
static void licActivate() {
  wstring k = trimW(g_licEdit.t);
  if (k.size() < 8) { g_licMsg = L"Type the license key you received."; g_licMsgErr = true; redraw(); return; }
  licRequest(1, toUtf8(k));
}
static void drawLicense() {
  if (!g_licOpen) return;
  const Theme& t = T();
  g_licRects.clear();
  float W = cw(), H = ch(), top = TITLE_H;
  fillRect(0, top, W, H - top, t.bg);
  float cwid = std::min(560.f, W - 40), x = (W - cwid) / 2, y = top + std::max(30.f, (H - top - 470) / 2);
  bool ok = g_lic.state == LS_OK;
  // the name
  drawText(L"Scriba", 6, x, y, t.accent, uiFont(30.f, true), 300, 40);
  y += 46;
  const wchar_t* sub = ok ? L"License" : L"Activate your license to start";
  drawText(sub, (int)wcslen(sub), x, y, t.fg, uiFont(15.f), cwid, 24);
  y += 40;
  // the card
  D2D1_RECT_F card = rc(x, y, cwid, ok ? 150 : 98);
  fillR(card, t.panel, 1.f, 8);
  strokeR(card, t.border, 1.f, 8);
  if (ok) {
    wstring l1 = L"Licensed to " + (g_lic.name.empty() ? wstring(L"you") : g_lic.name);
    drawText(l1.c_str(), (int)l1.size(), x + 20, y + 16, t.fg, uiFont(15.f, true), cwid - 40, 24);
    wstring l2 = L"Valid on this computer. Checked online, works without a connection until " + licDate(g_lic.exp) + L".";
    drawWrapped(l2, uiFont(12.5f), x + 20, y + 48, cwid - 40, t.statusText, 1.f, 18.f);
    if (g_lic.until) { wstring l3 = L"The license itself runs until " + licDate(g_lic.until) + L"."; drawText(l3.c_str(), (int)l3.size(), x + 20, y + 88, t.statusText, uiFont(12.5f), cwid - 40, 20); }
    wstring id = L"Computer: " + fromUtf8(g_licMachine.substr(0, 12)) + L"\x2026";
    drawText(id.c_str(), (int)id.size(), x + 20, y + 118, t.gutterFg, uiFont(11.5f, false, true), cwid - 40, 18);
  } else {
    wstring l1 = g_lic.state == LS_REVOKED ? L"This license is no longer valid on this computer." : g_lic.state == LS_EXPIRED ? L"The license on this computer has expired." : g_lic.state == LS_TAMPER ? L"The license on this computer could not be verified." : L"Scriba needs a license. Nothing works without one.";
    drawWrapped(l1, uiFont(13.5f), x + 20, y + 18, cwid - 40, t.fg, 1.f, 20.f);
    drawText(L"Enter the key you received by email and press Activate.", 54, x + 20, y + 58, t.statusText, uiFont(12.5f), cwid - 40, 20);
  }
  y += (ok ? 150 : 98) + 22;
  // the key
  if (!ok) {
    D2D1_RECT_F in = rc(x, y, cwid - 130, 40);
    drawInput(g_licEdit, in, !g_licBusy, L"SCRB-XXXX-XXXX-XXXX-XXXX");
    g_licRects.push_back(in); g_licKind[g_licRects.size() - 1] = 1;
    D2D1_RECT_F ab = rc(x + cwid - 118, y, 118, 40);
    bool hv = inRect(ab, g_mx, g_my);
    fillR(ab, t.accent, g_licBusy ? 0.5f : (hv ? 1.f : 0.9f), 6);
    float tw = textWidth(L"Activate", uiFont(13.5f, true));
    drawText(L"Activate", 8, ab.left + (118 - tw) / 2, ab.top + 10, 0x1a1a1a, uiFont(13.5f, true), 118, 22);
    g_licRects.push_back(ab); g_licKind[g_licRects.size() - 1] = 2;
    y += 56;
  }
  if (!g_licMsg.empty()) {
    drawWrapped(g_licMsg, uiFont(12.5f), x, y, cwid, g_licMsgErr ? 0xec7b7b : t.fg, 1.f, 18.f);
    y += wrappedHeight(g_licMsg, uiFont(12.5f), cwid, 18.f) + 14;
  }
  // buttons
  struct B { const wchar_t* l; int kind; bool show; };
  wstring buy = sStr("license_buy_url");
  B bs[] = {{L"Buy a license", 3, !buy.empty()}, {L"Check now", 7, ok}, {L"Deactivate this computer", 4, ok}, {L"Close", 5, ok}, {L"Quit Scriba", 6, !ok}};
  float bx = x;
  for (auto& b : bs) {
    if (!b.show) continue;
    float w = textWidth(b.l, uiFont(12.5f)) + 28;
    D2D1_RECT_F r = rc(bx, y, w, 32);
    bool hv = inRect(r, g_mx, g_my);
    fillR(r, t.fg, hv ? 0.12f : 0.06f, 5);
    strokeR(r, t.border, 1.f, 5);
    drawText(b.l, (int)wcslen(b.l), r.left + 14, r.top + 6, t.fg, uiFont(12.5f), w, 20);
    g_licRects.push_back(r); g_licKind[g_licRects.size() - 1] = b.kind;
    bx += w + 10;
  }
  wstring foot = L"Server: " + fromUtf8(licServer());
  drawText(foot.c_str(), (int)foot.size(), x, H - 34, t.gutterFg, uiFont(11.f, false, true), cwid, 18);
}
static void licMouse(float x, float y, int what) {
  if (what != 0) return;
  for (size_t i = 0; i < g_licRects.size() && i < 8; i++) {
    if (!inRect(g_licRects[i], x, y)) continue;
    switch (g_licKind[i]) {
      case 1: lineEditClick(g_licEdit, g_licRects[i], x); caretReset(); break;
      case 2: licActivate(); break;
      case 3: ShellExecuteW(g_hwnd, L"open", sStr("license_buy_url").c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
      case 4:
        if (appMsgBox(g_hwnd, L"Deactivate Scriba on this computer? You can activate the license again here or on another computer.", L"License", MB_YESNO | MB_ICONQUESTION) == IDYES) licRequest(3, g_licKey);
        break;
      case 5: g_licOpen = false; break;
      case 6: PostMessageW(g_hwnd, WM_CLOSE, 0, 0); break;
      case 7: if (!g_licBusy && !g_licKey.empty()) { g_licManual = true; g_licMsg = L"Checking with the server 26"; g_licMsgErr = false; licRequest(2, g_licKey); } break;
    }
    redraw();
    return;
  }
}
static bool licKey(WPARAM wp, bool ctrl, bool shift, bool alt) {
  (void)alt;
  if (wp == VK_ESCAPE && g_lic.state == LS_OK) { g_licOpen = false; redraw(); return true; }
  if (wp == VK_RETURN) { licActivate(); return true; }
  if (g_lic.state != LS_OK && !g_licBusy && editKey(g_licEdit, wp, ctrl, shift)) { for (auto& c : g_licEdit.t) c = (wchar_t)towupper(c); }
  caretReset();
  redraw();
  return true;
}
static void licChar(wchar_t c) {
  if (g_lic.state == LS_OK || g_licBusy) return;
  if (!(iswalnum(c) || c == L'-')) return;
  g_licEdit.insert(wstring(1, (wchar_t)towupper(c)));
  caretReset();
  redraw();
}
