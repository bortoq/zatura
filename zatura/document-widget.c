/* SPDX-License-Identifier: Zlib */

#include "document-widget.h"

#include <girara-gtk/settings.h>
#include <girara/log.h>
#include <math.h>

#include "adjustment.h"
#include "callbacks.h"
#include "page-widget.h"
#include "page.h"
#include "render.h"
#include "utils.h"
#include "zatura.h"

typedef struct {
  unsigned int pos;
  unsigned int size;
} document_widget_line_s;

typedef struct zatura_document_widget_private_s {
  zatura_t* zatura;
  zatura_document_t* document;
  GtkWidget** pages;
  guint page_widget_preload_source;
  unsigned int page_widget_preload_next;
  bool page_widgets_loaded;
  bool draw_signatures;

  /* Layout */
  document_widget_mode_t layout_mode;
  bool mode_change_pending; /**< Track pending layout mode changes; FIXME: remove this workaround */
  gboolean pages_right_to_left;
  unsigned int nrow;
  unsigned int ncol;
  document_widget_line_s* row_heights;
  document_widget_line_s* col_widths;
  unsigned int pages_per_row;     /**< number of pages in a row */
  unsigned int first_page_column; /**< column of the first page */
  unsigned int page_v_padding;    /**< padding between pages */
  unsigned int page_h_padding;    /**< padding between pages */
  int alloc_width;
  int alloc_height;

  GtkWidget* grid;

  /* Scrolling */
  GtkAdjustment* hadjustment;
  GtkAdjustment* vadjustment;
  GtkScrollablePolicy hscroll_policy;
  GtkScrollablePolicy vscroll_policy;
} ZaturaDocumentWidgetPrivate;

G_DEFINE_TYPE_WITH_CODE(ZaturaDocumentWidget, zatura_document_widget, GTK_TYPE_WIDGET,
                        G_ADD_PRIVATE(ZaturaDocumentWidget) G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

static void zatura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value,
                                                 GParamSpec* pspec);
static void zatura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zatura_document_widget_size_allocate(GtkWidget* widget, int width, int height, int baseline);
static void zatura_document_widget_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* minimum,
                                            int* natural, int* minimum_baseline, int* natural_baseline);
static void zatura_document_widget_dispose(GObject* object);
static void zatura_document_widget_finalize(GObject* object);
static gboolean zatura_document_widget_preload_pages(gpointer data);
static bool zatura_document_widget_page_is_visible(ZaturaDocumentWidget* document, unsigned int page_number);

