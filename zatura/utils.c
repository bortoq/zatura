/* SPDX-License-Identifier: Zlib */

#include "utils.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <girara/datastructures.h>
#include <girara-gtk/session.h>
#include <girara-gtk/settings.h>
#include <girara-gtk/statusbar.h>
#include <girara/utils.h>

#include "adjustment.h"
#include "links.h"
#include "zatura.h"
#include "internal.h"
#include "document.h"
#include "document-widget.h"
#include "page.h"
#include "render.h"
#include "plugin.h"
#include "content-type.h"
#include "index-element-object.h"

double zatura_correct_zoom_value(girara_session_t* session, const double zoom) {
  if (session == NULL) {
    return zoom;
  }

  /* zoom limitations */
  unsigned int zoom_min_int = 10;
  unsigned int zoom_max_int = 1000;
  girara_setting_get(session, "zoom-min", &zoom_min_int);
  girara_setting_get(session, "zoom-max", &zoom_max_int);

  const double zoom_min = zoom_min_int * 0.01;
  const double zoom_max = zoom_max_int * 0.01;

  return CLAMP(zoom, zoom_min, zoom_max);
}

bool file_valid_extension(zatura_t* zatura, const char* path) {
  if (zatura == NULL || path == NULL || zatura->plugins.manager == NULL) {
    return false;
  }

  g_autofree char* content_type = zatura_content_type_guess(zatura->content_type_context, path, NULL);
  if (content_type == NULL) {
    return false;
  }

  return zatura_plugin_manager_get_plugin(zatura->plugins.manager, content_type) != NULL;
}

/* build children store from a girara tree node */
static GListStore* index_element_build_children(girara_session_t* session, girara_tree_node_t* tree) {
  GListStore* store            = g_list_store_new(ZATURA_TYPE_INDEX_ELEMENT_OBJECT);
  girara_list_t* list          = girara_node_get_children(tree);
  zatura_t* zatura           = session->global.data;
  zatura_document_t* document = zatura != NULL ? zatura_get_document(zatura) : NULL;

  for (size_t idx = 0; idx != girara_list_size(list); ++idx) {
    girara_tree_node_t* node               = girara_list_nth(list, idx);
    zatura_index_element_t* index_element = girara_node_get_data(node);
    const zatura_link_type_t type         = zatura_link_get_type(index_element->link);
    const zatura_link_target_t target     = zatura_link_get_target(index_element->link);

    g_autofree char* page_label = NULL;
    g_autofree char* page_alt   = NULL;

    if (type == ZATURA_LINK_GOTO_DEST) {
      zatura_page_t* page = document != NULL ? zatura_document_get_page(document, target.page_number) : NULL;
      const char* label    = page != NULL ? zatura_page_get_label(page, NULL) : NULL;
      if (label != NULL) {
        page_label = g_strdup_printf(_("Page %s"), label);
        page_alt   = g_strdup_printf("(%u)", target.page_number + 1);
      } else {
        page_label = g_strdup_printf(_("Page %u"), target.page_number + 1);
      }
    } else {
      page_label = g_strdup(target.value);
    }

    ZaturaIndexElementObject* item = g_object_new(ZATURA_TYPE_INDEX_ELEMENT_OBJECT, NULL);
    item->title                     = g_markup_escape_text(index_element->title, -1);
    item->page_label                = g_steal_pointer(&page_label);
    item->page_alt                  = g_steal_pointer(&page_alt);
    /* the girara tree nodes are released right after this function returns, so we
       take ownership of the index_element pointer for our wrapper's lifetime */
    item->element = index_element;

    if (girara_node_get_num_children(node) > 0) {
      item->children = G_LIST_STORE(index_element_build_children(session, node));
    }

    g_list_store_append(store, item);
    g_object_unref(item);
  }
  return store;
}

GListModel* document_index_build_model(girara_session_t* session, girara_tree_node_t* tree) {
  return G_LIST_MODEL(index_element_build_children(session, tree));
}

