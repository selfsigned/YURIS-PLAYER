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
#include "encoding.h"


// helpers //

static inline bool read_u8(const uint8_t *data, size_t *offset, size_t max, uint8_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset >= max) return false;

    *out = data[*offset];
    *offset += 1;
    return true;
}

static inline bool read_u16(const uint8_t *data, size_t *offset, size_t max, uint16_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset + 2 > max) return false;

    uint16_t v = (uint16_t)data[*offset] |
                 (uint16_t)data[*offset + 1] << 8;
    *offset += 2;
    *out = v;
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

static inline bool read_u64(const uint8_t *data, size_t *offset, size_t max, uint64_t *out) {
    if (!data || !offset || !out) return false;
    if (*offset + 8 > max) return false;

    uint32_t lo, hi;
    if (!read_u32(data, offset, max, (uint32_t *)&lo)) return false;
    if (!read_u32(data, offset, max, (uint32_t *)&hi)) return false;
    *out = ((uint64_t)hi) << 32 | (uint64_t)lo;
    return true;
}

/// @brief read null terminated strings safely
/// @param offset external cursor in data buffer
/// @param max maximum offset to read from *data
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

/// @brief read fixed length chars and add a null terminator (not null terminated in input).
/// @param out should be at least field_len + 1 (for NULL terminator)
/// @param field_len exact length of the field in the input data
/// @return true if the field was fully read and output was null terminated.
static inline bool read_char_fixed(const uint8_t *data, size_t *offset, size_t max,
                                   char *out, size_t *out_len, size_t field_len)
{
    if (!data || !offset || !out || field_len == 0) return false;
    if (*offset >= max) return false;
    if (*offset + field_len > max) return false;

    memcpy(out, data + *offset, field_len);
    out[field_len] = '\0';

    *offset += field_len;
    if (out_len) *out_len = field_len;
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

// TODO: handle <3xx engine versions (e.g v255)
int parse_ystl(const uint8_t *data, size_t size, struct yuris_script_list *out) {
    if (!data || !out ) goto fail_invalid;
    size_t offset = 0;

    if (!read_u32(data, &offset, size, (uint32_t *)&out->magic) || strncmp(out->magic, "YSTL", 4) != 0) return -EBADMSG;
    if (!read_u32(data, &offset, size, &out->version)) goto fail_readsize;
    if (!read_u32(data, &offset, size, &out->script_count)) goto fail_readsize;
    if (out->script_count > YSTL_MAX_SCRIPTS) goto fail_quantity;

    for (uint32_t i = 0; i < out->script_count; ++i) {
        if (offset >= size) goto fail_readsize;
        struct ystl_script *script = &out->scripts[i];

        if (!read_u32(data, &offset, size, &script->idx)) goto fail_readsize;
        if (!read_u32(data, &offset, size, &script->path_length)) goto fail_readsize;
        if (script->path_length == 0 || script->path_length >= YSTL_MAX_PATH_LEN) goto fail_invalid;

        size_t path_read;
        char sjis_path[YSTL_MAX_PATH_LEN];
        if (!read_char_fixed(data, &offset, size, sjis_path, &path_read, script->path_length)) goto fail_readsize;
        if (path_read != script->path_length) goto fail_invalid;

        char *utf8_path = cp932_str_to_utf8(sjis_path);
        if (!utf8_path) return (errno) ? -errno : -EILSEQ;

        strncpy(script->path, utf8_path, YSTL_MAX_PATH_LEN*4);
        free(utf8_path);

        if (!read_u64(data, &offset, size, &script->_modification_time)) goto fail_readsize;
        if (!read_u32(data, &offset, size, &script->variable_count)) goto fail_readsize;
        if (!read_u32(data, &offset, size, &script->label_count)) goto fail_readsize;
        if (!read_u32(data, &offset, size, &script->text_count)) goto fail_readsize;
    }

    return 0;

    fail_invalid:
        ERROR("Invalid YSTL data\n");
        return -EINVAL;
    fail_quantity:
        ERROR("YSTL script amount exceeds maximum supported\n");
        return -EINVAL;
    fail_readsize:
        ERROR("Unexpected end of data while parsing YSTL\n");
        return -EINVAL;
}

int parse_ysv(const uint8_t *data, size_t size, struct yuris_variables *out) {
    if (!data || !out ) goto fail_invalid;
    size_t offset = 0;

    if (!read_u32(data, &offset, size, (uint32_t *)&out->magic) || strncmp(out->magic, "YSVR", 4) != 0) goto fail_invalid;
    if (!read_u32(data, &offset, size, &out->version)) goto fail_readsize;
    if (!read_u16(data, &offset, size, &out->variable_count)) goto fail_readsize;

    uint16_t max_var_idx = 0;
    out->variables = calloc(out->variable_count, sizeof(struct ysv_variable));
    if (!out->variables) goto fail_alloc;
    for (uint16_t i = 0; i < out->variable_count; ++i) {
        if (offset >= size) goto fail_readsize;
        struct ysv_variable *var = &out->variables[i];

        if (!read_u8(data, &offset, size, (uint8_t *)&var->scope)) goto fail_readsize;
        if (var->scope > YSV_SCOPE_FUNCTION) goto fail_invalid;
        if (out->version >= 481)
            if (!read_u8(data, &offset, size, &var->_global_or_user)) goto fail_readsize;
        if (!read_u16(data, &offset, size, &var->script_idx)) goto fail_readsize;
        if (!read_u16(data, &offset, size, &var->variable_idx)) goto fail_readsize;
        if (!read_u8(data, &offset, size, (uint8_t *)&var->type)) goto fail_readsize;
        if (var->type > YSV_EXPR) goto fail_invalid;

        if (!read_u8(data, &offset, size, &var->dimension_size)) goto fail_readsize;
        if (var->dimension_size > MAX_YSV_ARR_DIMENSIONS) goto fail_dim_qty;
        for (uint8_t dim_i = 0; dim_i < var->dimension_size; ++dim_i)
            if (!read_u32(data, &offset, size, &var->dimensions[dim_i])) goto fail_readsize;

        switch (var->type) {
            case YSC_ARG_ANY:
                break;
            case YSC_ARG_INT:
            case YSC_ARG_FLOAT:
                if (!read_u64(data, &offset, size, (uint64_t *)&var->initial_value)) goto fail_readsize;
                break;

            case YSC_ARG_STR:
                if (!read_u16(data, &offset, size, &var->initial_value.expr_val.length)) goto fail_readsize;

                var->initial_value.expr_val.expr = calloc(var->initial_value.expr_val.length, sizeof(uint8_t));
                if (!var->initial_value.expr_val.expr) goto fail_alloc;

                for (uint16_t expr_i = 0; expr_i < var->initial_value.expr_val.length; ++expr_i)
                    if (!read_u8(data, &offset, size, &var->initial_value.expr_val.expr[expr_i])) goto fail_readsize;
                break;
            default:
                goto fail_invalid;
        }

        if (var->variable_idx > max_var_idx) max_var_idx = var->variable_idx;
    }


    out->lookup = calloc(max_var_idx + 1, sizeof(struct ysv_variable *));
    for (uint16_t i = 0; i < out->variable_count; ++i) {
        struct ysv_variable *var = &out->variables[i];
        if (var->variable_idx <= max_var_idx)
            out->lookup[var->variable_idx] = var;
    }


    return 0;

    fail_invalid:
        ERROR("Invalid YSV data\n");
        return -EINVAL;
    fail_readsize:
        ERROR("Unexpected end of data while parsing YSV\n");
        return -EINVAL;
    fail_dim_qty:
        ERROR("YSV variable dimension size exceeds maximum supported\n");
        return -EINVAL;
    fail_alloc:
        ERROR("Failed to allocate memory for YSV variable expression\n");
        return -ENOMEM;
}

void free_ysv(struct yuris_variables *ysv) {
    if (!ysv) return;
    for (uint16_t i = 0; i < ysv->variable_count; ++i) {
        struct ysv_variable *var = &ysv->variables[i];
        if (var->type == YSV_EXPR && var->initial_value.expr_val.expr) {
            free(var->initial_value.expr_val.expr);
            var->initial_value.expr_val.expr = NULL;
        }
    }
    free(ysv->variables);
    free(ysv->lookup);
    ysv->variables = NULL;
    ysv->lookup = NULL;
}

int parse_ysl(const uint8_t *data, size_t size, struct yuris_labels *out) {
    if (!data || !out ) goto fail_invalid;
    size_t offset = 0;

    if (!read_u32(data, &offset, size, (uint32_t *)&out->magic) || strncmp(out->magic, "YSLB", 4) != 0) goto fail_invalid;
    if (!read_u32(data, &offset, size, &out->version)) goto fail_readsize;
    if (!read_u32(data, &offset, size, &out->label_count)) goto fail_readsize;
    offset += 256 * 4; // skip [0x100], engine's internal label lookup thing

    out->labels = calloc(out->label_count, sizeof(struct ysl_label));
    if (!out->labels) goto fail_alloc;
    for (uint32_t i = 0; i < out->label_count; ++i) {
        if (offset >= size) goto fail_readsize;
        struct ysl_label *label = &out->labels[i];

        uint8_t len = 0;
        size_t name_read = 0;
        if (!read_u8(data, &offset, size, &len)) goto fail_readsize;
        char *sjis_name = malloc(len + 1);
        if (!sjis_name) goto fail_alloc ;
        if (!read_char_fixed(data, &offset, size, sjis_name, &name_read, len) || name_read != len) {
            free(sjis_name);
            goto fail_readsize;
        }
        out->labels[i].name = cp932_str_to_utf8(sjis_name);
        free(sjis_name);

        if (!read_u32(data, &offset, size, &label->id)) goto fail_readsize;
        if (!read_u32(data, &offset, size, &label->ip)) goto fail_readsize;
        if (!read_u16(data, &offset, size, &label->script_idx)) goto fail_readsize;

        if (!read_u8(data, &offset, size, &label->if_lvl)) goto fail_readsize;
        if (!read_u8(data, &offset, size, &label->loop_lvl)) goto fail_readsize;
    }

    return 0;

    fail_invalid:
        ERROR("Invalid YSL data\n");
        return -EINVAL;
    fail_readsize:
        ERROR("Unexpected end of data while parsing YSL\n");
        return -EINVAL;
    fail_alloc:
        ERROR("Failed to allocate memory for YSL variable expression\n");
        return -ENOMEM;
}

void free_ysl(struct yuris_labels *ysl) {
    if (!ysl) return;
    for (uint32_t i = 0; i < ysl->label_count; ++i) {
        free(ysl->labels[i].name);
        ysl->labels[i].name = NULL;
    }
    free(ysl->labels);
    ysl->labels = NULL;
}