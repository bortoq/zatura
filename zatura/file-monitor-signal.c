/* SPDX-License-Identifier: Zlib */

#include "file-monitor-signal.h"

#include <girara/log.h>
#include <girara/utils.h>
#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

struct zatura_signalfilemonitor_s {
  ZaturaFileMonitor parent;
  gint handle;
};

G_DEFINE_TYPE(ZaturaSignalFileMonitor, zatura_signalfilemonitor, ZATURA_TYPE_FILEMONITOR)

static gboolean signal_handler(gpointer data) {
  if (data == NULL) {
    return TRUE;
  }

  ZaturaSignalFileMonitor* signalfilemonitor = data;

  girara_debug("SIGHUP received");
  g_signal_emit_by_name(signalfilemonitor, "reload-file");

  return TRUE;
}

static void start(ZaturaFileMonitor* file_monitor) {
#ifdef G_OS_UNIX
  ZaturaSignalFileMonitor* signal_file_monitor = ZATURA_SIGNALFILEMONITOR(file_monitor);

  signal_file_monitor->handle = g_unix_signal_add(SIGHUP, signal_handler, signal_file_monitor);
#endif
}

static void stop(ZaturaFileMonitor* file_monitor) {
#ifdef G_OS_UNIX
  ZaturaSignalFileMonitor* signal_file_monitor = ZATURA_SIGNALFILEMONITOR(file_monitor);

  if (signal_file_monitor->handle > 0) {
    g_source_remove(signal_file_monitor->handle);
    signal_file_monitor->handle = 0;
  }
#endif
}

static void zatura_signalfilemonitor_finalize(GObject* object) {
  stop(ZATURA_FILEMONITOR(object));

  G_OBJECT_CLASS(zatura_signalfilemonitor_parent_class)->finalize(object);
}

static void zatura_signalfilemonitor_class_init(ZaturaSignalFileMonitorClass* class) {
  ZaturaFileMonitorClass* filemonitor_class = ZATURA_FILEMONITOR_CLASS(class);
  filemonitor_class->start                   = start;
  filemonitor_class->stop                    = stop;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = zatura_signalfilemonitor_finalize;
}

static void zatura_signalfilemonitor_init(ZaturaSignalFileMonitor* signalfilemonitor) {
  signalfilemonitor->handle = 0;
}
