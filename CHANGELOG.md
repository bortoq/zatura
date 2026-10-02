# Changelog

## 2026.10.02 — first Zatura release

Based on Zathura 2026.07.18; plugin API 8 / ABI 9 remain compatible.

- Layout-independent physical shortcuts and unified normal/fullscreen bindings.
- Responsive contrast, brightness, gamma and saturation controls on keys 1–8.
- EPUB/FB2/FB2 ZIP support, viewport-fitted spreads, font-size controls and mirrored margins.
- Additional PDF, DjVu, PostScript, image and comic archive backends.
- Per-document viewing history and a togglable HH:MM statusbar clock (`t`).
- Recolor pixel transforms moved into `page-effects`, shared by rendering paths.
- Quieter selection defaults and silent bounds clamping for repeated adjustments.
- Complete English and Russian reading guides and annotated configuration.
- Makefile entry points, portable x86_64 musl archive and AppImage packaging.

Known limits: content anchors for saved bookmarks/jump history are not implemented;
cache sizes do not represent a total memory limit; Flatpak/AUR/deb packages and
cross-distribution/Wayland coverage remain follow-up work. Publisher CSS can
retain paragraph spacing or fixed font sizes. See `doc/REFACTORING.md`.
