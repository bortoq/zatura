/* SPDX-License-Identifier: Zlib */

#include "types.h"

#include <stdlib.h>
#include <stdckdint.h>
#include <girara/datastructures.h>
#include <glib.h>

#include "links.h"
#include "internal.h"

zatura_index_element_t* zatura_index_element_new(const char* title) {
  if (title == NULL) {
    return NULL;
  }

  zatura_index_element_t* res = g_try_malloc0(sizeof(zatura_index_element_t));
  if (res == NULL) {
    return NULL;
  }

  res->title = g_strdup(title);

  return res;
}

void zatura_index_element_free(zatura_index_element_t* index) {
  if (index == NULL) {
    return;
  }

  g_free(index->title);
  zatura_link_free(index->link);
  g_free(index);
}

zatura_image_buffer_t* zatura_image_buffer_create(unsigned int width, unsigned int height) {
  g_return_val_if_fail(width != 0, NULL);
  g_return_val_if_fail(height != 0, NULL);

  unsigned int size = 0;
  if (ckd_mul(&size, width, height) == true || ckd_mul(&size, size, 3) == true) {
    return NULL;
  }

  zatura_image_buffer_t* image_buffer = g_try_malloc(sizeof(zatura_image_buffer_t));
  if (image_buffer == NULL) {
    return NULL;
  }

  image_buffer->data = g_try_malloc0_n(size, sizeof(unsigned char));

  if (image_buffer->data == NULL) {
    g_free(image_buffer);
    return NULL;
  }

  image_buffer->width     = width;
  image_buffer->height    = height;
  image_buffer->rowstride = width * 3;

  return image_buffer;
}

void zatura_image_buffer_free(zatura_image_buffer_t* image_buffer) {
  if (image_buffer == NULL) {
    return;
  }

  g_free(image_buffer->data);
  g_free(image_buffer);
}

static void document_information_entry_free(void* data) {
  zatura_document_information_entry_t* entry = data;
  zatura_document_information_entry_free(entry);
}

girara_list_t* zatura_document_information_entry_list_new(void) {
  return girara_list_new_with_free(document_information_entry_free);
}

zatura_document_information_entry_t* zatura_document_information_entry_new(zatura_document_information_type_t type,
                                                                             const char* value) {
  if (value == NULL) {
    return NULL;
  }

  zatura_document_information_entry_t* entry = g_try_malloc0(sizeof(zatura_document_information_entry_t));
  if (entry == NULL) {
    return NULL;
  }

  entry->type  = type;
  entry->value = g_strdup(value);

  return entry;
}

void zatura_document_information_entry_free(void* data) {
  if (!data) {
    return;
  }

  zatura_document_information_entry_t* entry = data;
  g_free(entry->value);
  g_free(entry);
}

zatura_signature_info_t* zatura_signature_info_new(void) {
  return g_try_malloc0(sizeof(zatura_signature_info_t));
}

void zatura_signature_info_free(zatura_signature_info_t* signature) {
  if (signature == NULL) {
    return;
  }

  g_free(signature->signer);
  if (signature->time) {
    g_date_time_unref(signature->time);
  }
  g_free(signature);
}