static GtkListView* get_list_view(zatura_t* zatura) {
  return GTK_LIST_VIEW(gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(zatura->ui.index)));
}

/* descend the hierarchy recording the deepest element whose target page <= current_page;
   the path is built up from the root and stored as a chain of ZaturaIndexElementObject pointers */
typedef struct {
  ZaturaIndexElementObject** items;
  size_t depth;
  size_t capacity;
} index_path_t;

static void index_path_append(index_path_t* path, ZaturaIndexElementObject* item) {
  if (path->depth == path->capacity) {
    path->capacity = path->capacity == 0 ? 8 : path->capacity * 2;
    path->items    = g_realloc(path->items, path->capacity * sizeof(*path->items));
  }
  path->items[path->depth++] = item;
}

static void index_path_copy(index_path_t* dst, const index_path_t* src) {
  dst->depth = 0;
  for (size_t i = 0; i < src->depth; i++) {
    index_path_append(dst, src->items[i]);
  }
}

/* returns TRUE to signal early termination (matches gtk_tree_model_foreach semantics
   where the callback returning TRUE stops the entire walk) */
static gboolean find_deepest_path(GListModel* level, unsigned int current_page, index_path_t* current,
                                  index_path_t* best) {
  guint n = g_list_model_get_n_items(level);
  for (guint i = 0; i < n; i++) {
    g_autoptr(ZaturaIndexElementObject) item = g_list_model_get_item(level, i);
    if (item == NULL || item->element == NULL) {
      continue;
    }
    zatura_link_target_t target = zatura_link_get_target(item->element->link);
    if (target.page_number > current_page) {
      return TRUE;
    }
    index_path_append(current, item);
    index_path_copy(best, current);
    if (item->children != NULL) {
      if (find_deepest_path(G_LIST_MODEL(item->children), current_page, current, best)) {
        current->depth--;
        return TRUE;
      }
    }
    current->depth--;
  }
  return FALSE;
}

/* walk the flat tree-list model expanding ancestors along the captured path;
   return the resulting flat-model position of the deepest item */
static guint resolve_path_position(GListModel* flat, const index_path_t* path) {
  guint pos = 0;
  for (size_t level = 0; level < path->depth; level++) {
    ZaturaIndexElementObject* want = path->items[level];
    guint n                         = g_list_model_get_n_items(flat);
    for (guint i = 0; i < n; i++) {
      g_autoptr(GtkTreeListRow) r = g_list_model_get_item(flat, i);
      if (r == NULL) {
        continue;
      }
      g_autoptr(ZaturaIndexElementObject) it = gtk_tree_list_row_get_item(r);
      if (it == want) {
        pos = i;
        if (level + 1 < path->depth) {
          gtk_tree_list_row_set_expanded(r, TRUE);
        }
        break;
      }
    }
  }
  return pos;
}

static gboolean scroll_index_to_cursor(void* data) {
  zatura_t* zatura = data;
  GtkListView* view  = get_list_view(zatura);
  gtk_list_view_scroll_to(view, zatura->global.current_index_position, GTK_LIST_SCROLL_FOCUS | GTK_LIST_SCROLL_SELECT,
                          NULL);
  return G_SOURCE_REMOVE;
}

