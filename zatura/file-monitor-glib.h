/* SPDX-License-Identifier: Zlib */

#ifndef FILEMONITOR_GLIB_H
#define FILEMONITOR_GLIB_H

#include "file-monitor.h"

#define ZATURA_TYPE_GLIBFILEMONITOR (zatura_glibfilemonitor_get_type())
#define ZATURA_GLIBFILEMONITOR(obj)                                                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_GLIBFILEMONITOR, ZaturaGLibFileMonitor))
#define ZATURA_GLIBFILEMONITOR_CLASS(obj)                                                                             \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_GLIBFILEMONITOR, ZaturaGLibFileMonitorClass))
#define ZATURA_IS_GLIBFILEMONITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_GLIBFILEMONITOR))
#define ZATURA_IS_GLIBFILEMONITOR_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_GLIBFILEMONITOR))
#define ZATURA_GLIBFILEMONITOR_GET_CLASS(obj)                                                                         \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_GLIBFILEMONITOR, ZaturaGLibFileMonitorClass))

typedef struct zatura_glibfilemonitor_s ZaturaGLibFileMonitor;
typedef struct zatura_glibfilemonitor_class_s ZaturaGLibFileMonitorClass;

struct zatura_glibfilemonitor_class_s {
  ZaturaFileMonitorClass parent_class;
};

GType zatura_glibfilemonitor_get_type(void);

#endif
