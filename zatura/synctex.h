/* SPDX-License-Identifier: Zlib */

#ifndef SYNCTEX_H
#define SYNCTEX_H

#include "types.h"

bool synctex_get_input_line_column(zatura_t* zatura, const char* filename, unsigned int page, int x, int y,
                                   char** input_file, unsigned int* line, unsigned int* column);

void synctex_edit(zatura_t* zatura, const char* editor, zatura_page_t* page, int x, int y);

bool synctex_parse_input(const char* synctex, char** input_file, int* line, int* column);

girara_list_t* synctex_rectangles_from_position(zatura_t* zatura, const char* filename, const char* input_file,
                                                int line, int column, unsigned int* page,
                                                girara_list_t** secondary_rects);

void synctex_highlight_rects(zatura_t* zatura, unsigned int page, girara_list_t** rectangles);

bool synctex_view(zatura_t* zatura, const char* input_file, unsigned int line, unsigned int column);

#endif
