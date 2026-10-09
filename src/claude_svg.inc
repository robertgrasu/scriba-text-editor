// claude_svg.inc: the icons of the chat dock (same drawings as in Electron's claude.js) and the artwork of the Claude Code installation.
// The SVG path reader is in svg_path.inc.
struct SvgShape { int kind; std::string d; float a, b, c, d2, e; bool fill; float sw; };  // kind 0 path, 1 circle (a,b centre, c radius), 2 rect (a,b,c,d2 size, e radius)
struct SvgIconDef { float vb; vector<SvgShape> shapes; };
enum { CI_HISTORY = 1, CI_PLUS, CI_CHAT, CI_CLOSE, CI_SEND, CI_STOP, CI_BOLT, CI_CHECK, CI_SLASH, CI_FILE, CI_IMAGE, CI_SPARK, CI_USER, CI_UNDO, CI_STAR };
static const SvgIconDef& ccIconDef(int id) {
  static std::map<int, SvgIconDef> defs;
  if (defs.empty()) {
    auto P = [](const char* d, float sw) { return SvgShape{0, d, 0, 0, 0, 0, 0, false, sw}; };
    auto PF = [](const char* d) { return SvgShape{0, d, 0, 0, 0, 0, 0, true, 0}; };
    defs[CI_HISTORY] = {16, {SvgShape{1, "", 8, 8, 6, 0, 0, false, 1.3f}, P("M8 4.5V8l2.5 1.5", 1.3f)}};
    defs[CI_PLUS] = {16, {P("M8 3v10M3 8h10", 1.4f)}};
    defs[CI_CHAT] = {16, {P("M2.5 3.5h11v7h-6l-3 2.5v-2.5h-2z", 1.3f), P("M8 5.5v3M6.5 7h3", 1.3f)}};
    defs[CI_CLOSE] = {14, {P("M3 3l8 8M11 3l-8 8", 1.4f)}};
    defs[CI_SEND] = {16, {P("M8 13V3M3.5 7.5L8 3l4.5 4.5", 1.8f)}};
    defs[CI_STOP] = {14, {SvgShape{2, "", 3, 3, 8, 8, 1.5f, true, 0}}};
    defs[CI_BOLT] = {16, {P("M9 1.5L3.5 9H8l-1 5.5L12.5 7H8z", 1.3f)}};
    defs[CI_CHECK] = {14, {P("M2.5 7.5l3 3 6-6.5", 1.6f)}};
    defs[CI_SLASH] = {16, {P("M10.5 2.5l-5 11", 1.4f)}};
    defs[CI_FILE] = {16, {P("M3.5 1.5h6l3 3v10h-9z", 1.3f), P("M9.5 1.5v3h3", 1.3f)}};
    defs[CI_IMAGE] = {16, {SvgShape{2, "", 1.5f, 2.5f, 13, 11, 1.5f, false, 1.3f}, SvgShape{1, "", 5.5f, 6, 1.2f, 0, 0, false, 1.3f}, P("M2 12.5l4-4 3 3 2-2 3.5 3", 1.3f)}};
    defs[CI_SPARK] = {16, {PF("M8 0.8l1.3 4.4L13.7 6.5 9.3 7.8 8 12.2 6.7 7.8 2.3 6.5l4.4-1.3z")}};
    defs[CI_USER] = {16, {SvgShape{1, "", 8, 5.5f, 2.7f, 0, 0, false, 1.3f}, P("M2.8 14c.4-2.8 2.5-4.4 5.2-4.4s4.8 1.6 5.2 4.4", 1.3f)}};
    defs[CI_UNDO] = {16, {P("M3 6.5h7a3.5 3.5 0 010 7H6", 1.4f), P("M5.5 3.5L3 6.5l2.5 3", 1.4f)}};
    SvgIconDef star{48, {}};
    for (int k = 0; k < 4; k++) star.shapes.push_back(SvgShape{3, "", (float)(k * 45), 0, 0, 0, 0, true, 0});  // kind 3: the rounded bar rotated by a degrees
    defs[CI_STAR] = star;
  }
  static SvgIconDef none{16, {}};
  auto it = defs.find(id);
  return it == defs.end() ? none : it->second;
}
static ID2D1StrokeStyle* ccRoundStroke() {
  static ID2D1StrokeStyle* s = nullptr;
  if (!s) g_d2d->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), nullptr, 0, &s);
  return s;
}
// draws an icon of the dock scaled into r (the viewBox is square); rotate in degrees around the centre
static void ccDrawIcon(int id, const D2D1_RECT_F& r, DWORD col, float alpha = 1.f, float rotate = 0) {
  const SvgIconDef& def = ccIconDef(id);
  float size = std::min(r.right - r.left, r.bottom - r.top);
  float s = size / def.vb;
  D2D1_MATRIX_3X2_F old;
  g_rt->GetTransform(&old);
  float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
  D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Translation(-def.vb / 2, -def.vb / 2) * D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Rotation(rotate) * D2D1::Matrix3x2F::Translation(cx, cy);
  g_rt->SetTransform(m * old);
  g_br->SetColor(Rgb(col, alpha));
  for (auto& sh : def.shapes) {
    if (sh.kind == 0) {
      ID2D1PathGeometry* g = svgGeometry(sh.d);
      if (!g) continue;
      if (sh.fill) g_rt->FillGeometry(g, g_br);
      else g_rt->DrawGeometry(g, g_br, sh.sw, ccRoundStroke());
    } else if (sh.kind == 1) {
      D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(sh.a, sh.b), sh.c, sh.c);
      if (sh.fill) g_rt->FillEllipse(el, g_br);
      else g_rt->DrawEllipse(el, g_br, sh.sw);
    } else if (sh.kind == 2) {
      D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(sh.a, sh.b, sh.a + sh.c, sh.b + sh.d2), sh.e, sh.e);
      if (sh.fill) g_rt->FillRoundedRectangle(rr, g_br);
      else g_rt->DrawRoundedRectangle(rr, g_br, sh.sw);
    } else if (sh.kind == 3) {
      D2D1_MATRIX_3X2_F cur;
      g_rt->GetTransform(&cur);
      // a bar through the centre of the viewBox, turned around that centre
      g_rt->SetTransform(D2D1::Matrix3x2F::Rotation(sh.a, D2D1::Point2F(24, 24)) * cur);
      g_rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(21, 3, 27, 45), 3, 3), g_br);
      g_rt->SetTransform(cur);
    }
  }
  g_rt->SetTransform(old);
}