enum signals_e {
  PAGE_WIDGETS_LOADED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

enum properties_e {
  PROP_0,
  PROP_ZATURA,
  PROP_LAYOUT_MODE,
  PROP_PAGES_RIGHT_TO_LEFT,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
};

static void zatura_document_widget_class_init(ZaturaDocumentWidgetClass* class) {
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
  widget_class->size_allocate  = zatura_document_widget_size_allocate;
  widget_class->measure        = zatura_document_widget_measure;

  GObjectClass* object_class = G_OBJECT_CLASS(class);
  object_class->set_property = zatura_document_widget_set_property;
  object_class->get_property = zatura_document_widget_get_property;
  object_class->dispose      = zatura_document_widget_dispose;
  object_class->finalize     = zatura_document_widget_finalize;

  g_object_class_install_property(
      object_class, PROP_ZATURA,
      g_param_spec_pointer("zatura", "zatura", "the zatura instance",
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(object_class, PROP_LAYOUT_MODE,
                                  g_param_spec_int("layout-mode", "layout-mode", "set the page layout mode", 0,
                                                   DOCUMENT_WIDGET_MODE_COUNT, DOCUMENT_WIDGET_GRID,
                                                   G_PARAM_WRITABLE | G_PARAM_READABLE));

  g_object_class_install_property(object_class, PROP_PAGES_RIGHT_TO_LEFT,
                                  g_param_spec_boolean("pages-right-to-left", "pages-right-to-left",
                                                       "layout pages left to right", false,
                                                       G_PARAM_WRITABLE | G_PARAM_READABLE));

  g_object_class_override_property(object_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property(object_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property(object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property(object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  signals[PAGE_WIDGETS_LOADED] = g_signal_new("page-widgets-loaded", ZATURA_TYPE_DOCUMENT_WIDGET, G_SIGNAL_RUN_LAST, 0,
                                              NULL, NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 0);
}

static void zatura_document_widget_init(ZaturaDocumentWidget* widget) {
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(widget);

  priv->zatura                    = NULL;
  priv->document                   = NULL;
  priv->pages                      = NULL;
  priv->page_widget_preload_source = 0;
  priv->page_widget_preload_next   = 0;
  priv->page_widgets_loaded        = false;
  priv->draw_signatures            = false;
  priv->layout_mode                = DOCUMENT_WIDGET_GRID;
  priv->pages_per_row              = 1;
  priv->first_page_column          = 1;
  priv->nrow                       = 0;
  priv->ncol                       = 0;
  priv->row_heights                = NULL;
  priv->col_widths                 = NULL;

  /* clip the offset grid like GtkViewport does; gtk4 widgets do not clip children by default */
  gtk_widget_set_overflow(GTK_WIDGET(widget), GTK_OVERFLOW_HIDDEN);

  priv->grid = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(priv->grid), FALSE);
  gtk_grid_set_column_homogeneous(GTK_GRID(priv->grid), FALSE);
  gtk_widget_set_halign(priv->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(priv->grid, GTK_ALIGN_CENTER);
  gtk_widget_set_parent(priv->grid, GTK_WIDGET(widget));
}

GtkWidget* zatura_document_widget_new(zatura_t* zatura, zatura_document_t* zatura_document) {
  g_return_val_if_fail(zatura_document != NULL, NULL);

  GObject* ret = g_object_new(ZATURA_TYPE_DOCUMENT_WIDGET, "zatura", zatura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZaturaDocumentWidget* widget = ZATURA_DOCUMENT_WIDGET(ret);
  GtkWidget* gtk_widget         = GTK_WIDGET(widget);

  if (zatura_document_widget_set_document(widget, zatura_document) == false) {
    g_object_unref(ret);
    return NULL;
  }

  return gtk_widget;
}

bool zatura_document_widget_set_document(ZaturaDocumentWidget* document_widget, zatura_document_t* document) {
  g_return_val_if_fail(document_widget != NULL, false);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document_widget);
  zatura_document_widget_clear_pages(document_widget);

  if (!document) {
    return true;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(document);
  GtkWidget** pages                  = g_try_malloc0_n(number_of_pages, sizeof(GtkWidget*));
  if (pages == NULL) {
    return false;
  }

  priv->document            = document;
  priv->pages               = pages;
  priv->page_widgets_loaded = false;

  return true;
}

zatura_document_t* zatura_document_widget_get_document(ZaturaDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, NULL);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  return priv->document;
}

GtkWidget* zatura_document_widget_get_page(ZaturaDocumentWidget* document, unsigned int page_number) {
  g_return_val_if_fail(document != NULL, NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->document == NULL) {
    return NULL;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);

  if (priv->pages == NULL || page_number >= number_of_pages) {
    return NULL;
  }

  return priv->pages[page_number];
}

static void zatura_document_widget_set_scroll_adjustment(ZaturaDocumentWidget* widget, GtkOrientation orientation,
                                                          GtkAdjustment* adjustment) {
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(widget);
  GtkAdjustment** to_set;
  const gchar* prop_name;

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    to_set    = &priv->hadjustment;
    prop_name = "hadjustment";
  } else {
    to_set    = &priv->vadjustment;
    prop_name = "vadjustment";
  }

  if (adjustment && adjustment == *to_set) {
    return;
  }

  if (*to_set) {
    g_signal_handlers_disconnect_by_data(*to_set, widget);
    g_object_unref(*to_set);
  }

  if (!adjustment) {
    adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  }

  g_signal_connect_swapped(adjustment, "value-changed", G_CALLBACK(gtk_widget_queue_allocate), widget);

  *to_set = g_object_ref_sink(adjustment);

  g_object_notify(G_OBJECT(widget), prop_name);
}

static void zatura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value,
                                                 GParamSpec* pspec) {
  ZaturaDocumentWidget* document    = ZATURA_DOCUMENT_WIDGET(object);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  switch (prop_id) {
  case PROP_ZATURA:
    priv->zatura = g_value_get_pointer(value);
    break;
  case PROP_LAYOUT_MODE: {
    const document_widget_mode_t new_mode = g_value_get_int(value);
    if (priv->layout_mode == new_mode) {
      break;
    }
    /* Keep the selected page authoritative until the new geometry is allocated. */
    priv->mode_change_pending = priv->document != NULL;
    priv->layout_mode         = g_value_get_int(value);
    zatura_document_widget_update_mode(document);
    gtk_widget_queue_allocate(GTK_WIDGET(document));
    break;
  }
  case PROP_PAGES_RIGHT_TO_LEFT:
    priv->pages_right_to_left = g_value_get_boolean(value);
    break;
  case PROP_HADJUSTMENT:
    zatura_document_widget_set_scroll_adjustment(document, GTK_ORIENTATION_HORIZONTAL, g_value_get_object(value));
    break;
  case PROP_VADJUSTMENT:
    zatura_document_widget_set_scroll_adjustment(document, GTK_ORIENTATION_VERTICAL, g_value_get_object(value));
    break;
  case PROP_HSCROLL_POLICY:
    priv->hscroll_policy = g_value_get_enum(value);
    gtk_widget_queue_resize(GTK_WIDGET(document));
    break;
  case PROP_VSCROLL_POLICY:
    priv->vscroll_policy = g_value_get_enum(value);
    gtk_widget_queue_resize(GTK_WIDGET(document));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void zatura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  ZaturaDocumentWidget* document    = ZATURA_DOCUMENT_WIDGET(object);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  switch (prop_id) {
  case PROP_HADJUSTMENT:
    g_value_set_object(value, priv->hadjustment);
    break;
  case PROP_LAYOUT_MODE:
    g_value_set_int(value, priv->layout_mode);
    break;
  case PROP_PAGES_RIGHT_TO_LEFT:
    g_value_set_boolean(value, priv->pages_right_to_left);
    break;
  case PROP_VADJUSTMENT:
    g_value_set_object(value, priv->vadjustment);
    break;
  case PROP_HSCROLL_POLICY:
    g_value_set_enum(value, priv->hscroll_policy);
    break;
  case PROP_VSCROLL_POLICY:
    g_value_set_enum(value, priv->vscroll_policy);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

/* drawing */
static void zatura_document_widget_get_page_position(ZaturaDocumentWidget* document, unsigned int page_index,
                                                      unsigned int* row, unsigned int* col) {
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  const unsigned int c0   = priv->first_page_column;
  const unsigned int ncol = priv->pages_per_row;

  *row = (page_index + c0 - 1) / ncol;
  *col = (page_index + c0 - 1) % ncol;
}

static void zatura_document_widget_line_prefix_sum(document_widget_line_s* array, unsigned int n, unsigned int pad) {
  array[0].pos = 0;

  for (unsigned int i = 1; i < n; i++) {
    array[i].pos = array[i - 1].pos + array[i - 1].size + pad;
  }
}

static void zatura_document_widget_arrange_grid(ZaturaDocumentWidget* widget) {
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(widget);
  zatura_document_t* z_document     = priv->document;

  const unsigned int c0   = priv->first_page_column;
  const unsigned int ncol = priv->pages_per_row;
  const unsigned int npag = zatura_document_get_number_of_pages(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  const unsigned int page_v_padding = priv->page_v_padding;
  const unsigned int page_h_padding = priv->page_h_padding;

  memset(priv->row_heights, 0, nrow * sizeof(document_widget_line_s));
  memset(priv->col_widths, 0, ncol * sizeof(document_widget_line_s));

  // calculate the max width and height required for each column and row
  for (unsigned int i = 0; i < npag; i++) {
    zatura_page_t* page = zatura_document_get_page(z_document, i);

    unsigned int row = 0;
    unsigned int col = 0;
    zatura_document_widget_get_page_position(widget, i, &row, &col);

    unsigned int x = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
    unsigned int y = row;

    unsigned int page_width, page_height;
    page_calc_height_width(z_document, page, &page_height, &page_width, true);

    priv->row_heights[y].size = MAX(page_height, priv->row_heights[y].size);
    priv->col_widths[x].size  = MAX(page_width, priv->col_widths[x].size);
  }

  zatura_document_widget_line_prefix_sum(priv->col_widths, ncol, page_h_padding);
  zatura_document_widget_line_prefix_sum(priv->row_heights, nrow, page_v_padding);
}

void zatura_document_widget_update_mode(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_t* z_document     = priv->document;
  if (z_document == NULL || priv->grid == NULL) {
    return;
  }

  const bool single          = priv->layout_mode == DOCUMENT_WIDGET_SINGLE;
  const unsigned int npag    = zatura_document_get_number_of_pages(z_document);
  const unsigned int page_id = zatura_document_get_current_page_number(z_document);

  for (unsigned int i = 0; i < npag; i++) {
    GtkWidget* page_widget = zatura_document_widget_get_page(document, i);
    if (page_widget != NULL) {
      gtk_widget_set_visible(page_widget, single == false || i == page_id);
    }
  }

  if (single == true) {
    /* store the position to match the reset */
    zatura_document_t* z_document = priv->document;
    if (z_document != NULL) {
      zatura_document_set_position_x(z_document, 0.0);
      zatura_document_set_position_y(z_document, 0.0);
    }
    gtk_adjustment_set_value(priv->hadjustment, 0);
    gtk_adjustment_set_value(priv->vadjustment, 0);
  } else {
    zatura_document_widget_compute_layout(document);
  }

  gtk_widget_queue_resize(GTK_WIDGET(document));
}

bool zatura_document_widget_mode_change_pending(ZaturaDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, false);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  return priv->mode_change_pending;
}

/* Called after the bounds and viewport size have been updated, before placing the grid. */
static void apply_pending_page_anchor(ZaturaDocumentWidget* document) {
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->mode_change_pending == false || priv->document == NULL) {
    return;
  }

  /* Navigation while allocation was pending may have selected a newer page. */
  double x = 0.5, y = 0.5;
  if (priv->layout_mode == DOCUMENT_WIDGET_GRID) {
    bool vertical_center = false;
    girara_setting_get(priv->zatura->ui.session, "vertical-center", &vertical_center);
    page_number_to_position(priv->zatura, zatura_document_get_current_page_number(priv->document), 0.5,
                            vertical_center ? 0.5 : 0.0, &x, &y);
    bool zoom_center = false;
    girara_setting_get(priv->zatura->ui.session, "zoom-center", &zoom_center);
    const zatura_adjust_mode_t mode = zatura_document_get_adjust_mode(priv->document);
    if (zoom_center || mode == ZATURA_ADJUST_BESTFIT || mode == ZATURA_ADJUST_WIDTH) {
      x = 0.5;
    }
  } else {
    /* Single-page adjustments use page-local coordinates; start at the top. */
    y = 0.0;
  }

  zatura_document_set_position_x(priv->document, x);
  zatura_document_set_position_y(priv->document, y);
  zatura_adjustment_set_value_from_ratio(priv->hadjustment, x);
  zatura_adjustment_set_value_from_ratio(priv->vadjustment, y);
  zatura_document_set_position_x(priv->document, zatura_adjustment_get_ratio(priv->hadjustment));
  zatura_document_set_position_y(priv->document, zatura_adjustment_get_ratio(priv->vadjustment));
  priv->mode_change_pending = false;
  statusbar_page_number_update(priv->zatura);
}

static void size_allocate_single(ZaturaDocumentWidget* document, int width, int height, int baseline) {
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_t* z_document     = priv->document;

  zatura_page_t* page = zatura_document_get_page(z_document, zatura_document_get_current_page_number(z_document));
  if (page == NULL) {
    return;
  }

  unsigned int page_width = 0, page_height = 0;
  page_calc_height_width(z_document, page, &page_height, &page_width, true);

  if ((int)gtk_adjustment_get_upper(priv->hadjustment) != (int)page_width) {
    gtk_adjustment_set_upper(priv->hadjustment, page_width);
  }
  if ((int)gtk_adjustment_get_upper(priv->vadjustment) != (int)page_height) {
    gtk_adjustment_set_upper(priv->vadjustment, page_height);
  }

  apply_pending_page_anchor(document);

  const int value_h = gtk_adjustment_get_value(priv->hadjustment);
  const int value_v = gtk_adjustment_get_value(priv->vadjustment);
  const int clamp_h = MAX(MIN(-value_h, 0), -((int)page_width - width));
  const int clamp_v = MAX(MIN(-value_v, 0), -((int)page_height - height));

  const int x = ((int)page_width < width) ? (width - (int)page_width) / 2 : clamp_h;
  const int y = ((int)page_height < height) ? (height - (int)page_height) / 2 : clamp_v;

  const GtkAllocation allocation = {.x = x, .y = y, .width = (int)page_width, .height = (int)page_height};
  gtk_widget_size_allocate(priv->grid, &allocation, baseline);
}

static void zatura_document_widget_size_allocate(GtkWidget* widget, int width, int height, int baseline) {
  ZaturaDocumentWidget* document    = ZATURA_DOCUMENT_WIDGET(widget);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_t* z_document     = priv->document;

  bool size_changed = false;

  if (z_document != NULL) {
    size_changed       = width != priv->alloc_width || height != priv->alloc_height;
    priv->alloc_width  = width;
    priv->alloc_height = height;
  }

  if (z_document != NULL) {
    if (size_changed == true) {
      zatura_document_set_viewport_height(z_document, height);
      zatura_document_set_viewport_width(z_document, width);
      adjust_view(priv->zatura);
      /* the scale settled and the zoom is now fit so release the held render in this same frame */
      if (priv->zatura->sync.initial_render_held == true && priv->zatura->sync.scale_settled == true) {
        priv->zatura->sync.initial_render_held = false;
        render_focused_page_now(priv->zatura);
      }
    }
    zatura_document_widget_update_visible_pages(document);
  }

  /* set the page size after adjust_view so the changed handler stores the position against the
   * final document height and the view stays at the top on first open */
  if (priv->grid != NULL && size_changed == true) {
    gtk_adjustment_set_page_size(priv->hadjustment, width);
    gtk_adjustment_set_page_size(priv->vadjustment, height);
    gtk_adjustment_set_page_increment(priv->hadjustment, width * 0.9);
    gtk_adjustment_set_page_increment(priv->vadjustment, height * 0.9);
  }

  if (priv->grid != NULL) {
    /* position the grid by the adjustment values so navigation moves the view */
    /* read natural size from arrange_grid totals to avoid measuring during size_allocate */
    unsigned int doc_w = 0, doc_h = 0;
    if (priv->col_widths != NULL && priv->row_heights != NULL && priv->nrow > 0 && priv->ncol > 0) {
      zatura_document_widget_get_document_size(document, &doc_h, &doc_w);
    }

    const int alloc_w = MAX(width, (int)doc_w);
    const int alloc_h = MAX(height, (int)doc_h);

    if (priv->layout_mode == DOCUMENT_WIDGET_SINGLE && z_document != NULL) {
      size_allocate_single(document, width, height, baseline);
      return;
    }

    apply_pending_page_anchor(document);

    /* align tall documents to the top so the first page stays visible while the grid fills */
    gtk_widget_set_valign(priv->grid, (int)doc_h > height ? GTK_ALIGN_START : GTK_ALIGN_CENTER);

    const int x                    = -(int)gtk_adjustment_get_value(priv->hadjustment);
    const int y                    = -(int)gtk_adjustment_get_value(priv->vadjustment);
    const GtkAllocation allocation = {.x = x, .y = y, .width = alloc_w, .height = alloc_h};
    gtk_widget_size_allocate(priv->grid, &allocation, baseline);
  }
}

static void zatura_document_widget_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* minimum,
                                            int* natural, int* minimum_baseline, int* natural_baseline) {
  ZaturaDocumentWidget* document    = ZATURA_DOCUMENT_WIDGET(widget);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  if (priv->grid != NULL) {
    gtk_widget_measure(priv->grid, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
    return;
  }
  *minimum = *natural = 0;
  if (minimum_baseline != NULL) {
    *minimum_baseline = -1;
  }
  if (natural_baseline != NULL) {
    *natural_baseline = -1;
  }
}

static void zatura_document_widget_dispose(GObject* object) {
  ZaturaDocumentWidget* document    = ZATURA_DOCUMENT_WIDGET(object);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  zatura_document_widget_clear_pages(document);
  g_clear_pointer(&priv->grid, gtk_widget_unparent);

  g_clear_object(&priv->hadjustment);
  g_clear_object(&priv->vadjustment);

  G_OBJECT_CLASS(zatura_document_widget_parent_class)->dispose(object);
}

static void zatura_document_widget_finalize(GObject* object) {
  ZaturaDocumentWidget* document    = ZATURA_DOCUMENT_WIDGET(object);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  g_free(priv->col_widths);
  g_free(priv->row_heights);

  priv->col_widths  = NULL;
  priv->row_heights = NULL;

  G_OBJECT_CLASS(zatura_document_widget_parent_class)->finalize(object);
}

void zatura_document_widget_refresh_layout(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  priv->alloc_width                  = -1;
  priv->alloc_height                 = -1;
  zatura_document_t* z_document     = priv->document;

  if (z_document == NULL) {
    return;
  }

  const unsigned int c0   = priv->first_page_column;
  const unsigned int ncol = priv->pages_per_row;
  const unsigned int npag = zatura_document_get_number_of_pages(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  document_widget_line_s* tmp = g_try_realloc_n(priv->col_widths, ncol, sizeof(document_widget_line_s));
  if (tmp == NULL) {
    girara_error("Failed to allocate document grid (%u columns, %u rows)", ncol, nrow);
    return;
  }
  priv->col_widths = tmp;
  tmp              = g_try_realloc_n(priv->row_heights, nrow, sizeof(document_widget_line_s));
  if (tmp == NULL) {
    girara_error("Failed to allocate document grid (%u columns, %u rows)", ncol, nrow);
    return;
  }
  priv->row_heights = tmp;

  priv->ncol = ncol;
  priv->nrow = nrow;

  for (unsigned int i = 0; i < npag; i++) {
    GtkWidget* page_widget = zatura_document_widget_get_page(document, i);
    if (page_widget == NULL) {
      continue;
    }

    GtkWidget* parent = gtk_widget_get_parent(page_widget);
    if (parent == priv->grid) {
      gtk_grid_remove(GTK_GRID(priv->grid), page_widget);
    } else if (parent != NULL) {
      gtk_widget_unparent(page_widget);
    }

    unsigned int row = 0;
    unsigned int col = 0;
    zatura_document_widget_get_page_position(document, i, &row, &col);
    unsigned int x  = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
    GtkAlign halign = priv->ncol == 1 ? GTK_ALIGN_CENTER : (x == 0 ? GTK_ALIGN_END : GTK_ALIGN_START);
    gtk_widget_set_halign(page_widget, halign);
    gtk_grid_attach(GTK_GRID(priv->grid), page_widget, (int)x, (int)row, 1, 1);
  }

  zatura_document_widget_compute_layout(document);

  gtk_widget_set_visible(GTK_WIDGET(document), true);
  zatura_document_widget_update_mode(document);
  gtk_widget_queue_resize(GTK_WIDGET(document));

  /* the cached visibility flags are stale after pages move */
  zatura_document_widget_update_visible_pages(document);
}

static void zatura_document_widget_attach_page(ZaturaDocumentWidget* document, unsigned int page_index) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->grid == NULL || priv->ncol == 0) {
    return;
  }

  GtkWidget* page_widget = zatura_document_widget_get_page(document, page_index);
  if (page_widget == NULL || gtk_widget_get_parent(page_widget) == priv->grid) {
    return;
  }

  unsigned int row = 0;
  unsigned int col = 0;
  zatura_document_widget_get_page_position(document, page_index, &row, &col);
  const unsigned int x  = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
  const GtkAlign halign = priv->ncol == 1 ? GTK_ALIGN_CENTER : (x == 0 ? GTK_ALIGN_END : GTK_ALIGN_START);
  gtk_widget_set_halign(page_widget, halign);
  gtk_grid_attach(GTK_GRID(priv->grid), page_widget, (int)x, (int)row, 1, 1);
}

GtkWidget* zatura_document_widget_ensure_page(ZaturaDocumentWidget* document, unsigned int page_index) {
  g_return_val_if_fail(document != NULL, NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->pages == NULL || priv->document == NULL ||
      page_index >= zatura_document_get_number_of_pages(priv->document)) {
    return NULL;
  }

  GtkWidget* page_widget = priv->pages[page_index];
  if (page_widget == NULL) {
    zatura_page_t* page = zatura_document_get_page(priv->document, page_index);
    if (page == NULL) {
      return NULL;
    }

    page_widget = zatura_page_widget_new(priv->zatura, page);
    if (page_widget == NULL) {
      return NULL;
    }

    priv->pages[page_index] = g_object_ref_sink(page_widget);

    gtk_widget_set_halign(page_widget, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page_widget, GTK_ALIGN_CENTER);

    const bool show = priv->layout_mode != DOCUMENT_WIDGET_SINGLE ||
                      page_index == zatura_document_get_current_page_number(priv->document);
    gtk_widget_set_visible(page_widget, show);

    unsigned int page_height = 0;
    unsigned int page_width  = 0;
    page_calc_height_width(priv->document, page, &page_height, &page_width, true);
    zatura_page_widget_set_size_request(ZATURA_PAGE_WIDGET(page_widget), page_width, page_height);

    g_signal_connect(G_OBJECT(page_widget), "text-selected", G_CALLBACK(cb_page_widget_text_selected), priv->zatura);
    g_signal_connect(G_OBJECT(page_widget), "image-selected", G_CALLBACK(cb_page_widget_image_selected), priv->zatura);
    g_signal_connect(G_OBJECT(page_widget), "enter-link", G_CALLBACK(cb_page_widget_link), (gpointer) true);
    g_signal_connect(G_OBJECT(page_widget), "leave-link", G_CALLBACK(cb_page_widget_link), (gpointer) false);
    g_signal_connect(G_OBJECT(page_widget), "scaled-button-release", G_CALLBACK(cb_page_widget_scaled_button_release),
                     priv->zatura);
    g_object_set(G_OBJECT(page_widget), "draw-signatures", priv->draw_signatures, NULL);
  }

  zatura_document_widget_attach_page(document, page_index);
  return page_widget;
}

static gboolean zatura_document_widget_preload_pages(gpointer data) {
  ZaturaDocumentWidget* document    = ZATURA_DOCUMENT_WIDGET(data);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  if (priv->document == NULL) {
    priv->page_widget_preload_source = 0;
    return G_SOURCE_REMOVE;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  const gint64 deadline              = g_get_monotonic_time() + 4000;
  while (priv->page_widget_preload_next < number_of_pages && g_get_monotonic_time() < deadline) {
    zatura_document_widget_ensure_page(document, priv->page_widget_preload_next++);
  }

  if (priv->page_widget_preload_next < number_of_pages) {
    return G_SOURCE_CONTINUE;
  }

  zatura_document_widget_update_mode(document);
  priv->page_widget_preload_source = 0;
  priv->page_widgets_loaded        = true;
  g_signal_emit(document, signals[PAGE_WIDGETS_LOADED], 0);
  return G_SOURCE_REMOVE;
}

void zatura_document_widget_start_page_widget_preload(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_widget_stop_page_widget_preload(document);
  if (priv->document == NULL) {
    return;
  }

  priv->page_widget_preload_next = 0;
  priv->page_widgets_loaded      = false;
  priv->page_widget_preload_source =
      g_idle_add_full(G_PRIORITY_LOW, zatura_document_widget_preload_pages, document, NULL);
}

void zatura_document_widget_stop_page_widget_preload(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->page_widget_preload_source != 0) {
    g_source_remove(priv->page_widget_preload_source);
    priv->page_widget_preload_source = 0;
  }
  priv->page_widget_preload_next = 0;
  priv->page_widgets_loaded      = false;
}

bool zatura_document_widget_page_widgets_loaded(ZaturaDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, false);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  return priv->page_widgets_loaded;
}

static bool zatura_document_widget_page_is_visible(ZaturaDocumentWidget* document, unsigned int page_number) {
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->row_heights == NULL || priv->col_widths == NULL || priv->nrow == 0 ||
      priv->ncol == 0) {
    return false;
  }

  unsigned int row = 0;
  unsigned int col = 0;
  zatura_document_widget_get_page_position(document, page_number, &row, &col);
  if (row >= priv->nrow || col >= priv->ncol) {
    return false;
  }

  const unsigned int document_height = priv->row_heights[priv->nrow - 1].pos + priv->row_heights[priv->nrow - 1].size;
  const unsigned int document_width  = priv->col_widths[priv->ncol - 1].pos + priv->col_widths[priv->ncol - 1].size;
  if (document_height == 0 || document_width == 0) {
    return false;
  }

  const double page_x = ((double)priv->col_widths[col].pos + 0.5 * priv->col_widths[col].size) / (double)document_width;
  const double page_y =
      ((double)priv->row_heights[row].pos + 0.5 * priv->row_heights[row].size) / (double)document_height;
  const double pos_x = zatura_document_get_position_x(priv->document);
  const double pos_y = zatura_document_get_position_y(priv->document);

  unsigned int view_height = 0;
  unsigned int view_width  = 0;
  zatura_document_get_viewport_size(priv->document, &view_height, &view_width);

  return fabs(pos_x - page_x) < 0.5 * (double)(view_width + priv->col_widths[col].size) / (double)document_width &&
         fabs(pos_y - page_y) < 0.5 * (double)(view_height + priv->row_heights[row].size) / (double)document_height;
}

void zatura_document_widget_update_visible_pages(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->zatura == NULL || priv->zatura->sync.render_thread == NULL) {
    return;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zatura_page_t* page = zatura_document_get_page(priv->document, page_id);
    if (page == NULL) {
      continue;
    }

    const bool visible = zatura_document_widget_page_is_visible(document, page_id);
    if (visible) {
      zatura_document_widget_ensure_page(document, page_id);
    }

    GtkWidget* page_widget = zatura_document_widget_get_page(document, page_id);
    if (page_widget == NULL) {
      continue;
    }
    ZaturaPageWidget* zatura_page_widget = ZATURA_PAGE_WIDGET(page_widget);

    if (visible) {
      if (!zatura_page_get_visibility(page)) {
        zatura_page_set_visibility(page, true);
        zatura_renderer_page_cache_add(priv->zatura->sync.render_thread, page_id);
      }

      for (unsigned int i = priv->pages_per_row; i; --i) {
        if (page_id >= i) {
          GtkWidget* previous = zatura_document_widget_get_page(document, page_id - i);
          if (previous) {
            zatura_page_widget_update_view_time(ZATURA_PAGE_WIDGET(previous));
          }
        }
        if (page_id + i < number_of_pages) {
          GtkWidget* next = zatura_document_widget_get_page(document, page_id + i);
          if (next) {
            zatura_page_widget_update_view_time(ZATURA_PAGE_WIDGET(next));
          }
        }
      }
      zatura_page_widget_update_view_time(zatura_page_widget);
    } else {
      if (zatura_page_get_visibility(page)) {
        zatura_page_set_visibility(page, false);
        zatura_page_widget_abort_render_request(zatura_page_widget);
      }

      girara_list_t* results = NULL;
      g_object_get(G_OBJECT(page_widget), "search-results", &results, NULL);
      if (results != NULL) {
        g_object_set(G_OBJECT(page_widget), "search-current", 0, NULL);
      }
    }
  }
}

void zatura_document_widget_render_current_page(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (priv->document == NULL || priv->zatura == NULL || priv->zatura->sync.render_thread == NULL) {
    return;
  }

  const unsigned int current = zatura_document_get_current_page_number(priv->document);
  zatura_page_t* page       = zatura_document_get_page(priv->document, current);
  GtkWidget* page_widget     = zatura_document_widget_get_page(document, current);
  if (page == NULL || page_widget == NULL) {
    return;
  }

  cairo_surface_t* rendered = zatura_renderer_render_page(priv->zatura->sync.render_thread, page);
  if (rendered != NULL) {
    zatura_page_widget_update_surface(ZATURA_PAGE_WIDGET(page_widget), rendered, false);
    cairo_surface_destroy(rendered);
  }
}

bool zatura_document_widget_page_has_surface(ZaturaDocumentWidget* document, unsigned int page_number) {
  GtkWidget* page_widget = zatura_document_widget_get_page(document, page_number);
  return page_widget != NULL && zatura_page_widget_have_surface(ZATURA_PAGE_WIDGET(page_widget));
}

void zatura_document_widget_set_draw_signatures(ZaturaDocumentWidget* document, bool draw) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  priv->draw_signatures              = draw;
  if (priv->document == NULL) {
    return;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    GtkWidget* page_widget = zatura_document_widget_get_page(document, page);
    if (page_widget != NULL) {
      g_object_set(G_OBJECT(page_widget), "draw-signatures", draw, NULL);
    }
  }
}

void zatura_document_widget_set_draw_search_results(ZaturaDocumentWidget* document, bool draw) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    GtkWidget* page_widget = zatura_document_widget_get_page(document, page);
    if (page_widget) {
      g_object_set(G_OBJECT(page_widget), "draw-search-results", draw, NULL);
    }
  }
}

