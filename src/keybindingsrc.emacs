#
# emacs like keybindings for HTML Editor
#

binding "gtkhtml-bindings-emacs"
{
  bind "Home"               { "cursor_move" (up,   all) }
  bind "End"                { "cursor_move" (down, all) }
  bind "<Alt>less"          { "cursor_move" (up,   all) }
  bind "<Alt>greater"       { "cursor_move" (down, all) }
  bind "<Ctrl>b"            { "cursor_move" (left,  one) }
  bind "<Ctrl>f"            { "cursor_move" (right, one) }
  bind "<Alt>b"             { "cursor_move" (left,  word) }
  bind "<Alt>f"             { "cursor_move" (right, word) }
  bind "<Ctrl>p"            { "cursor_move" (up,    one) }
  bind "<Ctrl>n"            { "cursor_move" (down,  one) }

  bind "<Alt>v"             { "cursor_move" (up,    page) }
  bind "<Ctrl>v"            { "cursor_move" (down,  page) }

  bind "<Shift>Up"            { "command" (selection-move-up) }
  bind "<Shift>Down"          { "command" (selection-move-down) }
  bind "<Shift>Left"          { "command" (selection-move-left) }
  bind "<Shift>Right"         { "command" (selection-move-right) }

  bind "<Shift>Home"          { "command" (selection-move-bol) }
  bind "<Shift>End"           { "command" (selection-move-eol) }

  bind "<Shift>Page_Up"       { "command" (selection-move-pageup) }
  bind "<Shift>KP_Page_Up"    { "command" (selection-move-pageup) }
  bind "<Shift>Page_Down"     { "command" (selection-move-pagedown) }
  bind "<Shift>KP_Page_Down"  { "command" (selection-move-pagedown) }

  bind "<Ctrl><Shift>Home"    { "command" (selection-move-bod) }
  bind "<Ctrl><Shift>End"     { "command" (selection-move-eod) }

  bind "<Ctrl>Insert"         { "command" (copy) }
  bind "<Ctrl>KP_Insert"      { "command" (copy) }

  bind "<Shift>Delete"        { "command" (cut) }
  bind "<Shift>KP_Delete"     { "command" (cut) }

  bind "<Shift>Insert"        { "command" (paste) }
  bind "<Shift>KP_Insert"     { "command" (paste) }

  bind "<Ctrl>d"            { "command" (delete) }
  bind "<Ctrl>g"            { "command" (disable-selection) }
  bind "<Ctrl>m"            { "command" (insert-paragraph) }
  bind "<Ctrl>j"            { "command" (insert-paragraph) }
  bind "<Ctrl>w"            { "command" (cut) }
  bind "<Alt>w"             { "command" (copy) }
  bind "<Ctrl>y"            { "command" (paste) }
  bind "<Ctrl>space"        { "command" (set-mark) }

  bind "<Ctrl>k"            { "command" (cut-line) }

  bind "<Ctrl><Alt>b"       { "command" (toggle-bold) }
  bind "<Ctrl><Alt>i"       { "command" (toggle-italic) }
  bind "<Ctrl><Alt>u"       { "command" (toggle-underline) }
  bind "<Ctrl><Alt>s"       { "command" (toggle-strikeout) }

  bind "<Ctrl><Alt>l"       { "command" (align-left) }
  bind "<Ctrl><Alt>c"       { "command" (align-center) }
  bind "<Ctrl><Alt>r"       { "command" (align-right) }

  bind "Tab"                { "command" (indent-more) }
  bind "<Ctrl><Alt>Tab"     { "command" (indent-less) }

  bind "<Ctrl>0"            { "command" (style-normal) }
  bind "<Ctrl>1"            { "command" (style-header1) }
  bind "<Ctrl>2"            { "command" (style-header2) }
  bind "<Ctrl>3"            { "command" (style-header3) }
  bind "<Ctrl>4"            { "command" (style-header4) }
  bind "<Ctrl>5"            { "command" (style-header5) }
  bind "<Ctrl>6"            { "command" (style-header6) }
  bind "<Ctrl>7"            { "command" (style-pre) }
  bind "<Ctrl>8"            { "command" (style-address) }
  bind "<Ctrl><Alt>1"       { "command" (style-itemdot) }
  bind "<Ctrl><Alt>2"       { "command" (style-itemroman) }
  bind "<Ctrl><Alt>3"       { "command" (style-itemdigit) }

  bind "<Alt>1"             { "command" (size-minus-2) }
  bind "<Alt>2"             { "command" (size-minus-1) }
  bind "<Alt>3"             { "command" (size-plus-0) }
  bind "<Alt>4"             { "command" (size-plus-1) }
  bind "<Alt>5"             { "command" (size-plus-2) }
  bind "<Alt>6"             { "command" (size-plus-3) }
  bind "<Alt>7"             { "command" (size-plus-4) }

  bind "<Alt>c"             { "command" (capitalize-word) }
  bind "<Alt>l"             { "command" (downcase-word) }
  bind "<Alt>u"             { "command" (upcase-word) }

  bind "<Ctrl><Shift>s"     { "command" (spell-suggest) }
  bind "<Ctrl><Shift>p"     { "command" (spell-personal-add) }
  bind "<Ctrl><Shift>n"     { "command" (spell-session-add) }

  bind "<Ctrl>s"            { "command" (isearch-forward) }
  bind "<Ctrl>r"            { "command" (isearch-backward) }
}
