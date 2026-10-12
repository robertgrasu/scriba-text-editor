// settings.inc: settings.json (same keys and format as the Electron edition), keymap.json and snippets.json, with live reload.
// Included by main.cpp.
#include "../third_party/json.hpp"
using json = nlohmann::json;

static std::string toUtf8(const wstring& w) {
  if (w.empty()) return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}
static wstring fromUtf8(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  wstring w(n, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
  return w;
}

// JSON with comments and trailing commas (the Sublime style): comments are removed, then "," before } or ]
static std::string stripJsonComments(const std::string& src) {
  std::string out;
  size_t i = 0;
  bool inStr = false;
  while (i < src.size()) {
    char c = src[i], n = i + 1 < src.size() ? src[i + 1] : 0;
    if (inStr) {
      out += c;
      if (c == '\\') { if (i + 1 < src.size()) out += n; i += 2; continue; }
      if (c == '"') inStr = false;
      i++;
    } else if (c == '"') { inStr = true; out += c; i++; }
    else if (c == '/' && n == '/') { while (i < src.size() && src[i] != '\n') i++; }
    else if (c == '/' && n == '*') { i += 2; while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) i++; i += 2; }
    else { out += c; i++; }
  }
  // trailing commas
  std::string r;
  inStr = false;
  for (size_t k = 0; k < out.size(); k++) {
    char c = out[k];
    if (inStr) { r += c; if (c == '\\' && k + 1 < out.size()) r += out[++k]; else if (c == '"') inStr = false; continue; }
    if (c == '"') { inStr = true; r += c; continue; }
    if (c == ',') {
      size_t q = k + 1;
      while (q < out.size() && isspace((unsigned char)out[q])) q++;
      if (q < out.size() && (out[q] == '}' || out[q] == ']')) continue;
    }
    r += c;
  }
  return r;
}
static json parseJsonc(const std::string& src, const json& fallback) {
  std::string t = stripJsonComments(src);
  size_t a = t.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return fallback;
  json j = json::parse(t, nullptr, false);
  return j.is_discarded() ? fallback : j;
}

static json settingsDefaults() {
  return json{
      {"color_scheme", "Mariana"}, {"ui_style", "Classic"}, {"accent_color", ""}, {"font_face", "Consolas"}, {"font_size", 15},
      {"tab_size", 4}, {"translate_tabs_to_spaces", false}, {"detect_indentation", true}, {"word_wrap", true}, {"line_numbers", true},
      {"gutter", true}, {"show_minimap", true}, {"show_sidebar", true}, {"sidebar_tabs", true}, {"show_sidebar_button", true}, {"show_syntax", true}, {"show_indentation", true},
      {"show_line_endings", true}, {"show_encoding", true}, {"show_line_column", "enabled"}, {"highlight_line", false}, {"draw_white_space", "none"},
      {"rulers", json::array()}, {"scroll_past_end", true}, {"auto_complete", false}, {"indent_guides", true}, {"bracket_colors", true},
      {"spell_check", true}, {"git_integration", true}, {"lsp_enabled", true}, {"lsp_servers", json::object()}, {"auto_save", "off"},
      {"auto_save_delay", 2}, {"claude_path", ""}, {"claude_permission_mode", "acceptEdits"}, {"claude_model", ""},
      {"claude_allowed_tools", json::array()}, {"amxx_compiler_path", ""}, {"amxx_include_dir", ""}, {"amxx_output_dir", ""},
      {"amxx_debug_level", 2}, {"amxx_profiles", json::object()}, {"amxx_profile", ""}, {"amxx_check_invalid", true}, {"amxx_build_version", true}, {"license_server", "http://127.0.0.1:5057"}, {"license_buy_url", ""}, {"amxx_popup_style", "Default"}, {"amxx_console_style", "Default"},
      {"notes_visual", true}, {"visual_line_numbers", false}, {"visual_show_syntax", false}, {"visual_max_width", 0},
      {"auto_close_brackets", true}, {"match_brackets", true}, {"trim_trailing_white_space_on_save", false},
      {"ensure_newline_at_eof_on_save", false},
      {"folder_exclude_patterns", json::array({".svn", ".git", ".hg", "CVS"})},
      {"file_exclude_patterns", json::array({".DS_Store", "*.pyc", "*.pyo", "*.exe", "*.dll", "*.obj", "*.o", "*.a", "*.lib", "*.so", "*.dylib", "*.ncb", "*.sdf", "*.suo", "*.pdb", "*.idb"})},
      {"binary_file_patterns", json::array({"*.jpg", "*.jpeg", "*.png", "*.gif", "*.ttf", "*.tga", "*.dds", "*.ico", "*.eot", "*.pdf", "*.swf", "*.jar", "*.zip", "*.7z", "*.gz", "*.rar", "*.woff", "*.woff2", "*.mp3", "*.mp4", "*.exe", "*.dll"})},
  };
}
static json g_settings = settingsDefaults();  // defaults merged with the user's file
static json g_userSettings = json::object();  // only what the user changed
static const json g_defaults = settingsDefaults();

