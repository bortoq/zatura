# Audit fixes, 2026-10-06

This change addresses the findings in `zatura-audit-report-2026-10-06.md`.
Existing, uncommitted GPU rendering changes are preserved.

| Finding | Change |
| --- | --- |
| P1: seccomp read-only bypass | Shared production rules reject `O_CREAT`, `O_TRUNC`, `O_APPEND`, the unique `O_TMPFILE` bit and writable access modes for both `open` and `openat`. Direct syscall regressions run without Landlock or a display and check file contents and absence of a newly created file. |
| P1: core-only releases | Native packages and AppImage require the pinned API 8 / ABI 9 Poppler PDF engine. Both Flatpak build paths include it. Package checks require the actual installed viewer to complete a PDF render before accepting output. |
| P2: plugin build / multiarch | `make plugins` discovers installed SDK metadata under `lib*`, relocates staged SDK metadata without rewriting system dependencies, builds Poppler by default, and enables MuPDF PDF support when Poppler is not selected. Additional engines are selected through `PLUGINS`. |
| P2: integration coverage | A separate CI job uses MuPDF >=1.26 and Poppler and rejects missing/skipped reflow, anchors, history, PDF and cache tests. It builds the sandbox and runs syscall, X11 and Wayland checks. Native CI additionally builds Landlock and checks mandatory startup behavior. |
| P2: Meson minimum | Raised to 1.6 in project metadata and README. |
| P2: Flatpak home permissions | Removed blanket home access from both source manifest and native exporter. The existing GTK file dialog uses the portal; private XDG directories remain available. PDF smoke grants access only to a temporary fixture directory during the check. |
| P3: reproducibility | Native Debian base image is pinned by digest and APT uses a dated snapshot. CI sets `SOURCE_DATE_EPOCH`; native packaging defaults to the source commit timestamp, normalizes file times and tar ownership, and produces timestamp-stable AUR gzip metadata. |
| P3: Cairo cleanup | The error path destroys the newly created Cairo context before returning. |

A build configured with Landlock now refuses strict-sandbox startup when the
kernel cannot provide ABI 6. Explicit seccomp-only builds remain available;
the syscall regressions verify their file-opening restrictions independently.
X11 still provides incomplete process isolation, as the existing warning states.

PDF plugins add their own distribution dependencies and license notices.
The PDF-enabled native packages remain within the existing 1 MB size budget.
No release upload, repository commit or installed application replacement is
part of these source changes.

## Validation

- Full Poppler/MuPDF integration run: 18 suites passed, no failures or skips,
  including reflow, anchors, history, PDF effects, cache and X11 sandbox.
- Installed viewer PDF smoke: a real PDF opened and finished rendering.
- Python, shell and Flatpak JSON syntax checks passed; `git diff --check` passed.
- Full final engine run: **22 suites passed**, no failures or skips, including
  the four Weston/Wayland suites. Logs: `build/engines/meson-logs/testlog.txt`.
- Native seccomp + Landlock regressions: **2 passed**, no skips. The host kernel
  reports Landlock ABI 3, and the actual sandbox refuses startup with a nonzero
  exit status. Logs: `build/native-sandbox/meson-logs/testlog.txt`.
- Native Debian build: 13 passed, 3 expected reflow-dependent skips; these three
  suites pass in the separate full-engine job.
- Debian multiarch SDK (`lib/x86_64-linux-gnu/pkgconfig`): plugin compilation,
  installation and PDF rendering passed.
- PDF render checks passed for extracted Debian and Arch packages, executable
  AppImage and native Flatpak against GNOME Platform 49. Artifacts in `dist/`:
  Debian 218,892 bytes; Arch 233,779 bytes; AppImage 28,121,592 bytes;
  Flatpak 2,670,040 bytes.
- Repeating Debian, Arch and AUR packaging produced byte-identical SHA256 hashes.
- The source Flatpak manifest includes checksum-pinned Poppler and OpenJPEG
  because GNOME Platform does not provide the PDF runtime. Its archives and
  configuration options were checked; this source build was not run because
  the local installation lacks GNOME SDK and flatpak-builder. The native
  Flatpak export and render check above were run successfully.
- Interactive file selection through the desktop portal was not automated.
  File access during the package render test was limited to its temporary
  fixture directory.
