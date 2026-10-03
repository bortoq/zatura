# Build and packaging

See [README](../README.md) for native requirements and build commands.
`make help` lists targets. Existing builds keep their prefix unless reconfigured
with `MESON_ARGS='--prefix=/path'`. Build engines after installing the core.

Portable archive: extract, then run `zatura-VERSION/AppRun document.pdf`.
AppImage: make executable and run; without FUSE use `--appimage-extract-and-run`
or `--appimage-extract` followed by `squashfs-root/AppRun`.

```sh
make portable RUNTIME=build/runtime
make deb
make arch
make flatpak
make profile
APPIMAGE_EXTRACT_AND_RUN=1 make appimage APPIMAGETOOL=/path/to/appimagetool
make source SOURCE_ARGS='--extra-source girara=subprojects/girara'
```

The bundle packer requires a complete installed x86_64 musl runtime, including
engines, schemas, image loaders, MIME/libmagic data and Ghostscript resources.
It replaces `dist/Zatura.AppDir`; it does not assemble a native glibc build's dependencies.
Pin AppImage tool/runtime versions for releases. `make source` uses committed files;
add external PDF/Poppler and GdkPixbuf trees with `--extra-source NAME=PATH`.
Verify downloads with SHA256SUMS. Bundles use dynamic libraries and host fonts/display.
Dependency sources and notices: [SOURCES.md](SOURCES.md).

`make arch` emits the package and a checksummed PKGBUILD for AUR submission;
no AUR upload is performed. `make flatpak` needs the user-installed
`org.freedesktop.Platform//25.08`; install it from Flathub, then install the
produced bundle with `flatpak install --user dist/zatura-VERSION-x86_64.flatpak`.
The deb/Arch packages keep the private runtime in `/opt/zatura`.

For a clean container build: `docker build -t zatura-builder -f packaging/Dockerfile .`,
then `docker run --rm -v "$PWD/build-ci:/work" zatura-builder`.
Package `build-ci/build/runtime`. Base distribution and source revisions are fixed;
APK versions are recorded in BUILD-MANIFEST.json (distribution updates can change them).
