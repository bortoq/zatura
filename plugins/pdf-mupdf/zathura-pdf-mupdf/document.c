/* SPDX-License-Identifier: Zlib */

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#include <glib-2.0/glib.h>

#include "plugin.h"
#include <girara/log.h>
#include <girara/utils.h>

#define LENGTH(x) (sizeof(x) / sizeof((x)[0]))

/* route mupdf warnings to the girara log instead of raw stderr */
static void mupdf_warning_callback(void* GIRARA_UNUSED(user), const char* message) {
  girara_debug("mupdf: %s", message);
}

/* route mupdf errors to the girara log instead of raw stderr */
static void mupdf_error_callback(void* GIRARA_UNUSED(user), const char* message) {
  girara_error("mupdf: %s", message);
}

/* Decode the first FB2 member directly from ZIP. Keep the original archive
 * path in Zatura so history, reload and monitoring refer to the user's file. */
static fz_document* open_fb2_zip(fz_context* ctx, const char* path) {
  fz_stream* file = NULL;
  fz_archive* archive = NULL;
  fz_stream* member = NULL;
  fz_document* book = NULL;
  fz_var(file);
  fz_var(archive);
  fz_var(member);
  fz_var(book);
  fz_try(ctx) {
    file = fz_open_file(ctx, path);
    archive = fz_try_open_archive_with_stream(ctx, file);
    if (archive) {
      const int count = MIN(fz_count_archive_entries(ctx, archive), 4096);
      for (int i = 0; i < count; ++i) {
        const char* name = fz_list_archive_entry(ctx, archive, i);
        const size_t length = name ? strlen(name) : 0;
        if (length >= 4 && g_ascii_strcasecmp(name + length - 4, ".fb2") == 0) {
          member = fz_open_archive_entry(ctx, archive, name);
          book = fz_open_document_with_stream(ctx, "book.fb2", member);
          break;
        }
      }
    }
  }
  fz_always(ctx) {
    fz_drop_stream(ctx, member);
    fz_drop_archive(ctx, archive);
    fz_drop_stream(ctx, file);
  }
  fz_catch(ctx) {
    fz_drop_document(ctx, book);
    fz_rethrow(ctx);
  }
  return book;
}

zathura_error_t pdf_document_open(zathura_document_t* document) {
  zathura_error_t error = ZATHURA_ERROR_OK;
  if (document == NULL) {
    error = ZATHURA_ERROR_INVALID_ARGUMENTS;
    goto error_ret;
  }

  mupdf_document_t* mupdf_document = calloc(1, sizeof(mupdf_document_t));
  if (mupdf_document == NULL) {
    error = ZATHURA_ERROR_OUT_OF_MEMORY;
    goto error_ret;
  }

  g_mutex_init(&mupdf_document->mutex);

  mupdf_document->ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
  if (mupdf_document->ctx == NULL) {
    error = ZATHURA_ERROR_UNKNOWN;
    goto error_free;
  }

  fz_set_warning_callback(mupdf_document->ctx, mupdf_warning_callback, NULL);
  fz_set_error_callback(mupdf_document->ctx, mupdf_error_callback, NULL);

  /* open document */
  const char* path     = zathura_document_get_path(document);
  const char* password = zathura_document_get_password(document);

  fz_try(mupdf_document->ctx) {
    fz_register_document_handlers(mupdf_document->ctx);

    /* Prefer the renamed application directory, with legacy compatibility. */
    char* xdg_path = girara_get_xdg_path(XDG_CONFIG);
    if (xdg_path != NULL) {
      char* css_path  = g_build_filename(xdg_path, "zatura", "epub.css", NULL);
      if (!g_file_test(css_path, G_FILE_TEST_EXISTS)) {
        g_free(css_path);
        css_path = g_build_filename(xdg_path, "zathura", "epub.css", NULL);
      }
      gchar* user_css = NULL;
      if (g_file_get_contents(css_path, &user_css, NULL, NULL) == TRUE) {
        fz_set_user_css(mupdf_document->ctx, user_css);
        g_free(user_css);
      }
      g_free(css_path);
      g_free(xdg_path);
    }

    /* Zatura owns page margins; retain paragraph styles and other user CSS. */
    char* css = g_strconcat(fz_user_css(mupdf_document->ctx) ? fz_user_css(mupdf_document->ctx) : "",
        "\n@page { margin: 0 !important; }\n"
        "html, body, FictionBook { margin: 0 !important; padding: 0 !important; }\n", NULL);
    fz_set_user_css(mupdf_document->ctx, css);
    g_free(css);

    mupdf_document->document = open_fb2_zip(mupdf_document->ctx, path);
    if (mupdf_document->document == NULL) {
      mupdf_document->document = fz_open_document(mupdf_document->ctx, path);
    }
  }
  fz_catch(mupdf_document->ctx) {
    error = ZATHURA_ERROR_UNKNOWN;
    goto error_free;
  }

  if (mupdf_document->document == NULL) {
    error = ZATHURA_ERROR_UNKNOWN;
    goto error_free;
  }

  /* authenticate if password is required and given */
  fz_try(mupdf_document->ctx) {
    if (fz_needs_password(mupdf_document->ctx, mupdf_document->document) != 0) {
      if (password == NULL || fz_authenticate_password(mupdf_document->ctx, mupdf_document->document, password) == 0) {
        error = ZATHURA_ERROR_INVALID_PASSWORD;
      }
    }
  }
  fz_catch(mupdf_document->ctx) {
    error = ZATHURA_ERROR_UNKNOWN;
  }
  if (error != ZATHURA_ERROR_OK) {
    goto error_free;
  }

  fz_try(mupdf_document->ctx) {
    zathura_document_set_number_of_pages(document, fz_count_pages(mupdf_document->ctx, mupdf_document->document));
  }
  fz_catch(mupdf_document->ctx) {
    error = ZATHURA_ERROR_UNKNOWN;
    goto error_free;
  }
  zathura_document_set_data(document, mupdf_document);

  return ZATHURA_ERROR_OK;

error_free:

  if (mupdf_document != NULL) {
    g_mutex_clear(&mupdf_document->mutex);
    if (mupdf_document->document != NULL) {
      fz_drop_document(mupdf_document->ctx, mupdf_document->document);
    }
    if (mupdf_document->ctx != NULL) {
      fz_drop_context(mupdf_document->ctx);
    }

    free(mupdf_document);
  }

  zathura_document_set_data(document, NULL);

error_ret:

  return error;
}