bool zatura_document_widget_prepare_links(ZaturaDocumentWidget* document) {
  g_return_val_if_fail(document != NULL, false);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return false;
  }

  bool show_links                    = false;
  unsigned int page_offset           = 0;
  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zatura_page_t* page = zatura_document_get_page(priv->document, page_id);
    if (!page) {
      continue;
    }

    GtkWidget* page_widget = zatura_document_widget_get_page(document, page_id);
    if (!page_widget) {
      continue;
    }

    GObject* object = G_OBJECT(page_widget);
    g_object_set(object, "draw-search-results", FALSE, NULL);
    const bool visible = zatura_page_get_visibility(page);
    g_object_set(object, "draw-links", visible, NULL);
    if (visible) {
      int number_of_links = 0;
      g_object_get(object, "number-of-links", &number_of_links, NULL);
      show_links |= number_of_links != 0;
      g_object_set(object, "offset-links", page_offset, NULL);
      page_offset += number_of_links;
    }
  }
  return show_links;
}

void zatura_document_widget_hide_links(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    GtkWidget* page_widget = zatura_document_widget_get_page(document, page);
    if (page_widget != NULL) {
      g_object_set(G_OBJECT(page_widget), "draw-links", FALSE, NULL);
    }
  }
}

zatura_link_t* zatura_document_widget_get_visible_link(ZaturaDocumentWidget* document, unsigned int index) {
  g_return_val_if_fail(document != NULL, NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return NULL;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  for (unsigned int page = 0; page < number_of_pages; ++page) {
    zatura_page_t* zatura_page = zatura_document_get_page(priv->document, page);
    GtkWidget* page_widget       = zatura_document_widget_get_page(document, page);
    if (zatura_page != NULL && zatura_page_get_visibility(zatura_page) && page_widget != NULL) {
      zatura_link_t* link = zatura_page_widget_link_get(ZATURA_PAGE_WIDGET(page_widget), index);
      if (link != NULL) {
        return link;
      }
    }
  }
  return NULL;
}

unsigned int zatura_document_widget_get_search_result_count(ZaturaDocumentWidget* document, unsigned int end_page) {
  g_return_val_if_fail(document, 0);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  if (!priv->document) {
    return 0;
  }

  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);
  end_page                           = MIN(end_page, number_of_pages);

  unsigned int count = 0;
  for (unsigned int page = 0; page < end_page; ++page) {
    GtkWidget* page_widget = zatura_document_widget_get_page(document, page);
    if (page_widget != NULL) {
      int page_count = 0;
      g_object_get(G_OBJECT(page_widget), "search-length", &page_count, NULL);
      if (page_count > 0) {
        count += (unsigned int)page_count;
      }
    }
  }

  return count;
}

