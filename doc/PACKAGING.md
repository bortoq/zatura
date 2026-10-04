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
separate plugins, not embedded in the native core packages. Old distro Zathura
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
