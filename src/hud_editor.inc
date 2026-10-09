// hud_editor.inc: the AMXX HUD Editor of the AMXXEditorV4 package (destro-.github.io/AMXXEditorV4/HUDEditor.html) as a page of the editor:
// a HUD / DHUD message with text, colours, position (drag it on the picture), timings, effects and channel, a preview that plays the
// Half-Life animation (fade, flicker, typewriter), and the generated Pawn code, which can be copied, shared as a link or inserted in the plugin.
// Included by main.cpp after amxx_tools.inc.

struct HudMsg {
  wstring text = L"Welcome to the server!";
  int r = 90, g = 190, b = 255, r2 = 255, g2 = 255, b2 = 250;
  double x = 0.45, y = 0.15;
  bool cx = false, cy = false;
  int effect = 0;
  double fx = 0.3, hold = 3.0, fadeIn = 1.0, fadeOut = 0.75;
  int channel = -1;
  bool dhud = false;
};
enum { HF_R, HF_G, HF_B, HF_R2, HF_G2, HF_B2, HF_X, HF_Y, HF_FX, HF_HOLD, HF_FIN, HF_FOUT, HF_COUNT };
struct HudFld { double step, mn, mx; int dec; };
static const HudFld HUD_FLD[HF_COUNT] = {{10, 0, 255, 0}, {10, 0, 255, 0}, {10, 0, 255, 0}, {10, 0, 255, 0}, {10, 0, 255, 0}, {10, 0, 255, 0},
                                         {0.01, -1, 1, 2}, {0.01, -1, 1, 2}, {0.1, 0, 10, 2}, {0.1, 0.1, 60, 2}, {0.05, 0.05, 5, 2}, {0.05, 0.05, 5, 2}};
static const float HUD_REF_H[5] = {600, 768, 864, 960, 1080};
static const wchar_t* HUD_RES[5] = {L"800\x00d7" L"600", L"1024\x00d7" L"768", L"1152\x00d7" L"864", L"1280\x00d7" L"960", L"1440\x00d7" L"1080"};
static const wchar_t* HUD_MAPS[4] = {L"de_dust2", L"de_inferno", L"Ingame HUD", L"Ingame DHUD"};
static const char* HUD_SHARE = "https://destro-.github.io/AMXXEditorV4/HUDEditor.html#";

enum { HK_CLOSE = 1, HK_TYPE, HK_AREA, HK_FIELD, HK_SWATCH, HK_CENTERX, HK_CENTERY, HK_EFF, HK_CH, HK_PLAY, HK_STOP, HK_MAP, HK_RES, HK_COPY, HK_SHARE, HK_PASTE, HK_INSERT, HK_CANVAS, HK_SIDE, HK_PRESET, HK_STEP, HK_RESET };
struct HudHit { D2D1_RECT_F r; int kind, idx; bool side; };

static bool g_hudOpen = false;
static HudMsg g_hm;
static int g_hudMap = 0, g_hudRes = 0;
static bool g_hudPlaying = false, g_hudFlick = false, g_hudDrag = false;
static ULONGLONG g_hudStart = 0;
static LineEdit g_hf[HF_COUNT];
static TextArea g_hArea;
static int g_hFocus = -1;  // -1 nothing, -2 the text box, else a HF_ field
static float g_hScroll = 0, g_hContent = 0, g_hSideViewTop = 0, g_hSideViewH = 0;
static vector<HudHit> g_hHits;
static D2D1_RECT_F g_hCanvas{0, 0, 0, 0}, g_hTextBox{0, 0, 0, 0};
static float g_hDragX = 0, g_hDragY = 0;
static bool g_hSnapX = false, g_hSnapY = false;
static wstring g_hMsg;
static ULONGLONG g_hMsgAt = 0;
static ID2D1Bitmap* g_hBg[4] = {nullptr, nullptr, nullptr, nullptr};
static ID2D1RenderTarget* g_hBgRt = nullptr;

/* ---------------------------------------------------------------- model <-> fields */
static wstring hudNum(double v, int dec) {
  wchar_t b[48];
  swprintf_s(b, L"%.*f", dec, v);
  wstring s = b;
  if (dec > 0) {
    while (!s.empty() && s.back() == L'0') s.pop_back();
    if (!s.empty() && s.back() == L'.') s.pop_back();
  }
  if (s == L"-0") s = L"0";
  return s;
}
static double hudGet(int f) {
  const HudMsg& m = g_hm;
  switch (f) {
    case HF_R: return m.r; case HF_G: return m.g; case HF_B: return m.b;
    case HF_R2: return m.r2; case HF_G2: return m.g2; case HF_B2: return m.b2;
    case HF_X: return m.cx ? -1 : m.x; case HF_Y: return m.cy ? -1 : m.y;
    case HF_FX: return m.fx; case HF_HOLD: return m.hold; case HF_FIN: return m.fadeIn; default: return m.fadeOut;
  }
}
static void hudLoadFields() {
  for (int i = 0; i < HF_COUNT; i++) if (i != g_hFocus) g_hf[i].set(hudNum(hudGet(i), HUD_FLD[i].dec));
}
static void hudStop() {
  g_hudPlaying = false;
  KillTimer(g_hwnd, 13);
}
// a field was typed in: the model follows (an empty or invalid number counts as 0, as in the web tool)
static void hudCommit(int f) {
  double v = wcstod(g_hf[f].t.c_str(), nullptr);
  HudMsg& m = g_hm;
  auto cl = [](double x, double a, double b) { return std::max(a, std::min(b, x)); };
  switch (f) {
    case HF_R: m.r = (int)cl(v, 0, 255); break; case HF_G: m.g = (int)cl(v, 0, 255); break; case HF_B: m.b = (int)cl(v, 0, 255); break;
    case HF_R2: m.r2 = (int)cl(v, 0, 255); break; case HF_G2: m.g2 = (int)cl(v, 0, 255); break; case HF_B2: m.b2 = (int)cl(v, 0, 255); break;
    case HF_X: m.x = v; m.cx = v == -1; break;
    case HF_Y: m.y = v; m.cy = v == -1; break;
    case HF_FX: m.fx = v != 0 ? v : 0.1; break;
    case HF_HOLD: m.hold = v != 0 ? v : 3; break;
    case HF_FIN: m.fadeIn = std::max(v != 0 ? v : 0.5, 0.05); break;
    default: m.fadeOut = std::max(v != 0 ? v : 0.5, 0.05); break;
  }
  hudStop();
}

