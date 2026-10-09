// icons.inc: file and folder icons like Electron's (src/icons.js). The SVG text of every icon is in icons_data.inc (generated);
// this reads the little subset of SVG they use (path, circle, ellipse, rect, text) and draws it with Direct2D.

struct IcShape {
  int kind = 0;  // 0 path, 1 circle, 2 ellipse, 3 rect, 4 text
  ID2D1PathGeometry* geo = nullptr;
  float a = 0, b = 0, c = 0, d = 0, e = 0;  // circle: cx cy r; ellipse: cx cy rx ry; rect: x y w h + e = rx; text: x y + e = font size
  bool hasFill = false, hasStroke = false, bold = false;
  DWORD fillCol = 0, strokeCol = 0;
  float fillA = 1, strokeA = 1, sw = 1;
  int cap = 0, join = 0;
  vector<float> dash;
  wstring text;
};
struct IcParsed { vector<IcShape> shapes; };

static std::string icAttr(const std::string& tag, const char* name) {
  std::string key = std::string(" ") + name + "=\"";
  size_t p = tag.find(key);
  if (p == std::string::npos) return "";
  p += key.size();
  size_t q = tag.find('"', p);
  return q == std::string::npos ? "" : tag.substr(p, q - p);
}
static bool icColor(const std::string& s, DWORD& out) {
  if (s.size() == 7 && s[0] == '#') { out = (DWORD)strtoul(s.c_str() + 1, nullptr, 16); return true; }
  return false;
}
static IcParsed* icParse(const char* svg) {
  static std::map<const char*, IcParsed*> cache;
  auto it = cache.find(svg);
  if (it != cache.end()) return it->second;
  auto* res = new IcParsed();
  std::string s = svg;
  size_t p = 0;
  while ((p = s.find('<', p)) != std::string::npos) {
    size_t q = s.find('>', p);
    if (q == std::string::npos) break;
    std::string tag = s.substr(p, q - p);
    std::string name;
    for (size_t k = 1; k < tag.size() && (isalpha((unsigned char)tag[k])); k++) name += tag[k];
    IcShape sh;
    bool ok = true;
    if (name == "path") {
      sh.kind = 0;
      sh.geo = svgGeometry(icAttr(tag, "d"));
      ok = sh.geo != nullptr;
    } else if (name == "circle") {
      sh.kind = 1;
      sh.a = (float)atof(icAttr(tag, "cx").c_str());
      sh.b = (float)atof(icAttr(tag, "cy").c_str());
      sh.c = (float)atof(icAttr(tag, "r").c_str());
    } else if (name == "ellipse") {
      sh.kind = 2;
      sh.a = (float)atof(icAttr(tag, "cx").c_str());
      sh.b = (float)atof(icAttr(tag, "cy").c_str());
      sh.c = (float)atof(icAttr(tag, "rx").c_str());
      sh.d = (float)atof(icAttr(tag, "ry").c_str());
    } else if (name == "rect") {
      sh.kind = 3;
      sh.a = (float)atof(icAttr(tag, "x").c_str());
      sh.b = (float)atof(icAttr(tag, "y").c_str());
      sh.c = (float)atof(icAttr(tag, "width").c_str());
      sh.d = (float)atof(icAttr(tag, "height").c_str());
      sh.e = (float)atof(icAttr(tag, "rx").c_str());
    } else if (name == "text") {
      sh.kind = 4;
      sh.a = (float)atof(icAttr(tag, "x").c_str());
      sh.b = (float)atof(icAttr(tag, "y").c_str());
      sh.e = (float)atof(icAttr(tag, "font-size").c_str());
      sh.bold = icAttr(tag, "font-weight") == "700";
      size_t te = s.find("</text>", q);
      if (te != std::string::npos) {
        std::string tx = s.substr(q + 1, te - q - 1);
        for (auto& pr : {std::pair<const char*, const char*>{"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}}) {
          size_t k;
          while ((k = tx.find(pr.first)) != std::string::npos) tx.replace(k, strlen(pr.first), pr.second);
        }
        sh.text = fromUtf8(tx);
      }
    } else ok = false;
    if (ok) {
      std::string f = icAttr(tag, "fill");
      sh.hasFill = icColor(f, sh.fillCol);
      if (sh.kind != 4 && f.empty()) { sh.hasFill = true; sh.fillCol = 0; }  // SVG's default fill is black
      std::string fo = icAttr(tag, "fill-opacity");
      sh.fillA = fo.empty() ? 1.f : (float)atof(fo.c_str());
      sh.hasStroke = icColor(icAttr(tag, "stroke"), sh.strokeCol);
      sh.sw = icAttr(tag, "stroke-width").empty() ? 1.f : (float)atof(icAttr(tag, "stroke-width").c_str());
      sh.cap = icAttr(tag, "stroke-linecap") == "round" ? 1 : 0;
      sh.join = icAttr(tag, "stroke-linejoin") == "round" ? 1 : 0;
      std::string da = icAttr(tag, "stroke-dasharray");
      if (!da.empty()) {
        const char* c = da.c_str();
        char* e;
        for (;;) {
          float v = strtof(c, &e);
          if (e == c) break;
          sh.dash.push_back(v);
          c = e;
        }
      }
      res->shapes.push_back(sh);
    }
    p = q + 1;
  }
  cache[svg] = res;
  return res;
}
static ID2D1StrokeStyle* icStroke(const IcShape& sh) {
  static std::map<std::string, ID2D1StrokeStyle*> cache;
  std::string key = std::to_string(sh.cap) + std::to_string(sh.join);
  for (float d : sh.dash) key += "," + std::to_string(d);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  D2D1_STROKE_STYLE_PROPERTIES pr = D2D1::StrokeStyleProperties(sh.cap ? D2D1_CAP_STYLE_ROUND : D2D1_CAP_STYLE_FLAT, sh.cap ? D2D1_CAP_STYLE_ROUND : D2D1_CAP_STYLE_FLAT,
                                                                 sh.cap ? D2D1_CAP_STYLE_ROUND : D2D1_CAP_STYLE_FLAT, sh.join ? D2D1_LINE_JOIN_ROUND : D2D1_LINE_JOIN_MITER, 10.f,
                                                                 sh.dash.empty() ? D2D1_DASH_STYLE_SOLID : D2D1_DASH_STYLE_CUSTOM, 0.f);
  ID2D1StrokeStyle* st = nullptr;
  g_d2d->CreateStrokeStyle(pr, sh.dash.empty() ? nullptr : sh.dash.data(), (UINT32)sh.dash.size(), &st);
  cache[key] = st;
  return st;
}
// draws an icon (viewBox 0 0 16 16) at x, y, size
static void icDraw(const char* svg, float x, float y, float size, float alpha = 1.f) {
  IcParsed* ip = icParse(svg);
  float s = size / 16.f;
  D2D1_MATRIX_3X2_F old;
  g_rt->GetTransform(&old);
  g_rt->SetTransform(D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(x, y) * old);
  for (auto& sh : ip->shapes) {
    if (sh.kind == 4) continue;
    auto paint = [&](auto fillFn, auto strokeFn) {
      if (sh.hasFill) { g_br->SetColor(Rgb(sh.fillCol, sh.fillA * alpha)); fillFn(); }
      if (sh.hasStroke) { g_br->SetColor(Rgb(sh.strokeCol, sh.strokeA * alpha)); strokeFn(); }
    };
    ID2D1StrokeStyle* st = icStroke(sh);
    if (sh.kind == 0) paint([&] { g_rt->FillGeometry(sh.geo, g_br); }, [&] { g_rt->DrawGeometry(sh.geo, g_br, sh.sw, st); });
    else if (sh.kind == 1) {
      D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(sh.a, sh.b), sh.c, sh.c);
      paint([&] { g_rt->FillEllipse(el, g_br); }, [&] { g_rt->DrawEllipse(el, g_br, sh.sw, st); });
    } else if (sh.kind == 2) {
      D2D1_ELLIPSE el = D2D1::Ellipse(D2D1::Point2F(sh.a, sh.b), sh.c, sh.d);
      paint([&] { g_rt->FillEllipse(el, g_br); }, [&] { g_rt->DrawEllipse(el, g_br, sh.sw, st); });
    } else if (sh.kind == 3) {
      D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(sh.a, sh.b, sh.a + sh.c, sh.b + sh.d), sh.e, sh.e);
      paint([&] { g_rt->FillRoundedRectangle(rr, g_br); }, [&] { g_rt->DrawRoundedRectangle(rr, g_br, sh.sw, st); });
    }
  }
  g_rt->SetTransform(old);
  // the marks inside the pages are drawn without the scale transform, so the font is hinted for its real size
  for (auto& sh : ip->shapes) {
    if (sh.kind != 4 || sh.text.empty()) continue;
    float fs = sh.e * s;
    IDWriteTextFormat* f = nullptr;
    g_dw->CreateTextFormat(L"Segoe UI", nullptr, sh.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fs, L"en-us", &f);
    if (!f) continue;
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextLayout* lay = nullptr;
    g_dw->CreateTextLayout(sh.text.c_str(), (UINT32)sh.text.size(), f, 200, 100, &lay);
    if (lay) {
      DWRITE_TEXT_METRICS tm{};
      lay->GetMetrics(&tm);
      UINT32 n = 0;
      lay->GetLineMetrics(nullptr, 0, &n);
      DWRITE_LINE_METRICS lm{};
      if (n) { DWRITE_LINE_METRICS buf[4]; lay->GetLineMetrics(buf, std::min<UINT32>(n, 4), &n); lm = buf[0]; }
      g_br->SetColor(Rgb(sh.fillCol, alpha));
      g_rt->DrawTextLayout(D2D1::Point2F(x + sh.a * s - tm.width / 2, y + sh.b * s - lm.baseline), lay, g_br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
      lay->Release();
    }
    f->Release();
  }
}
#include "icons_data.inc"
static const char* icSvgFor(const wstring& name, bool dir, bool open) {
  std::string lower = toUtf8(name);
  for (auto& c : lower) c = (char)tolower((unsigned char)c);
  if (dir) {
    if (open) { for (auto& e : ICON_DIR_OPEN) if (lower == e.key) return e.svg; return ICON_DIR_DEFAULT_OPEN; }
    for (auto& e : ICON_DIR) if (lower == e.key) return e.svg;
    return ICON_DIR_DEFAULT;
  }
  for (auto& e : ICON_NAME) if (lower == e.key) return e.svg;
  size_t dot = lower.rfind('.');
  if (dot != std::string::npos && (dot > 0 || lower.size() > 1)) {
    std::string ext = lower.substr(dot + 1);
    for (auto& e : ICON_EXT) if (ext == e.key) return e.svg;
  }
  return ICON_FILE_DEFAULT;
}
static void drawNameIcon(const wstring& name, bool dir, bool open, float x, float y, float size = 18.f, float alpha = 1.f) { icDraw(icSvgFor(name, dir, open), x, y, size, alpha); }