void zatura_document_widget_compute_layout(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_widget_arrange_grid(document);

  /* update allocation values */
  unsigned int doc_height = 0, doc_width = 0;
  zatura_document_widget_get_document_size(document, &doc_height, &doc_width);

  const double stored_x = zatura_document_get_position_x(priv->document);
  const double stored_y = zatura_document_get_position_y(priv->document);
  gtk_adjustment_set_upper(priv->vadjustment, doc_height);
  gtk_adjustment_set_upper(priv->hadjustment, doc_width);
  // Better set vadjustment first because it's likely that hadjustment_value_changed
  // callback to call excess zatura_document_set_current_page_number
  zatura_adjustment_set_value_from_ratio(priv->vadjustment, stored_y);
  zatura_adjustment_set_value_from_ratio(priv->hadjustment, stored_x);

  float scroll_step = 40;
  girara_setting_get(priv->zatura->ui.session, "scroll-step", &scroll_step);

  gtk_adjustment_set_step_increment(priv->vadjustment, scroll_step);
}

void zatura_document_widget_get_cell_pos(ZaturaDocumentWidget* document, unsigned int page_index, unsigned int* pos_x,
                                          unsigned int* pos_y) {
  g_return_if_fail(document != NULL && pos_x != NULL && pos_y != NULL);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_t* z_document     = priv->document;

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  const unsigned int npag = zatura_document_get_number_of_pages(z_document);
  if (page_index >= npag) {
    girara_warning("tried to get cell size for page %u, document has %u pages", page_index, npag);
    return;
  }

  unsigned int row, col;
  zatura_document_widget_get_page_position(document, page_index, &row, &col);

  *pos_x = priv->col_widths[col].pos;
  *pos_y = priv->row_heights[row].pos;
}

