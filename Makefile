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
LINUXDEPLOY ?= linuxdeploy
NATIVE_STAGE ?= build/native-stage
FLATPAK_BUILDER ?= flatpak-builder
SOURCE_ARGS ?=
PACKAGE_VERSION := $(shell sed -n "s/.*version: '\([^']*\)'.*/\1/p" meson.build | head -1)

.PHONY: all configure build test install plugins portable appimage source deb arch flatpak flatpak-native profile clean help
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
	$(PYTHON) tools/package-native-portable.py --stage "$(NATIVE_STAGE)" --output "$(DISTDIR)"
appimage:
	$(PYTHON) tools/package-appimage.py --stage "$(NATIVE_STAGE)" --output "$(DISTDIR)" --linuxdeploy "$(LINUXDEPLOY)"
source:
	$(PYTHON) tools/package-source.py --output "$(DISTDIR)" $(SOURCE_ARGS)
deb arch:
	$(PYTHON) tools/package-linux.py $@ --stage "$(NATIVE_STAGE)" --output "$(DISTDIR)"
flatpak:
	$(FLATPAK_BUILDER) --force-clean --repo="$(DISTDIR)/flatpak-repo" "$(BUILDDIR)/flatpak" packaging/io.github.bortoq.zatura.json
	xvfb-run -a $(PYTHON) tools/check-viewer-pdf.py --flatpak-build "$(BUILDDIR)/flatpak"
	flatpak build-bundle "$(DISTDIR)/flatpak-repo" "$(DISTDIR)/zatura-$(PACKAGE_VERSION)-x86_64.flatpak" io.github.bortoq.zatura --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo
	$(PYTHON) tools/check-package-size.py "$(DISTDIR)/zatura-$(PACKAGE_VERSION)-x86_64.flatpak" 15000000
flatpak-native:
	$(PYTHON) tools/package-flatpak.py --stage "$(NATIVE_STAGE)" --output "$(DISTDIR)"
profile: build
	./tools/profile-render.sh "$(BUILDDIR)/tests/benchmark_render" "$(DISTDIR)/render-profile.csv"
clean:
	$(PYTHON) tools/clean-generated.py --builddir "$(BUILDDIR)" --distdir "$(DISTDIR)"
help:
	@printf '%s\n' 'make build / test / install       Meson build, tests and install' \
	 'make configure PREFIX=... MESON_ARGS=...  Set up a native build' \
	 'make plugins RUNTIME=...         Build bundled plugins after install' \
	 'make portable NATIVE_STAGE=...        Pack a native install without libraries' \
	 'make appimage LINUXDEPLOY=...     Bundle linked libraries with linuxdeploy' \
	 'make deb / arch / flatpak        Package native install / GNOME Platform build' \
	 'make clean                      Remove build and distribution output trees' \
	 'make profile                    Measure scanned-page cache/filter costs' \
	 'make source SOURCE_ARGS=...      Archive committed sources and optional dependencies' \
	 'DESTDIR=... make install         Stage files for distribution packages'
