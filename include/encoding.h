
/* YURIS-PLAYER - An open source YU-RIS engine reimplementation
Copyright (C) 2026 Selfsigned <me@selfsigned.dev>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef ENCODING_H
#define ENCODING_H

#include <stddef.h>

/// @brief Convert a CP932 buffer to a new null-terminated UTF-8 string.
/// @return UTF-8 that the caller should free, NULL on error
char *cp932_to_utf8(const char *input, size_t input_len, size_t *out_len);

/// @brief Convert a NULL-terminated CP932 string to a new NULL-terminated UTF-8 string.
/// @return UTF-8 that the caller should free, NULL on error
char *cp932_str_to_utf8(const char *input);

#endif