void zatura_document_widget_get_cell_size(ZaturaDocumentWidget* document, unsigned int page_index,
                                           unsigned int* height, unsigned int* width) {
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_t* z_document     = priv->document;

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  const unsigned int npag = zatura_document_get_number_of_pages(z_document);
  if (page_index >= npag) {
    girara_warning("tried to get cell size for page %u, document has %u pages", page_index, npag);
    return;
  }

  unsigned int row, col;
  zatura_document_widget_get_page_position(document, page_index, &row, &col);

  *height = priv->row_heights[row].size;
  *width  = priv->col_widths[col].size;
}

void zatura_document_widget_get_row(ZaturaDocumentWidget* document, unsigned int row, unsigned int* pos,
                                     unsigned int* size) {
  g_return_if_fail(document != NULL && pos != NULL && size != NULL);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  if (row >= priv->nrow) {
    girara_warning("tried to get row %u size, document has %u rows", row, priv->nrow);
    return;
  }

  *pos  = priv->row_heights[row].pos;
  *size = priv->row_heights[row].size;
}

void zatura_document_widget_get_col(ZaturaDocumentWidget* document, unsigned int col, unsigned int* pos,
                                     unsigned int* size) {
  g_return_if_fail(document != NULL && pos != NULL && size != NULL);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  if (col >= priv->ncol) {
    girara_warning("tried to get col %u size, document has %u columns", col, priv->ncol);
    return;
  }

  *pos  = priv->col_widths[col].pos;
  *size = priv->col_widths[col].size;
}