void index_scroll_to_current_page(zatura_t* zatura) {
  GtkListView* view            = get_list_view(zatura);
  GtkSelectionModel* selection = gtk_list_view_get_model(view);
  unsigned int current_page    = zatura_document_get_current_page_number(zatura_get_document(zatura));

  /* the selection wraps a GtkTreeListModel; its underlying model is the root GListStore */
  GtkTreeListModel* tree_model = GTK_TREE_LIST_MODEL(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(selection)));
  GListModel* root             = gtk_tree_list_model_get_model(tree_model);

  index_path_t current = {0};
  index_path_t best    = {0};
  find_deepest_path(root, current_page, &current, &best);
  guint pos = resolve_path_position(G_LIST_MODEL(selection), &best);

  g_free(current.items);
  g_free(best.items);

  zatura->global.current_index_position = pos;
  g_idle_add(scroll_index_to_cursor, zatura);
}
static zatura_rectangle_t rotate_rectangle(zatura_rectangle_t rectangle, unsigned int degree, double height,
                                            double width) {
  zatura_rectangle_t tmp;
  switch (degree) {
  case 90:
    tmp.x1 = height - rectangle.y2;
    tmp.x2 = height - rectangle.y1;
    tmp.y1 = rectangle.x1;
    tmp.y2 = rectangle.x2;
    break;
  case 180:
    tmp.x1 = width - rectangle.x2;
    tmp.x2 = width - rectangle.x1;
    tmp.y1 = height - rectangle.y2;
    tmp.y2 = height - rectangle.y1;
    break;
  case 270:
    tmp.x1 = rectangle.y1;
    tmp.x2 = rectangle.y2;
    tmp.y1 = width - rectangle.x2;
    tmp.y2 = width - rectangle.x1;
    break;
  default:
    return rectangle;
  }

  return tmp;
}

zatura_rectangle_t recalc_rectangle(zatura_page_t* page, zatura_rectangle_t rectangle) {
  if (page == NULL) {
    return rectangle;
  }

  zatura_document_t* document = zatura_page_get_document(page);
  if (document == NULL) {
    return rectangle;
  }

  double page_height = zatura_page_get_height(page);
  double page_width  = zatura_page_get_width(page);
  double scale       = zatura_document_get_scale(document);

  zatura_rectangle_t tmp =
      rotate_rectangle(rectangle, zatura_document_get_rotation(document), page_height, page_width);
  tmp.x1 *= scale;
  tmp.x2 *= scale;
  tmp.y1 *= scale;
  tmp.y2 *= scale;

  return tmp;
}

GtkWidget* zatura_page_get_widget(zatura_t* zatura, zatura_page_t* page) {
  if (zatura == NULL || page == NULL || zatura->ui.document_widget == NULL) {
    return NULL;
  }

  if (zatura_page_get_document(page) != zatura_document_widget_get_document(zatura->ui.document_widget)) {
    return NULL;
  }

  unsigned int page_number = zatura_page_get_index(page);
  return zatura_document_widget_get_page(zatura->ui.document_widget, page_number);
}

GtkWidget* zatura_page_get_widget_by_number(zatura_t* zatura, unsigned int page_number) {
  if (zatura == NULL || !zatura_has_document(zatura) || zatura->ui.document_widget == NULL) {
    return NULL;
  }

  return zatura_document_widget_get_page(zatura->ui.document_widget, page_number);
}

void document_draw_search_results(zatura_t* zatura, bool value) {
  if (zatura_has_document(zatura) == false || zatura->ui.document_widget == NULL) {
    return;
  }

  /* set state of search results highlight */
  zatura->global.are_search_results_highlighted = value;

  /* nothing to highlight until the preload is done */
  if (zatura_document_widget_page_widgets_loaded(zatura->ui.document_widget) == false) {
    return;
  }
  zatura_document_widget_set_draw_search_results(zatura->ui.document_widget, value);
}

char* zatura_get_version_string(const zatura_plugin_manager_t* plugin_manager, bool markup) {
  if (plugin_manager == NULL) {
    return NULL;
  }

  GString* string = g_string_new(NULL);

  /* zatura version */
  g_string_append(string, "zatura " ZATURA_VERSION);
  g_string_append_printf(string, "\ngirara " GIRARA_VERSION " (runtime: %s)", girara_version());

  const char* format = (markup == true) ? "\n<i>(plugin)</i> %s (%s) <i>(%s)</i>" : "\n(plugin) %s (%s) (%s)";

  /* plugin information */
  girara_list_t* plugins = zatura_plugin_manager_get_plugins(plugin_manager);
  if (plugins != NULL) {
    for (size_t idx = 0; idx != girara_list_size(plugins); ++idx) {
      const zatura_plugin_t* plugin = girara_list_nth(plugins, idx);
      const char* name               = zatura_plugin_get_name(plugin);
      const char* version            = zatura_plugin_get_version(plugin);
      g_string_append_printf(string, format, (name == NULL) ? "-" : name, version, zatura_plugin_get_path(plugin));
    }
  }

  return g_string_free_and_steal(string);
}

