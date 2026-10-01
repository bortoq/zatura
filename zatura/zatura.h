/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_H
#define ZATURA_H

#include <stdbool.h>
#include <girara-gtk/types.h>
#include <girara-gtk/session.h>
#include <gtk/gtk.h>
#ifdef WITH_SYNCTEX
#include <synctex/synctex_parser.h>
#endif
#include "macros.h"
#include "types.h"
#include "jumplist.h"
#include "file-monitor.h"

enum {
  NEXT,
  PREVIOUS,
  LEFT,
  RIGHT,
  UP,
  DOWN,
  BOTTOM,
  TOP,
  HIDE,
  HIGHLIGHT,
  DELETE_LAST_WORD,
  DELETE_LAST_CHAR,
  DEFAULT,
  ERROR,
  WARNING,
  NEXT_GROUP,
  PREVIOUS_GROUP,
  ZOOM_IN,
  ZOOM_OUT,
  ZOOM_ORIGINAL,
  ZOOM_SPECIFIC,
  FORWARD,
  BACKWARD,
  CONTINUOUS,
  DELETE_LAST,
  EXPAND,
  EXPAND_RECURSIVE,
  EXPAND_ALL,
  COLLAPSE_ALL,
  COLLAPSE_RECURSIVE,
  COLLAPSE,
  TOGGLE,
  SELECT,
  GOTO_DEFAULT,
  GOTO_LABELS,
  GOTO_OFFSET,
  HALF_UP,
  HALF_DOWN,
  FULL_UP,
  FULL_DOWN,
  PARTIAL_UP,
  PARTIAL_DOWN,
  HALF_LEFT,
  HALF_RIGHT,
  FULL_LEFT,
  FULL_RIGHT,
  NEXT_CHAR,
  PREVIOUS_CHAR,
  DELETE_TO_LINE_START,
  APPEND_FILEPATH,
  ROTATE_CW,
  ROTATE_CCW,
  PAGE_BOTTOM,
  PAGE_TOP,
  BIDIRECTIONAL,
  ZOOM_SMOOTH,
  SMOOTH_UP,
  SMOOTH_DOWN,
};

/* unspecified page number */
enum {
  ZATURA_PAGE_NUMBER_UNSPECIFIED = INT_MIN,
};

/* cache constants */
enum {
  ZATURA_PAGE_CACHE_DEFAULT_SIZE     = 16,
  ZATURA_PAGE_CACHE_MAX_SIZE         = 1024,
  ZATURA_PAGE_THUMBNAIL_DEFAULT_SIZE = 4 * 1024 * 1024
};

/* forward declaration for types from database.h */
typedef struct _ZaturaDatabase zatura_database_t;
typedef struct zatura_fileinfo_s zatura_fileinfo_t;
/* forward declaration for types from content-type.h */
typedef struct zatura_content_type_context_s zatura_content_type_context_t;

struct zatura_s {
  struct {
    girara_session_t* session; /**< girara interface session */

    struct {
      GtkLabel* buffer;       /**< buffer statusbar entry */
      GtkLabel* file;         /**< file statusbar entry */
      GtkLabel* page_number;  /**< page number statusbar entry */
      GtkLabel* search_count; /**< search count statusbar entry */
    } statusbar;

    struct {
      GdkRGBA highlight_color;        /**< Color for highlighting */
      GdkRGBA highlight_color_fg;     /**< Color for highlighting (foreground) */
      GdkRGBA highlight_color_active; /** Color for highlighting */
      GdkRGBA render_loading_bg;      /**< Background color for render "Loading..." */
      GdkRGBA render_loading_fg;      /**< Foreground color for render "Loading..." */
      GdkRGBA signature_success;      /**> Color for highlighing valid signatures */
      GdkRGBA signature_warning;      /**> Color for highlighing  signatures with warnings */
      GdkRGBA signature_error;        /**> Color for highlighing invalid signatures */
    } colors;

    GtkWidget* view;                        /**< Scrolled Window */
    ZaturaDocumentWidget* document_widget; /**< Widget that contains all rendered pages */
    GtkWidget* index;                       /**< Widget to show the index of the document */
  } ui;

  struct {
    ZaturaRenderer* render_thread; /**< The thread responsible for rendering the pages */
    bool initial_render_held;       /**< holds the focused page first render until the view is painted */
    bool scale_settled;       /**< set when the device scale settled so the viewport allocation renders the page */
    bool view_painted;        /**< set after the first frame so a display that never changes scale renders next frame */
    bool initial_render_done; /**< set once the first render has happened so later opens do not hold */
    gulong initial_render_handler;    /**< handler id used to release the hold */
    GObject* initial_render_instance; /**< instance the release handler is connected to */
    char* pending_search_input;       /**< search query received before the widgets finished loading */
    int pending_search_direction;     /**< direction for a search received before loading finished */
  } sync;