void zatura_document_widget_get_document_size(ZaturaDocumentWidget* document, unsigned int* height,
                                               unsigned int* width) {
  g_return_if_fail(document != NULL && height != NULL && width != NULL);
  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  if (!priv->col_widths || !priv->row_heights) {
    return;
  }

  document_widget_line_s last_row = priv->row_heights[priv->nrow - 1];
  document_widget_line_s last_col = priv->col_widths[priv->ncol - 1];

  *height = last_row.pos + last_row.size;
  *width  = last_col.pos + last_col.size;
}

void zatura_document_widget_clear_pages(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  priv->mode_change_pending          = false;
  zatura_document_widget_stop_page_widget_preload(document);
  const unsigned int number_of_pages =
      priv->document != NULL && priv->pages != NULL ? zatura_document_get_number_of_pages(priv->document) : 0;

  for (unsigned int i = 0; i < number_of_pages; ++i) {
    GtkWidget* page_widget = priv->pages[i];
    if (page_widget != NULL) {
      if (gtk_widget_get_parent(page_widget) != NULL) {
        gtk_widget_unparent(page_widget);
      }
      g_clear_object(&priv->pages[i]);
    }
  }

  g_clear_pointer(&priv->pages, g_free);
  g_clear_pointer(&priv->col_widths, g_free);
  g_clear_pointer(&priv->row_heights, g_free);

  priv->document     = NULL;
  priv->nrow         = 0;
  priv->ncol         = 0;
  priv->alloc_width  = -1;
  priv->alloc_height = -1;
}