zathura_error_t pdf_document_free(zathura_document_t* document, void* data) {
  mupdf_document_t* mupdf_document = data;

  if (document == NULL || mupdf_document == NULL) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  g_mutex_lock(&mupdf_document->mutex);

  fz_drop_document(mupdf_document->ctx, mupdf_document->document);
  fz_drop_context(mupdf_document->ctx);

  g_mutex_unlock(&mupdf_document->mutex);
  g_mutex_clear(&mupdf_document->mutex);

  free(mupdf_document);
  zathura_document_set_data(document, NULL);

  return ZATHURA_ERROR_OK;
}

zathura_error_t pdf_document_save_as(zathura_document_t* document, void* data, const char* path) {
  mupdf_document_t* mupdf_document = data;

  if (document == NULL || mupdf_document == NULL || path == NULL) {
    return ZATHURA_ERROR_INVALID_ARGUMENTS;
  }

  g_mutex_lock(&mupdf_document->mutex);
  fz_try(mupdf_document->ctx) {
    pdf_save_document(mupdf_document->ctx, (pdf_document*)mupdf_document->document, path, NULL);
  }
  fz_catch(mupdf_document->ctx) {
    g_mutex_unlock(&mupdf_document->mutex);
    return ZATHURA_ERROR_UNKNOWN;
  }
  g_mutex_unlock(&mupdf_document->mutex);

  return ZATHURA_ERROR_OK;
}

girara_list_t* pdf_document_get_information(zathura_document_t* document, void* data, zathura_error_t* error) {
  mupdf_document_t* mupdf_document = data;

  if (document == NULL || mupdf_document == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_INVALID_ARGUMENTS;
    }
  }

  girara_list_t* list = zathura_document_information_entry_list_new();
  if (list == NULL) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_UNKNOWN;
    }
    return NULL;
  }

  g_mutex_lock(&mupdf_document->mutex);
  fz_try(mupdf_document->ctx) {
    pdf_document* pdf_document = pdf_specifics(mupdf_document->ctx, mupdf_document->document);
    if (pdf_document == NULL) {
      girara_list_free(list);
      list = NULL;
      break;
    }

    pdf_obj* trailer   = pdf_trailer(mupdf_document->ctx, pdf_document);
    pdf_obj* info_dict = pdf_dict_get(mupdf_document->ctx, trailer, PDF_NAME(Info));

    /* get string values */
    typedef struct info_value_s {
      const char* property;
      zathura_document_information_type_t type;
    } info_value_t;

    static const info_value_t string_values[] = {
        {"Title", ZATHURA_DOCUMENT_INFORMATION_TITLE},     {"Author", ZATHURA_DOCUMENT_INFORMATION_AUTHOR},
        {"Subject", ZATHURA_DOCUMENT_INFORMATION_SUBJECT}, {"Keywords", ZATHURA_DOCUMENT_INFORMATION_KEYWORDS},
        {"Creator", ZATHURA_DOCUMENT_INFORMATION_CREATOR}, {"Producer", ZATHURA_DOCUMENT_INFORMATION_PRODUCER},
    };

    for (unsigned int i = 0; i < LENGTH(string_values); i++) {
      pdf_obj* value = pdf_dict_gets(mupdf_document->ctx, info_dict, string_values[i].property);
      if (value == NULL) {
        continue;
      }

      const char* str_value = pdf_to_text_string(mupdf_document->ctx, value);
      if (str_value == NULL || strlen(str_value) == 0) {
        continue;
      }

      zathura_document_information_entry_t* entry =
          zathura_document_information_entry_new(string_values[i].type, str_value);

      if (entry != NULL) {
        girara_list_append(list, entry);
      }
    }

    static const info_value_t time_values[] = {
        {"CreationDate", ZATHURA_DOCUMENT_INFORMATION_CREATION_DATE},
        {"ModDate", ZATHURA_DOCUMENT_INFORMATION_MODIFICATION_DATE},
    };

    for (unsigned int i = 0; i < LENGTH(time_values); i++) {
      pdf_obj* value = pdf_dict_gets(mupdf_document->ctx, info_dict, time_values[i].property);
      if (value == NULL) {
        continue;
      }

      const char* str_value = pdf_to_text_string(mupdf_document->ctx, value);
      if (str_value == NULL || strlen(str_value) == 0) {
        continue;
      }

      zathura_document_information_entry_t* entry =
          zathura_document_information_entry_new(time_values[i].type,
                                                 str_value // FIXME: Convert to common format
          );

      if (entry != NULL) {
        girara_list_append(list, entry);
      }
    }
  }
  fz_catch(mupdf_document->ctx) {
    if (error != NULL) {
      *error = ZATHURA_ERROR_UNKNOWN;
    }
    girara_list_free(list);
    list = NULL;
  }
  g_mutex_unlock(&mupdf_document->mutex);

  return list;
}

