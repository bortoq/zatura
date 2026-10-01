/* SPDX-License-Identifier: Zlib */

#include "file-monitor.h"

#include <girara/log.h>
#include <girara/utils.h>

#include "file-monitor-glib.h"
#ifdef G_OS_UNIX
#include "file-monitor-signal.h"
#endif
#include "file-monitor-noop.h"
#include "macros.h"

typedef struct {
  char* file_path;
} ZaturaFileMonitorPrivate;

G_DEFINE_TYPE_WITH_CODE(ZaturaFileMonitor, zatura_filemonitor, G_TYPE_OBJECT, G_ADD_PRIVATE(ZaturaFileMonitor))

enum {
  PROP_0,
  PROP_FILE_PATH,
};

static void finalize(GObject* object) {
  ZaturaFileMonitor* file_monitor = ZATURA_FILEMONITOR(object);
  ZaturaFileMonitorPrivate* priv  = zatura_filemonitor_get_instance_private(file_monitor);

  if (priv->file_path != NULL) {
    g_free(priv->file_path);
  }

  G_OBJECT_CLASS(zatura_filemonitor_parent_class)->finalize(object);
}

static void set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  ZaturaFileMonitor* file_monitor = ZATURA_FILEMONITOR(object);
  ZaturaFileMonitorPrivate* priv  = zatura_filemonitor_get_instance_private(file_monitor);

  switch (prop_id) {
  case PROP_FILE_PATH:
    if (priv->file_path != NULL) {
      g_free(priv->file_path);
    }
    priv->file_path = g_value_dup_string(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  ZaturaFileMonitor* file_monitor = ZATURA_FILEMONITOR(object);
  ZaturaFileMonitorPrivate* priv  = zatura_filemonitor_get_instance_private(file_monitor);

  switch (prop_id) {
  case PROP_FILE_PATH:
    g_value_set_string(value, priv->file_path);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void zatura_filemonitor_class_init(ZaturaFileMonitorClass* class) {
  /* set up methods */
  class->start = NULL;
  class->stop  = NULL;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->finalize     = finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  /* add properties */
  g_object_class_install_property(
      object_class, PROP_FILE_PATH,
      g_param_spec_string("file-path", "file-path", "file path to monitor", NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /* add signals */
  g_signal_new("reload-file", ZATURA_TYPE_FILEMONITOR, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_generic,
               G_TYPE_NONE, 0);
}

static void zatura_filemonitor_init(ZaturaFileMonitor* file_monitor) {
  ZaturaFileMonitorPrivate* priv = zatura_filemonitor_get_instance_private(file_monitor);
  priv->file_path                 = NULL;
}

const char* zatura_filemonitor_get_filepath(ZaturaFileMonitor* file_monitor) {
  ZaturaFileMonitorPrivate* priv = zatura_filemonitor_get_instance_private(file_monitor);
  return priv->file_path;
}

void zatura_filemonitor_start(ZaturaFileMonitor* file_monitor) {
  ZATURA_FILEMONITOR_GET_CLASS(file_monitor)->start(file_monitor);
}

void zatura_filemonitor_stop(ZaturaFileMonitor* file_monitor) {
  ZATURA_FILEMONITOR_GET_CLASS(file_monitor)->stop(file_monitor);
}

ZaturaFileMonitor* zatura_filemonitor_new(const char* file_path, zatura_filemonitor_type_t filemonitor_type) {
  g_return_val_if_fail(file_path != NULL, NULL);

  GObject* ret = NULL;
  switch (filemonitor_type) {
  case ZATURA_FILEMONITOR_GLIB:
    girara_debug("using glib file monitor");
    ret = g_object_new(ZATURA_TYPE_GLIBFILEMONITOR, "file-path", file_path, NULL);
    break;
#ifdef G_OS_UNIX
  case ZATURA_FILEMONITOR_SIGNAL:
    girara_debug("using SIGHUP file monitor");
    ret = g_object_new(ZATURA_TYPE_SIGNALFILEMONITOR, "file-path", file_path, NULL);
    break;
#endif
  case ZATURA_FILEMONITOR_NOOP:
    girara_debug("using noop file monitor");
    ret = g_object_new(ZATURA_TYPE_NOOPFILEMONITOR, "file-path", file_path, NULL);
    break;
  default:
    girara_debug("invalid filemonitor type: %d", filemonitor_type);
    g_return_val_if_fail(false, NULL);
  }

  if (ret == NULL) {
    return NULL;
  }

  return ZATURA_FILEMONITOR(ret);
}