  struct {
    void* manager; /**< Plugin manager */
  } plugins;

  struct {
    gchar* config_dir; /**< Path to the configuration directory */
    gchar* data_dir;   /**< Path to the data directory */
    gchar* cache_dir;  /**< Path to the cache directory */
  } config;

  struct {
    GtkPrintSettings* settings; /**< Print settings */
    GtkPageSetup* page_setup;   /**< Saved page setup */
  } print;

  struct {
    girara_list_t* marks;                 /**< Marker */
    char** arguments;                     /**> Arguments that were passed at startup */
    char* search_string;                  /**< Current search string */
    int search_direction;                 /**< Current search direction (FORWARD or BACKWARD) */
    bool are_search_results_highlighted;  /**< Current state of the highlight of the search results */
    GdkModifierType synctex_edit_modmask; /**< Modifier to trigger synctex edit */
    GdkModifierType highlighter_modmask;  /**< Modifier to draw with a highlighter */
    bool double_click_follow;             /**< Double/Single click to follow link */
    guint current_index_position;         /**< current row in index */
    int current_search_result;
    int total_search_results;
  } global;

  struct {
    girara_mode_t normal;       /**< Normal mode */
    girara_mode_t fullscreen;   /**< Fullscreen mode */
    girara_mode_t index;        /**< Index mode */
    girara_mode_t insert;       /**< Insert mode */
    girara_mode_t presentation; /**< Presentation mode */
  } modes;

  struct {
    girara_list_t* bookmarks; /**< bookmarks */
  } bookmarks;

  zatura_jumplist_t jumplist;

  struct {
    guint refresh_view;
#ifdef G_OS_UNIX
    guint sigterm;
#endif
    gulong monitors_handler; /**< Signal handler for monitors items-changed */
    gulong destroy_handler;  /**< Signal handler for the window's destroy signal */
  } signals;

  struct {
    gchar* file;
  } stdin_support;

  zatura_document_t* document;                       /**< The current document */
  zatura_document_t* predecessor_document;           /**< The document from before a reload */
  ZaturaDocumentWidget* predecessor_document_widget; /**< The document widget from before a reload */
  zatura_database_t* database;                       /**< The database */
  ZaturaDbus* dbus;                                  /**< D-Bus service */

  /**
   * File monitor
   */
  struct {
    ZaturaFileMonitor* monitor; /**< File monitor */
    gchar* password;             /**< Save password */
  } file_monitor;

  /**
   * Bisect stage
   */
  struct {
    unsigned int last_jump; /**< Page jumped to by bisect */
    unsigned int start;     /**< Bisection range - start */
    unsigned int end;       /**< Bisection range - end */
  } bisect;

  /**
   * Storage for shortcuts.
   */
  struct {
    struct {
      int x;
      int y;
    } mouse;
    struct {
      unsigned int pages;
    } toggle_page_mode;
    struct {
      int pages;
      char* first_page_column_list;
      double zoom;
      bool is_status_bar_visible;
      bool is_input_bar_visible;
      document_widget_mode_t layout_mode;
    } toggle_presentation_mode;
  } shortcut;

  /**
   * Storage for gestures.
   */
  struct {
    double initial_zoom;
  } gesture;

  /**
   * Context for MIME type detection
   */
  zatura_content_type_context_t* content_type_context;

#ifdef WITH_SYNCTEX
  /**
   * SyncTeX context. The scanner object is cached for better performance.
   */
  struct {
    synctex_scanner_p scanner;
  } synctex;
#endif
};

/**
 * Creates a zatura session
 *
 * @return zatura session object or NULL if zatura could not be creeated
 */
zatura_t* zatura_create(void);

/**
 * Initializes zatura
 *
 * @param zatura The zatura session
 * @return true if initialization has been successful
 */
bool zatura_init(zatura_t* zatura);

/**
 * Free zatura session
 *
 * @param zatura The zatura session
 */
void zatura_free(zatura_t* zatura);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(zatura_t, zatura_free)

/**
 * Set the path to the configuration directory
 *
 * @param zatura The zatura session
 * @param dir Directory path
 */
void zatura_set_config_dir(zatura_t* zatura, const char* dir);

/**
 * Set the path to the data directory
 *
 * @param zatura The zatura session
 * @param dir Directory path
 */
void zatura_set_data_dir(zatura_t* zatura, const char* dir);

/**
 * Set the path to the cache directory.
 *
 * @param zatura The Zatura session
 * @param dir Directory path
 */
void zatura_set_cache_dir(zatura_t* zatura, const char* dir);

/**
 * Set the path to the plugin directory
 *
 * @param zatura The zatura session
 * @param dir Directory path
 */
void zatura_set_plugin_dir(zatura_t* zatura, const char* dir);

/**
 * Sets the program parameters
 *
 * @param zatura The zatura session
 * @param argv List of arguments
 */