/* ---------------------------------------------------------------- the Pawn code */
static wstring hudFloat(double v) {
  double r = std::round(v * 100) / 100;
  wstring s = hudNum(r, 2);
  return s.find(L'.') == wstring::npos ? s + L".0" : s;
}
static wstring hudEscape(const wstring& s) {
  wstring o;
  for (wchar_t c : s) { if (c == L'^') o += L"^^"; else if (c == L'"') o += L"^\""; else if (c == L'\n') o += L"^n"; else o += c; }
  return o;
}
// DHUD messages have no channel and no second colour (set_dhudmessage takes 10 arguments)
static wstring hudCode() {
  const HudMsg& m = g_hm;
  double x = m.cx ? -1.0 : m.x, y = m.cy ? -1.0 : m.y;
  wstring a = std::to_wstring(m.r) + L", " + std::to_wstring(m.g) + L", " + std::to_wstring(m.b) + L", " + hudFloat(x) + L", " + hudFloat(y) + L", " +
              std::to_wstring(m.effect) + L", " + hudFloat(m.fx) + L", " + hudFloat(m.hold) + L", " + hudFloat(m.fadeIn) + L", " + hudFloat(m.fadeOut);
  if (!m.dhud) {
    a += L", " + std::to_wstring(m.channel);
    bool def = m.r2 >= 250 && m.g2 >= 250 && m.b2 >= 250;
    if (!def) a += L", 0, {" + std::to_wstring(m.r2) + L", " + std::to_wstring(m.g2) + L", " + std::to_wstring(m.b2) + L", 0}";
  }
  return wstring(m.dhud ? L"set_dhudmessage(" : L"set_hudmessage(") + a + L")\n" + (m.dhud ? L"show_dhudmessage" : L"show_hudmessage") + L"(0, \"" + hudEscape(m.text) + L"\")";
}

/* ---------------------------------------------------------------- share links (base64url JSON, like the web tool) */
static std::string hudB64(const std::string& in) {
  static const char* tb = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string o;
  size_t i = 0;
  for (; i + 2 < in.size(); i += 3) {
    unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8) | (unsigned char)in[i + 2];
    o += tb[v >> 18]; o += tb[(v >> 12) & 63]; o += tb[(v >> 6) & 63]; o += tb[v & 63];
  }
  if (i + 1 == in.size()) { unsigned v = (unsigned char)in[i] << 16; o += tb[v >> 18]; o += tb[(v >> 12) & 63]; }
  else if (i + 2 == in.size()) { unsigned v = ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8); o += tb[v >> 18]; o += tb[(v >> 12) & 63]; o += tb[(v >> 6) & 63]; }
  return o;
}
static std::string hudUnB64(const std::string& in) {
  std::string o;
  unsigned v = 0;
  int bits = 0;
  for (char c : in) {
    int d = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '-' || c == '+' ? 62 : c == '_' || c == '/' ? 63 : -1;
    if (d < 0) continue;
    v = (v << 6) | d;
    bits += 6;
    if (bits >= 8) { bits -= 8; o += (char)((v >> bits) & 255); }
  }
  return o;
}
static std::string hudShareUrl() {
  const HudMsg& m = g_hm;
  json o = {{"t", toUtf8(m.text)}, {"r", m.r}, {"g", m.g}, {"b", m.b}, {"r2", m.r2}, {"g2", m.g2}, {"b2", m.b2},
            {"x", std::round((m.cx ? -1 : m.x) * 100) / 100}, {"y", std::round((m.cy ? -1 : m.y) * 100) / 100}, {"eff", m.effect},
            {"fx", m.fx}, {"h", m.hold}, {"fi", m.fadeIn}, {"fo", m.fadeOut}, {"ch", m.channel}, {"dhud", m.dhud ? 1 : 0}};
  return std::string(HUD_SHARE) + hudB64(o.dump());
}
// a link (or just its part after #) of the web tool: the new format or the old "1+255+255+50+-1.0+0.5+0+0.0+4.0+0.0+0.0+3"
static bool hudLoadHash(std::string h) {
  size_t p = h.find('#');
  if (p != std::string::npos) h = h.substr(p + 1);
  while (!h.empty() && isspace((unsigned char)h.back())) h.pop_back();
  if (h.empty()) return false;
  HudMsg m = g_hm;
  auto cl = [](double v, double a, double b) { return std::max(a, std::min(b, v)); };
  if (h.find('+') != std::string::npos) {
    vector<std::string> parts;
    size_t s = 0;
    while (true) { size_t q = h.find('+', s); if (q == std::string::npos) { parts.push_back(h.substr(s)); break; } parts.push_back(h.substr(s, q - s)); s = q + 1; }
    auto get = [&](size_t i, double def) { if (i >= parts.size() || parts[i].empty() || parts[i] == "*") return def; char* e; double v = strtod(parts[i].c_str(), &e); return e == parts[i].c_str() ? def : v; };
    m.dhud = parts[0] == "2";
    m.r = (int)cl(get(1, 200), 0, 255); m.g = (int)cl(get(2, 100), 0, 255); m.b = (int)cl(get(3, 0), 0, 255);
    m.x = get(4, -1.0); m.y = get(5, 0.35); m.effect = (int)get(6, 0); m.fx = get(7, 6.0); m.hold = get(8, 12.0); m.fadeIn = get(9, 0.1); m.fadeOut = get(10, 0.2);
    m.channel = !m.dhud ? (int)get(11, -1) : -1;
  } else {
    try {
      json o = json::parse(hudUnB64(h));
      auto num = [&](const char* k, double def) { return o.contains(k) && o[k].is_number() ? o[k].get<double>() : def; };
      if (o.contains("t") && o["t"].is_string()) m.text = fromUtf8(o["t"].get<std::string>());
      m.r = (int)num("r", m.r); m.g = (int)num("g", m.g); m.b = (int)num("b", m.b);
      m.r2 = (int)num("r2", m.r2); m.g2 = (int)num("g2", m.g2); m.b2 = (int)num("b2", m.b2);
      m.x = num("x", m.x); m.y = num("y", m.y); m.effect = (int)num("eff", m.effect);
      m.fx = num("fx", m.fx); m.hold = num("h", m.hold); m.fadeIn = num("fi", m.fadeIn); m.fadeOut = num("fo", m.fadeOut);
      m.channel = (int)num("ch", m.channel); m.dhud = num("dhud", 0) == 1;
    } catch (...) { return false; }
  }
  m.cx = m.x == -1;
  m.cy = m.y == -1;
  m.effect = std::max(0, std::min(2, m.effect));
  g_hm = m;
  g_hArea.set(g_hm.text);
  hudLoadFields();
  hudStop();
  return true;
}

