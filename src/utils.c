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

#include "utils.h"

bool read_u8(const uint8_t *data, size_t *offset, size_t max, uint8_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset >= max) return false;

    *out = data[*offset];
    *offset += 1;
    return true;
}

bool read_u16(const uint8_t *data, size_t *offset, size_t max, uint16_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset + 2 > max) return false;

    uint16_t v = (uint16_t)data[*offset] |
                 (uint16_t)data[*offset + 1] << 8;
    *offset += 2;
    *out = v;
    return true;
}

bool read_u32(const uint8_t *data, size_t *offset, size_t max, uint32_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset + 4 > max) return false;

    uint32_t v = (uint32_t)data[*offset] |
                 (uint32_t)data[*offset + 1] << 8 |
                 (uint32_t)data[*offset + 2] << 16 |
                 (uint32_t)data[*offset + 3] << 24;
    *offset += 4;
    *out = v;
    return true;
}

bool read_u64(const uint8_t *data, size_t *offset, size_t max, uint64_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset + 8 > max) return false;

    uint32_t lo, hi;
    if (!read_u32(data, offset, max, (uint32_t *)&lo)) return false;
    if (!read_u32(data, offset, max, (uint32_t *)&hi)) return false;
    *out = ((uint64_t)hi) << 32 | (uint64_t)lo;
    return true;
}

bool read_str(const uint8_t *data, size_t *offset, size_t max, char *out, size_t *out_len, size_t max_len) {
    if (!data || !offset || !out || max_len == 0) return false;
    if (*offset >= max) return false;

    size_t off = *offset;
    const uint8_t *end = memchr(data + off, '\0', max - off);
    if (!end) return false;

    size_t len = end - (data + off);
    size_t to_copy = (len < max_len - 1) ? len : (max_len - 1);
    memcpy(out, data + off, to_copy);
    out[to_copy] = '\0';

    *offset = off + len + 1;
    if (out_len) *out_len = len;
    return true;
}

bool read_fixed(const uint8_t *data,
    size_t *offset,
    size_t max,
    char *out,
    size_t *out_len,
    size_t field_len) {
    if (!data || !offset || !out || field_len == 0) return false;
    if (*offset >= max) return false;
    if (*offset + field_len > max) return false;

    memcpy(out, data + *offset, field_len);
    out[field_len] = '\0';

    *offset += field_len;
    if (out_len) *out_len = field_len;
    return true;
}
