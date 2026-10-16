SCRIBA
======

A fast, lightweight text editor for Windows, built natively in C++.

There is no installer: run Scriba.exe from any folder. It does not need administrator rights.
This build has no license check and does not connect to any license server.

1. START
   Double click Scriba.exe.
   Windows may show "Windows protected your PC" (SmartScreen), because the program is not signed yet.
   Press "More info", then "Run anyway".

2. WHERE THE DATA IS
   %APPDATA%\scriba-cpp   (settings, notes, the session; text that was not saved is kept when you close the window)
   Settings: Preferences > Settings (Ctrl+,). Key bindings: Preferences > Key Bindings.

3. THINGS TO TRY
   - Ctrl+P: open a file by name.  Ctrl+Shift+P: every command.  Ctrl+R: go to a symbol.
   - File > Open Folder: the side bar with the files; the magnifier next to FOLDERS searches by file name.
   - Ctrl+Shift+F: search in all files.
   - Preferences > Settings > Appearance: colour schemes and interface styles.
   - Notes: quick notes in Markdown, with a formatted (visual) mode.

4. IF SOMETHING GOES WRONG
   The files crash.txt and scriba.log (if they exist) in %APPDATA%\scriba-cpp tell what happened. They hold technical data only
   (addresses, errors, file paths), not the content of your documents. Open an issue on GitHub and attach them.

The third party parts that are used are listed in THIRD_PARTY.md, with their licenses in the folder licenses.
