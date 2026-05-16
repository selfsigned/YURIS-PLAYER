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

#ifndef UTILS_H
#define UTILS_H
/// @file utils.h
/// @brief smol utility functions and macro

#include <stdbool.h>
#include <SDL2/SDL.h>

// posix stuff //
#include <limits.h>
#ifndef NAME_MAX // lol @ windows in 2026, use posix n00bs
#define NAME_MAX 255 
#endif

#define MAX_PATH_LEN NAME_MAX + 1


// Log macros //
#define DEBUG(...) SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define INFO(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define WARN(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define ERROR(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)


// Helper functions //

// Bound checked read helpers
bool read_u8(const uint8_t *data, size_t *offset, size_t max, uint8_t *out);
bool read_u16(const uint8_t *data, size_t *offset, size_t max, uint16_t *out);
bool read_u32(const uint8_t *data, size_t *offset, size_t max, uint32_t *out);
bool read_u64(const uint8_t *data, size_t *offset, size_t max, uint64_t *out);
/// @brief read null terminated strings safely
/// @param offset external cursor in data buffer
/// @param max maximum offset to read from *data
bool read_str(const uint8_t *data, size_t *offset, size_t max, char *out, size_t *out_len, size_t max_len);
/// @brief read fixed length and add a null terminator (not null terminated in input).
/// @param out should be at least field_len + 1 (for NULL terminator)
/// @param field_len exact length of the field in the input data
/// @return true if the field was fully read and output was null terminated.
bool read_fixed(const uint8_t *data, size_t *offset, size_t max,
                                   char *out, size_t *out_len, size_t field_len);

#endif