/* ---------------------------------------------------------------- the artwork of the user's own Claude Code installation */
struct CcArt { ID2D1PathGeometry* geo = nullptr; float vw = 0, vh = 0; };
static CcArt g_ccClawd, g_ccLogo;
static bool g_ccArtLoaded = false;
static bool ccLoadSvgArt(const wstring& path, CcArt& out) {
  vector<char> b;
  if (path.empty() || !readAll(path, b)) return false;
  std::string s(b.begin(), b.end());
  size_t vb = s.find("viewBox=\"");
  if (vb == std::string::npos) return false;
  float x, y, w, h;
  if (sscanf_s(s.c_str() + vb + 9, "%f %f %f %f", &x, &y, &w, &h) != 4) return false;
  size_t d = s.find(" d=\"");
  if (d == std::string::npos) return false;
  size_t e = s.find('"', d + 4);
  if (e == std::string::npos) return false;
  out.geo = svgGeometry(s.substr(d + 4, e - d - 4));
  out.vw = w;
  out.vh = h;
  return out.geo != nullptr;
}
static void ccLoadArt() {
  if (g_ccArtLoaded) return;
  g_ccArtLoaded = true;
  ccLoadSvgArt(clVsCodeFile(L"resources\\clawd.svg"), g_ccClawd);
  ccLoadSvgArt(clVsCodeFile(L"resources\\claude-logo.svg"), g_ccLogo);
}
static void ccDrawArt(const CcArt& a, float x, float y, float w, DWORD col) {
  if (!a.geo) return;
  float s = w / a.vw;
  D2D1_MATRIX_3X2_F old;
  g_rt->GetTransform(&old);
  g_rt->SetTransform(D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(x, y) * old);
  g_br->SetColor(Rgb(col));
  g_rt->FillGeometry(a.geo, g_br);
  g_rt->SetTransform(old);
}
