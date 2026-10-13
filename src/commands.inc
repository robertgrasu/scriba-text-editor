// commands.inc: the command registry (same ids, titles and default keys as the Electron edition), the key map with
// chords like "Ctrl+K Ctrl+D", and the menus. Included by main.cpp.

// X(enum name, command id, title, default keys, flags): flag 1 = Markdown only, 2 = hidden from the palette
#define CMD_LIST(X) \
  X(NEW, "new_file", L"File: New File", L"Ctrl+N", 0) \
  X(NEWNOTE, "new_note", L"File: New Note", L"Ctrl+Alt+N", 0) \
  X(OPENNOTE, "open_note", L"File: Open Note\x2026", L"Ctrl+Alt+O", 0) \
  X(TOGGLEVISUAL, "toggle_visual", L"View: Toggle Visual Mode (formatted text)", L"Ctrl+Alt+E", 0) \
  X(VLINES, "toggle_vlines", L"View: Line Numbers in Visual / Preview", L"", 0) \
  X(VSYMS, "toggle_vsyntax", L"View: Show Markdown Symbols While Editing", L"", 0) \
  X(H1, "fmt_h1", L"Format: Heading 1", L"Ctrl+Alt+1", 1) \
  X(H2, "fmt_h2", L"Format: Heading 2", L"Ctrl+Alt+2", 1) \
  X(H3, "fmt_h3", L"Format: Heading 3", L"Ctrl+Alt+3", 1) \
  X(PARA, "fmt_p", L"Format: Plain Paragraph", L"Ctrl+Alt+0", 1) \
  X(BOLD, "fmt_bold", L"Format: Bold", L"Ctrl+B", 1) \
  X(ITALIC, "fmt_italic", L"Format: Italic", L"Ctrl+I", 1) \
  X(STRIKE, "fmt_strike", L"Format: Strikethrough", L"Ctrl+Shift+X", 1) \
  X(CODE, "fmt_code", L"Format: Inline Code", L"Ctrl+Alt+C", 1) \
  X(UL, "fmt_ul", L"Format: Bulleted List", L"Ctrl+Alt+U", 1) \
  X(OL, "fmt_ol", L"Format: Numbered List", L"", 1) \
  X(TASK, "fmt_task", L"Format: To-do List", L"Ctrl+Alt+T", 1) \
  X(QUOTE, "fmt_quote", L"Format: Quote", L"Ctrl+Alt+Q", 1) \
  X(CODEBLOCK, "fmt_codeblock", L"Format: Code Block", L"", 1) \
  X(LINK, "fmt_link", L"Format: Link\x2026", L"Ctrl+Alt+L", 1) \
  X(IMAGE, "fmt_image", L"Format: Insert Image\x2026", L"", 0) \
  X(TABLE, "fmt_table", L"Format: Table", L"", 1) \
  X(HR, "fmt_hr", L"Format: Divider", L"", 1) \
  X(SBFILES, "sb_files", L"View: Side Bar: Files", L"Ctrl+K Ctrl+E", 0) \
  X(SBNOTES, "sb_notes", L"View: Side Bar: Notes", L"Ctrl+K Ctrl+N", 0) \
  X(SBOUTLINE, "sb_outline", L"View: Side Bar: Outline", L"Ctrl+K Ctrl+O", 0) \
  X(FINDUNDER, "find_under", L"Find: Find Under", L"Ctrl+F3", 0) \
  X(FINDUNDERPREV, "find_under_prev", L"Find: Find Under Previous", L"Ctrl+Shift+F3", 0) \
  X(REPLACENEXT, "replace_next", L"Find: Replace Next", L"Ctrl+Shift+H", 0) \
  X(SETMARK, "set_mark", L"Edit: Set Mark", L"Ctrl+K Ctrl+Space", 0) \
  X(SELTOMARK, "select_to_mark", L"Selection: Select to Mark", L"Ctrl+K Ctrl+A", 0) \
  X(SWAPMARK, "swap_with_mark", L"Edit: Swap with Mark", L"Ctrl+K Ctrl+X", 0) \
  X(DELTOMARK, "delete_to_mark", L"Edit: Delete to Mark", L"", 0) \
  X(SHOWCENTER, "show_at_center", L"View: Scroll Caret to Center", L"Ctrl+K Ctrl+C", 0) \
  X(SCROLLUP, "scroll_line_up", L"View: Scroll Up One Line", L"Ctrl+Up", 0) \
  X(SCROLLDOWN, "scroll_line_down", L"View: Scroll Down One Line", L"Ctrl+Down", 0) \
  X(SWITCHFILE, "switch_file", L"Goto: Switch Between Header and Source", L"Alt+O", 0) \
  X(CLOSETAG, "close_tag", L"Edit: Close Tag", L"Alt+.", 0) \
  X(OVERWRITE, "toggle_overwrite", L"Edit: Toggle Overwrite Mode", L"Insert", 0) \
  X(JUMPBACK, "jump_back", L"Goto: Jump Back", L"Alt+-", 0) \
  X(JUMPFWD, "jump_forward", L"Goto: Jump Forward", L"Alt+Shift+-", 0) \
  X(PASTEHIST, "paste_from_history", L"Edit: Paste from History\x2026", L"Ctrl+K Ctrl+V", 0) \
  X(FOCUSG1, "focus_group_1", L"View: Focus Group 1", L"Ctrl+1", 0) \
  X(FOCUSG2, "focus_group_2", L"View: Focus Group 2", L"Ctrl+2", 0) \
  X(MOVEG1, "move_to_group_1", L"View: Move File to Group 1", L"Ctrl+Shift+1", 0) \
  X(MOVEG2, "move_to_group_2", L"View: Move File to Group 2", L"Ctrl+Shift+2", 0) \
  X(NEWPANE, "new_pane", L"View: New Group", L"Ctrl+K Ctrl+Up", 0) \
  X(CLOSEPANE, "close_pane", L"View: Close Group", L"Ctrl+K Ctrl+Down", 0) \
  X(FOLDL2, "fold_level_2", L"Code Folding: Fold Level 2", L"Ctrl+K Ctrl+2", 0) \
  X(FOLDL3, "fold_level_3", L"Code Folding: Fold Level 3", L"Ctrl+K Ctrl+3", 0) \
  X(FOLDL4, "fold_level_4", L"Code Folding: Fold Level 4", L"Ctrl+K Ctrl+4", 0) \
  X(FOLDL5, "fold_level_5", L"Code Folding: Fold Level 5", L"Ctrl+K Ctrl+5", 0) \
  X(FOLDL6, "fold_level_6", L"Code Folding: Fold Level 6", L"Ctrl+K Ctrl+6", 0) \
  X(FOLDL7, "fold_level_7", L"Code Folding: Fold Level 7", L"Ctrl+K Ctrl+7", 0) \
  X(FOLDL8, "fold_level_8", L"Code Folding: Fold Level 8", L"Ctrl+K Ctrl+8", 0) \
  X(FOLDL9, "fold_level_9", L"Code Folding: Fold Level 9", L"Ctrl+K Ctrl+9", 0) \
  X(NOTEPIN, "pin_note", L"Notes: Pin / Unpin Note", L"Ctrl+Alt+P", 0) \
  X(NOTERENAME, "rename_note", L"Notes: Rename Note\x2026", L"", 0) \
  X(NOTECOLORPICK, "note_color", L"Notes: Set Note Color\x2026", L"", 0) \
  X(MDPREVIEW, "md_preview", L"View: Toggle Markdown Preview", L"Ctrl+Alt+M", 0) \
  X(SEARCHNOTES, "search_notes", L"File: Search Notes", L"Ctrl+Alt+F", 0) \
  X(NEWWINDOW, "new_window", L"File: New Window", L"Ctrl+Shift+N", 0) \
  X(OPEN, "open_file", L"File: Open File\x2026", L"Ctrl+O", 0) \
  X(OPENFOLDER, "open_folder", L"File: Open Folder\x2026", L"", 0) \
  X(REOPEN, "reopen_closed", L"File: Reopen Closed File", L"Ctrl+Shift+T", 0) \
  X(SAVE, "save", L"File: Save", L"Ctrl+S", 0) \
  X(SAVEAS, "save_as", L"File: Save As\x2026", L"Ctrl+Shift+S", 0) \
  X(SAVEALL, "save_all", L"File: Save All", L"", 0) \
  X(SAVEENC, "save_encoding", L"File: Save with Encoding\x2026", L"", 0) \
  X(CLOSE, "close_file", L"File: Close File", L"Ctrl+W", 0) \
  X(CLOSEOTHERS, "close_others", L"File: Close Other Files", L"", 0) \
  X(CLOSEALL, "close_all", L"File: Close All Files", L"", 0) \
  X(CLOSEWINDOW, "close_window", L"File: Close Window", L"Ctrl+Shift+W", 0) \
  X(EXIT, "exit", L"File: Exit", L"", 0) \
  X(UNDO, "undo", L"Edit: Undo", L"Ctrl+Z", 0) \
  X(REDO, "redo", L"Edit: Redo", L"Ctrl+Y", 0) \
  X(CUT, "cut", L"Edit: Cut", L"Ctrl+X", 0) \
  X(COPY, "copy", L"Edit: Copy", L"Ctrl+C", 0) \
  X(PASTE, "paste", L"Edit: Paste", L"Ctrl+V", 0) \
  X(PASTEINDENT, "paste_indent", L"Edit: Paste and Indent", L"Ctrl+Shift+V", 0) \
  X(DUP, "duplicate_line", L"Edit: Duplicate Line", L"Ctrl+Shift+D", 0) \
  X(DELLINE, "delete_line", L"Edit: Delete Line", L"Ctrl+Shift+K", 0) \
  X(DELEOL, "delete_eol", L"Edit: Delete to End of Line", L"Ctrl+K Ctrl+K", 0) \
  X(DELBOL, "delete_bol", L"Edit: Delete to Beginning of Line", L"Ctrl+K Ctrl+Backspace", 0) \
  X(JOIN, "join_lines", L"Edit: Join Lines", L"Ctrl+J", 0) \
  X(SWAPUP, "swap_up", L"Edit: Swap Line Up", L"Ctrl+Shift+Up", 0) \
  X(SWAPDOWN, "swap_down", L"Edit: Swap Line Down", L"Ctrl+Shift+Down", 0) \
  X(INSAFTER, "insert_after", L"Edit: Insert Line After", L"Ctrl+Enter", 0) \
  X(INSBEFORE, "insert_before", L"Edit: Insert Line Before", L"Ctrl+Shift+Enter", 0) \
  X(TRANSPOSE, "transpose", L"Edit: Transpose", L"Ctrl+T", 0) \
  X(COMMENT, "comment", L"Edit: Toggle Comment", L"Ctrl+/", 0) \
  X(BLOCKCOMMENT, "block_comment", L"Edit: Toggle Block Comment", L"Ctrl+Shift+/", 0) \
  X(INDENT, "indent", L"Edit: Indent", L"Ctrl+]", 0) \
  X(UNINDENT, "unindent", L"Edit: Unindent", L"Ctrl+[", 0) \
  X(REINDENT, "reindent", L"Edit: Reindent", L"", 0) \
  X(WRAPPARA, "wrap_paragraph", L"Edit: Wrap Paragraph at Ruler", L"Alt+Q", 0) \
  X(SORT, "sort_lines", L"Edit: Sort Lines", L"F9", 0) \
  X(SORTCI, "sort_lines_ci", L"Edit: Sort Lines (Case Insensitive)", L"Ctrl+F9", 0) \
  X(TRIMWS, "trim_ws", L"Edit: Trim Trailing White Space", L"", 0) \
  X(UPPER, "upper", L"Edit: Convert Case: UPPERCASE", L"Ctrl+K Ctrl+U", 0) \
  X(LOWER, "lower", L"Edit: Convert Case: lowercase", L"Ctrl+K Ctrl+L", 0) \
  X(TITLECASE, "title", L"Edit: Convert Case: Title Case", L"", 0) \
  X(SELALL, "select_all", L"Selection: Select All", L"Ctrl+A", 0) \
  X(SELLINE, "select_line", L"Selection: Expand Selection to Line", L"Ctrl+L", 0) \
  X(SELWORD, "expand_word", L"Selection: Expand Selection to Word", L"Ctrl+K Ctrl+W", 0) \
  X(EXPANDSCOPE, "expand_scope", L"Selection: Expand Selection to Scope", L"Ctrl+Shift+Space", 0) \
  X(SELBRACKETS, "select_brackets", L"Selection: Select Contents of Brackets", L"Ctrl+Shift+M", 0) \
  X(GOTOBRACKET, "goto_bracket", L"Goto: Jump to Matching Bracket", L"Ctrl+M", 0) \
  X(SPLITSEL, "split_lines", L"Selection: Split into Lines", L"Ctrl+Shift+L", 0) \
  X(ADDNEXT, "next_occ", L"Selection: Add Next Occurrence", L"Ctrl+D", 0) \
  X(SKIPOCC, "skip_occ", L"Selection: Skip Occurrence", L"Ctrl+K Ctrl+D", 0) \
  X(ADDABOVE, "cursor_up", L"Selection: Add Cursor Above", L"Ctrl+Alt+Up", 0) \
  X(ADDBELOW, "cursor_down", L"Selection: Add Cursor Below", L"Ctrl+Alt+Down", 0) \
  X(FIND, "find", L"Find: Find\x2026", L"Ctrl+F", 0) \
  X(REPLACE, "replace", L"Find: Replace\x2026", L"Ctrl+H", 0) \
  X(FINDNEXT, "find_next", L"Find: Find Next", L"F3", 0) \
  X(FINDPREV, "find_prev", L"Find: Find Previous", L"Shift+F3", 0) \
  X(USESEL, "use_selection", L"Find: Use Selection for Find", L"Ctrl+E", 0) \
  X(FIF, "find_in_files", L"Find: Find in Files\x2026", L"Ctrl+Shift+F", 0) \
  X(NEXTRES, "next_result", L"Find: Next Result", L"F4", 0) \
  X(PREVRES, "prev_result", L"Find: Previous Result", L"Shift+F4", 0) \
  X(FOLD, "fold", L"Code Folding: Fold", L"Ctrl+Shift+[", 0) \
  X(UNFOLD, "unfold", L"Code Folding: Unfold", L"Ctrl+Shift+]", 0) \
  X(FOLDALL, "fold_all", L"Code Folding: Fold All", L"Ctrl+K Ctrl+1", 0) \
  X(UNFOLDALL, "unfold_all", L"Code Folding: Unfold All", L"Ctrl+K Ctrl+J", 0) \
  X(BMTOGGLE, "bm_toggle", L"Bookmarks: Toggle Bookmark", L"Ctrl+F2", 0) \
  X(BMNEXT, "bm_next", L"Bookmarks: Next Bookmark", L"F2", 0) \
  X(BMPREV, "bm_prev", L"Bookmarks: Previous Bookmark", L"Shift+F2", 0) \
  X(BMSELECT, "bm_select", L"Bookmarks: Select All Bookmarks", L"Alt+F2", 0) \
  X(BMCLEAR, "bm_clear", L"Bookmarks: Clear Bookmarks", L"Ctrl+Shift+F2", 0) \
  X(SIDEBAR, "toggle_sidebar", L"View: Toggle Side Bar", L"Ctrl+K Ctrl+B", 0) \
  X(MINIMAP, "toggle_minimap", L"View: Toggle Minimap", L"", 0) \
  X(WRAP, "word_wrap", L"View: Toggle Word Wrap", L"Alt+Z", 0) \
  X(STATUSBAR, "status_bar", L"View: Toggle Status Bar", L"", 0) \
  X(FULLSCREEN, "fullscreen", L"View: Toggle Full Screen", L"F11", 0) \
  X(DISTFREE, "distraction", L"View: Toggle Distraction Free Mode", L"Shift+F11", 0) \
  X(ZOOMIN, "zoom_in", L"View: Increase Font Size", L"Ctrl+=", 0) \
  X(ZOOMOUT, "zoom_out", L"View: Decrease Font Size", L"Ctrl+-", 0) \
  X(ZOOM0, "zoom_reset", L"View: Reset Font Size", L"Ctrl+0", 0) \
  X(PICKSYNTAX, "pick_syntax", L"View: Syntax\x2026", L"", 0) \
  X(PICKSCHEME, "pick_scheme", L"UI: Select Color Scheme\x2026", L"", 0) \
  X(PICKUI, "pick_ui", L"UI: Select Interface Style\x2026", L"", 0) \
  X(PICKINDENT, "pick_indent", L"Indentation: Indentation\x2026", L"", 0) \
  X(PICKEOL, "pick_eol", L"Line Endings: Line Endings\x2026", L"", 0) \
  X(TOSPACES, "spaces", L"Indentation: Convert Indentation to Spaces", L"", 0) \
  X(TOTABS, "tabs", L"Indentation: Convert Indentation to Tabs", L"", 0) \
  X(GUESSINDENT, "guess_indent", L"Indentation: Guess Settings From Buffer", L"", 0) \
  X(COMPLETE, "complete", L"Edit: Show Suggestions", L"Ctrl+Space", 0) \
  X(GOTOANY, "goto_anything", L"Goto: Goto Anything\x2026", L"Ctrl+P", 0) \
  X(GOTOSYMBOL, "goto_symbol", L"Goto: Goto Symbol\x2026", L"Ctrl+R", 0) \
  X(GOTOLINE, "goto_line", L"Goto: Goto Line\x2026", L"Ctrl+G", 0) \
  X(NEXTTAB, "next_tab", L"Goto: Next Tab", L"Ctrl+PageDown", 0) \
  X(PREVTAB, "prev_tab", L"Goto: Previous Tab", L"Ctrl+PageUp", 0) \
  X(NEXTTAB2, "next_tab2", L"Goto: Next Tab (Ctrl+Tab)", L"Ctrl+Tab", 2) \
  X(PREVTAB2, "prev_tab2", L"Goto: Previous Tab (Ctrl+Shift+Tab)", L"Ctrl+Shift+Tab", 2) \
  X(TABRIGHT, "move_tab_right", L"Goto: Move Tab Right", L"Ctrl+Shift+PageDown", 0) \
  X(TABLEFT, "move_tab_left", L"Goto: Move Tab Left", L"Ctrl+Shift+PageUp", 0) \
  X(PALETTE, "palette", L"Tools: Command Palette\x2026", L"Ctrl+Shift+P", 0) \
  X(ADDFOLDER, "add_folder", L"Project: Add Folder to Project\x2026", L"", 0) \
  X(CLOSEFOLDER, "remove_folders", L"Project: Remove All Folders from Project", L"", 0) \
  X(REVEAL, "reveal_sidebar", L"Project: Reveal in Side Bar", L"", 0) \
  X(REFRESHFOLDER, "refresh_folders", L"Project: Refresh Folders", L"", 0) \
  X(SETTINGS, "settings", L"Preferences: Settings", L"Ctrl+,", 0) \
  X(KEYBINDINGS, "keybindings", L"Preferences: Key Bindings", L"", 0) \
  X(SETTINGSJSON, "settings_json", L"Preferences: Settings (JSON)", L"", 0) \
  X(KEYSJSON, "keybindings_json", L"Preferences: Key Bindings (JSON)", L"", 0) \
  X(SNIPPETSJSON, "snippets_json", L"Preferences: Snippets (JSON)", L"", 0) \
  X(OPENRECENT, "open_recent", L"File: Open Recent\x2026", L"Ctrl+Alt+R", 0) \
  X(PROJSAVE, "project_save", L"Project: Save Project As\x2026", L"", 0) \
  X(PROJOPEN, "project_open", L"Project: Open Project\x2026", L"", 0) \
  X(RECENTCLEAR, "recent_clear", L"File: Clear Recent Items", L"", 0) \
  X(MACROTOGGLE, "macro_toggle", L"Macros: Record / Stop Recording", L"Ctrl+Q", 0) \
  X(MACROPLAY, "macro_play", L"Macros: Play Last Macro", L"Ctrl+Shift+Q", 0) \
  X(MACROSAVE, "macro_save", L"Macros: Save Last Macro As\x2026", L"", 0) \
  X(MACRORUN, "macro_run", L"Macros: Run Saved Macro\x2026", L"", 0) \
  X(RUNCMD, "run_command", L"Tools: Run Command\x2026", L"Ctrl+Shift+B", 0) \
  X(RUNSTOP, "run_stop", L"Tools: Stop Running Command", L"", 0) \
  X(CLAUDE, "claude_panel", L"Tools: Claude Code\x2026", L"Ctrl+Alt+J", 0) \
  X(CLAUDENEW, "claude_new", L"Tools: Claude Code: New Conversation", L"", 0) \
  X(CLAUDEASK, "claude_ask", L"Tools: Claude Code: Ask About Selection\x2026", L"Ctrl+Alt+Shift+J", 0) \
  X(CLAUDESTOP, "claude_stop", L"Tools: Claude Code: Stop", L"", 0) \
  X(CLAUDETRUST, "claude_trust", L"Tools: Claude Code: Trust This Folder\x2026", L"", 0) \
  X(CLAUDEADD, "claude_add_selection", L"Tools: Claude Code: Add Selection to Chat", L"Ctrl+Alt+K", 0) \
  X(CLAUDEEXPLAIN, "claude_explain", L"Tools: Claude Code: Explain This", L"", 0) \
  X(CLAUDEFIX, "claude_fix", L"Tools: Claude Code: Find and Fix Problems", L"", 0) \
  X(CLAUDETESTS, "claude_tests", L"Tools: Claude Code: Write Tests", L"", 0) \
  X(CLAUDEREFACTOR, "claude_refactor", L"Tools: Claude Code: Refactor This", L"", 0) \
  X(CLAUDEDOC, "claude_comment", L"Tools: Claude Code: Document This", L"", 0) \
  X(LAYOUT1, "layout_single", L"View: Layout: Single", L"Alt+Shift+1", 0) \
  X(LAYOUT2, "layout_cols", L"View: Layout: Two Columns", L"Alt+Shift+2", 0) \
  X(LAYOUT3, "layout_rows", L"View: Layout: Two Rows", L"Alt+Shift+8", 0) \
  X(OTHERGROUP, "pane_other", L"View: Focus Other Group", L"F6", 0) \
  X(MOVETAB, "pane_move", L"View: Move File to Other Group", L"Ctrl+K Ctrl+M", 0) \
  X(GITNEXT, "git_next", L"Git: Next Change", L"Alt+F3", 0) \
  X(GITPREV, "git_prev", L"Git: Previous Change", L"Shift+Alt+F3", 0) \
  X(GITREVERT, "git_revert", L"Git: Revert This Change", L"", 0) \
  X(GITDIFF, "git_diff", L"Git: Show Changes in This File", L"", 0) \
  X(GITCHANGED, "git_changed_files", L"Git: Changed Files\x2026", L"", 0) \
  X(AMXCOMPILE, "amxx_compile", L"AMX Mod X: Compile Plugin", L"F7", 0) \
  X(AMXGOTODEF, "amxx_goto_def", L"AMX Mod X: Go to Definition", L"", 0) \
  X(GOTODEF, "goto_definition", L"Goto: Go to Definition", L"Ctrl+F12", 0) \
  X(LSPREFS, "lsp_references", L"Goto: Find References", L"Shift+F12", 0) \
  X(LSPRENAME, "lsp_rename", L"Edit: Rename Symbol\x2026", L"Shift+F6", 0) \
  X(LSPFORMAT, "lsp_format", L"Edit: Format Document", L"Shift+Alt+F", 0) \
  X(LSPPROBLEMS, "lsp_problems", L"Tools: Show Problems in This File\x2026", L"Shift+F8", 0) \
  X(LSPSTATUS, "lsp_status", L"Tools: Language Servers\x2026", L"", 0) \
  X(LSPRESTART, "lsp_restart", L"Tools: Restart Language Servers", L"", 0) \
  X(AMXCOMPILER, "amxx_pick_compiler", L"AMX Mod X: Choose Compiler (amxxpc)\x2026", L"", 0) \
  X(AMXPROFILE, "amxx_pick_profile", L"AMX Mod X: Select Build Profile\x2026", L"", 0) \
  X(AMXCLOSE, "amxx_close_panel", L"AMX Mod X: Close Compile Results", L"", 0) \
  X(AMXHUD, "amxx_hud", L"AMX Mod X: HUD Editor\x2026", L"", 0) \
  X(AMXPOPUP, "amxx_popup_style", L"AMX Mod X: Popup Style\x2026", L"", 0) \
  X(AMXCONSOLE, "amxx_console_style", L"AMX Mod X: Compile Results Style\x2026", L"", 0) \
  X(AMXUNCOMP, "amxx_uncompress", L"AMX Mod X: AMXX Uncompress\x2026", L"", 0) \
  X(AMXLYSIS, "amxx_lysis_decompile", L"AMX Mod X: Lysis Decompile\x2026", L"", 0) \
  X(AMXFUNCS, "amxx_func_list", L"AMX Mod X: Functions List", L"", 0) \
  X(AMXTREE, "amxx_tree", L"AMX Mod X: Includes Tree", L"", 0) \
  X(AMXSEARCH, "amxx_search_all", L"AMX Mod X: Search All", L"Ctrl+Alt+F", 0) \
  X(AMXSTYLE, "amxx_style", L"AMX Mod X: Color Style\x2026", L"", 0) \
  X(AMXNEWPLUGIN, "amxx_new_plugin", L"AMX Mod X: New Plug-In", L"", 0) \
  X(AMXNEWINC, "amxx_new_include", L"AMX Mod X: New Include", L"", 0) \
  X(LICENSE, "license", L"Help: License\x2026", L"", 0) \
  X(ABOUT, "about", L"Help: About", L"", 0) \
  X(TAB1, "tab_1", L"Goto: Tab 1", L"Alt+1", 2) \
  X(TAB2, "tab_2", L"Goto: Tab 2", L"Alt+2", 2) \
  X(TAB3, "tab_3", L"Goto: Tab 3", L"Alt+3", 2) \
  X(TAB4, "tab_4", L"Goto: Tab 4", L"Alt+4", 2) \
  X(TAB5, "tab_5", L"Goto: Tab 5", L"Alt+5", 2) \
  X(TAB6, "tab_6", L"Goto: Tab 6", L"Alt+6", 2) \
  X(TAB7, "tab_7", L"Goto: Tab 7", L"Alt+7", 2) \
  X(TAB8, "tab_8", L"Goto: Tab 8", L"Alt+8", 2) \
  X(TAB9, "tab_9", L"Goto: Tab 9", L"Alt+9", 2)