static wstring userDir() {
  wstring r = dataDir() + L"\\User";
  CreateDirectoryW(r.c_str(), nullptr);
  return r;
}
static wstring settingsPath() { return userDir() + L"\\settings.json"; }
static wstring keymapPath() { return userDir() + L"\\keymap.json"; }
static wstring snippetsPath() { return userDir() + L"\\snippets.json"; }

static const char* SETTINGS_TEMPLATE =
    "// Scriba settings. Comments and trailing commas are allowed.\n{\n"
    "\t// \"color_scheme\": \"Mariana\",\n\t// \"font_face\": \"Consolas\",\n\t// \"font_size\": 15,\n\t// \"tab_size\": 4,\n"
    "\t// \"translate_tabs_to_spaces\": false,\n\t// \"detect_indentation\": true,\n\t// \"word_wrap\": true,\n\t// \"line_numbers\": true,\n"
    "\t// \"gutter\": true,\n\t// \"show_minimap\": true,\n\t// \"show_sidebar\": true,\n\t// \"highlight_line\": false,\n"
    "\t// \"draw_white_space\": \"none\",      // none | selection | all\n\t// \"rulers\": [80],\n\t// \"scroll_past_end\": true,\n"
    "\t// \"auto_close_brackets\": true,\n\t// \"match_brackets\": true,\n\t// \"trim_trailing_white_space_on_save\": false,\n"
    "\t// \"ensure_newline_at_eof_on_save\": false,\n\t// \"folder_exclude_patterns\": [\".svn\", \".git\", \".hg\", \"CVS\"],\n"
    "\t// \"file_exclude_patterns\": [\".DS_Store\", \"*.pyc\", \"*.exe\", \"*.dll\"],\n\t// \"binary_file_patterns\": [\"*.jpg\", \"*.png\", \"*.gif\", \"*.zip\", \"*.pdf\"],\n}\n";
static const char* KEYMAP_TEMPLATE =
    "// Scriba key bindings. Overrides the defaults, e.g.:\n// [\n//\t{ \"keys\": [\"ctrl+shift+x\"], \"command\": \"duplicate_line\" },\n"
    "//\t{ \"keys\": [\"ctrl+d\"], \"command\": \"next_occ\" },\n// ]\n[\n]\n";
static const char* SNIPPETS_TEMPLATE =
    "// Scriba snippets. Type the trigger and press Tab.\n"
    "// A body is a string or an array of lines. ${1:name} is a stop (Tab moves on), ${} a stop without text.\n"
    "// Set a trigger to null to remove a built-in one. Language names are the ones in View > Syntax.\n"
    "// {\n//   \"all\": { \"sig\": \"-- Robert\" },\n//   \"JavaScript\": { \"cl\": \"console.log(${});\", \"afn\": null },\n"
    "//   \"Pawn (AMX Mod X)\": { \"hudmsg\": [\"set_hudmessage(255, 255, 255, -1.0, 0.3);\", \"show_hudmessage(${1:0}, \\\"${}\\\");\"] }\n// }\n{\n}\n";

