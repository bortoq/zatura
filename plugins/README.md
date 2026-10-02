# Vendored plugins

Sources from https://github.com/pwmt, version 2026.07.18.

| Directory | Upstream commit |
| --- | --- |
| cb | `9a9cc46a68aed0fa73d327663b2e5ffd22d8adb4` |
| djvu | `05ae75471008dd716df23dadf57ceaed636790fb` |
| pdf-mupdf | `ee8a05754c91796811743d44de6936dd1d6d4495` |
| ps | `dda8e707c8abef089c927a72a07cc2c45e799573` |

Local MuPDF changes: direct FB2 ZIP member reading; optional v1/v2 reflow extensions with content bookmarks and mirrored page margins; FB2 XML and XPS MIME aliases; prefer zatura/epub.css with legacy fallback. Plugin desktop entries launch zatura.
PDF support is disabled for MuPDF to retain the existing Poppler backend.