// commands that exist for the program only (no id, not in the palette): palette rows, internal actions
enum Cmd {
  C_NONE = 0,
#define X(e, id, title, keys, flags) C_##e,
  CMD_LIST(X)
#undef X
  C_NOTECOLOR0, C_NOTECOLOR1, C_NOTECOLOR2, C_NOTECOLOR3, C_NOTECOLOR4, C_NOTECOLOR5, C_NOTECOLOR6,
  C_GOTOEND, C_GOTOSTART, C_SBTOGGLEICON, C_FMTBAR, C_GUIDES, C_BRKCOL, C_IMPORTNOTES, C_TABLEHOLD, C_NOTEDELETE,
  C_SCHEME_BASE = 5000,  // + the index of a colour scheme: "UI: Color Scheme: <name>" in the palette (up to 100)
  C_SYNTAX_BASE = 5100,  // + the index of a language: "Set Syntax: <name>"
  C_NA = -1
};

static const struct { const wchar_t* name; Lang lang; } SYNTAXES[] = {
    {L"Plain Text", L_PLAIN}, {L"JavaScript", L_CLIKE}, {L"TypeScript", L_CLIKE}, {L"C", L_CLIKE}, {L"C++", L_CLIKE}, {L"C#", L_CLIKE},
    {L"Java", L_CLIKE}, {L"Go", L_CLIKE}, {L"Rust", L_CLIKE}, {L"PHP", L_CLIKE}, {L"JSON", L_CLIKE}, {L"CSS", L_CLIKE},
    {L"Pawn (AMX Mod X)", L_PAWN}, {L"Python", L_PY}, {L"Ruby", L_PY}, {L"YAML", L_PY}, {L"Config", L_PY}, {L"Shell", L_SH}, {L"Batch", L_SH},
    {L"PowerShell", L_SH}, {L"Markdown", L_MD}, {L"HTML", L_HTML}, {L"XML", L_HTML}, {L"Diff", L_DIFF},
    {L"Lua", L_CLIKE}, {L"Kotlin", L_CLIKE}, {L"TOML", L_PY}, {L"TypeScript React", L_CLIKE}};

