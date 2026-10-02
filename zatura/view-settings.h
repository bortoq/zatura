/* SPDX-License-Identifier: Zlib */
#ifndef ZATURA_VIEW_SETTINGS_H
#define ZATURA_VIEW_SETTINGS_H
#include "types.h"
char* zatura_view_settings_capture(zathura_t* app, zathura_document_t* document);
void zatura_view_settings_restore(zathura_t* app, const char* json);
void zatura_view_settings_restore_layout(zathura_document_t* document, const char* json);
zathura_adjust_mode_t zatura_view_settings_adjust_mode(const char* json);
#endif
