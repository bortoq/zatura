/* SPDX-License-Identifier: Zlib */

#include "file-monitor-noop.h"

#include <girara/macros.h>

struct zatura_noopfilemonitor_s {
  ZaturaFileMonitor parent;
};

G_DEFINE_TYPE(ZaturaNoopFileMonitor, zatura_noopfilemonitor, ZATURA_TYPE_FILEMONITOR)

static void start(ZaturaFileMonitor* GIRARA_UNUSED(file_monitor)) {}

static void stop(ZaturaFileMonitor* GIRARA_UNUSED(file_monitor)) {}

static void zatura_noopfilemonitor_class_init(ZaturaNoopFileMonitorClass* class) {
  ZaturaFileMonitorClass* filemonitor_class = ZATURA_FILEMONITOR_CLASS(class);
  filemonitor_class->start                   = start;
  filemonitor_class->stop                    = stop;
}

static void zatura_noopfilemonitor_init(ZaturaNoopFileMonitor* GIRARA_UNUSED(noopfilemonitor)) {}