void zatura_set_argv(zatura_t* zatura, char** argv);

/**
 * Calculate and store the monitor PPI for the view widget
 *
 * @param zatura The zatura session
 */
void zatura_update_view_ppi(zatura_t* zatura);

/**
 * Opens a file
 *
 * @param zatura The zatura session
 * @param path The path to the file
 * @param password The password of the file
 * @param page_number Open given page number
 * @param file_info Open given page number
 *
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_open(zatura_t* zatura, const char* path, const char* uri, const char* password, int page_number,
                   zatura_fileinfo_t* file_info);

/* render the focused page synchronously once the device scale has settled */
void render_focused_page_now(zatura_t* zatura);

/**
 * Opens a file
 *
 * @param zatura The zatura session
 * @param path The path to the file
 * @param password The password of the file
 * @param synctex Open at the given SyncTeX string
 *
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_open_synctex(zatura_t* zatura, const char* path, const char* uri, const char* password,
                           const char* synctex);

/**
 * Opens a file (idle)
 *
 * @param zatura The zatura session
 * @param path The path to the file
 * @param password The password of the file
 * @param page_number Open given page number
 * @param mode Open in given page mode
 * @param synctex SyncTeX string
 */
void document_open_idle(zatura_t* zatura, const char* path, const char* password, int page_number, const char* mode,
                        const char* synctex, const char* bookmark_name, const char* search_string);

/**
 * Save a open file
 *
 * @param zatura The zatura session
 * @param path The path
 * @param overwrite Overwrite existing file
 *
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_save(zatura_t* zatura, const char* path, bool overwrite);

/**
 * Get fileinfo (zoom, current page, etc).
 *
 * @param zatura The zatura session
 *
 * @return file_info (caller needs to g_free(file_info.first_page_column_list))
 */
zatura_fileinfo_t zatura_get_fileinfo(zatura_t* zatura);

/**
 * Get fileinfo of the predecessor document (zoom, current page, etc).
 *
 * @param zatura The zatura session
 *
 * @return file_info (caller needs to g_free(file_info.first_page_column_list))
 */
zatura_fileinfo_t zatura_get_prefileinfo(zatura_t* zatura);

/**
 * Frees the "predecessor" buffers used for smooth-reload
 *
 * @param zatura The zatura session
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_predecessor_free(zatura_t* zatura);

/**
 * Closes the current opened document
 *
 * @param zatura The zatura session
 * @param keep_monitor Set to true if monitor should be kept (sc_reload)
 * @return If no error occurred true, otherwise false, is returned.
 */
bool document_close(zatura_t* zatura, bool keep_monitor);

/**
 * Opens the page with the given number
 *
 * @param zatura The zatura session
 * @param page_id The id of the page that should be set
 * @return If no error occurred true, otherwise false, is returned.
 */
bool page_set(zatura_t* zatura, unsigned int page_id);

/**
 * Moves to the given position
 *
 * @param zatura Zatura session
 * @param position_x X coordinate
 * @param position_y Y coordinate
 * @return If no error occurred true, otherwise false, is returned.
 */
bool position_set(zatura_t* zatura, double position_x, double position_y);

/**
 * Refresh the page view
 *
 * @param zatura Zatura session
 */
void refresh_view(zatura_t* zatura);

/**
 * Recompute the scale according to settings
 *
 * @param zatura Zatura session
 */
bool adjust_view(zatura_t* zatura);

/**
 * Updates the page number in the statusbar. Note that 1 will be added to the
 * displayed number
 *
 * @param zatura The zatura session
 */
void statusbar_page_number_update(zatura_t* zatura);

/**
 * Gets the nicely formatted filename of the loaded document according to settings
 *
 * @param zatura The zatura session
 * @param statusbar Whether return value will be dispalyed in status bar
 *
 * return Printable filename. Free with g_free.
 */
char* get_formatted_filename(zatura_t* zatura, bool statusbar);

/**
 * Check wether a document is opened
 *
 * @param zatura The zatura session
 * @return bool indicating whether a document is open
 */
bool zatura_has_document(zatura_t* zatura);

/**
 * Obtain the currently opened document
 *
 * @param zatura The zatura session
 * @return the currently opened document
 */
zatura_document_t* zatura_get_document(zatura_t* zatura);

/**
 * Modify and normalize the current search result count
 * so that it always inferior or equal to the total count
 *
 * @param zatura The zatura session
 * @param diff The amount to modify
 */
void zatura_modify_current_search_result(zatura_t* zatura, int diff);

/**
 * Set the current search result count to the last one before the current page
 *
 * @param zatura The zatura session
 * @param current_page_number The current page number
 */
void zatura_set_current_search_result_previous_pages(zatura_t* zatura, unsigned int current_page_number);

#endif // ZATURA_H
