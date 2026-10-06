# Packaging

Zatura uses GTK4 (>=4.12) and GLib >=2.84. Build native glibc packages in the
target distribution; the old musl private runtime is not a package input.
Girara's small utility core is linked statically; GTK/GLib remain shared.

```sh
./tools/build-native.sh
make deb NATIVE_STAGE=build/native-stage
make arch NATIVE_STAGE=build/native-stage  # build on Arch first
make appimage NATIVE_STAGE=build/native-stage LINUXDEPLOY=/path/to/linuxdeploy
make flatpak  # flatpak-builder, org.gnome.Sdk and Platform 49 from Flathub
```

If host binfmt intercepts Meson `.exe` probes on x86_64, set
`MESON_NATIVE_ARGS="--cross-file=packaging/glibc-x86_64.ini"` to use the ELF
loader explicitly. This keeps the native ABI.

For Debian use `packaging/Dockerfile.native` with the repository mounted at
`/src`. Native packages include the executable, desktop/icon, manuals and
application metadata/license notices. Debian dependencies come from
`dpkg-shlibdeps`; Arch uses `depends`. Document engines (API 8, ABI 9) are
built against the staged SDK; Poppler PDF ships in every package. Optional
engines can be built with `PLUGINS` (see README). Old distro Zathura
plugins with another ABI cannot be used.

Flatpak builds against `org.gnome.Platform`; `/app` contains only application
files and dependencies absent from that platform. No private runtime is copied.
AppImage uses linuxdeploy's ELF dependency collection and explicit GTK4 resources.
Drivers and optional media/TLS modules stay on the host. SQLite and libmagic
are direct core dependencies and must remain available; libmagic also needs its
identification database. The recent glibc loader/libc/libm are included in
AppImage so a recent build can start on older glibc hosts. Size limits: native packages 1 MB, Flatpak 15 MB,
AppImage 40 MB (decimal bytes, excludes the shared Flatpak platform).

`make portable` archives native application files without dependencies.
`make source` archives committed source; use `SOURCE_ARGS` to include dependency
sources. Dependency notices: [SOURCES.md](SOURCES.md).

`make flatpak-native` can export a native staged core without installing an SDK;
it first checks that the binary runs against GNOME Platform 49. The source
manifest is preferred for distribution builds. Both paths share that platform.

`make clean` removes the entire generated build/distribution trees and Python
bytecode caches. It rejects targets containing tracked source files. Install the
program outside the build tree before cleaning. `~/bin/zz` uses the permanent
installation at `~/.local/lib/zatura` on this machine.
Run native build containers with `--user "$(id -u):$(id -g)" --env HOME=/tmp`
to keep generated files writable for cleanup by your user.

Flatpak has no blanket home-directory permission. Open documents with the GTK
file chooser (`Alt+o`) or the desktop file association, which grant portal access
to the selected file. Application settings use its private XDG directories.

Native build inputs use a fixed Debian image digest and APT snapshot. Package
timestamps and ownership are normalized; `SOURCE_DATE_EPOCH` defaults to the
source commit timestamp. CI records it explicitly.

Packaging requires `xvfb-run` and rejects an artifact whose installed viewer
cannot open and finish rendering a PDF. Native packages are extracted before
this check; AppImage is executed with extraction mode, and Flatpak checks its
built installation before export.

When exporting native Flatpak on a different distribution, supply the linked
libraries from the build distribution, for example:

```sh
python3 tools/package-flatpak.py --stage build/native-stage \
  --library-dir dist/Zatura.AppDir/usr/lib
```

Only libraries missing from GNOME Platform are copied. The directory should
come from the matching AppImage build; its license notices are copied too.
Flatpak checks can also use an existing headless display when `xvfb-run` is
unavailable.

The native script deliberately disables seccomp and Landlock. Sandbox behavior
and its descriptor/kernel requirements are described in [SANDBOX.md](SANDBOX.md).
Before publishing new artifacts, bump the project version/tag; do not reuse the
identifiers of an older public release.
