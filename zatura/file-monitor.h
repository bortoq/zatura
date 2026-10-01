/* SPDX-License-Identifier: Zlib */

#ifndef FILEMONITOR_H
#define FILEMONITOR_H

#include <stdbool.h>
#include <girara/types.h>
#include <glib-object.h>

#define ZATURA_TYPE_FILEMONITOR (zatura_filemonitor_get_type())
#define ZATURA_FILEMONITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_FILEMONITOR, ZaturaFileMonitor))
#define ZATURA_FILEMONITOR_CLASS(obj)                                                                                 \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_FILEMONITOR, ZaturaFileMonitorClass))
#define ZATURA_IS_FILEMONITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_FILEMONITOR))
#define ZATURA_IS_FILEMONITOR_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_FILEMONITOR))
#define ZATURA_FILEMONITOR_GET_CLASS(obj)                                                                             \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_FILEMONITOR, ZaturaFileMonitorClass))

typedef struct zatura_filemonitor_s ZaturaFileMonitor;
typedef struct zatura_filemonitor_class_s ZaturaFileMonitorClass;

/**
 * Base class for all file monitors.
 *
 * The signal 'reload-file' is emitted if the monitored file changed.
 */
struct zatura_filemonitor_s {
  GObject parent;
};

struct zatura_filemonitor_class_s {
  GObjectClass parent_class;

  void (*start)(ZaturaFileMonitor*);
  void (*stop)(ZaturaFileMonitor*);
};

/**
 * Get the type of the filemonitor.
 *
 * @return the type
 */
GType zatura_filemonitor_get_type(void);

/**
 * Type of file monitor.
 */
typedef enum zatura_filemonitor_type_e {
  ZATURA_FILEMONITOR_GLIB,   /**< Use filemonitor from GLib */
  ZATURA_FILEMONITOR_SIGNAL, /**< Reload when receiving SIGHUP */
  ZATURA_FILEMONITOR_NOOP    /**< Monitor that does nothing */
} zatura_filemonitor_type_t;

/**
 * Create a new file monitor.
 *
 * @param file_path file to monitor
 * @param filemonitor_type type of file monitor
 * @return new file monitor instance
 */
ZaturaFileMonitor* zatura_filemonitor_new(const char* file_path, zatura_filemonitor_type_t filemonitor_type);

/**
 * Get path of the monitored file.
 *
 * @return path of monitored file
 */
const char* zatura_filemonitor_get_filepath(ZaturaFileMonitor* file_monitor);

/**
 * Start file monitor.
 */
void zatura_filemonitor_start(ZaturaFileMonitor* file_monitor);

/**
 * Stop file monitor.
 */
void zatura_filemonitor_stop(ZaturaFileMonitor* file_monitor);

#endif
