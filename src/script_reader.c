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

#include "script_reader.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "utils.h"


// helpers //

static inline bool read_u8(const uint8_t *data, size_t *offset, size_t max, uint8_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset >= max) return false;

    *out = data[*offset];
    (*offset) += 1;
    return true;
}

static inline bool read_u32(const uint8_t *data, size_t *offset, size_t max, uint32_t *out) {
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

static inline bool read_str(const uint8_t *data, size_t *offset, size_t max, char *out, size_t *out_len, size_t max_len) {
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


// parsing //

int parse_ysc(const uint8_t *data, size_t size, struct yuris_commands *out) {
    if (!data || size <= sizeof(uint32_t)*4 || !out ) goto fail_invalid;
    else if (strncmp((const char *)data, "YSCM", 4) != 0) return -EBADMSG;

    size_t offset = 0;
    for (offset = 0; offset < 4; ++offset)
        out->magic[offset] = data[offset];

    if (!read_u32(data, &offset, size, &out->version)) goto fail_readsize;
    if (!read_u32(data, &offset, size, &out->command_count)) goto fail_readsize;
    if (!read_u32(data, &offset, size, &out->padding)) goto fail_readsize;
    if (out->command_count > MAX_YSC_COMMANDS) goto fail_quantity;
    if (out->padding) goto fail_invalid;
    
    for (uint32_t i = 0; i < out->command_count; ++i) {
        if (offset >= size) goto fail_readsize;
        struct ysc_command *cmd = &out->commands[i];

        size_t len = 0;
        if (!read_str(data, &offset, size, cmd->name, &len, MAX_YSC_FUNCNAME_LEN) || len == 0) goto fail_readsize;
        if (!read_u8(data, &offset, size, &cmd->arg_count)) goto fail_readsize;
        if (cmd->arg_count > MAX_YSC_ARGS) goto fail_quantity;

        for (uint8_t arg_i = 0; arg_i < cmd->arg_count; ++arg_i) {
            if (offset >= size) goto fail_readsize;
            struct ysc_arg *arg = &cmd->args[arg_i];

            if (!read_str(data, &offset, size, arg->name, NULL, MAX_YSC_FUNCNAME_LEN)) goto fail_readsize;
            if (!read_u8(data, &offset, size, (uint8_t *)&arg->type)) goto fail_readsize;
            if (arg->type > YSC_ARG_STR) goto fail_invalid;
            if (!read_u8(data, &offset, size, &arg->chk)) goto fail_readsize;
        }
    }

    // TODO parse errors (CP932 encoded) and b256
    
    return 0;

    fail_invalid:
        ERROR("Invalid YSC data\n");
        return -EINVAL;
    fail_quantity:
        ERROR("YSC command count or argument count exceeds maximum supported\n");
        return -EINVAL;
    fail_readsize:
        ERROR("Unexpected end of data while parsing YSC\n");
        return -EINVAL;
}