void zatura_document_widget_clear_thumbnails(ZaturaDocumentWidget* document) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  const unsigned int number_of_pages = zatura_document_get_number_of_pages(priv->document);

  for (unsigned int i = 0; i < number_of_pages; ++i) {
    GtkWidget* page_widget = priv->pages[i];

    /* the widget exists only if the background fill already created it */
    if (page_widget != NULL) {
      zatura_page_widget_clear_thumbnail(ZATURA_PAGE_WIDGET(page_widget));
    }
  }
}

void zatura_document_widget_render_all(ZaturaDocumentWidget* document) {
  if (!document) {
    return;
  }

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  zatura_document_t* z_document     = priv->document;
  if (!z_document) {
    return;
  }

  priv->alloc_width  = -1;
  priv->alloc_height = -1;

  zatura_document_widget_compute_layout(document);

  /* unmark all pages */
  const unsigned int number_of_pages = zatura_document_get_number_of_pages(z_document);
  for (unsigned int page_id = 0; page_id < number_of_pages; ++page_id) {
    zatura_page_t* page = zatura_document_get_page(z_document, page_id);

    unsigned int page_height = 0, page_width = 0;
    page_calc_height_width(z_document, page, &page_height, &page_width, true);

    girara_debug("Queuing resize for page %u to %u x %u.", page_id, page_width, page_height);
    GtkWidget* page_widget = zatura_document_widget_get_page(document, page_id);
    if (page_widget != NULL) {
      zatura_page_widget_set_size_request(ZATURA_PAGE_WIDGET(page_widget), page_width, page_height);
      gtk_widget_queue_resize(page_widget);
    }
  }
}

