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
#include "encoding.h"

#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

char *cp932_to_utf8(const char *input, size_t input_len, size_t *out_len) {
    if (!input || input_len == 0) return NULL;

    iconv_t descriptor = iconv_open("UTF-8", "CP932");
    if (descriptor == (iconv_t)-1) return NULL;

    size_t out_size = input_len * 4 + 1;
    char *output = malloc(out_size);
    if (!output) {
        iconv_close(descriptor);
        return NULL;
    }
    size_t out_left = out_size;

    char *out_ptr = output;
    char *in_ptr = (char *)input;
    size_t ret = iconv(descriptor, &in_ptr, &input_len, &out_ptr, &out_left);
    iconv_close(descriptor);

    if (ret == (size_t)-1) {
        free(output);
        return NULL;
    }

    size_t written = out_size - out_left;
    *out_ptr = '\0';

    if (out_len) *out_len = written;
    return output;
}

char *cp932_str_to_utf8(const char *input) {
    if (!input)
        return NULL;
    return cp932_to_utf8(input, strlen(input), NULL);
}