/* ---------------------------------------------------------------- the HUD of Half-Life: positions, colours, animation */
struct HudCol { float alpha; int r, g, b; };
static HudCol hudCharColor(const HudMsg& m, double t, int idx, int total) {
  double fadeIn = std::max(m.fadeIn, 0.001), fadeOut = std::max(m.fadeOut, 0.001), hold = std::max(m.hold, 0.0);
  int sr = m.r, sg = m.g, sb = m.b;
  int dr = m.dhud ? m.r : m.r2, dg = m.dhud ? m.g : m.g2, db = m.dhud ? m.b : m.b2;
  auto cl01 = [](double v) { return (float)std::max(0.0, std::min(1.0, v)); };
  if (m.effect == 0 || m.effect == 1) {
    double fadeTime = fadeIn + hold, a;
    if (t < fadeIn) a = t / fadeIn;
    else if (t > fadeTime) a = fadeOut > 0 ? 1 - (t - fadeTime) / fadeOut : 0;
    else a = 1;
    return {cl01(a), sr, sg, sb};
  }
  double charTime = idx * fadeIn, fadeTime = fadeIn * total + hold;
  if (charTime > t) return {0, sr, sg, sb};
  double delta = t - charTime;
  if (t > fadeTime) return {fadeOut > 0 ? cl01(1 - (t - fadeTime) / fadeOut) : 0.f, sr, sg, sb};
  if (delta > m.fx) return {1, sr, sg, sb};
  double k = delta / std::max(m.fx, 0.001);
  return {1, (int)std::lround(dr + (sr - dr) * k), (int)std::lround(dg + (sg - dg) * k), (int)std::lround(db + (sb - db) * k)};
}
static double hudDuration() {
  const HudMsg& m = g_hm;
  int chars = std::max(1, (int)m.text.size());
  return m.effect == 2 ? std::max(m.fadeIn, 0.001) * chars + std::max(m.hold, 0.0) + std::max(m.fadeOut, 0.001) + 0.1
                       : std::max(m.fadeIn, 0.001) + std::max(m.hold, 0.0) + std::max(m.fadeOut, 0.001) + 0.1;
}
static void hudPlay() {
  g_hudPlaying = true;
  g_hudStart = GetTickCount64();
  SetTimer(g_hwnd, 13, 16, nullptr);
  redraw();
}
static void hudTick() {
  if (!g_hudOpen || !g_hudPlaying) { KillTimer(g_hwnd, 13); return; }
  if ((GetTickCount64() - g_hudStart) / 1000.0 >= hudDuration()) hudStop();
  redraw();
}
static void hudSay(const wstring& s) { g_hMsg = s; g_hMsgAt = GetTickCount64(); redraw(); }

static ID2D1Bitmap* hudBackground(int idx) {
  if (g_hBgRt != g_rt) {
    for (auto& b : g_hBg) if (b) { b->Release(); b = nullptr; }
    g_hBgRt = g_rt;
  }
  if (g_hBg[idx]) return g_hBg[idx];
  HRSRC rs = FindResourceW(nullptr, MAKEINTRESOURCEW(201 + idx), RT_RCDATA);
  if (!rs) return nullptr;
  HGLOBAL hg = LoadResource(nullptr, rs);
  const BYTE* data = (const BYTE*)LockResource(hg);
  DWORD size = SizeofResource(nullptr, rs);
  if (!data || !size) return nullptr;
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  IWICImagingFactory* f = nullptr;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f)))) return nullptr;
  IWICStream* st = nullptr;
  IWICBitmapDecoder* dec = nullptr;
  IWICBitmapFrameDecode* fr = nullptr;
  IWICFormatConverter* conv = nullptr;
  if (SUCCEEDED(f->CreateStream(&st)) && SUCCEEDED(st->InitializeFromMemory(const_cast<BYTE*>(data), size)) &&
      SUCCEEDED(f->CreateDecoderFromStream(st, nullptr, WICDecodeMetadataCacheOnDemand, &dec)) && SUCCEEDED(dec->GetFrame(0, &fr)) &&
      SUCCEEDED(f->CreateFormatConverter(&conv)) && SUCCEEDED(conv->Initialize(fr, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut)))
    g_rt->CreateBitmapFromWicBitmap(conv, nullptr, &g_hBg[idx]);
  if (conv) conv->Release();
  if (fr) fr->Release();
  if (dec) dec->Release();
  if (st) st->Release();
  f->Release();
  return g_hBg[idx];
}

static void hudGlyphs(const wchar_t* s, int n, float x, float y, DWORD col, float alpha, IDWriteTextFormat* f, bool glow) {
  if (n <= 0 || alpha <= 0) return;
  if (glow) {
    g_br->SetColor(Rgb(col, alpha * 0.16f));
    for (auto o : {std::make_pair(-1.5f, 0.f), std::make_pair(1.5f, 0.f), std::make_pair(0.f, -1.5f), std::make_pair(0.f, 1.5f)})
      g_rt->DrawText(s, (UINT32)n, f, D2D1::RectF(x + o.first, y + o.second, x + 4000, y + 200), g_br, D2D1_DRAW_TEXT_OPTIONS_NONE);
  }
  g_br->SetColor(Rgb(col, alpha));
  g_rt->DrawText(s, (UINT32)n, f, D2D1::RectF(x, y, x + 4000, y + 200), g_br, D2D1_DRAW_TEXT_OPTIONS_NONE);
}
static DWORD hudRgb(int r, int g, int b) { return ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b; }

