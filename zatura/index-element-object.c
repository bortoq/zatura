/* SPDX-License-Identifier: Zlib */

#include "index-element-object.h"

G_DEFINE_TYPE(ZaturaIndexElementObject, zatura_index_element_object, G_TYPE_OBJECT)

static void zatura_index_element_object_dispose(GObject* object) {
  ZaturaIndexElementObject* self = ZATURA_INDEX_ELEMENT_OBJECT(object);
  g_clear_object(&self->children);
  G_OBJECT_CLASS(zatura_index_element_object_parent_class)->dispose(object);
}

static void zatura_index_element_object_finalize(GObject* object) {
  ZaturaIndexElementObject* self = ZATURA_INDEX_ELEMENT_OBJECT(object);
  g_clear_pointer(&self->title, g_free);
  g_clear_pointer(&self->page_label, g_free);
  g_clear_pointer(&self->page_alt, g_free);
  if (self->element != NULL) {
    zatura_index_element_free(self->element);
    self->element = NULL;
  }
  G_OBJECT_CLASS(zatura_index_element_object_parent_class)->finalize(object);
}

static void zatura_index_element_object_class_init(ZaturaIndexElementObjectClass* klass) {
  GObjectClass* object_class = G_OBJECT_CLASS(klass);
  object_class->dispose      = zatura_index_element_object_dispose;
  object_class->finalize     = zatura_index_element_object_finalize;
}

static void zatura_index_element_object_init(ZaturaIndexElementObject* UNUSED(self)) {}