void zatura_document_widget_set_page_layout(ZaturaDocumentWidget* document, unsigned int page_v_padding,
                                             unsigned int page_h_padding, unsigned int pages_per_row,
                                             unsigned int first_page_column) {
  g_return_if_fail(document != NULL);

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);

  priv->page_v_padding = page_v_padding;
  priv->page_h_padding = page_h_padding;
  priv->pages_per_row  = pages_per_row;

  /* keep grid spacing in sync with the padding arrange_grid uses */
  /* otherwise position_to_page_number reads stale positions and navigation jumps */
  if (priv->grid != NULL) {
    gtk_grid_set_row_spacing(GTK_GRID(priv->grid), page_v_padding);
    gtk_grid_set_column_spacing(GTK_GRID(priv->grid), page_h_padding);
  }

  if (first_page_column < 1) {
    first_page_column = 1;
  } else if (first_page_column > pages_per_row) {
    first_page_column = ((first_page_column - 1) % pages_per_row) + 1;
  }

  priv->first_page_column = first_page_column;
}

unsigned int zatura_document_widget_get_page_v_padding(ZaturaDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  return priv->page_v_padding;
}

unsigned int zatura_document_widget_get_page_h_padding(ZaturaDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  return priv->page_h_padding;
}

unsigned int zatura_document_widget_get_pages_per_row(ZaturaDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  return priv->pages_per_row;
}

unsigned int zatura_document_widget_get_first_page_column(ZaturaDocumentWidget* document) {
  if (!document) {
    return 0;
  }

  ZaturaDocumentWidgetPrivate* priv = zatura_document_widget_get_instance_private(document);
  return priv->first_page_column;
}