struct CmdDef {
  int cmd;
  const char* id;
  const wchar_t* title;
  const wchar_t* keys;  // default
  int flags;
};
static const CmdDef g_cmdDefs[] = {
#define X(e, id, title, keys, flags) {C_##e, id, title, keys, flags},
    CMD_LIST(X)
#undef X
};
static const int N_CMDS = (int)(sizeof(g_cmdDefs) / sizeof(g_cmdDefs[0]));
static const CmdDef* cmdDef(int cmd) {
  for (int i = 0; i < N_CMDS; i++) if (g_cmdDefs[i].cmd == cmd) return &g_cmdDefs[i];
  return nullptr;
}
static const CmdDef* cmdById(const std::string& id) {
  for (int i = 0; i < N_CMDS; i++) if (id == g_cmdDefs[i].id) return &g_cmdDefs[i];
  return nullptr;
}
static wstring cmdLabel(int cmd) {  // menu label: the title without its "Group: " prefix
  const CmdDef* c = cmdDef(cmd);
  if (!c) return L"";
  wstring t = c->title;
  size_t p = t.find(L": ");
  return p == wstring::npos ? t : t.substr(p + 2);
}

/* ---------------------------------------------------------------- key map */
struct KeyChord {
  bool ctrl = false, alt = false, shift = false;
  int vk = 0;
  bool operator==(const KeyChord& o) const { return ctrl == o.ctrl && alt == o.alt && shift == o.shift && vk == o.vk; }
};
struct Binding { vector<KeyChord> chords; int cmd; };
static vector<Binding> g_keymap;
static std::map<int, wstring> g_keyText;  // effective keys of a command, for menus and the key bindings page
static wstring lowerStr(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }

