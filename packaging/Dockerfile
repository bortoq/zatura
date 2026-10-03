FROM alpine:3.24.1
RUN apk add --no-cache build-base meson ninja pkgconf gettext-dev glib-dev gtk4.0-dev json-glib-dev file-dev sqlite-dev xxhash-dev libxkbcommon-dev cairo-dev poppler-dev mupdf-dev djvulibre-dev libspectre-dev libarchive-dev shared-mime-info ghostscript xvfb-run xvfb dbus font-dejavu curl git ca-certificates gdk-pixbuf-dev libpng-dev libjpeg-turbo-dev tiff-dev giflib-dev
WORKDIR /src
COPY . /src
ENTRYPOINT ["/src/tools/build-bundle-runtime.sh"]
