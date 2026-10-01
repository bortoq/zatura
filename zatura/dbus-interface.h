/* SPDX-License-Identifier: Zlib */

#ifndef DBUS_INTERFACE_H
#define DBUS_INTERFACE_H

#include <stdbool.h>
#include <girara/types.h>
#include <glib-object.h>
#include <sys/types.h>
#include "types.h"

typedef struct zatura_dbus_class_s ZaturaDbusClass;

struct zatura_dbus_s {
  GObject parent;
};

struct zatura_dbus_class_s {
  GObjectClass parent_class;
};

#define ZATURA_TYPE_DBUS (zatura_dbus_get_type())
#define ZATURA_DBUS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_DBUS, ZaturaDbus))
#define ZATURA_DBUS_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_DBUS, ZaturaDbus))
#define ZATURA_IS_DBUS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_DBUS))
#define ZATURA_IS_DBUS_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_DBUS))
#define ZATURA_DBUS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_DBUS, ZaturaDbusClass))

GType zatura_dbus_get_type(void);

ZaturaDbus* zatura_dbus_new(zatura_t* zatura);
const char* zatura_dbus_get_name(zatura_t* zatura);

/**
 * Emit the 'DocumentOpen' signal on the D-Bus connection.
 *
 * @param zatura Zatura session
 * @param file_path document path
 */
void zatura_dbus_document_open(zatura_t* zatura, const char* file_path);

/**
 * Emit the 'DocumentClose' signal on the D-Bus connection.
 *
 * @param zatura Zatura session
 * @param file_path document path
 */
void zatura_dbus_document_close(zatura_t* zatura, const char* file_path);

/**
 * Emit the 'Edit' signal on the D-Bus connection.
 *
 * @param zatura Zatura session
 * @param page page
 * @param x x coordinate
 * @param y y coordinate
 */
void zatura_dbus_edit(zatura_t* zatura, unsigned int page, unsigned int x, unsigned int y);

/**
 * Highlight rectangles in a zatura instance that has filename open.
 * input_file, line and column determine the rectangles to display and are
 * passed to SyncTeX.
 *
 * @param filename path of the document
 * @param input_file path of the input file
 * @param line line index (starts at 0)
 * @param column column index (starts at 0)
 * @param hint zatura process ID that has filename open
 */
int zatura_dbus_synctex_position(const char* filename, const char* input_file, int line, int column, pid_t hint);

#endif