static int vkFromName(const wstring& n0) {
  wstring n = lowerStr(n0);
  static const struct { const wchar_t* name; int vk; } T2[] = {
      {L"up", VK_UP}, {L"down", VK_DOWN}, {L"left", VK_LEFT}, {L"right", VK_RIGHT}, {L"pageup", VK_PRIOR}, {L"pagedown", VK_NEXT},
      {L"home", VK_HOME}, {L"end", VK_END}, {L"enter", VK_RETURN}, {L"return", VK_RETURN}, {L"tab", VK_TAB}, {L"space", VK_SPACE},
      {L"backspace", VK_BACK}, {L"delete", VK_DELETE}, {L"del", VK_DELETE}, {L"insert", VK_INSERT}, {L"esc", VK_ESCAPE},
      {L"escape", VK_ESCAPE}, {L"=", VK_OEM_PLUS}, {L"+", VK_OEM_PLUS}, {L"-", VK_OEM_MINUS}, {L",", VK_OEM_COMMA}, {L".", VK_OEM_PERIOD},
      {L"/", VK_OEM_2}, {L"[", VK_OEM_4}, {L"]", VK_OEM_6}, {L"\\", VK_OEM_5}, {L";", VK_OEM_1}, {L"'", VK_OEM_7}, {L"`", VK_OEM_3}};
  for (auto& t : T2) if (n == t.name) return t.vk;
  if (n.size() >= 2 && n[0] == L'f') {
    int k = _wtoi(n.c_str() + 1);
    if (k >= 1 && k <= 24) return VK_F1 + k - 1;
  }
  if (n.size() == 1 && ((n[0] >= L'a' && n[0] <= L'z') || (n[0] >= L'0' && n[0] <= L'9'))) return (int)towupper(n[0]);
  return 0;
}
static vector<KeyChord> parseKeys(const wstring& keys) {
  vector<KeyChord> out;
  size_t p = 0;
  while (p < keys.size()) {
    while (p < keys.size() && keys[p] == L' ') p++;
    if (p >= keys.size()) break;
    size_t q = keys.find(L' ', p);
    if (q == wstring::npos) q = keys.size();
    wstring part = keys.substr(p, q - p);
    p = q;
    KeyChord c;
    // modifiers separated by '+'; the last token is the key (it can itself be '+')
    size_t s = 0;
    wstring keyTok;
    while (s <= part.size()) {
      size_t e = part.find(L'+', s);
      if (e == wstring::npos) { keyTok = part.substr(s); break; }
      if (e == s && s + 1 >= part.size()) { keyTok = L"+"; break; }  // "Ctrl++"
      wstring mod = lowerStr(part.substr(s, e - s));
      if (mod == L"ctrl") c.ctrl = true;
      else if (mod == L"alt") c.alt = true;
      else if (mod == L"shift") c.shift = true;
      else if (mod == L"cmd" || mod == L"super" || mod == L"meta") c.ctrl = true;
      else if (!mod.empty()) { keyTok = part.substr(s); break; }
      s = e + 1;
    }
    c.vk = vkFromName(keyTok);
    if (!c.vk) return {};
    out.push_back(c);
  }
  return out;
}
static wstring chordText(const KeyChord& c) {
  wstring s;
  if (c.ctrl) s += L"Ctrl+";
  if (c.alt) s += L"Alt+";
  if (c.shift) s += L"Shift+";
  static const struct { int vk; const wchar_t* n; } N2[] = {
      {VK_UP, L"Up"}, {VK_DOWN, L"Down"}, {VK_LEFT, L"Left"}, {VK_RIGHT, L"Right"}, {VK_PRIOR, L"PageUp"}, {VK_NEXT, L"PageDown"},
      {VK_HOME, L"Home"}, {VK_END, L"End"}, {VK_RETURN, L"Enter"}, {VK_TAB, L"Tab"}, {VK_SPACE, L"Space"}, {VK_BACK, L"Backspace"},
      {VK_DELETE, L"Delete"}, {VK_INSERT, L"Insert"}, {VK_ESCAPE, L"Esc"}, {VK_OEM_PLUS, L"="}, {VK_OEM_MINUS, L"-"},
      {VK_OEM_COMMA, L","}, {VK_OEM_PERIOD, L"."}, {VK_OEM_2, L"/"}, {VK_OEM_4, L"["}, {VK_OEM_6, L"]"}, {VK_OEM_5, L"\\"},
      {VK_OEM_1, L";"}, {VK_OEM_7, L"'"}, {VK_OEM_3, L"`"}};
  for (auto& n : N2) if (n.vk == c.vk) return s + n.n;
  if (c.vk >= VK_F1 && c.vk <= VK_F24) return s + L"F" + std::to_wstring(c.vk - VK_F1 + 1);
  return s + wstring(1, (wchar_t)c.vk);
}
static wstring keysText(const vector<KeyChord>& cs) {
  wstring r;
  for (size_t i = 0; i < cs.size(); i++) { if (i) r += L" "; r += chordText(cs[i]); }
  return r;
}
// rebuilds the key map from the defaults plus the user's list (entries: {"keys": [...], "command": "id"}); a command
// with a user binding loses its default keys, and "command": "-id" removes the binding
struct UserKey { vector<wstring> keys; std::string command; };
static vector<UserKey> g_userKeys;
static void rebuildKeymap() {
  g_keymap.clear();
  g_keyText.clear();
  std::set<int> overridden;
  for (auto& u : g_userKeys) {
    std::string id = u.command;
    if (!id.empty() && id[0] == '-') id = id.substr(1);
    if (const CmdDef* c = cmdById(id)) overridden.insert(c->cmd);
  }
  for (int i = 0; i < N_CMDS; i++) {
    const CmdDef& c = g_cmdDefs[i];
    if (!c.keys[0] || overridden.count(c.cmd)) continue;
    vector<KeyChord> cs = parseKeys(c.keys);
    if (!cs.empty()) g_keymap.push_back({cs, c.cmd});
  }
  static const struct { const wchar_t* keys; const char* id; } ALIASES[] = {
      {L"Ctrl+Shift+Z", "redo"}, {L"Ctrl+F4", "close_file"}, {L"F12", "goto_definition"}, {L"Ctrl+.", "git_next"},
      {L"Ctrl+K Ctrl+Z", "git_revert"}, {L"Ctrl+K Ctrl+/", "git_diff"}, {L"Ctrl+K Ctrl+;", "git_diff"},
      {L"Ctrl+Shift+Backspace", "delete_bol"}, {L"Ctrl+Shift+Delete", "delete_eol"}, {L"Ctrl+K Ctrl+0", "unfold_all"},
      {L"Ctrl+K Ctrl+Left", "pane_other"}, {L"Ctrl+K Ctrl+Right", "pane_other"},
      {L"Ctrl+K Ctrl+Shift+Left", "pane_move"}, {L"Ctrl+K Ctrl+Shift+Right", "pane_move"}, {L"Alt+Shift+O", "switch_file"}};
  for (auto& a : ALIASES) {
    const CmdDef* c = cmdById(a.id);
    if (!c || overridden.count(c->cmd)) continue;
    vector<KeyChord> cs = parseKeys(a.keys);
    if (!cs.empty()) g_keymap.push_back({cs, c->cmd});
  }
  for (auto& u : g_userKeys) {
    if (u.command.empty() || u.command[0] == '-') continue;
    const CmdDef* c = cmdById(u.command);
    if (!c) continue;
    for (auto& k : u.keys) {
      vector<KeyChord> cs = parseKeys(k);
      if (!cs.empty()) g_keymap.push_back({cs, c->cmd});
    }
  }
  for (auto& b : g_keymap) if (!g_keyText.count(b.cmd)) g_keyText[b.cmd] = keysText(b.chords);
}
static wstring effectiveKeys(int cmd) {
  auto it = g_keyText.find(cmd);
  return it == g_keyText.end() ? L"" : it->second;
}