GdkClipboard* get_selection(zatura_t* zatura) {
  g_return_val_if_fail(zatura != NULL, NULL);

  g_autofree char* value = NULL;
  girara_setting_get(zatura->ui.session, "selection-clipboard", &value);
  if (value == NULL) {
    return NULL;
  }

  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(zatura->ui.session->gtk.window));
  if (g_strcmp0(value, "primary") == 0) {
    return gdk_display_get_primary_clipboard(display);
  } else if (g_strcmp0(value, "clipboard") == 0) {
    return gdk_display_get_clipboard(display);
  } else if (g_strcmp0(value, "false") == 0) {
    return NULL;
  }

  girara_error("Invalid value for the selection-clipboard setting");
  return NULL;
}

static char* write_first_page_column_list(unsigned int* first_page_columns, unsigned int size) {
  if (first_page_columns == NULL) {
    return NULL;
  }

  GString* buffer = g_string_new(NULL);
  for (unsigned int i = 0; i < size; i++) {
    if (i != 0) {
      g_string_append_printf(buffer, ":%u", first_page_columns[i]);
    } else {
      g_string_append_printf(buffer, "%u", first_page_columns[i]);
    }
  }

  return g_string_free_and_steal(buffer);
}

static unsigned int* parse_first_page_column_list(const char* first_page_column_list, unsigned int* size) {
  if (first_page_column_list == NULL || size == NULL) {
    return NULL;
  }

  g_auto(GStrv) tokens = g_strsplit(first_page_column_list, ":", 0);
  unsigned int length  = g_strv_length(tokens);

  unsigned int* settings = g_malloc_n(length, sizeof(unsigned int));
  for (unsigned int i = 0; i < length; i++) {
    guint64 column = 1;

    if (g_ascii_string_to_unsigned(tokens[i], 10, 1, UINT_MAX, &column, NULL) && column <= UINT_MAX) {
      settings[i] = (unsigned int)column;
    } else {
      settings[i] = 1;
    }
  }

  *size = length;
  return settings;
}

unsigned int find_first_page_column(const char* first_page_column_list, const unsigned int pages_per_row) {
  /* sanity checks */
  unsigned int first_page_column = 1;
  g_return_val_if_fail(first_page_column_list != NULL, first_page_column);
  g_return_val_if_fail(pages_per_row > 0, first_page_column);

  unsigned int size                 = 0;
  g_autofree unsigned int* settings = parse_first_page_column_list(first_page_column_list, &size);

  if (pages_per_row <= size) {
    first_page_column = settings[pages_per_row - 1];
  } else if (size > 0) {
    first_page_column = settings[size - 1];
  }

  return MIN(first_page_column, pages_per_row);
}

char* increment_first_page_column(const char* first_page_column_list, const unsigned int pages_per_row, int incr) {
  /* sanity checks */
  if (first_page_column_list == NULL) {
    first_page_column_list = "";
  }
  /* This function is a no-op for 1 column layout */
  if (pages_per_row <= 1) {
    return g_strdup(first_page_column_list);
  }

  unsigned int size      = 0;
  unsigned int* settings = parse_first_page_column_list(first_page_column_list, &size);

  /* Lookup current setting. Signed value to avoid negative overflow when modifying it later. */
  int column = 1;
  if (pages_per_row <= size) {
    column = settings[pages_per_row - 1];
  } else if (size > 0) {
    column = settings[size - 1];
  }

  /* increment and normalise to [1,pages_per_row]. */
  column += incr;
  column %= pages_per_row; /* range [-pages_per_row+1, pages_per_row-1] */
  if (column <= 0) {
    column += pages_per_row; /* range [1, pages_per_row] */
  }

  /* Write back, creating the new cell if necessary. */
  if (pages_per_row <= size) {
    settings[pages_per_row - 1] = column;
  } else {
    /* extend settings array */
    settings = g_realloc_n(settings, pages_per_row, sizeof(*settings));
    for (unsigned int i = size; i < pages_per_row - 1; i++) {
      /* The value of the last set cell is normally used for all largers pages_per_row,
       * so duplicate it to the newly created cells. */
      settings[i] = size > 0 ? settings[size - 1] : 1;
    }
    settings[pages_per_row - 1] = column;
    size                        = pages_per_row;
  }

  return write_first_page_column_list(settings, size);
}

