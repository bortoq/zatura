# SPDX-License-Identifier: Zlib
# Convenience entry points; Meson remains the build system.
BUILDDIR ?= build
PREFIX ?= /usr/local
JOBS ?= 4
MESON ?= meson
PYTHON ?= python3
MESON_ARGS ?=
RUNTIME ?= $(BUILDDIR)/runtime
DISTDIR ?= dist
APPIMAGETOOL ?= appimagetool
SOURCE_ARGS ?=
APPIMAGE_ARGS ?=

.PHONY: all configure build test install plugins portable appimage source deb arch flatpak profile clean help
all: build
configure:
	@if test -f "$(BUILDDIR)/meson-private/coredata.dat"; then \
		$(MESON) setup --reconfigure "$(BUILDDIR)" $(MESON_ARGS); \
	else $(MESON) setup "$(BUILDDIR)" --prefix="$(PREFIX)" $(MESON_ARGS); fi
build:
	@if ! test -f "$(BUILDDIR)/meson-private/coredata.dat"; then $(MAKE) configure; fi
	$(MESON) compile -C "$(BUILDDIR)" -j "$(JOBS)"
test: build
	$(MESON) test -C "$(BUILDDIR)" --print-errorlogs
install: build
	$(MESON) install -C "$(BUILDDIR)"
plugins:
	./tools/build-plugins.sh "$(abspath $(RUNTIME))"
portable:
	$(PYTHON) tools/package-runtime.py --runtime "$(RUNTIME)" --output "$(DISTDIR)"
appimage: portable
	ARCH=x86_64 $(APPIMAGETOOL) --no-appstream $(APPIMAGE_ARGS) "$(DISTDIR)/Zatura.AppDir" "$(DISTDIR)/Zatura-$$(cat $(DISTDIR)/Zatura.AppDir/VERSION)-x86_64.AppImage"
source:
	$(PYTHON) tools/package-source.py --output "$(DISTDIR)" $(SOURCE_ARGS)
deb arch flatpak: portable
	$(PYTHON) tools/package-linux.py $@ --appdir "$(DISTDIR)/Zatura.AppDir" --output "$(DISTDIR)"
profile: build
	./tools/profile-render.sh "$(BUILDDIR)/tests/benchmark_render" "$(DISTDIR)/render-profile.csv"
clean:
	@if test -f "$(BUILDDIR)/meson-private/coredata.dat"; then $(MESON) compile -C "$(BUILDDIR)" --clean; fi
help:
	@printf '%s\n' 'make build / test / install       Meson build, tests and install' \
	 'make configure PREFIX=... MESON_ARGS=...  Set up a native build' \
	 'make plugins RUNTIME=...         Build bundled plugins after install' \
	 'make portable RUNTIME=...        Pack a complete musl runtime' \
	 'make appimage APPIMAGETOOL=...    Pack AppImage from the same runtime' \
	 'make deb / arch / flatpak        Package the installed private runtime' \
	 'make profile                    Measure scanned-page cache/filter costs' \
	 'make source SOURCE_ARGS=...      Archive committed sources and optional dependencies' \
	 'DESTDIR=... make install         Stage files for distribution packages'