/* Optional Zatura reflow extension; upstream plugin API/ABI stays unchanged. */
#include <gmodule.h>
#include <zathura/reflow.h>
static bool reflow_supported(zathura_document_t* document) {
  mupdf_document_t* data = zathura_document_get_data(document);
  if (!data) { return false; }
  bool supported = false;
  g_mutex_lock(&data->mutex);
  fz_try(data->ctx) { supported = fz_is_document_reflowable(data->ctx, data->document) != 0; }
  fz_catch(data->ctx) { supported = false; }
  g_mutex_unlock(&data->mutex);
  return supported;
}

static zathura_error_t reflow_layout(zathura_document_t* document, float width, float height,
                                      float font_size, unsigned int* page) {
  mupdf_document_t* data = zathura_document_get_data(document);
  zathura_error_t error = ZATHURA_ERROR_OK;
  g_mutex_lock(&data->mutex);
  fz_try(data->ctx) {
    const fz_location old = fz_location_from_page_number(data->ctx, data->document, *page);
    const fz_bookmark bookmark = fz_make_bookmark(data->ctx, data->document, old);
    fz_layout_document(data->ctx, data->document, width, height, font_size);
    const int count = fz_count_pages(data->ctx, data->document);
    const fz_location location = fz_lookup_bookmark(data->ctx, data->document, bookmark);
    const int target = fz_page_number_from_location(data->ctx, data->document, location);
    zathura_document_set_number_of_pages(document, count);
    *page = target >= 0 ? target : 0;
  }
  fz_catch(data->ctx) { error = ZATHURA_ERROR_UNKNOWN; }
  g_mutex_unlock(&data->mutex);
  return error;
}

G_MODULE_EXPORT const zatura_reflow_plugin_t zatura_reflow_v1 = {
  .supported = reflow_supported,
  .layout = reflow_layout,
};

/* Page parity follows the actual grid, including a right-hand first page and RTL. */
fz_point mupdf_reflow_offset(const mupdf_document_t* data, unsigned int page) {
  if (!data->reflow_margins_active) { return (fz_point){0, 0}; }
  const zatura_reflow_margins_t* m = &data->margins;
  unsigned int column = (page + m->first_column - 1) % m->columns;
  if (m->right_to_left) { column = m->columns - 1 - column; }
  return (fz_point){m->columns == 1 || column == 0 ? m->outer : m->inner, m->top};
}

static zathura_error_t reflow_layout_v2(zathura_document_t* document, float width, float height,
    float font_size, const zatura_reflow_margins_t* margins, unsigned int* page) {
  mupdf_document_t* data = zathura_document_get_data(document);
  zathura_error_t error = ZATHURA_ERROR_OK;
  g_mutex_lock(&data->mutex);
  fz_try(data->ctx) {
    const fz_location old = fz_location_from_page_number(data->ctx, data->document, *page);
    const fz_bookmark bookmark = fz_make_bookmark(data->ctx, data->document, old);
    fz_layout_document(data->ctx, data->document, width - margins->outer - margins->inner,
                       height - margins->top - margins->bottom, font_size);
    const int count = fz_count_pages(data->ctx, data->document);
    const fz_location location = fz_lookup_bookmark(data->ctx, data->document, bookmark);
    const int target = fz_page_number_from_location(data->ctx, data->document, location);
    zathura_document_set_number_of_pages(document, count);
    *page = target >= 0 ? target : 0;
    data->reflow_width = width;
    data->reflow_height = height;
    data->margins = *margins;
    data->reflow_margins_active = true;
  }
  fz_catch(data->ctx) { error = ZATHURA_ERROR_UNKNOWN; }
  g_mutex_unlock(&data->mutex);
  return error;
}

G_MODULE_EXPORT const zatura_reflow_plugin_v2_t zatura_reflow_v2 = {
  .layout = reflow_layout_v2,
};