bool parse_color(GdkRGBA* color, const char* str) {
  if (!gdk_rgba_parse(color, str)) {
    girara_warning("Failed to parse color string '%s'.", str);
    return false;
  }
  return true;
}

typedef struct zatura_point_s {
  uintptr_t x;
  uintptr_t y;
} zatura_point_t;

static int cmp_point(const void* va, const void* vb) {
  const zatura_point_t* a = va;
  const zatura_point_t* b = vb;

  if (a->x == b->x) {
    if (a->y == b->y) {
      return 0;
    }

    return a->y < b->y ? -1 : 1;
  }

  return a->x < b->x ? -1 : 1;
}

static inline uintptr_t ufloor(double f) {
  return floor(f);
}

static inline uintptr_t uceil(double f) {
  return ceil(f);
}

static int cmp_uint(const void* vx, const void* vy) {
  const uintptr_t x = (uintptr_t)vx;
  const uintptr_t y = (uintptr_t)vy;

  return x == y ? 0 : (x > y ? 1 : -1);
}

static int cmp_rectangle(const void* vr1, const void* vr2) {
  const zatura_rectangle_t* r1 = vr1;
  const zatura_rectangle_t* r2 = vr2;

  // we only care about equlity here, no ordering
  return (ufloor(r1->x1) == ufloor(r2->x1) && uceil(r1->x2) == uceil(r2->x2) && ufloor(r1->y1) == ufloor(r2->y1) &&
          uceil(r1->y2) == uceil(r2->y2))
             ? 0
             : -1;
}

static bool girara_list_append_unique(girara_list_t* l, girara_compare_function_t cmp, void* item) {
  if (girara_list_find(l, cmp, item) != NULL) {
    return false;
  }

  girara_list_append(l, item);
  return true;
}

static void append_unique_point(girara_list_t* list, const uintptr_t x, const uintptr_t y) {
  zatura_point_t* p = g_try_malloc(sizeof(zatura_point_t));
  if (p == NULL) {
    return;
  }

  p->x = x;
  p->y = y;

  if (girara_list_append_unique(list, cmp_point, p) == false) {
    g_free(p);
  }
}

static void rectangle_to_points(void* vrect, void* vlist) {
  const zatura_rectangle_t* rect = vrect;
  girara_list_t* list             = vlist;

  append_unique_point(list, ufloor(rect->x1), ufloor(rect->y1));
  append_unique_point(list, ufloor(rect->x1), uceil(rect->y2));
  append_unique_point(list, uceil(rect->x2), ufloor(rect->y1));
  append_unique_point(list, uceil(rect->x2), uceil(rect->y2));
}

static void append_unique_uint(girara_list_t* list, const uintptr_t v) {
  girara_list_append_unique(list, cmp_uint, (void*)v);
}

