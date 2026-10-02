#!/bin/sh
set -eu
meson setup --reconfigure /tmp/zatura-pixbuf-build "${1:?pass the GdkPixbuf 2.44.7 source directory}" --prefix "${2:?pass the absolute runtime prefix}" --libdir lib -Dbuildtype=release -Dglycin=disabled -Dbuiltin_loaders=all -Dothers=enabled -Dpng=enabled -Djpeg=enabled -Dtiff=enabled -Dgif=enabled -Dtests=false -Dinstalled_tests=false -Dintrospection=disabled -Dman=false -Dthumbnailer=disabled
meson compile -C /tmp/zatura-pixbuf-build -j4
meson install -C /tmp/zatura-pixbuf-build
