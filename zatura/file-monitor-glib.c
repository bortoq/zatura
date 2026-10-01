/* SPDX-License-Identifier: Zlib */

#include "file-monitor-glib.h"

#include <girara/utils.h>
#include <girara/log.h>
#include <gio/gio.h>

#include "macros.h"

struct zatura_glibfilemonitor_s {
  ZaturaFileMonitor parent;
  GFileMonitor* monitor; /**< File monitor */
  GFile* file;           /**< File for file monitor */
};

G_DEFINE_TYPE(ZaturaGLibFileMonitor, zatura_glibfilemonitor, ZATURA_TYPE_FILEMONITOR)

static void file_changed(GFileMonitor* UNUSED(monitor), GFile* file, GFile* UNUSED(other_file), GFileMonitorEvent event,
                         gpointer user_data) {
  ZaturaGLibFileMonitor* file_monitor = user_data;

  switch (event) {
  case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
  case G_FILE_MONITOR_EVENT_CREATED: {
    g_autofree char* uri = g_file_get_uri(file);
    girara_debug("received file-monitor event for %s", uri);

    g_signal_emit_by_name(file_monitor, "reload-file");
    break;
  }
  default:
    return;
  }
}

static void start(ZaturaFileMonitor* file_monitor) {
  ZaturaGLibFileMonitor* glib_file_monitor = ZATURA_GLIBFILEMONITOR(file_monitor);

  const char* file_path = zatura_filemonitor_get_filepath(file_monitor);

  /* install file monitor */
  glib_file_monitor->file = g_file_new_for_path(file_path);
  if (glib_file_monitor->file == NULL) {
    return;
  }

  glib_file_monitor->monitor =
      g_file_monitor_file(glib_file_monitor->file, G_FILE_MONITOR_WATCH_HARD_LINKS, NULL, NULL);
  if (glib_file_monitor->monitor != NULL) {
    g_signal_connect_object(G_OBJECT(glib_file_monitor->monitor), "changed", G_CALLBACK(file_changed),
                            glib_file_monitor, 0);
  }
}

static void stop(ZaturaFileMonitor* file_monitor) {
  ZaturaGLibFileMonitor* glib_file_monitor = ZATURA_GLIBFILEMONITOR(file_monitor);

  if (glib_file_monitor->monitor != NULL) {
    g_file_monitor_cancel(glib_file_monitor->monitor);
  }

  g_clear_object(&glib_file_monitor->monitor);
  g_clear_object(&glib_file_monitor->file);
}

static void dispose(GObject* object) {
  stop(ZATURA_FILEMONITOR(object));

  G_OBJECT_CLASS(zatura_glibfilemonitor_parent_class)->dispose(object);
}

static void finalize(GObject* object) {
  G_OBJECT_CLASS(zatura_glibfilemonitor_parent_class)->finalize(object);
}

static void zatura_glibfilemonitor_class_init(ZaturaGLibFileMonitorClass* class) {
  ZaturaFileMonitorClass* filemonitor_class = ZATURA_FILEMONITOR_CLASS(class);
  filemonitor_class->start                   = start;
  filemonitor_class->stop                    = stop;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->dispose      = dispose;
  object_class->finalize     = finalize;
}

static void zatura_glibfilemonitor_init(ZaturaGLibFileMonitor* glibfilemonitor) {
  glibfilemonitor->monitor = NULL;
  glibfilemonitor->file    = NULL;
}