// transform a rectangle into multiple new ones according a grid of points
static void cut_rectangle(const zatura_rectangle_t* rect, girara_list_t* points, girara_list_t* rectangles) {
  // Lists of ordred relevant points
  g_autoptr(girara_list_t) xs = girara_sorted_list_new(cmp_uint);
  g_autoptr(girara_list_t) ys = girara_sorted_list_new(cmp_uint);

  append_unique_uint(xs, uceil(rect->x2));
  append_unique_uint(ys, uceil(rect->y2));

  for (size_t idx = 0; idx != girara_list_size(points); ++idx) {
    const zatura_point_t* pt = girara_list_nth(points, idx);
    if (pt->x > ufloor(rect->x1) && pt->x < uceil(rect->x2)) {
      append_unique_uint(xs, pt->x);
    }
    if (pt->y > ufloor(rect->y1) && pt->y < uceil(rect->y2)) {
      append_unique_uint(ys, pt->y);
    }
  }

  double x = ufloor(rect->x1);
  for (size_t idx = 0; idx != girara_list_size(xs); ++idx) {
    const uintptr_t cx = (uintptr_t)girara_list_nth(xs, idx);
    double y           = ufloor(rect->y1);
    for (size_t inner_idx = 0; inner_idx != girara_list_size(ys); ++inner_idx) {
      const uintptr_t cy     = (uintptr_t)girara_list_nth(ys, inner_idx);
      zatura_rectangle_t* r = g_try_malloc(sizeof(zatura_rectangle_t));

      *r = (zatura_rectangle_t){x, y, cx, cy};
      y  = cy;
      girara_list_append_unique(rectangles, cmp_rectangle, r);
    }
    x = cx;
  }
}

girara_list_t* flatten_rectangles(girara_list_t* rectangles) {
  girara_list_t* new_rectangles   = girara_list_new_with_free(g_free);
  g_autoptr(girara_list_t) points = girara_list_new_with_free(g_free);
  girara_list_foreach(rectangles, rectangle_to_points, points);

  for (size_t idx = 0; idx != girara_list_size(rectangles); ++idx) {
    const zatura_rectangle_t* r = girara_list_nth(rectangles, idx);
    cut_rectangle(r, points, new_rectangles);
  }
  return new_rectangles;
}

