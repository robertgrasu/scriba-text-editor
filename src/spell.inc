// spell.inc: spell checking in notes with the Windows spell checking API (ISpellChecker). English and Romanian are used when the system has
// them; a word is wrong only when every available dictionary rejects it. Misspelled words get a red wavy line; right click offers suggestions.
#include <spellcheck.h>

static vector<ISpellChecker*> g_spCheckers;
static bool g_spInit = false;
static std::map<wstring, bool> g_spCache;  // word -> spelled right
static void spellInit() {
  if (g_spInit) return;
  g_spInit = true;
  ISpellCheckerFactory* fac = nullptr;
  if (FAILED(CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) || !fac) return;
  for (const wchar_t* lang : {L"en-US", L"ro-RO"}) {
    BOOL sup = FALSE;
    if (FAILED(fac->IsSupported(lang, &sup)) || !sup) continue;
    ISpellChecker* c = nullptr;
    if (SUCCEEDED(fac->CreateSpellChecker(lang, &c)) && c) g_spCheckers.push_back(c);
  }
  fac->Release();
}
static bool spellWordOk(const wstring& w) {
  spellInit();
  if (g_spCheckers.empty()) return true;
  auto it = g_spCache.find(w);
  if (it != g_spCache.end()) return it->second;
  bool ok = false;
  for (auto* c : g_spCheckers) {
    IEnumSpellingError* en = nullptr;
    if (FAILED(c->Check(w.c_str(), &en)) || !en) { ok = true; break; }
    ISpellingError* err = nullptr;
    bool bad = en->Next(&err) == S_OK;
    if (err) err->Release();
    en->Release();
    if (!bad) { ok = true; break; }
  }
  if (g_spCache.size() > 20000) g_spCache.clear();
  g_spCache[w] = ok;
  return ok;
}
static vector<wstring> spellSuggest(const wstring& w) {
  vector<wstring> out;
  spellInit();
  for (auto* c : g_spCheckers) {
    IEnumString* en = nullptr;
    if (FAILED(c->Suggest(w.c_str(), &en)) || !en) continue;
    LPOLESTR s = nullptr;
    ULONG got = 0;
    while (out.size() < 5 && en->Next(1, &s, &got) == S_OK && got) {
      out.push_back(s);
      CoTaskMemFree(s);
    }
    en->Release();
    if (out.size() >= 5) break;
  }
  return out;
}
static void spellAdd(const wstring& w) {
  spellInit();
  for (auto* c : g_spCheckers) { c->Add(w.c_str()); break; }
  g_spCache.clear();
}
static bool spellIsWordCh(wchar_t c) { return iswalpha(c); }
// the misspelled words of s[a, b): ranges
static void spellMisses(const wstring& s, int a, int b, vector<std::pair<int, int>>& out) {
  int n = (int)s.size();
  int i = std::max(0, a);
  // start at the beginning of a word that crosses a
  while (i > 0 && (spellIsWordCh(s[i - 1]) || (s[i - 1] == L'\'' && i > 1 && spellIsWordCh(s[i - 2])))) i--;
  while (i < n && i < b) {
    if (!spellIsWordCh(s[i])) { i++; continue; }
    int j = i;
    while (j < n && (spellIsWordCh(s[j]) || (s[j] == L'\'' && j + 1 < n && spellIsWordCh(s[j + 1])))) j++;
    bool skip = j - i < 2;
    if (i > 0 && wcschr(L"#/\\@_.-:", s[i - 1]) && !(s[i - 1] == L'.' && i > 1 && iswspace(s[i - 2]))) skip = true;
    if (j < n && (iswdigit(s[j]) || s[j] == L'_' || s[j] == L'/' || s[j] == L'\\')) skip = true;
    if (i > 0 && iswdigit(s[i - 1])) skip = true;
    if (!skip && !spellWordOk(s.substr(i, j - i))) out.push_back({i, j});
    i = j;
  }
}
// the misspelled word under (line, col), if any
static bool spellWordAt(Doc& d, int l, int c, int& a, int& b, wstring& word) {
  if (l < 0 || l >= (int)d.lines.size()) return false;
  const wstring& s = d.lines[l];
  vector<std::pair<int, int>> m;
  spellMisses(s, 0, (int)s.size(), m);
  for (auto& r : m) if (c >= r.first && c <= r.second) { a = r.first; b = r.second; word = s.substr(a, b - a); return true; }
  return false;
}
