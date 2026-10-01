/* SPDX-License-Identifier: Zlib */

#ifndef FILEMONITOR_NOOP_H
#define FILEMONITOR_NOOP_H

#include "file-monitor.h"

#define ZATURA_TYPE_NOOPFILEMONITOR (zatura_noopfilemonitor_get_type())
#define ZATURA_NOOPFILEMONITOR(obj)                                                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_NOOPFILEMONITOR, ZaturaNoopFileMonitor))
#define ZATURA_NOOPFILEMONITOR_CLASS(obj)                                                                             \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_NOOPFILEMONITOR, ZaturaNoopFileMonitorClass))
#define ZATURA_IS_NOOPFILEMONITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_NOOPFILEMONITOR))
#define ZATURA_IS_NOOPFILEMONITOR_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_NOOPFILEMONITOR))
#define ZATURA_NOOPFILEMONITOR_GET_CLASS(obj)                                                                         \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_NOOPFILEMONITOR, ZaturaNoopFileMonitorClass))

typedef struct zatura_noopfilemonitor_s ZaturaNoopFileMonitor;
typedef struct zatura_noopfilemonitor_class_s ZaturaNoopFileMonitorClass;

struct zatura_noopfilemonitor_class_s {
  ZaturaFileMonitorClass parent_class;
};

GType zatura_noopfilemonitor_get_type(void);

#endif
