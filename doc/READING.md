# Reading and configuration

Configuration: `~/.config/zatura/zaturarc`. Use `:set name value` for runtime changes;
Tab completes names. Defaults belong in the file; saved document views take precedence.
See the [full option manual](man/zaturarc.5.rst) and [generated defaults](zaturarc.example).
Shortcuts use physical US key positions; text entry uses the active layout.

| Keys | Action |
| --- | --- |
| `j` / `k`, wheel | Scroll |
| Space / Shift+Space | Next / previous spread |
| `/`, `n` / `N` | Search, next / previous match |
| Tab | Contents |
| `a` / `s`, `+` / `-` | Fit page / width, zoom |
| `d`, Ctrl+- / Ctrl++ | One / two columns, book text size |
| `1` / `2`, `3` / `4` | Contrast, brightness |
| `5` / `6`, `7` / `8` | Gamma, saturation |
| `F11`, `F5`, `t`, `q` | Fullscreen, presentation, clock, quit |

Image controls range from −100 to 100; 0 is neutral. Printing/export remain unchanged.
Reset mapping: `map <C-0> reset_page_effects`.

```conf
set reflow-font-size 12
set reflow-margin-top 4
set reflow-margin-bottom 4
set reflow-margin-outer 4
set reflow-margin-inner 4
set database "sqlite"
set save-view-settings true
set statusbar-show-time false
```

Book font size is in points; margins are logical GTK pixels. Inner margins face
spread centers; outer margins face window edges (both sides in one column).
The central gap is `2 * inner + page-h-padding`. Publisher paragraph styles remain;
custom CSS belongs in `~/.config/zatura/epub.css`. Fixed-layout pages are unaffected.

MuPDF opens reflowable books; Poppler handles PDF; other engines handle DjVu,
PostScript/EPS and image/comic archives. Plugins must match API 8 / ABI 9.
View history is stored per document in `$XDG_DATA_HOME/zatura/bookmarks.sqlite`
(default `~/.local/share/zatura/`). Normal close/exit saves; forced termination cannot.
`database "null"` disables history. Book bookmarks/jumps use content anchors across
layout changes; changed document text or older engines retain numeric fallbacks.
`page-cache-memory` defaults to 256 MiB for cached pixel buffers. Visible pages
may exceed it; it is not a limit on total process or GPU memory. Clock visibility requires `s` in `guioptions`.