/* ---------------------------------------------------------------- menus */
struct MItem { wstring label; int cmd; };  // cmd 0 and an empty label: separator
struct MMenu { wstring label; vector<MItem> items; float x = 0, w = 0; };
static vector<MMenu> g_menus;
static void buildMenus() {
  struct Def { const wchar_t* name; std::vector<const char*> ids; };
  static const Def defs[] = {
      {L"File", {"new_file", "new_note", "new_window", "-", "open_file", "open_note", "open_recent", "open_folder", "reopen_closed", "-", "save", "save_as", "save_all", "save_encoding", "-", "rename_note", "pin_note", "note_color", "-", "close_file", "close_others", "close_all", "close_window", "-", "exit"}},
      {L"Edit", {"undo", "redo", "-", "cut", "copy", "paste", "paste_indent", "paste_from_history", "-", "duplicate_line", "delete_line", "delete_eol", "delete_bol", "join_lines", "swap_up", "swap_down", "insert_after", "insert_before", "transpose", "-", "set_mark", "select_to_mark", "swap_with_mark", "delete_to_mark", "-", "close_tag", "toggle_overwrite", "-", "comment", "block_comment", "indent", "unindent", "reindent", "wrap_paragraph", "-", "sort_lines", "sort_lines_ci", "trim_ws", "complete", "lsp_rename", "lsp_format", "-", "upper", "lower", "title"}},
      {L"Notes", {"sb_notes", "search_notes", "-", "toggle_visual", "md_preview", "toggle_vlines", "toggle_vsyntax", "-", "fmt_h1", "fmt_h2", "fmt_h3", "fmt_p", "-", "fmt_bold", "fmt_italic", "fmt_strike", "fmt_code", "-", "fmt_ul", "fmt_ol", "fmt_task", "fmt_quote", "fmt_codeblock", "-", "fmt_link", "fmt_image", "fmt_table", "fmt_hr"}},
      {L"Selection", {"select_all", "select_line", "expand_word", "expand_scope", "select_brackets", "split_lines", "-", "next_occ", "skip_occ", "cursor_up", "cursor_down"}},
      {L"Find", {"find", "replace", "find_next", "find_prev", "use_selection", "find_under", "find_under_prev", "replace_next", "-", "find_in_files", "next_result", "prev_result"}},
      {L"View", {"toggle_sidebar", "sb_files", "sb_notes", "sb_outline", "toggle_minimap", "status_bar", "word_wrap", "-", "layout_single", "layout_cols", "layout_rows", "pane_other", "pane_move", "new_pane", "close_pane", "-", "pick_syntax", "pick_scheme", "pick_ui", "pick_indent", "pick_eol", "-", "fold", "unfold", "fold_all", "unfold_all", "fold_level_2", "fold_level_3", "fold_level_4", "-", "scroll_line_up", "scroll_line_down", "show_at_center", "-", "zoom_in", "zoom_out", "zoom_reset", "-", "fullscreen", "distraction"}},
      {L"Goto", {"goto_anything", "goto_symbol", "goto_line", "goto_bracket", "switch_file", "-", "jump_back", "jump_forward", "-", "goto_definition", "lsp_references", "-", "bm_toggle", "bm_next", "bm_prev", "bm_select", "bm_clear", "-", "next_tab", "prev_tab", "move_tab_left", "move_tab_right"}},
      {L"Tools", {"palette", "-", "claude_panel", "claude_new", "claude_ask", "claude_add_selection", "claude_explain", "claude_fix", "claude_tests", "claude_refactor", "claude_comment", "claude_stop", "claude_trust", "-", "lsp_problems", "lsp_status", "lsp_restart", "-", "run_command", "run_stop", "-", "macro_toggle", "macro_play", "macro_save", "macro_run"}},
      {L"Git", {"git_changed_files", "git_diff", "-", "git_next", "git_prev", "git_revert"}},
      {L"Project", {"open_folder", "add_folder", "remove_folders", "refresh_folders", "-", "project_open", "project_save", "-", "reveal_sidebar", "find_in_files"}},
      {L"Preferences", {"settings", "keybindings", "-", "settings_json", "keybindings_json", "snippets_json", "-", "pick_scheme", "pick_ui"}},
      {L"Help", {"license", "about"}},
      {L"AMX Mod X", {"amxx_compile", "amxx_goto_def", "-", "amxx_func_list", "amxx_tree", "amxx_search_all", "-", "amxx_uncompress", "amxx_lysis_decompile", "-", "amxx_pick_profile", "amxx_pick_compiler", "amxx_style", "amxx_popup_style", "amxx_console_style", "-", "amxx_hud", "amxx_new_plugin", "amxx_new_include", "-", "amxx_close_panel"}},
  };
  g_menus.clear();
  for (auto& d : defs) {
    MMenu m;
    m.label = d.name;
    for (const char* id : d.ids) {
      if (id[0] == '-') { m.items.push_back({L"", 0}); continue; }
      const CmdDef* c = cmdById(id);
      if (c) m.items.push_back({cmdLabel(c->cmd), c->cmd});
    }
    g_menus.push_back(m);
  }
}
