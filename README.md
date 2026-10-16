<p align="center">
  <img src="docs/icon.png" width="96" alt="Scriba">
</p>

<h1 align="center">Scriba</h1>

<p align="center">
  A fast, native text editor for Windows, in the style of Sublime Text.<br>
  Written in C++ with Win32 and Direct2D: no browser engine, no runtime to install.
</p>

<p align="center">
  <a href="https://github.com/robertgrasu/scriba-text-editor/releases/latest"><b>Download for Windows</b></a>
  &nbsp;·&nbsp; <a href="#features">Features</a>
  &nbsp;·&nbsp; <a href="#build-from-source">Build</a>
</p>

<p align="center">
  <img src="docs/screenshots/editor.png" alt="Scriba editing a C++ file with the project in the side bar">
</p>

> This is the **native C++ edition**. The earlier Electron edition is kept on the
> [`electron-edition`](https://github.com/robertgrasu/scriba-text-editor/tree/electron-edition) branch.

## Features

- **Editing**: multiple cursors, folding, bookmarks, snippets, macros, indentation guides, coloured bracket pairs, a minimap, split view with two groups.
- **Go anywhere**: command palette, Goto Anything, Goto Symbol, Goto Line, jump back and forward, recent files and projects, a file name search in the side bar.
- **Search**: find and replace with regex, whole word and "in selection"; Find in Files and Replace in Files.
- **Syntax highlighting** for about 30 languages, parsed with [tree-sitter](https://tree-sitter.github.io/) (20 grammars), plus 38 colour schemes and four interface styles.
- **Git**: changed lines in the gutter, the branch in the status bar, next and previous change, revert a change, diff of the file.
- **Language servers**: errors as you type, completion, hover, go to definition, find references, rename, format.
- **Run Command**: runs a command line in the project folder with live output and clickable `file:line` links.
- **Notes**: quick Markdown notes with a formatted (visual) mode, tags, colours and search.
- **Claude Code**: a chat dock that runs your installed `claude` in the project folder (needs Claude Code installed and signed in).
- **Settings as JSON**, like Sublime: `settings.json`, `keymap.json`, `snippets.json`, with a settings page and live reload. The default key map follows Sublime Text, including chords such as `Ctrl+K Ctrl+D`.
- **Open from other programs**: pass a file on the command line (WinSCP, "Open with"); it opens as a tab in the window that is already running.

<p align="center">
  <img src="docs/screenshots/amxx.png" alt="A Pawn plugin with the AMX Mod X colour style">
</p>

### AMX Mod X

Tools for writing Pawn plugins for AMX Mod X: colouring that follows the AMXX-Editor schemes, completion and hover help from the includes,
unknown function marks, go to definition, compile with `amxxpc` and a results panel, an includes tree, a functions list, a HUD Editor and
an AMXX Uncompress command.

## Download

Get `Scriba-<version>-windows-x64.zip` from the [releases page](https://github.com/robertgrasu/scriba-text-editor/releases), unzip it and run
`Scriba.exe`. There is no installer and it does not need administrator rights. The public build has **no license check** and does not connect
to any server.

Windows may show "Windows protected your PC" because the program is not signed yet: press "More info", then "Run anyway".

Settings, notes and the session are kept in `%APPDATA%\scriba-cpp`. If the program stops unexpectedly it writes `crash.txt` and `scriba.log`
there (technical data only); attach them to an issue.

## Build from source

You need Visual Studio Build Tools (the C++ workload), CMake and Ninja. Then:

```powershell
.\build.ps1                      # build\Scriba.exe
.\tests\run-all.ps1              # runs every scripted scenario against build\Scriba.exe
.\tools\release.ps1 -Free        # the public zip, without the license check, in dist\
```

The tests drive the real program with a small script language (`SCRIBA_TEST=<script>`: keys, clicks, screenshots) and check the result.
By default a build has a license check; `-DSCRIBA_FREE=ON` (what `release.ps1 -Free` uses) takes it out. The code of the license
server is in [`license_server/`](license_server) and is only needed for the builds that keep the check.

Source layout: everything is included into `src/main.cpp` as `.inc` modules (`commands.inc` the command registry, key map and menus,
`edit_cmds.inc`, `exec.inc`, `settings*.inc`, `panel.inc`, `git.inc`, `lsp.inc`, `claude_*.inc`, `amxx*.inc`, ...). `TODO.md` lists what
is still missing.

## Third party

tree-sitter and its grammars, and the JSON library by Niels Lohmann, are MIT licensed: see [THIRD_PARTY.md](THIRD_PARTY.md) and
`third_party/licenses/`. The AMX Mod X features follow the AMXX-Editor package by Destro; the game screenshots in `res/hud` come from Counter-Strike.