// the picture with the message on it; the message is where CHudMessage::XPosition / YPosition of the game puts it
static void hudDrawCanvas(const D2D1_RECT_F& cv) {
  HudMsg& m = g_hm;
  float W = cv.right - cv.left, H = cv.bottom - cv.top;
  fillR(cv, 0x000000, 1.f);
  if (ID2D1Bitmap* bg = hudBackground(g_hudMap)) g_rt->DrawBitmap(bg, cv, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
  g_rt->PushAxisAlignedClip(cv, D2D1_ANTIALIAS_MODE_ALIASED);
  float scale = H / (m.dhud ? 500.f : HUD_REF_H[g_hudRes]);
  float FS = 17.f * scale, LH = FS * 1.2f;
  IDWriteTextFormat* f = uiFont(FS, false, true);
  vector<wstring> lines;
  {
    size_t p = 0;
    wstring t = m.text.empty() || trimW(m.text).empty() ? L"<TEXT>" : m.text;
    while (true) { size_t q = t.find(L'\n', p); if (q == wstring::npos) { lines.push_back(t.substr(p)); break; } lines.push_back(t.substr(p, q - p)); p = q + 1; }
  }
  float cwid = textWidth(L"MMMMMMMMMM", f) / 10.f;
  vector<float> lw;
  float totalW = 0;
  for (auto& l : lines) { lw.push_back(cwid * (float)l.size()); totalW = std::max(totalW, lw.back()); }
  float totalH = lines.size() * LH;
  double yy = std::min(m.y, 0.98);
  float ty = (LH - FS * 1.3f) / 2;
  if (!g_hudPlaying) {
    float px = (m.cx || m.x == -1) ? (W - totalW) / 2 : m.x < 0 ? (float)(1 + m.x) * W - totalW : (float)m.x * W;
    float py = (m.cy || m.y == -1) ? (H - totalH) / 2 : yy < 0 ? (float)(1 + yy) * H - totalH : (float)yy * H;
    float bright = std::max(0.f, std::min(1.f, (m.r + m.g + m.b) / 255.f));
    float op = std::max(bright, 0.15f);
    DWORD col = hudRgb(m.r, m.g, m.b);
    for (size_t i = 0; i < lines.size(); i++)
      hudGlyphs(lines[i].c_str(), (int)lines[i].size(), cv.left + px, cv.top + py + i * LH + ty, col, op, f, bright > 0.12f);
    g_hTextBox = rc(cv.left + px, cv.top + py, std::max(totalW, 20.f), totalH);
    strokeR(rc(g_hTextBox.left - 3, g_hTextBox.top - 3, g_hTextBox.right - g_hTextBox.left + 6, g_hTextBox.bottom - g_hTextBox.top + 6), 0xffffff, bright > 0.12f ? 0.15f : 0.4f);
    g_hHits.push_back({g_hTextBox, HK_CANVAS, 0, false});
  } else {
    double t = (GetTickCount64() - g_hudStart) / 1000.0;
    if (m.effect == 1) g_hudFlick = (rand() % 100) < 10;
    int total = std::max(1, (int)m.text.size());
    double baseY = (m.cy || m.y == -1) ? (H - totalH) / 2 : m.y < 0 ? (1 + m.y) * H - totalH : m.y * H;
    baseY = std::max(0.0, std::min(baseY, (double)(H - totalH)));
    int gi = 0;
    for (size_t li = 0; li < lines.size(); li++) {
      double bx = (m.cx || m.x == -1) ? (W - lw[li]) / 2 : m.x < 0 ? (1 + m.x) * W - totalW : m.x * W;
      bx = std::max(0.0, std::min(bx, (double)(W - lw[li])));
      float cx = (float)bx;
      for (wchar_t ch : lines[li]) {
        if (m.text.empty()) break;
        HudCol c = hudCharColor(m, t, gi++, total);
        if (c.alpha > 0) {
          float bright = std::max(0.f, std::min(1.f, (c.r + c.g + c.b) / 255.f));
          float fa = c.alpha * bright;
          wchar_t s[2] = {ch, 0};
          if (fa > 0) hudGlyphs(s, 1, cv.left + cx, cv.top + (float)baseY + li * LH + ty, hudRgb(c.r, c.g, c.b), fa, f, true);
          if (m.effect == 1 && g_hudFlick) {
            float fb = std::max(0.f, std::min(1.f, (m.r2 + m.g2 + m.b2) / 255.f));
            hudGlyphs(s, 1, cv.left + cx, cv.top + (float)baseY + li * LH + ty, hudRgb(m.r2, m.g2, m.b2), c.alpha * fb, f, true);
          }
        }
        cx += cwid;
      }
    }
  }
  if (g_hudDrag) {  // guides at the centre of the picture and the position being set
    g_br->SetColor(Rgb(0x00d4ff, 0.7f));
    if (m.cx) g_rt->DrawLine(D2D1::Point2F(cv.left + W / 2, cv.top), D2D1::Point2F(cv.left + W / 2, cv.bottom), g_br, 1.f);
    if (m.cy) g_rt->DrawLine(D2D1::Point2F(cv.left, cv.top + H / 2), D2D1::Point2F(cv.right, cv.top + H / 2), g_br, 1.f);
    wstring pos = L"x " + hudNum(hudGet(HF_X), 2) + L"   y " + hudNum(hudGet(HF_Y), 2);
    float pw = textWidth(pos, uiFont(11.f, false, true)) + 16;
    float px0 = std::max(cv.left + 4, std::min(cv.right - pw - 4, g_hTextBox.left)), py0 = std::min(cv.bottom - 26, g_hTextBox.bottom + 8);
    fillR(rc(px0, py0, pw, 20), 0x000000, 0.7f, 5);
    drawText(pos.c_str(), (int)pos.size(), px0 + 8, py0 + 2, 0xffffff, uiFont(11.f, false, true), pw, 18);
  }
  g_rt->PopAxisAlignedClip();
  if (!g_hudPlaying && !g_hudDrag) {
    const wchar_t* hint = L"DRAG THE TEXT TO MOVE IT";
    float w = textWidth(hint, uiFont(10.f));
    drawText(hint, (int)wcslen(hint), cv.left + (W - w) / 2, cv.top + 8, 0xffffff, uiFont(10.f), w + 8, 16);
    g_br->SetColor(Rgb(0x000000, 0));
  }
}

/* ---------------------------------------------------------------- drawing the page */
static void hudHit(const D2D1_RECT_F& r, int kind, int idx, bool side) { g_hHits.push_back({r, kind, idx, side}); }
static void hudLabel(const wchar_t* s, float x, float y) { drawText(s, (int)wcslen(s), x, y, T().statusText, uiFont(10.5f, false, true), 260, 16); }
static void hudSection(const wchar_t* s, float x, float& y, float w) {
  const Theme& t = T();
  fillRect(x, y + 7, 9, 1, t.accent);
  drawText(s, (int)wcslen(s), x + 14, y, t.accent, uiFont(10.5f, true, true), w, 16);
  y += 22;
}
static void hudButton(const D2D1_RECT_F& r, const wchar_t* label, bool on, int kind, int idx, bool side, DWORD onColor = 0) {
  const Theme& t = T();
  bool hv = inRect(r, g_mx, g_my) && (!side || (g_my >= g_hSideViewTop && g_my < g_hSideViewTop + g_hSideViewH));
  DWORD ac = onColor ? onColor : t.accent;
  fillR(r, on ? ac : t.fg, on ? 0.14f : (hv ? 0.1f : 0.05f), 4);
  strokeR(r, on ? ac : t.border, on ? 0.9f : 0.7f, 4);
  float w = textWidth(label, uiFont(11.f, false, true));
  drawText(label, (int)wcslen(label), r.left + (r.right - r.left - w) / 2, r.top + (r.bottom - r.top - 16) / 2, on ? ac : (hv ? t.fg : t.statusText), uiFont(11.f, false, true), w + 8, 18);
  hudHit(r, kind, idx, side);
}
static void hudNumField(int f, const D2D1_RECT_F& r) {
  const Theme& t = T();
  drawInput(g_hf[f], r, g_hFocus == f, L"");
  hudHit(r, HK_FIELD, f, true);
  bool hv = inRect(r, g_mx, g_my) && g_my >= g_hSideViewTop && g_mx < 300;
  if (hv || g_hFocus == f) {  // small up / down arrows at the right end
    float cx = r.right - 13, cy = (r.top + r.bottom) / 2;
    for (int k = 0; k < 2; k++) {
      D2D1_RECT_F ar = rc(cx - 9, k ? cy : r.top + 2, 18, k ? r.bottom - cy - 2 : cy - r.top - 2);
      bool ah = inRect(ar, g_mx, g_my);
      if (ah) fillR(ar, t.fg, 0.12f, 3);
      float ay = k ? cy + (r.bottom - cy) / 2 : r.top + (cy - r.top) / 2;
      g_br->SetColor(Rgb(ah ? t.fg : t.statusText));
      float d = k ? -2.5f : 2.5f;
      g_rt->DrawLine(D2D1::Point2F(cx - 3.5f, ay + d), D2D1::Point2F(cx, ay - d), g_br, 1.4f);
      g_rt->DrawLine(D2D1::Point2F(cx + 3.5f, ay + d), D2D1::Point2F(cx, ay - d), g_br, 1.4f);
      hudHit(ar, HK_STEP, f * 2 + k, true);
    }
  }
}
static void hudSwatch(const D2D1_RECT_F& r, int r0, int g0, int b0, int which) {
  fillR(r, hudRgb(r0, g0, b0), 1.f, 4);
  strokeR(r, inRect(r, g_mx, g_my) ? T().fg : T().border, 0.9f, 4);
  hudHit(r, HK_SWATCH, which, true);
}

static void drawHudEditor() {
  if (!g_hudOpen) return;
  const Theme& t = T();
  g_hHits.clear();
  float W = cw(), H = ch(), top = TITLE_H, headH = 52, sideW = 300;
  fillRect(0, top, W, H - top, t.bg);
  // header
  fillRect(0, top, W, headH, t.panel);
  fillRect(0, top + headH - 1, W, 1, t.border);
  drawText(L"AMXX", 4, 18, top + 8, t.accent, uiFont(16.f, true, true), 80, 24);
  drawText(L"HUD Editor", 10, 70, top + 8, t.fg, uiFont(16.f, true, true), 200, 24);
  drawText(L"HUD MESSAGE EDITOR TOOL  \x2022  FOR AMX MOD X", 41, 18, top + 31, t.statusText, uiFont(9.5f, false, true), 500, 16);
  {
    D2D1_RECT_F rr = rc(W - 18 - 28 - 12 - 74, top + 13, 74, 26);
    hudButton(rr, L"RESET", false, HK_RESET, 0, false, 0xff8a1f);
    D2D1_RECT_F xr = rc(W - 18 - 28, top + 12, 28, 28);
    bool xh = inRect(xr, g_mx, g_my);
    g_br->SetColor(Rgb(xh ? t.fg : t.statusText));
    g_rt->DrawLine(D2D1::Point2F(xr.left + 8, xr.top + 8), D2D1::Point2F(xr.right - 8, xr.bottom - 8), g_br, 1.6f);
    g_rt->DrawLine(D2D1::Point2F(xr.right - 8, xr.top + 8), D2D1::Point2F(xr.left + 8, xr.bottom - 8), g_br, 1.6f);
    hudHit(xr, HK_CLOSE, 0, false);
  }
  float bodyTop = top + headH, bodyH = H - bodyTop;
  g_hSideViewTop = bodyTop;
  g_hSideViewH = bodyH;

  /* ---- left column: the controls (scrolls) ---- */
  fillRect(0, bodyTop, sideW, bodyH, t.panel);
  fillRect(sideW - 1, bodyTop, 1, bodyH, t.border);
  g_rt->PushAxisAlignedClip(rc(0, bodyTop, sideW - 1, bodyH), D2D1_ANTIALIAS_MODE_ALIASED);
  {
    HudMsg& m = g_hm;
    float x = 14, w = sideW - 28, y = bodyTop + 12 - g_hScroll;
    float y0 = y;
    // HUD / DHUD
    hudButton(rc(x, y, w / 2, 30), L"HUD MESSAGE", !m.dhud, HK_TYPE, 0, true);
    hudButton(rc(x + w / 2, y, w / 2, 30), L"DHUD MESSAGE", m.dhud, HK_TYPE, 1, true);
    y += 42;
    // text
    hudSection(L"MESSAGE TEXT", x, y, w);
    {
      D2D1_RECT_F r = rc(x, y, w, 76);
      fillR(r, inputBg(), 1.f, 5);
      strokeR(r, g_hFocus == -2 ? t.accent : t.border, g_hFocus == -2 ? 0.9f : 0.6f, 5);
      g_rt->PushAxisAlignedClip(rc(r.left + 3, r.top + 3, w - 6, 70), D2D1_ANTIALIAS_MODE_ALIASED);
      int vis = 4, first = std::max(0, std::min(g_hArea.l - vis + 1, (int)g_hArea.lines.size() - vis));
      first = std::max(0, first);
      for (int i = first; i < (int)g_hArea.lines.size() && i < first + vis; i++) {
        float ly = r.top + 6 + (i - first) * 17;
        drawText(g_hArea.lines[i].c_str(), (int)g_hArea.lines[i].size(), r.left + 9, ly, t.fg, uiFont(12.f, false, true), w - 16, 18);
        if (g_hFocus == -2 && i == g_hArea.l && g_caretOn) fillRect(r.left + 9 + textWidth(g_hArea.lines[i].substr(0, g_hArea.c), uiFont(12.f, false, true)), ly, 1.5f, 16, t.caret);
      }
      if (g_hArea.text().empty() && g_hFocus != -2) drawText(L"Write your HUD message\x2026", 24, r.left + 9, r.top + 6, t.gutterFg, uiFont(12.f, false, true), w - 16, 18);
      g_rt->PopAxisAlignedClip();
      hudHit(r, HK_AREA, 0, true);
      y += 88;
    }
    // colour
    hudSection(L"COLOR", x, y, w);
    {
      float fw = (w - 34 - 18) / 3;
      const wchar_t* nm[3] = {L"RED", L"GREEN", L"BLUE"};
      for (int i = 0; i < 3; i++) hudLabel(nm[i], x + i * (fw + 6), y);
      y += 16;
      for (int i = 0; i < 3; i++) hudNumField(HF_R + i, rc(x + i * (fw + 6), y, fw, 30));
      hudSwatch(rc(x + w - 30, y, 30, 30), m.r, m.g, m.b, 0);
      y += 38;
      {  // quick colours
        static const int pc[8][3] = {{255, 255, 255}, {255, 70, 70}, {255, 150, 0}, {255, 230, 0}, {60, 255, 100}, {0, 220, 255}, {80, 120, 255}, {255, 100, 200}};
        float sw = (w - 7 * 5) / 8;
        for (int i = 0; i < 8; i++) {
          D2D1_RECT_F pr = rc(x + i * (sw + 5), y, sw, 16);
          fillR(pr, hudRgb(pc[i][0], pc[i][1], pc[i][2]), 1.f, 3);
          strokeR(pr, inRect(pr, g_mx, g_my) ? t.fg : t.border, 0.9f, 3);
          hudHit(pr, HK_PRESET, i, true);
        }
        y += 28;
      }
      if (!m.dhud) {
        hudLabel(L"SECONDARY COLOR (EFFECTS)", x, y);
        y += 16;
        for (int i = 0; i < 3; i++) hudNumField(HF_R2 + i, rc(x + i * (fw + 6), y, fw, 30));
        hudSwatch(rc(x + w - 30, y, 30, 30), m.r2, m.g2, m.b2, 1);
        y += 40;
      }
    }
    // position
    hudSection(L"POSITION", x, y, w);
    {
      float hw = (w - 8) / 2;
      hudLabel(L"X", x, y); hudLabel(L"Y", x + hw + 8, y);
      y += 16;
      hudNumField(HF_X, rc(x, y, hw, 30)); hudNumField(HF_Y, rc(x + hw + 8, y, hw, 30));
      y += 36;
      hudButton(rc(x, y, hw, 24), L"CENTER X", m.cx, HK_CENTERX, 0, true);
      hudButton(rc(x + hw + 8, y, hw, 24), L"CENTER Y", m.cy, HK_CENTERY, 0, true);
      y += 36;
    }
    // timings
    hudSection(L"TIMING", x, y, w);
    {
      float hw = (w - 8) / 2;
      hudLabel(L"FX TIME", x, y); hudLabel(L"HOLD TIME", x + hw + 8, y);
      y += 16;
      hudNumField(HF_FX, rc(x, y, hw, 30)); hudNumField(HF_HOLD, rc(x + hw + 8, y, hw, 30));
      y += 38;
      hudLabel(L"FADE IN", x, y); hudLabel(L"FADE OUT", x + hw + 8, y);
      y += 16;
      hudNumField(HF_FIN, rc(x, y, hw, 30)); hudNumField(HF_FOUT, rc(x + hw + 8, y, hw, 30));
      y += 42;
    }
    hudSection(L"EFFECT", x, y, w);
    {
      const wchar_t* nm[3] = {L"[0] Normal", L"[1] Flicker", L"[2] Typewriter"};
      float bw = w / 3;
      for (int i = 0; i < 3; i++) hudButton(rc(x + i * bw, y, bw, 28), nm[i], m.effect == i, HK_EFF, i, true);
      y += 42;
    }
    if (!m.dhud) {
      hudSection(L"CHANNEL", x, y, w);
      const wchar_t* nm[5] = {L"Auto", L"0", L"1", L"2", L"3"};
      float bw = w / 5;
      for (int i = 0; i < 5; i++) hudButton(rc(x + i * bw, y, bw, 28), nm[i], m.channel == i - 1, HK_CH, i - 1, true);
      y += 42;
    }
    g_hContent = y - y0 + 24;
  }
  g_rt->PopAxisAlignedClip();
  if (g_hContent > bodyH) {  // thin scroll thumb
    float th = std::max(24.f, bodyH * bodyH / g_hContent), ty = bodyTop + (bodyH - th) * (g_hScroll / std::max(1.f, g_hContent - bodyH));
    fillR(rc(sideW - 5, ty, 3, th), t.scroll, 1.f, 1.5f);
  }

  /* ---- right: preview, maps / resolutions, code ---- */
  float rx = sideW, rw = W - sideW, tbH = 36, mapH = 38, codeH = 128;
  {  // toolbar
    fillRect(rx, bodyTop, rw, tbH, t.panel);
    fillRect(rx, bodyTop + tbH - 1, rw, 1, t.border);
    drawText(L"\x25CF PREVIEW", 9, rx + 14, bodyTop + 10, t.accent, uiFont(10.5f, true, true), 160, 18);
    if (g_hudPlaying) drawText(L"\x25B6 PLAYING", 9, rx + 120, bodyTop + 10, 0x00ff88, uiFont(10.5f, false, true), 160, 18);
    D2D1_RECT_F stop = rc(rx + rw - 14 - 78, bodyTop + 5, 78, 26), play = rc(stop.left - 8 - 84, bodyTop + 5, 84, 26);
    hudButton(play, L"\x25B6 PLAY", g_hudPlaying, HK_PLAY, 0, false, 0x00d27a);
    hudButton(stop, L"\x25A0 STOP", false, HK_STOP, 0, false);
  }
  float cvTop = bodyTop + tbH, cvH = H - cvTop - mapH - codeH;
  g_hCanvas = rc(rx, cvTop, rw, std::max(60.f, cvH));
  hudDrawCanvas(g_hCanvas);
  {  // maps and resolutions
    float y = cvTop + g_hCanvas.bottom - g_hCanvas.top;
    y = g_hCanvas.bottom;
    fillRect(rx, y, rw, mapH, t.panel);
    fillRect(rx, y, rw, 1, t.border);
    float x = rx + 12;
    drawText(L"MAP:", 4, x, y + 11, t.accent, uiFont(10.f, false, true), 50, 16);
    x += 38;
    for (int i = 0; i < 4; i++) {
      float w = textWidth(HUD_MAPS[i], uiFont(11.f, false, true)) + 18;
      hudButton(rc(x, y + 6, w, 26), HUD_MAPS[i], g_hudMap == i, HK_MAP, i, false);
      x += w + 4;
    }
    x += 10;
    drawText(L"RESOLUTION:", 11, x, y + 11, t.accent, uiFont(10.f, false, true), 90, 16);
    x += 84;
    for (int i = 0; i < 5; i++) {
      float w = textWidth(HUD_RES[i], uiFont(11.f, false, true)) + 18;
      if (x + w > W - 8) break;
      hudButton(rc(x, y + 6, w, 26), HUD_RES[i], g_hudRes == i, HK_RES, i, false, 0xff8a1f);
      x += w + 4;
    }
  }
  {  // generated code
    float y = H - codeH;
    fillRect(rx, y, rw, codeH, t.bg);
    fillRect(rx, y, rw, 1, t.border);
    fillRect(rx, y + 1, rw, 32, t.panel);
    drawText(L"Generated code for AMX Mod X", 32, rx + 14, y + 9, 0xff8a1f, uiFont(10.5f, true, true), 300, 18);
    float bx = rx + rw - 12;
    struct B { const wchar_t* l; int k; } bs[4] = {{L"INSERT IN EDITOR", HK_INSERT}, {L"COPY", HK_COPY}, {L"SHARE LINK", HK_SHARE}, {L"PASTE LINK", HK_PASTE}};
    for (auto& b : bs) {
      float w = textWidth(b.l, uiFont(10.5f, false, true)) + 20;
      bx -= w;
      hudButton(rc(bx, y + 4, w, 26), b.l, false, b.k, 0, false, 0x00d27a);
      bx -= 6;
    }
    wstring code = hudCode();
    IDWriteTextFormat* mf = uiFont(12.5f, false, true);
    float cwid = textWidth(L"MMMMMMMMMM", mf) / 10.f, ly = y + 42;
    g_rt->PushAxisAlignedClip(rc(rx, y + 34, rw, codeH - 34), D2D1_ANTIALIAS_MODE_ALIASED);
    size_t p = 0;
    while (p <= code.size() && ly < H) {
      size_t q = code.find(L'\n', p);
      wstring ln = code.substr(p, q == wstring::npos ? wstring::npos : q - p);
      vector<Span> sp;
      int st = 0;
      tokenizePawn(ln, st, sp);
      int at = 0;
      auto draw = [&](int a, int b, int kind) { if (b > a) drawText(ln.c_str() + a, b - a, rx + 18 + a * cwid, ly, kind ? synCol(kind) : t.fg, mf, (b - a) * cwid + 8, 20); };
      for (auto& s : sp) { draw(at, s.a, 0); draw(s.a, s.b, s.kind); at = s.b; }
      draw(at, (int)ln.size(), 0);
      ly += 21;
      if (q == wstring::npos) break;
      p = q + 1;
    }
    g_rt->PopAxisAlignedClip();
  }
  if (!g_hMsg.empty() && GetTickCount64() - g_hMsgAt < 2500) {
    float w = textWidth(g_hMsg, uiFont(12.f)) + 28;
    D2D1_RECT_F r = rc(rx + (rw - w) / 2, bodyTop + tbH + 14, w, 30);
    fillR(r, t.menu, 0.95f, 6);
    strokeR(r, t.accent, 0.8f, 6);
    drawText(g_hMsg.c_str(), (int)g_hMsg.size(), r.left + 14, r.top + 6, t.fg, uiFont(12.f), w, 20);
  }
}

/* ---------------------------------------------------------------- input */
static void hudClose() {
  g_hudOpen = false;
  g_hFocus = -1;
  g_hudDrag = false;
  hudStop();
  redraw();
}
static void hudOpen() {
  if (g_suOpen) suClose();
  g_hudOpen = true;
  g_hFocus = -1;
  g_hScroll = 0;
  g_hArea.set(g_hm.text);
  hudLoadFields();
  caretReset();
  redraw();
}
static void hudInsertInEditor() {
  Doc& d = D();
  wstring code = hudCode();
  // the lines after the first follow the indentation of the line the caret is in
  wstring ind;
  const wstring& cl = d.lines[d.cur.l];
  for (size_t i = 0; i < cl.size() && (cl[i] == L' ' || cl[i] == L'\t'); i++) ind += cl[i];
  wstring text;
  for (wchar_t c : code) { text += c; if (c == L'\n') text += ind; }
  text += L"\n" + ind;
  hudClose();
  d.snapshot(0);
  Pos a = d.cur < d.anc ? d.cur : d.anc, b = d.cur < d.anc ? d.anc : d.cur;
  d.replaceRange(a, b, text);
  int nl = (int)std::count(text.begin(), text.end(), L'\n');
  d.cur = d.anc = {a.l + nl, (int)ind.size()};
  edited();
  flash(L"HUD message inserted");
}
static void hudStepField(int f, int dir) {
  const HudFld& fl = HUD_FLD[f];
  double v = wcstod(g_hf[f].t.c_str(), nullptr) + dir * fl.step;
  v = std::max(fl.mn, std::min(fl.mx, v));
  g_hf[f].set(hudNum(std::round(v * 100) / 100, fl.dec));
  hudCommit(f);
}
static void hudDragTo(float mx, float my) {
  HudMsg& m = g_hm;
  float W = g_hCanvas.right - g_hCanvas.left, H = g_hCanvas.bottom - g_hCanvas.top;
  float FS = 17.f * (H / (m.dhud ? 500.f : HUD_REF_H[g_hudRes]));
  float px = std::max(0.f, std::min(W, mx - g_hCanvas.left - g_hDragX)), py = std::max(0.f, std::min(H - FS, my - g_hCanvas.top - g_hDragY));
  m.x = std::round(px / W * 1000) / 1000;
  m.y = std::round(py / H * 1000) / 1000;
  float bw = g_hTextBox.right - g_hTextBox.left, bh = g_hTextBox.bottom - g_hTextBox.top;
  m.cx = std::fabs(px + bw / 2 - W / 2) < 6;  // near the middle: it snaps and is saved as -1.0 (centred), like the buttons
  m.cy = std::fabs(py + bh / 2 - H / 2) < 6;
  g_hFocus = -1;
  hudLoadFields();
  redraw();
}
static bool hudPickColor(int& r, int& g, int& b) {
  static COLORREF custom[16];
  CHOOSECOLORW cc{};
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = g_hwnd;
  cc.rgbResult = RGB(r, g, b);
  cc.lpCustColors = custom;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;
  if (!ChooseColorW(&cc)) return false;
  r = GetRValue(cc.rgbResult); g = GetGValue(cc.rgbResult); b = GetBValue(cc.rgbResult);
  return true;
}
static void hudMouse(float x, float y, int what) {  // 0 down, 1 up
  if (what == 1) { if (g_hudDrag) { g_hudDrag = false; ReleaseCapture(); } return; }
  const HudHit* hit = nullptr;
  for (int i = (int)g_hHits.size() - 1; i >= 0; i--) {
    const HudHit& h = g_hHits[i];
    if (!inRect(h.r, x, y)) continue;
    if (h.side && (y < g_hSideViewTop || x >= 300)) continue;
    hit = &h;
    break;
  }
  HudMsg& m = g_hm;
  if (!hit) { g_hFocus = -1; redraw(); return; }
  int idx = hit->idx;
  if (hit->kind != HK_FIELD && hit->kind != HK_AREA) g_hFocus = -1;
  switch (hit->kind) {
    case HK_CLOSE: hudClose(); return;
    case HK_TYPE: m.dhud = idx == 1; hudStop(); break;
    case HK_AREA: {
      g_hFocus = -2;
      float ly = (y - (hit->r.top + 6)) / 17;
      int vis = 4, first = std::max(0, std::min(g_hArea.l - vis + 1, (int)g_hArea.lines.size() - vis));
      first = std::max(0, first);
      g_hArea.l = std::max(0, std::min((int)g_hArea.lines.size() - 1, first + (int)std::floor(std::max(0.f, ly))));
      const wstring& s = g_hArea.lines[g_hArea.l];
      int best = (int)s.size();
      for (int i = 0; i <= (int)s.size(); i++) if (textWidth(s.substr(0, i), uiFont(12.f, false, true)) >= x - hit->r.left - 9) { best = i; break; }
      g_hArea.c = best;
      caretReset();
      break;
    }
    case HK_FIELD: g_hFocus = idx; lineEditClick(g_hf[idx], hit->r, x); caretReset(); break;
    case HK_SWATCH: {
      int r = idx ? m.r2 : m.r, g = idx ? m.g2 : m.g, b = idx ? m.b2 : m.b;
      if (hudPickColor(r, g, b)) { if (idx) { m.r2 = r; m.g2 = g; m.b2 = b; } else { m.r = r; m.g = g; m.b = b; } hudStop(); hudLoadFields(); }
      break;
    }
    case HK_PRESET: {
      static const int pc[8][3] = {{255, 255, 255}, {255, 70, 70}, {255, 150, 0}, {255, 230, 0}, {60, 255, 100}, {0, 220, 255}, {80, 120, 255}, {255, 100, 200}};
      m.r = pc[idx][0]; m.g = pc[idx][1]; m.b = pc[idx][2];
      hudStop();
      hudLoadFields();
      break;
    }
    case HK_STEP: g_hFocus = -1; hudStepField(idx / 2, idx % 2 ? -1 : 1); hudLoadFields(); break;
    case HK_RESET: g_hm = HudMsg(); g_hArea.set(g_hm.text); hudStop(); g_hFocus = -1; hudLoadFields(); hudSay(L"Back to the defaults"); break;
    case HK_CENTERX: m.cx = !m.cx; hudStop(); hudLoadFields(); break;
    case HK_CENTERY: m.cy = !m.cy; hudStop(); hudLoadFields(); break;
    case HK_EFF:
      m.effect = idx;
      if (idx == 2 && m.fadeIn > 0.3) m.fadeIn = 0.1;  // typewriter: fade in is the time per character
      hudStop();
      hudLoadFields();
      break;
    case HK_CH: m.channel = idx; break;
    case HK_PLAY: hudPlay(); break;
    case HK_STOP: hudStop(); break;
    case HK_MAP: g_hudMap = idx; break;
    case HK_RES: g_hudRes = idx; break;
    case HK_COPY: clipSet(hudCode()); hudSay(L"Code copied"); break;
    case HK_SHARE: clipSet(fromUtf8(hudShareUrl())); hudSay(L"Link copied"); break;
    case HK_PASTE: if (hudLoadHash(toUtf8(clipGet()))) hudSay(L"Message loaded from the link"); else hudSay(L"The clipboard has no HUD link"); break;
    case HK_INSERT: hudInsertInEditor(); return;
    case HK_CANVAS:
      if (!g_hudPlaying) { g_hudDrag = true; g_hDragX = x - hit->r.left; g_hDragY = y - hit->r.top; SetCapture(g_hwnd); }
      break;
  }
  redraw();
}
static void hudMove(float x, float y) {
  if (g_hudDrag) hudDragTo(x, y);
  else redraw();
}
static void hudWheel(int delta, float x, float y) {
  if (g_hFocus >= 0 || (x < 300)) {
    for (auto& h : g_hHits) if (h.kind == HK_FIELD && inRect(h.r, x, y)) { hudStepField(h.idx, delta > 0 ? 1 : -1); hudLoadFields(); redraw(); return; }
  }
  if (x < 300) {
    g_hScroll = std::max(0.f, std::min(std::max(0.f, g_hContent - g_hSideViewH), g_hScroll - delta / 120.f * 60.f));
    redraw();
  }
}
static bool hudKey(WPARAM wp, bool ctrl, bool shift, bool alt) {
  (void)alt;
  if (wp == VK_ESCAPE) { if (g_hFocus != -1) g_hFocus = -1; else hudClose(); redraw(); return true; }
  if (wp == VK_TAB) {
    int n = g_hFocus == -2 ? HF_R : g_hFocus + (shift ? -1 : 1);
    if (g_hFocus == -1) n = HF_R;
    if (n < 0) n = HF_COUNT - 1;
    if (n >= HF_COUNT || (g_hm.dhud && n >= HF_R2 && n <= HF_B2)) n = n >= HF_COUNT ? -2 : HF_X;
    g_hFocus = n;
    if (n >= 0) g_hf[n].a = 0, g_hf[n].c = (int)g_hf[n].t.size();
    caretReset();
    redraw();
    return true;
  }
  if (wp == VK_RETURN && ctrl) { if (g_hudPlaying) hudStop(); else hudPlay(); redraw(); return true; }
  if (g_hFocus == -1 && wp == VK_SPACE) { if (g_hudPlaying) hudStop(); else hudPlay(); redraw(); return true; }
  if (g_hFocus == -2) {
    bool changed = g_hArea.key(wp, ctrl);
    if (ctrl && wp == 'C') {}
    if (changed) { g_hm.text = g_hArea.text(); hudStop(); }
    caretReset();
    redraw();
    return true;
  }
  if (g_hFocus >= 0) {
    if (wp == VK_UP || wp == VK_DOWN) { hudStepField(g_hFocus, wp == VK_UP ? 1 : -1); g_hf[g_hFocus].c = g_hf[g_hFocus].a = (int)g_hf[g_hFocus].t.size(); redraw(); return true; }
    if (wp == VK_RETURN) { g_hFocus = -1; hudLoadFields(); redraw(); return true; }
    if (editKey(g_hf[g_hFocus], wp, ctrl, shift)) hudCommit(g_hFocus);
    caretReset();
    redraw();
    return true;
  }
  return true;
}
static void hudChar(wchar_t c) {
  if (g_hFocus == -2) {
    g_hArea.insert(wstring(1, c));
    g_hm.text = g_hArea.text();
    hudStop();
  } else if (g_hFocus >= 0) {
    bool intOnly = HUD_FLD[g_hFocus].dec == 0;
    if (!(iswdigit(c) || (!intOnly && (c == L'.' || c == L'-')))) return;
    g_hf[g_hFocus].insert(wstring(1, c));
    hudCommit(g_hFocus);
  } else return;
  caretReset();
  redraw();
}
// the mouse pointer over the page: text cursor on the fields, hand on the buttons, move over the message
static LPCWSTR hudCursor(float x, float y) {
  if (g_hudDrag) return IDC_SIZEALL;
  for (int i = (int)g_hHits.size() - 1; i >= 0; i--) {
    const HudHit& h = g_hHits[i];
    if (!inRect(h.r, x, y) || (h.side && (y < g_hSideViewTop || x >= 300))) continue;
    if (h.kind == HK_FIELD || h.kind == HK_AREA) return IDC_IBEAM;
    if (h.kind == HK_CANVAS) return g_hudPlaying ? IDC_ARROW : IDC_SIZEALL;
    return IDC_HAND;
  }
  return IDC_ARROW;
}
