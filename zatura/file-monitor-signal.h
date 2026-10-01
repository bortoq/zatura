/* SPDX-License-Identifier: Zlib */

#ifndef FILEMONITOR_SIGNAL_H
#define FILEMONITOR_SIGNAL_H

#include "file-monitor.h"

#define ZATURA_TYPE_SIGNALFILEMONITOR (zatura_signalfilemonitor_get_type())
#define ZATURA_SIGNALFILEMONITOR(obj)                                                                                 \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_SIGNALFILEMONITOR, ZaturaSignalFileMonitor))
#define ZATURA_SIGNALFILEMONITOR_CLASS(obj)                                                                           \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATURA_TYPE_SIGNALFILEMONITOR, ZaturaSignalFileMonitorClass))
#define ZATURA_IS_SIGNALFILEMONITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_SIGNALFILEMONITOR))
#define ZATURA_IS_SIGNALFILEMONITOR_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATURA_TYPE_SIGNALFILEMONITOR))
#define ZATURA_SIGNALFILEMONITOR_GET_CLASS(obj)                                                                       \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_SIGNALFILEMONITOR, ZaturaSignalFileMonitorClass))

typedef struct zatura_signalfilemonitor_s ZaturaSignalFileMonitor;
typedef struct zatura_signalfilemonitor_class_s ZaturaSignalFileMonitorClass;

struct zatura_signalfilemonitor_class_s {
  ZaturaFileMonitorClass parent_class;
};

GType zatura_signalfilemonitor_get_type(void);

#endif