bool search_document(zatura_t* zatura, girara_argument_t* argument, bool disable_notify) {
  g_return_val_if_fail(argument != NULL, false);
  g_return_val_if_fail(zatura->document != NULL, false);

  /* navigating results needs every page widget so do nothing until the preload is done */
  if (zatura_document_widget_page_widgets_loaded(zatura->ui.document_widget) == false) {
    return false;
  }

  girara_session_t* session = zatura->ui.session;

  const unsigned int num_pages = zatura_document_get_number_of_pages(zatura->document);
  const unsigned int cur_page  = zatura_document_get_current_page_number(zatura->document);
  GtkWidget* cur_page_widget = zatura_page_get_widget(zatura, zatura_document_get_page(zatura->document, cur_page));
  bool new_search            = argument->data != NULL;
  bool nohlsearch            = false;
  bool first_time_after_abort = false;

  girara_setting_get(session, "nohlsearch", &nohlsearch);
  if (nohlsearch == false) {
    gboolean draw = FALSE;
    g_object_get(G_OBJECT(cur_page_widget), "draw-search-results", &draw, NULL);

    if (draw == false) {
      first_time_after_abort = true;
    }

    document_draw_search_results(zatura, true);
  }

  int diff = argument->n == FORWARD ? 1 : -1;
  if (zatura->global.search_direction == BACKWARD) {
    diff = -diff;
  }

  zatura_page_t* target_page = NULL;
  int target_idx              = 0;

  for (unsigned int page_id = 0; page_id < num_pages; ++page_id) {
    int tmp              = cur_page + diff * page_id;
    zatura_page_t* page = zatura_document_get_page(zatura->document, (tmp + num_pages) % num_pages);
    if (page == NULL) {
      continue;
    }

    GtkWidget* page_widget = zatura_page_get_widget(zatura, page);

    int num_search_results = 0, current = -1;
    g_object_get(G_OBJECT(page_widget), "search-current", &current, "search-length", &num_search_results, NULL);
    if (num_search_results == 0 || current == -1) {
      continue;
    }

    if (new_search == true || first_time_after_abort == true || (tmp + num_pages) % num_pages != cur_page) {
      target_page = page;
      target_idx  = diff == 1 ? 0 : num_search_results - 1;
      break;
    }

    if (diff == 1 && (current < num_search_results - 1 || num_pages == 1)) {
      /* the next result is on the same page */
      target_page = page;
      target_idx  = (current + 1) % num_search_results;
    } else if (diff == -1 && (current > 0 || num_pages == 1)) {
      target_page = page;
      target_idx  = (current - 1 + num_search_results) % num_search_results;
    } else {
      /* the next result is on a different page */
      g_object_set(G_OBJECT(page_widget), "search-current", -1, NULL);

      for (unsigned int npage_id = 1; npage_id < num_pages; ++npage_id) {
        int ntmp                     = cur_page + diff * (page_id + npage_id);
        zatura_page_t* npage        = zatura_document_get_page(zatura->document, (ntmp + 2 * num_pages) % num_pages);
        GtkWidget* npage_page_widget = zatura_page_get_widget(zatura, npage);
        g_object_get(G_OBJECT(npage_page_widget), "search-length", &num_search_results, NULL);
        if (num_search_results != 0) {
          target_page = npage;
          target_idx  = diff == 1 ? 0 : num_search_results - 1;
          break;
        }
      }
    }

    break;
  }

  if (target_page != NULL) {
    girara_list_t* results   = NULL;
    GtkWidget* page_widget   = zatura_page_get_widget(zatura, target_page);
    GObject* obj_page_widget = G_OBJECT(page_widget);
    g_object_set(obj_page_widget, "search-current", target_idx, NULL);
    g_object_get(obj_page_widget, "search-results", &results, NULL);

    /* Need to adjust rectangle to page scale and orientation */
    zatura_rectangle_t* rect     = girara_list_nth(results, target_idx);
    zatura_rectangle_t rectangle = recalc_rectangle(target_page, *rect);

    bool search_hadjust = true;
    girara_setting_get(session, "search-hadjust", &search_hadjust);

    /* compute the position of the center of the page */
    double pos_x = 0;
    double pos_y = 0;
    page_number_to_position(zatura, zatura_page_get_index(target_page), 0.5, 0.5, &pos_x, &pos_y);

    /* correction to center the current result                          */
    /* NOTE: rectangle is in viewport units, already scaled and rotated */
    unsigned int cell_height = 0;
    unsigned int cell_width  = 0;
    zatura_document_widget_get_cell_size(ZATURA_DOCUMENT_WIDGET(zatura->ui.document_widget),
                                          zatura_page_get_index(target_page), &cell_height, &cell_width);

    unsigned int doc_height = 0;
    unsigned int doc_width  = 0;
    zatura_document_widget_get_document_size(ZATURA_DOCUMENT_WIDGET(zatura->ui.document_widget), &doc_height,
                                              &doc_width);

    /* compute the center of the rectangle, which will be aligned to the center
       of the viewport */
    const double center_y = (rectangle.y1 + rectangle.y2) / 2;
    pos_y += (center_y - (double)cell_height / 2) / (double)doc_height;

    if (search_hadjust == true) {
      const double center_x = (rectangle.x1 + rectangle.x2) / 2;
      pos_x += (center_x - (double)cell_width / 2) / (double)doc_width;
    }

    /* move to position */
    zatura_jumplist_add(zatura);
    position_set(zatura, pos_x, pos_y);
    zatura_jumplist_add(zatura);

    unsigned int current_page_number = zatura_document_get_current_page_number(zatura->document);
    zatura_set_current_search_result_previous_pages(zatura, current_page_number);
    zatura_modify_current_search_result(zatura, target_idx + 1);

    g_autofree char* tmp = g_strdup_printf(_("[Search %d/%d]"), zatura->global.current_search_result,
                                           zatura->global.total_search_results);
    girara_statusbar_item_set_text(zatura->ui.statusbar.search_count, tmp);
  } else if (argument->data != NULL && !disable_notify) {
    const char* input             = argument->data;
    g_autofree char* escaped_text = g_markup_printf_escaped(_("Pattern not found: %s"), input);
    girara_notify(session, GIRARA_ERROR, "%s", escaped_text);
    girara_statusbar_item_set_text(zatura->ui.statusbar.search_count, "");
  }

  return false;
}
