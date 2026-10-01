/* SPDX-License-Identifier: Zlib */

#ifndef TYPES_H
#define TYPES_H

#include <girara/datastructures.h>
#include <glib.h>

#include "macros.h"

/**
 * Document
 */
typedef struct zatura_document_s zatura_document_t;
/**
 * Document widget
 */
typedef struct zatura_document_widget_s ZaturaDocumentWidget;
typedef struct zatura_document_widget_class_s ZaturaDocumentWidgetClass;
/**
 * Page
 */
typedef struct zatura_page_s zatura_page_t;
/**
 * Page widget
 */
typedef struct zatura_page_widget_s ZaturaPageWidget;
typedef struct zatura_page_widget_class_s ZaturaPageWidgetClass;
/**
 * Zatura
 */
typedef struct zatura_s zatura_t;

/**
 * Plugin manager
 */
typedef struct zatura_plugin_manager_s zatura_plugin_manager_t;

/**
 * Renderer
 */
typedef struct zatura_renderer_s ZaturaRenderer;

/**
 * Render request
 */
typedef struct zatura_render_request_s ZaturaRenderRequest;

/**
 * D-Bus manager
 */
typedef struct zatura_dbus_s ZaturaDbus;

/**
 * Error types
 */
typedef enum zatura_plugin_error_e {
  ZATURA_ERROR_OK,                /**< No error occurred */
  ZATURA_ERROR_UNKNOWN,           /**< An unknown error occurred */
  ZATURA_ERROR_OUT_OF_MEMORY,     /**< Out of memory */
  ZATURA_ERROR_NOT_IMPLEMENTED,   /**< The called function has not been implemented */
  ZATURA_ERROR_INVALID_ARGUMENTS, /**< Invalid arguments have been passed */
  ZATURA_ERROR_INVALID_PASSWORD   /**< The provided password is invalid */
} zatura_error_t;

/**
 * Possible information entry types
 */
typedef enum zatura_document_information_type_e {
  ZATURA_DOCUMENT_INFORMATION_TITLE,             /**< Title of the document */
  ZATURA_DOCUMENT_INFORMATION_AUTHOR,            /**< Author of the document */
  ZATURA_DOCUMENT_INFORMATION_SUBJECT,           /**< Subject of the document */
  ZATURA_DOCUMENT_INFORMATION_KEYWORDS,          /**< Keywords of the document */
  ZATURA_DOCUMENT_INFORMATION_CREATOR,           /**< Creator of the document */
  ZATURA_DOCUMENT_INFORMATION_PRODUCER,          /**< Producer of the document */
  ZATURA_DOCUMENT_INFORMATION_CREATION_DATE,     /**< Creation data */
  ZATURA_DOCUMENT_INFORMATION_MODIFICATION_DATE, /**< Modification data */
  ZATURA_DOCUMENT_INFORMATION_OTHER,             /**< Any other information */
  ZATURA_DOCUMENT_INFORMATION_FORMAT             /**< Format of the document */
} zatura_document_information_type_t;

/**
 * Plugin
 */
typedef struct zatura_plugin_s zatura_plugin_t;

/**
 * Document information entry
 *
 * Represents a single entry in the returned list from the \ref
 * zatura_document_get_information function
 */
typedef struct zatura_document_information_entry_s zatura_document_information_entry_t;

/**
 * Image buffer
 */
typedef struct zatura_image_buffer_s {
  unsigned char* data;    /**< Image buffer data */
  unsigned int height;    /**< Height of the image */
  unsigned int width;     /**< Width of the image */
  unsigned int rowstride; /**< Rowstride of the image */
} zatura_image_buffer_t;

/**
 * Adjust mode
 */
typedef enum zatura_adjust_mode_e {
  ZATURA_ADJUST_NONE,       /**< No adjustment */
  ZATURA_ADJUST_BESTFIT,    /**< Adjust to best-fit */
  ZATURA_ADJUST_WIDTH,      /**< Adjust to width */
  ZATURA_ADJUST_INPUTBAR,   /**< Focusing the inputbar */
  ZATURA_ADJUST_MODE_NUMBER /**< Number of adjust modes */
} zatura_adjust_mode_t;

typedef enum zatura_equal_mode_e {
  ZATURA_EQUAL_NONE,        /**< No equalisation */
  ZATURA_EQUAL_WIDTH,       /**< Equal page widths */
  ZATURA_EQUAL_HEIGHT,      /**< Equal page heights */
  ZATURA_EQUAL_MODE_NUMBER, /**< Number of equalisation modes */
} zatura_equal_mode_t;

/**
 * Creates an image buffer
 *
 * @param width Width of the image stored in the buffer
 * @param height Height of the image stored in the buffer
 * @return Image buffer or NULL if an error occurred
 */
ZATURA_PLUGIN_API zatura_image_buffer_t* zatura_image_buffer_create(unsigned int width, unsigned int height);

/**
 * Frees the image buffer
 *
 * @param buffer The image buffer
 */
ZATURA_PLUGIN_API void zatura_image_buffer_free(zatura_image_buffer_t* buffer);

/**
 * Rectangle structure.
 * The coordinate system has its origin in the left upper corner. The x axes
 * goes to the right, the y access goes down.
 */
typedef struct zatura_rectangle_s {
  double x1; /**< X coordinate of point 1 */
  double y1; /**< Y coordinate of point 1 */
  double x2; /**< X coordinate of point 2 */
  double y2; /**< Y coordinate of point 2 */
} zatura_rectangle_t;

/**
 * Image structure
 */
typedef struct zatura_image_s {
  zatura_rectangle_t position; /**< Coordinates of the image */
  void* data;                   /**< Custom data of the plugin */
} zatura_image_t;

