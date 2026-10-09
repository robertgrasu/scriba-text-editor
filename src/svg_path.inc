// claude_svg.inc: a small SVG path reader (M L H V C S Q T A Z, absolute and relative) that makes Direct2D geometry, and the icons of the chat
// dock, which are the same drawings as in Electron's claude.js.

static bool svgSkip(const char*& p, const char* e) {
  while (p < e && (isspace((unsigned char)*p) || *p == ',')) p++;
  return p < e;
}
static bool svgNum(const char*& p, const char* e, double& v) {
  if (!svgSkip(p, e)) return false;
  char* end = nullptr;
  v = strtod(p, &end);
  if (end == p) return false;
  p = end;
  return true;
}
static bool svgFlag(const char*& p, const char* e, bool& v) {
  if (!svgSkip(p, e)) return false;
  if (*p != '0' && *p != '1') return false;
  v = *p == '1';
  p++;
  return true;
}
// an arc as cubic Beziers (the endpoint form of SVG converted to a centre form)
static void svgArc(ID2D1GeometrySink* sink, double x0, double y0, double rx, double ry, double phiDeg, bool large, bool sweep, double x, double y) {
  if (rx == 0 || ry == 0 || (x0 == x && y0 == y)) { sink->AddLine(D2D1::Point2F((float)x, (float)y)); return; }
  rx = fabs(rx);
  ry = fabs(ry);
  double phi = phiDeg * 3.14159265358979 / 180.0, cp = cos(phi), sp = sin(phi);
  double dx = (x0 - x) / 2, dy = (y0 - y) / 2;
  double x1 = cp * dx + sp * dy, y1 = -sp * dx + cp * dy;
  double lam = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
  if (lam > 1) { double s = sqrt(lam); rx *= s; ry *= s; }
  double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
  double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
  double co = den == 0 ? 0 : sqrt(std::max(0.0, num / den));
  if (large == sweep) co = -co;
  double cxp = co * rx * y1 / ry, cyp = -co * ry * x1 / rx;
  double cx = cp * cxp - sp * cyp + (x0 + x) / 2, cy = sp * cxp + cp * cyp + (y0 + y) / 2;
  auto ang = [](double ux, double uy, double vx, double vy) {
    double a = atan2(ux * vy - uy * vx, ux * vx + uy * vy);
    return a;
  };
  double th1 = ang(1, 0, (x1 - cxp) / rx, (y1 - cyp) / ry);
  double dth = ang((x1 - cxp) / rx, (y1 - cyp) / ry, (-x1 - cxp) / rx, (-y1 - cyp) / ry);
  if (!sweep && dth > 0) dth -= 2 * 3.14159265358979;
  else if (sweep && dth < 0) dth += 2 * 3.14159265358979;
  int segs = (int)ceil(fabs(dth) / (3.14159265358979 / 2) - 1e-9);
  if (segs < 1) segs = 1;
  double d = dth / segs, t = 4.0 / 3.0 * tan(d / 4);
  double a = th1;
  for (int i = 0; i < segs; i++) {
    double a2 = a + d;
    double c1 = cos(a), s1 = sin(a), c2 = cos(a2), s2 = sin(a2);
    double p1x = c1 - t * s1, p1y = s1 + t * c1, p2x = c2 + t * s2, p2y = s2 - t * c2, p3x = c2, p3y = s2;
    auto tr = [&](double px, double py) {
      double X = rx * px, Y = ry * py;
      return D2D1::Point2F((float)(cp * X - sp * Y + cx), (float)(sp * X + cp * Y + cy));
    };
    sink->AddBezier(D2D1::BezierSegment(tr(p1x, p1y), tr(p2x, p2y), tr(p3x, p3y)));
    a = a2;
  }
}
static ID2D1PathGeometry* svgGeometry(const std::string& dstr) {
  static std::map<std::string, ID2D1PathGeometry*> cache;
  auto it = cache.find(dstr);
  if (it != cache.end()) return it->second;
  ID2D1PathGeometry* geo = nullptr;
  if (FAILED(g_d2d->CreatePathGeometry(&geo)) || !geo) { cache[dstr] = nullptr; return nullptr; }
  ID2D1GeometrySink* sink = nullptr;
  geo->Open(&sink);
  sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  const char* p = dstr.c_str();
  const char* e = p + dstr.size();
  double cx = 0, cy = 0, sx = 0, sy = 0, lcx = 0, lcy = 0;  // current point, subpath start, last control point
  char cmd = 0, prev = 0;
  bool open = false;
  auto begin = [&](double x, double y) {
    if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->BeginFigure(D2D1::Point2F((float)x, (float)y), D2D1_FIGURE_BEGIN_FILLED);
    open = true;
    sx = x;
    sy = y;
  };
  auto P = [](double x, double y) { return D2D1::Point2F((float)x, (float)y); };
  while (svgSkip(p, e)) {
    if (isalpha((unsigned char)*p)) { cmd = *p++; }
    else if (!cmd) break;
    bool rel = islower((unsigned char)cmd);
    char C = (char)toupper((unsigned char)cmd);
    double v[7];
    if (C == 'Z') {
      if (open) { sink->EndFigure(D2D1_FIGURE_END_CLOSED); open = false; }
      cx = sx;
      cy = sy;
      prev = 'Z';
      cmd = 0;
      continue;
    }
    auto need = [&](int n) {
      for (int i = 0; i < n; i++) if (!svgNum(p, e, v[i])) return false;
      return true;
    };
    if (C == 'M') {
      if (!need(2)) break;
      if (rel) { v[0] += cx; v[1] += cy; }
      begin(v[0], v[1]);
      cx = v[0];
      cy = v[1];
      cmd = rel ? 'l' : 'L';  // further pairs are line-tos
      prev = 'M';
      continue;
    }
    if (!open) begin(cx, cy);
    if (C == 'L') { if (!need(2)) break; if (rel) { v[0] += cx; v[1] += cy; } sink->AddLine(P(v[0], v[1])); cx = v[0]; cy = v[1]; }
    else if (C == 'H') { if (!need(1)) break; double x = rel ? cx + v[0] : v[0]; sink->AddLine(P(x, cy)); cx = x; }
    else if (C == 'V') { if (!need(1)) break; double y = rel ? cy + v[0] : v[0]; sink->AddLine(P(cx, y)); cy = y; }
    else if (C == 'C') {
      if (!need(6)) break;
      if (rel) for (int i = 0; i < 6; i += 2) { v[i] += cx; v[i + 1] += cy; }
      sink->AddBezier(D2D1::BezierSegment(P(v[0], v[1]), P(v[2], v[3]), P(v[4], v[5])));
      lcx = v[2]; lcy = v[3]; cx = v[4]; cy = v[5];
    } else if (C == 'S') {
      if (!need(4)) break;
      if (rel) for (int i = 0; i < 4; i += 2) { v[i] += cx; v[i + 1] += cy; }
      double c1x = (prev == 'C' || prev == 'S') ? 2 * cx - lcx : cx, c1y = (prev == 'C' || prev == 'S') ? 2 * cy - lcy : cy;
      sink->AddBezier(D2D1::BezierSegment(P(c1x, c1y), P(v[0], v[1]), P(v[2], v[3])));
      lcx = v[0]; lcy = v[1]; cx = v[2]; cy = v[3];
    } else if (C == 'Q') {
      if (!need(4)) break;
      if (rel) for (int i = 0; i < 4; i += 2) { v[i] += cx; v[i + 1] += cy; }
      sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(P(v[0], v[1]), P(v[2], v[3])));
      lcx = v[0]; lcy = v[1]; cx = v[2]; cy = v[3];
    } else if (C == 'T') {
      if (!need(2)) break;
      if (rel) { v[0] += cx; v[1] += cy; }
      double qx = (prev == 'Q' || prev == 'T') ? 2 * cx - lcx : cx, qy = (prev == 'Q' || prev == 'T') ? 2 * cy - lcy : cy;
      sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(P(qx, qy), P(v[0], v[1])));
      lcx = qx; lcy = qy; cx = v[0]; cy = v[1];
    } else if (C == 'A') {
      double rx, ry, rot, x, y;
      bool fa, fs;
      if (!svgNum(p, e, rx) || !svgNum(p, e, ry) || !svgNum(p, e, rot) || !svgFlag(p, e, fa) || !svgFlag(p, e, fs) || !svgNum(p, e, x) || !svgNum(p, e, y)) break;
      if (rel) { x += cx; y += cy; }
      svgArc(sink, cx, cy, rx, ry, rot, fa, fs, x, y);
      cx = x; cy = y;
    } else break;
    prev = C;
  }
  if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
  sink->Close();
  sink->Release();
  cache[dstr] = geo;
  return geo;
}