static void writeFileUtf8(const wstring& path, const std::string& text) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD wr;
  WriteFile(h, text.data(), (DWORD)text.size(), &wr, nullptr);
  CloseHandle(h);
}
static std::string readFileText(const wstring& path) {
  vector<char> b;
  if (!readAll(path, b)) return "";
  size_t off = (b.size() >= 3 && (unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF) ? 3 : 0;
  return std::string(b.begin() + off, b.end());
}
static void ensureUserFiles() {
  if (GetFileAttributesW(settingsPath().c_str()) == INVALID_FILE_ATTRIBUTES) writeFileUtf8(settingsPath(), SETTINGS_TEMPLATE);
  if (GetFileAttributesW(keymapPath().c_str()) == INVALID_FILE_ATTRIBUTES) writeFileUtf8(keymapPath(), KEYMAP_TEMPLATE);
  if (GetFileAttributesW(snippetsPath().c_str()) == INVALID_FILE_ATTRIBUTES) writeFileUtf8(snippetsPath(), SNIPPETS_TEMPLATE);
}

/* typed access */
static bool sBool(const char* k) { auto& v = g_settings[k]; return v.is_boolean() ? v.get<bool>() : (g_defaults.contains(k) ? g_defaults[k].get<bool>() : false); }
static int sInt(const char* k) { auto& v = g_settings[k]; return v.is_number() ? (int)v.get<double>() : (g_defaults.contains(k) && g_defaults[k].is_number() ? (int)g_defaults[k].get<double>() : 0); }
static wstring sStr(const char* k) { auto& v = g_settings[k]; return v.is_string() ? fromUtf8(v.get<std::string>()) : L""; }
static vector<wstring> sList(const char* k) {
  vector<wstring> r;
  auto& v = g_settings[k];
  if (v.is_array()) for (auto& e : v) if (e.is_string()) r.push_back(fromUtf8(e.get<std::string>()));
  return r;
}

static std::function<void(bool previewOnly)> g_applySettings;  // set by the main file: pushes the settings into the editor
static bool g_settingsReady = false;
static FILETIME g_setStamp{}, g_keyStamp{}, g_snipStamp{};
static DWORD g_settingsWrittenAt = 0;
static bool g_settingsDirty = false;
static void loadKeymapFile();
static void loadSnippetsFile();

static void mergeSettings() {
  g_settings = settingsDefaults();
  for (auto it = g_userSettings.begin(); it != g_userSettings.end(); ++it) g_settings[it.key()] = it.value();
}
static void loadSettingsFile() {
  ensureUserFiles();
  json u = parseJsonc(readFileText(settingsPath()), json::object());
  g_userSettings = u.is_object() ? u : json::object();
  mergeSettings();
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (GetFileAttributesExW(settingsPath().c_str(), GetFileExInfoStandard, &fa)) g_setStamp = fa.ftLastWriteTime;
  loadKeymapFile();
  loadSnippetsFile();
}
static void saveSettingsFileNow() {
  g_settingsDirty = false;
  std::string text = "// Scriba settings. Edit here or in Preferences > Settings. Comments are allowed.\n" + g_userSettings.dump(1, '\t') + "\n";
  writeFileUtf8(settingsPath(), text);
  g_settingsWrittenAt = GetTickCount();
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (GetFileAttributesExW(settingsPath().c_str(), GetFileExInfoStandard, &fa)) g_setStamp = fa.ftLastWriteTime;
}
static void setSetting(const char* key, const json& value) {
  if (g_defaults.contains(key) && g_defaults[key] == value) g_userSettings.erase(key);
  else g_userSettings[key] = value;
  g_settings[key] = value;
  g_settingsDirty = true;
  SetTimer(g_hwnd, 5, 300, nullptr);
  if (g_applySettings) g_applySettings(false);
}
// called by the timer: writes our own changes, and picks up edits made to the files by hand
static void settingsTick(bool timerId5) {
  if (timerId5) { KillTimer(g_hwnd, 5); if (g_settingsDirty) saveSettingsFileNow(); return; }
  if (g_settingsDirty || GetTickCount() - g_settingsWrittenAt < 1500) return;
  WIN32_FILE_ATTRIBUTE_DATA fa;
  bool changed = false;
  if (GetFileAttributesExW(settingsPath().c_str(), GetFileExInfoStandard, &fa) && CompareFileTime(&fa.ftLastWriteTime, &g_setStamp) != 0) changed = true;
  if (changed) {
    loadSettingsFile();
    if (g_applySettings) g_applySettings(false);
    redraw();
  }
  FILETIME k{}, s{};
  if (GetFileAttributesExW(keymapPath().c_str(), GetFileExInfoStandard, &fa)) k = fa.ftLastWriteTime;
  if (CompareFileTime(&k, &g_keyStamp) != 0) { loadKeymapFile(); redraw(); }
  if (GetFileAttributesExW(snippetsPath().c_str(), GetFileExInfoStandard, &fa)) s = fa.ftLastWriteTime;
  if (CompareFileTime(&s, &g_snipStamp) != 0) { loadSnippetsFile(); }
}

/* key bindings of the user */
static void loadKeymapFile() {
  g_userKeys.clear();
  json km = parseJsonc(readFileText(keymapPath()), json::array());
  if (km.is_array()) {
    for (auto& b : km) {
      if (!b.is_object() || !b.contains("keys") || !b.contains("command") || !b["keys"].is_array() || !b["command"].is_string()) continue;
      UserKey u;
      u.command = b["command"].get<std::string>();
      for (auto& k : b["keys"]) if (k.is_string()) u.keys.push_back(fromUtf8(k.get<std::string>()));
      g_userKeys.push_back(u);
    }
  }
  rebuildKeymap();
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (GetFileAttributesExW(keymapPath().c_str(), GetFileExInfoStandard, &fa)) g_keyStamp = fa.ftLastWriteTime;
}
static json g_userSnippets = json::object();
static void loadSnippetsFile() {
  json sn = parseJsonc(readFileText(snippetsPath()), json::object());
  g_userSnippets = sn.is_object() ? sn : json::object();
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (GetFileAttributesExW(snippetsPath().c_str(), GetFileExInfoStandard, &fa)) g_snipStamp = fa.ftLastWriteTime;
}
// the key bindings page and the keymap file: bind `cmd` to `keys` ("ctrl+shift+x"), or remove the user's binding
static void saveKeymapFile(const vector<UserKey>& list) {
  json arr = json::array();
  for (auto& u : list) {
    json o;
    o["keys"] = json::array();
    for (auto& k : u.keys) o["keys"].push_back(toUtf8(k));
    o["command"] = u.command;
    arr.push_back(o);
  }
  writeFileUtf8(keymapPath(), "// Scriba key bindings (edited by Preferences > Key Bindings). Overrides the defaults.\n" + arr.dump(1, '\t') + "\n");
  g_userKeys = list;
  rebuildKeymap();
  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (GetFileAttributesExW(keymapPath().c_str(), GetFileExInfoStandard, &fa)) g_keyStamp = fa.ftLastWriteTime;
}
