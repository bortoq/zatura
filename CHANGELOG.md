# Changelog

## 2026.10.03.2

- Native Debian/Arch core packages with system dependencies instead of private runtime copies.
- GNOME Platform 49 Flatpak and linuxdeploy AppImage packaging with enforced size budgets.
- Girara utility core linked statically in native distribution builds; engines remain separate plugins.

## 2026.10.03.1

- Stable serialized content anchors for book bookmarks, quickmarks and jump history; SQLite schema 7 migration.
- Shared raw/processed/thumbnail cache budget (`page-cache-memory`, default 256 MiB); visible buffers stay pinned.
- Cache/filter profiling tool and recorded scanned-page/HiDPI measurements.
- deb, Arch/AUR and Flatpak packaging targets, standalone container build and CI.
- Minimal documentation and compact generated configuration defaults.

## 2026.10.02 — first Zatura release

Based on Zathura 2026.07.18; plugin API 8 / ABI 9 remain compatible.

- Layout-independent physical shortcuts and unified normal/fullscreen bindings.
- Responsive contrast, brightness, gamma and saturation controls on keys 1–8.
- EPUB/FB2/FB2 ZIP support, viewport-fitted spreads, font-size controls and mirrored margins.
- Additional PDF, DjVu, PostScript, image and comic archive backends.
- Per-document viewing history and a togglable HH:MM statusbar clock (`t`).
- Recolor pixel transforms moved into `page-effects`, shared by rendering paths.
- Quieter selection defaults and silent bounds clamping for repeated adjustments.
- Reading guide and generated configuration defaults.
- Makefile entry points, portable x86_64 musl archive and AppImage packaging.

Known limits: content anchors for saved bookmarks/jump history are not implemented;
cache sizes do not represent a total memory limit; Flatpak/AUR/deb packages and
cross-distribution/Wayland coverage remain follow-up work. Publisher CSS can
retain paragraph spacing or fixed font sizes. See `doc/REFACTORING.md`.
