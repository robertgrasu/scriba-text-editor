// extras.inc: snippets (type a trigger, press Tab) and macros (record keys and typed text, play them back).
// Included by main.cpp after files.inc.

/* ---------------------------------------------------------------- snippets */
struct SnipDef { wstring desc, body; };
struct BuiltinSnip { const wchar_t* lang; const wchar_t* trig; const wchar_t* desc; const wchar_t* body; };
static const BuiltinSnip BUILTIN_SNIPS[] = {
    {L"all", L"todo", L"TODO comment", L"TODO: ${}"},
    {L"JavaScript", L"fn", L"function", L"function ${1:name}(${2:args}) {\n\t${}\n}"},
    {L"JavaScript", L"afn", L"arrow function", L"const ${1:name} = (${2:args}) => {\n\t${}\n};"},
    {L"JavaScript", L"for", L"for loop", L"for (let ${1:i} = 0; ${1:i} < ${2:arr}.length; ${1:i}++) {\n\t${}\n}"},
    {L"JavaScript", L"forof", L"for...of", L"for (const ${1:item} of ${2:items}) {\n\t${}\n}"},
    {L"JavaScript", L"if", L"if", L"if (${1:condition}) {\n\t${}\n}"},
    {L"JavaScript", L"ife", L"if / else", L"if (${1:condition}) {\n\t${}\n} else {\n\t\n}"},
    {L"JavaScript", L"log", L"console.log", L"console.log(${});"},
    {L"JavaScript", L"try", L"try / catch", L"try {\n\t${}\n} catch (${1:err}) {\n\t\n}"},
    {L"JavaScript", L"imp", L"import", L"import { ${} } from '${1:module}';"},
    {L"JavaScript", L"cls", L"class", L"class ${1:Name} {\n\tconstructor(${2:args}) {\n\t\t${}\n\t}\n}"},
    {L"JavaScript", L"sw", L"switch", L"switch (${1:value}) {\n\tcase ${2:x}:\n\t\t${}\n\t\tbreak;\n\tdefault:\n\t\tbreak;\n}"},
    {L"Python", L"def", L"function", L"def ${1:name}(${2:args}):\n\t${}"},
    {L"Python", L"cls", L"class", L"class ${1:Name}:\n\tdef __init__(self${2:, args}):\n\t\t${}"},
    {L"Python", L"ifm", L"if __name__ == main", L"if __name__ == '__main__':\n\t${}"},
    {L"Python", L"for", L"for loop", L"for ${1:item} in ${2:items}:\n\t${}"},
    {L"Python", L"try", L"try / except", L"try:\n\t${}\nexcept ${1:Exception} as ${2:e}:\n\t"},
    {L"Python", L"with", L"with", L"with ${1:open(path)} as ${2:f}:\n\t${}"},
    {L"Python", L"imp", L"import", L"import ${}"},
    {L"C++", L"main", L"main", L"int main(int argc, char** argv) {\n\t${}\n\treturn 0;\n}"},
    {L"C++", L"for", L"for loop", L"for (int ${1:i} = 0; ${1:i} < ${2:n}; ++${1:i}) {\n\t${}\n}"},
    {L"C++", L"cls", L"class", L"class ${1:Name} {\npublic:\n\t${1:Name}();\n\t~${1:Name}();\n\n${}\n};"},
    {L"C++", L"inc", L"#include", L"#include <${}>"},
    {L"C++", L"cout", L"std::cout", L"std::cout << ${} << std::endl;"},
    {L"Java", L"main", L"main", L"public static void main(String[] args) {\n\t${}\n}"},
    {L"Java", L"sout", L"System.out.println", L"System.out.println(${});"},
    {L"Java", L"for", L"for loop", L"for (int ${1:i} = 0; ${1:i} < ${2:n}; ${1:i}++) {\n\t${}\n}"},
    {L"Rust", L"fn", L"function", L"fn ${1:name}(${2:args}) {\n\t${}\n}"},
    {L"Rust", L"main", L"main", L"fn main() {\n\t${}\n}"},
    {L"Rust", L"impl", L"impl", L"impl ${1:Type} {\n\t${}\n}"},
    {L"Rust", L"println", L"println!", L"println!(\"${}\");"},
    {L"Go", L"func", L"function", L"func ${1:name}(${2:args}) {\n\t${}\n}"},
    {L"Go", L"iferr", L"if err != nil", L"if err != nil {\n\t${}\n}"},
    {L"Go", L"main", L"main", L"func main() {\n\t${}\n}"},
    {L"PHP", L"fn", L"function", L"function ${1:name}(${2:args}) {\n\t${}\n}"},
    {L"PHP", L"foreach", L"foreach", L"foreach ($${1:items} as $${2:item}) {\n\t${}\n}"},
    {L"HTML", L"html5", L"HTML5 page", L"<!doctype html>\n<html lang=\"${1:en}\">\n<head>\n\t<meta charset=\"utf-8\">\n\t<title>${2:Title}</title>\n</head>\n<body>\n\t${}\n</body>\n</html>"},
    {L"HTML", L"a", L"link", L"<a href=\"${1:#}\">${}</a>"},
    {L"HTML", L"div", L"div", L"<div class=\"${1:name}\">\n\t${}\n</div>"},
    {L"CSS", L"flex", L"flex container", L"display: flex;\nalign-items: ${1:center};\njustify-content: ${2:center};"},
    {L"Shell", L"sh", L"bash header", L"#!/usr/bin/env bash\nset -euo pipefail\n\n${}"},
    {L"Shell", L"if", L"if", L"if [ ${1:condition} ]; then\n\t${}\nfi"},
    {L"Markdown", L"link", L"link", L"[${1:text}](${2:url})"},
    {L"Markdown", L"img", L"image", L"![${1:alt}](${2:path})"},
    {L"Markdown", L"code", L"code block", L"```${1:lang}\n${}\n```"},
    {L"Markdown", L"task", L"to-do list", L"- [ ] ${}"},
    {L"Markdown", L"tbl", L"table", L"| ${1:A} | ${2:B} |\n| --- | --- |\n| ${} |  |"},
    {L"Pawn (AMX Mod X)", L"plugin", L"new AMX Mod X plugin", L"#include <amxmodx>\n\n#define PLUGIN  \"${1:Plugin name}\"\n#define VERSION \"${2:1.0}\"\n#define AUTHOR  \"${3:Author}\"\n\npublic plugin_init()\n{\n\tregister_plugin(PLUGIN, VERSION, AUTHOR);\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"pinit", L"plugin_init", L"public plugin_init()\n{\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"pprecache", L"plugin_precache", L"public plugin_precache()\n{\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"pcfg", L"plugin_cfg", L"public plugin_cfg()\n{\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"pend", L"plugin_end", L"public plugin_end()\n{\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"cput", L"client_putinserver", L"public client_putinserver(id)\n{\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"cdisc", L"client_disconnected", L"public client_disconnected(id)\n{\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"regcmd", L"register_clcmd", L"register_clcmd(\"${1:say /command}\", \"${2:cmd_handler}\");"},
    {L"Pawn (AMX Mod X)", L"regcvar", L"register_cvar", L"register_cvar(\"${1:name}\", \"${2:value}\");"},
    {L"Pawn (AMX Mod X)", L"regevent", L"register_event", L"register_event(\"${1:DeathMsg}\", \"${2:event_handler}\", \"${3:a}\");"},
    {L"Pawn (AMX Mod X)", L"forp", L"loop over players", L"for (new id = 1; id <= MaxClients; id++)\n{\n\tif (!is_user_connected(id))\n\t\tcontinue;\n\n\t${}\n}"},
    {L"Pawn (AMX Mod X)", L"alive", L"return if the player is dead", L"if (!is_user_alive(${1:id}))\n\treturn ${2:PLUGIN_HANDLED};"},
    {L"Pawn (AMX Mod X)", L"cmd", L"command handler", L"public ${1:cmd_handler}(id)\n{\n\t${}\n\treturn PLUGIN_HANDLED;\n}"},
    {L"Pawn (AMX Mod X)", L"cprint", L"client_print_color", L"client_print_color(${1:id}, print_team_default, \"${2:^\\4[Plugin] ^\\1Message}\");"},
};
static std::map<wstring, SnipDef> snippetsFor(wstring syntax) {
  std::map<wstring, SnipDef> out;
  if (syntax == L"TypeScript") syntax = L"JavaScript";
  auto addUser = [&](const char* key) {
    if (!g_userSnippets.contains(key) || !g_userSnippets[key].is_object()) return;
    for (auto it = g_userSnippets[key].begin(); it != g_userSnippets[key].end(); ++it) {
      wstring trig = fromUtf8(it.key());
      const json& v = it.value();
      if (v.is_null()) { out.erase(trig); continue; }
      SnipDef sd;
      if (v.is_string()) sd.body = fromUtf8(v.get<std::string>());
      else if (v.is_array()) { for (size_t i = 0; i < v.size(); i++) { if (i) sd.body += L"\n"; if (v[i].is_string()) sd.body += fromUtf8(v[i].get<std::string>()); } }
      else if (v.is_object()) {
        const json* b = v.contains("body") ? &v["body"] : v.contains("b") ? &v["b"] : nullptr;
        if (!b) continue;
        if (b->is_string()) sd.body = fromUtf8(b->get<std::string>());
        else if (b->is_array()) for (size_t i = 0; i < b->size(); i++) { if (i) sd.body += L"\n"; if ((*b)[i].is_string()) sd.body += fromUtf8((*b)[i].get<std::string>()); }
        if (v.contains("description") && v["description"].is_string()) sd.desc = fromUtf8(v["description"].get<std::string>());
      }
      out[trig] = sd;
    }
  };
  for (auto& b : BUILTIN_SNIPS) if (wcscmp(b.lang, L"all") == 0 || syntax == b.lang) out[b.trig] = {b.desc, b.body};
  addUser("all");
  addUser(toUtf8(syntax).c_str());
  if (syntax == L"Shell") addUser("Shell Script");
  return out;
}

struct SnipRange { Pos a, b; };
struct SnipSession {
  bool active = false;
  Doc* doc = nullptr;
  vector<vector<SnipRange>> groups;  // by tab order
  int cur = -1;
} g_snip;
static Pos mapPosThroughEdit(Pos p, Pos a, Pos b, Pos e) {
  if (!(a < p) && !(a == p)) return p;
  if (p < b) return a == p ? p : e;  // inside the replaced text
  if (p == b && !(a == b)) return e;
  if (p.l == b.l) return {e.l, e.c + (p.c - b.c)};
  return {p.l + (e.l - b.l), p.c};
}
static void snipEnd() {
  if (g_snip.doc) g_snip.doc->onEdit = nullptr;
  g_snip = SnipSession();
}
static void snipGoto(int gi) {
  Doc& d = *g_snip.doc;
  g_snip.cur = gi;
  auto& rs = g_snip.groups[gi];
  d.extra.clear();
  d.anc = rs[0].a;
  d.cur = rs[0].b;
  for (size_t i = 1; i < rs.size(); i++) d.extra.push_back({rs[i].b, rs[i].a});
  moved(true);
}
static bool snipNext(int dir) {
  if (!g_snip.active || g_snip.doc != &D()) return false;
  int n = (int)g_snip.groups.size(), ni = g_snip.cur + dir;
  if (ni < 0) return true;
  if (ni >= n) {
    // the last stop: leave the snippet with the cursor where it stands
    Doc& d = *g_snip.doc;
    d.extra.clear();
    d.anc = d.cur;
    snipEnd();
    redraw();
    return true;
  }
  snipGoto(ni);
  return true;
}
// inserts a snippet body in place of the text between a and bEnd and starts the session of its stops
static void snipInsertBody(Doc& d, Pos a, Pos bEnd, const wstring& body) {
  const wstring& s = d.lines[a.l];
  wstring indent;
  for (wchar_t c : s) { if (c == L' ' || c == L'\t') indent += c; else break; }
  wstring unit = indentUnit(d);
  wstring text;
  struct Stop { int idx; int off, len; };
  vector<Stop> stops;
  for (size_t i = 0; i < body.size(); i++) {
    wchar_t c = body[i];
    if (c == L'$' && i + 1 < body.size() && body[i + 1] == L'{') {
      size_t close = body.find(L'}', i + 2);
      if (close != wstring::npos) {
        wstring inner = body.substr(i + 2, close - i - 2);
        int idx = -1;
        wstring ph;
        size_t k = 0;
        while (k < inner.size() && iswdigit(inner[k])) k++;
        if (k > 0) idx = _wtoi(inner.substr(0, k).c_str());
        if (k < inner.size() && inner[k] == L':') ph = inner.substr(k + 1);
        stops.push_back({idx, (int)text.size(), (int)ph.size()});
        text += ph;
        i = close;
        continue;
      }
    }
    if (c == L'\n') { text += L"\n" + indent; continue; }
    if (c == L'\t') { text += unit; continue; }
    text += c;
  }
  // stop order: numbered ones by number, then the anonymous ones in document order
  int maxIdx = 0;
  for (auto& st : stops) maxIdx = std::max(maxIdx, st.idx);
  for (auto& st : stops) if (st.idx < 0) st.idx = ++maxIdx;
  d.replaceRange(a, bEnd, text);
  auto posAt = [&](int off) {
    Pos p = a;
    for (int i = 0; i < off; i++) { if (text[i] == L'\n') { p.l++; p.c = 0; } else p.c++; }
    return p;
  };
  std::map<int, vector<SnipRange>> byIdx;
  for (auto& st : stops) byIdx[st.idx].push_back({posAt(st.off), posAt(st.off + st.len)});
  snipEnd();
  d.extra.clear();
  if (byIdx.empty()) { d.anc = d.cur; return; }
  for (auto& kv : byIdx) g_snip.groups.push_back(kv.second);
  g_snip.active = true;
  g_snip.doc = &d;
  d.onEdit = [](Pos a2, Pos b2, Pos e2) {
    for (size_t gi = 0; gi < g_snip.groups.size(); gi++) {
      if ((int)gi == g_snip.cur) continue;
      for (auto& r : g_snip.groups[gi]) { r.a = mapPosThroughEdit(r.a, a2, b2, e2); r.b = mapPosThroughEdit(r.b, a2, b2, e2); }
    }
  };
  snipGoto(0);
  // a single anonymous stop at the end is just the cursor
  if (g_snip.groups.size() == 1 && g_snip.groups[0].size() == 1 && g_snip.groups[0][0].a == g_snip.groups[0][0].b) { d.anc = d.cur; snipEnd(); }
}
// expands the trigger before the cursor; false when there is none
static bool snipExpand() {
  Doc& d = D();
  if (d.hasSel() || !d.extra.empty()) return false;
  const wstring& s = d.lines[d.cur.l];
  int e = d.cur.c, b = e;
  while (b > 0 && (isIdCh(s[b - 1]) || s[b - 1] == L'-')) b--;
  if (b == e) return false;
  auto defs = snippetsFor(d.langName);
  auto it = defs.find(s.substr(b, e - b));
  if (it == defs.end()) return false;
  d.snapshot(0);
  snipInsertBody(d, {d.cur.l, b}, d.cur, it->second.body);
  edited();
  return true;
}
/* ---------------------------------------------------------------- macros */
struct MacroStep { wstring text; wstring paste; bool isKey = false; KeyChord k; int vk = 0; };
static vector<MacroStep> g_macro, g_macroRec;
static bool g_macroPlaying = false;
static void macroState(bool rec) {
  g_recording = rec;
  redraw();
}
static void macroStart() {
  g_macroRec.clear();
  macroState(true);
  flash(L"Recording macro: press the shortcut again to stop");
}
static void macroStop() {
  if (!g_recording) return;
  g_macro = g_macroRec;
  g_macroRec.clear();
  macroState(false);
  flash(g_macro.empty() ? L"Macro is empty" : L"Macro recorded (" + std::to_wstring(g_macro.size()) + L" step" + (g_macro.size() == 1 ? L"" : L"s") + L"). Play it with Ctrl+Shift+Q");
}
static void macroRecordKey(WPARAM wp, bool ctrl, bool shift, bool alt) {
  if (!g_recording || g_macroPlaying) return;
  MacroStep s;
  s.isKey = true;
  s.k = KeyChord{ctrl, alt, shift, (int)wp};
  s.vk = (int)wp;
  g_macroRec.push_back(s);
}
static void macroRecordText(wchar_t c) {
  if (!g_recording || g_macroPlaying) return;
  if (!g_macroRec.empty() && !g_macroRec.back().isKey && g_macroRec.back().paste.empty()) g_macroRec.back().text += c;
  else { MacroStep s; s.text = wstring(1, c); g_macroRec.push_back(s); }
}
static void macroRecordPaste(const wstring& t) {
  if (!g_recording || g_macroPlaying) return;
  MacroStep s;
  s.paste = t;
  g_macroRec.push_back(s);
}
static bool onKey(WPARAM wp, bool ctrl, bool shift, bool alt, bool sysKey);
static void onChar(wchar_t c);
static void macroPlay(const vector<MacroStep>& steps, int times = 1) {
  if (steps.empty()) { flash(L"No macro recorded yet (Ctrl+Q starts recording)"); return; }
  if (g_macroPlaying) return;
  g_macroPlaying = true;
  for (int t = 0; t < times; t++) {
    for (auto& s : steps) {
      if (!s.paste.empty()) { multi([&] { D().insertText(s.paste, 0); }); edited(); }
      else if (!s.text.empty()) { for (wchar_t c : s.text) onChar(c); }
      else if (s.isKey) onKey((WPARAM)s.k.vk, s.k.ctrl, s.k.shift, s.k.alt, s.k.alt);
    }
  }
  g_macroPlaying = false;
}
static wstring macrosPath() { return userDir() + L"\\macros.json"; }
static json macrosRead() {
  json j = parseJsonc(readFileText(macrosPath()), json::object());
  return j.is_object() ? j : json::object();
}
static json macroToJson(const vector<MacroStep>& steps) {
  json a = json::array();
  for (auto& s : steps) {
    json o;
    if (!s.paste.empty()) o["paste"] = toUtf8(s.paste);
    else if (!s.text.empty()) o["text"] = toUtf8(s.text);
    else {
      o["vk"] = s.k.vk;
      o["ctrl"] = s.k.ctrl;
      o["alt"] = s.k.alt;
      o["shift"] = s.k.shift;
    }
    a.push_back(o);
  }
  return a;
}
static vector<MacroStep> macroFromJson(const json& a) {
  vector<MacroStep> r;
  if (!a.is_array()) return r;
  for (auto& o : a) {
    MacroStep s;
    if (o.contains("paste") && o["paste"].is_string()) s.paste = fromUtf8(o["paste"].get<std::string>());
    else if (o.contains("text") && o["text"].is_string()) s.text = fromUtf8(o["text"].get<std::string>());
    else if (o.contains("vk") && o["vk"].is_number()) {
      s.isKey = true;
      s.k = KeyChord{o.value("ctrl", false), o.value("alt", false), o.value("shift", false), (int)o["vk"].get<double>()};
      s.vk = s.k.vk;
    } else continue;
    r.push_back(s);
  }
  return r;
}
static void macroSaveAs() {
  if (g_macro.empty()) { flash(L"Record a macro first (Ctrl+Q)"); return; }
  json all = macrosRead();
  openPick(L"Name of the macro", [all](const wstring& q) {
    vector<PItem> out;
    wstring typed = trimW(q);
    auto save = [](const wstring& name) {
      json a = macrosRead();
      a[toUtf8(name)] = macroToJson(g_macro);
      writeFileUtf8(macrosPath(), a.dump(1) + "\n");
      flash(L"Macro saved: " + name);
    };
    for (auto it = all.begin(); it != all.end(); ++it) {
      wstring nm = fromUtf8(it.key());
      if (!typed.empty() && fuzzy(nm, typed) < 0) continue;
      PItem pi;
      pi.label = nm;
      pi.right = L"overwrite";
      pi.run = [nm, save] { save(nm); };
      out.push_back(pi);
    }
    if (!typed.empty() && !all.contains(toUtf8(typed))) {
      PItem pi;
      pi.label = L"Save macro as “" + typed + L"”";
      pi.right = L"Enter";
      pi.run = [typed, save] { save(typed); };
      out.insert(out.begin(), pi);
    }
    return out;
  });
}
static void macroRunSaved() {
  json all = macrosRead();
  vector<PItem> items;
  for (auto it = all.begin(); it != all.end(); ++it) {
    PItem pi;
    pi.label = fromUtf8(it.key());
    vector<MacroStep> steps = macroFromJson(it.value());
    pi.right = std::to_wstring(steps.size()) + L" steps";
    pi.run = [steps] { g_macro = steps; macroPlay(steps); };
    items.push_back(pi);
  }
  if (items.empty()) { flash(L"No saved macros yet: record one and use Save Macro As"); return; }
  openPick(L"Run a saved macro", listProvider(items));
}