/**
 * Possible link types
 */
typedef enum zatura_link_type_e {
  ZATURA_LINK_INVALID,     /**< Invalid type */
  ZATURA_LINK_NONE,        /**< No action */
  ZATURA_LINK_GOTO_DEST,   /**< Links to a page */
  ZATURA_LINK_GOTO_REMOTE, /**< Links to a page */
  ZATURA_LINK_URI,         /**< Links to an external source */
  ZATURA_LINK_LAUNCH,      /**< Links to an external source */
  ZATURA_LINK_NAMED        /**< Links to an external source */
} zatura_link_type_t;

typedef enum zatura_link_destination_type_e {
  ZATURA_LINK_DESTINATION_UNKNOWN,
  ZATURA_LINK_DESTINATION_XYZ,
  ZATURA_LINK_DESTINATION_FIT,
  ZATURA_LINK_DESTINATION_FITH,
  ZATURA_LINK_DESTINATION_FITV,
  ZATURA_LINK_DESTINATION_FITR,
  ZATURA_LINK_DESTINATION_FITB,
  ZATURA_LINK_DESTINATION_FITBH,
  ZATURA_LINK_DESTINATION_FITBV
} zatura_link_destination_type_t;

typedef struct zatura_link_target_s {
  zatura_link_destination_type_t destination_type;
  char* value;              /**< Value */
  unsigned int page_number; /**< Page number */
  double left;              /**< Left coordinate */
  double right;             /**< Right coordinate */
  double top;               /**< Top coordinate */
  double bottom;            /**< Bottom coordinate */
  double zoom;              /**< Zoom */
} zatura_link_target_t;

/**
 * Link
 */
typedef struct zatura_link_s zatura_link_t;

/**
 * Index element
 */
typedef struct zatura_index_element_s {
  char* title; /**< Title of the element */
  zatura_link_t* link;
} zatura_index_element_t;

/**
 * Form type
 */
typedef enum zatura_form_type_e {
  ZATURA_FORM_CHECKBOX, /**< Checkbox */
  ZATURA_FORM_TEXTFIELD /**< Textfield */
} zatura_form_type_t;

/**
 * Form element
 */
typedef struct zatura_form_s {
  zatura_rectangle_t position; /**< Position */
  zatura_form_type_t type;     /**< Type */
} zatura_form_t;

/**
 * Jump
 */
typedef struct zatura_jump_s {
  double x;
  double y;
  unsigned int page;
} zatura_jump_t;

/**
 * Create new index element
 *
 * @param title Title of the index element
 * @return Index element
 */
ZATURA_PLUGIN_API zatura_index_element_t* zatura_index_element_new(const char* title);

/**
 * Free index element
 *
 * @param index The index element
 */
ZATURA_PLUGIN_API void zatura_index_element_free(zatura_index_element_t* index);

/**
 * Creates a list that should be used to store \ref
 * zatura_document_information_entry_t entries
 *
 * @return A list or NULL if an error occurred
 */
ZATURA_PLUGIN_API girara_list_t* zatura_document_information_entry_list_new(void);

/**
 * Creates a new document information entry
 *
 * @param type The type
 * @param value The value
 *
 * @return A new entry or NULL if an error occurred
 */
ZATURA_PLUGIN_API zatura_document_information_entry_t*
zatura_document_information_entry_new(zatura_document_information_type_t type, const char* value);

/**
 * Frees a document information entry
 *
 * @param entry The entry that should be freed
 */
ZATURA_PLUGIN_API void zatura_document_information_entry_free(void* entry);

/**
 * Context for MIME type detection
 */
typedef struct zatura_content_type_context_s zatura_content_type_context_t;

/**
 * Device scaling structure.
 */
typedef struct zatura_device_factors_s {
  double x;
  double y;
} zatura_device_factors_t;

/**
 * Signature state
 */
typedef enum zatura_signature_state_e {
  ZATURA_SIGNATURE_INVALID,
  ZATURA_SIGNATURE_VALID,
  ZATURA_SIGNATURE_CERTIFICATE_UNTRUSTED,
  ZATURA_SIGNATURE_CERTIFICATE_EXPIRED,
  ZATURA_SIGNATURE_CERTIFICATE_REVOKED,
  ZATURA_SIGNATURE_CERTIFICATE_INVALID,
  ZATURA_SIGNATURE_ERROR,
} zatura_signature_state_t;

static inline void zatura_check_set_error(zatura_error_t* error, zatura_error_t code) {
  if (error != NULL) {
    *error = code;
  }
}
/**
 * Signature information
 */
typedef struct zatura_signature_info_s {
  char* signer;
  GDateTime* time;
  zatura_rectangle_t position;
  zatura_signature_state_t state;
} zatura_signature_info_t;

/**
 *  Creates a new siganture info.
 *
 * @return A new signature info or NULL if an error occurred
 */
ZATURA_PLUGIN_API zatura_signature_info_t* zatura_signature_info_new(void);

/**
 * Frees a signature info
 *
 * @param signature The signature info to be freed
 */
ZATURA_PLUGIN_API void zatura_signature_info_free(zatura_signature_info_t* signature);

/**
 * Quickmark list entry
 */
struct zatura_mark_s {
  int key;           /**> Marks key */
  double position_x; /**> Horizontal adjustment */
  double position_y; /**> Vertical adjustment */
  unsigned int page; /**> Page number */
  double zoom;       /**> Zoom level */
};

typedef struct zatura_mark_s zatura_mark_t;

typedef enum document_widget_mode_e {
  DOCUMENT_WIDGET_GRID,
  DOCUMENT_WIDGET_SINGLE,
  DOCUMENT_WIDGET_MODE_COUNT,
} document_widget_mode_t;

#endif // TYPES_H
