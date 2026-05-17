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

#ifndef EXPR_H
#define EXPR_H

/// @file expr.h
/// @brief re-implementation of the stack-based expression VM
/// The first byte is the Opcode, followed by a 16 bit argument length

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#define EXPR_STACK_SIZE 256
#define NUMBER_STR_BUF_SIZE 64

// Helpers to create new expr vals without syntax bloat
#define INT_V(v) ((struct expr_value){.type = EXPR_INT, .value.i = (v)})
#define FLT_V(v) ((struct expr_value){.type = EXPR_FLOAT, .value.f = (v)})
#define STR_V(p,l) ((struct expr_value){.type = EXPR_STR, .value.s = {.ptr = (p), .len = (l)}})
#define FREE_EXPR(v) do { \
    if ((v).type == EXPR_STR) { \
        if ((v).value.s.ptr) \
            free((v).value.s.ptr); \
        (v).value.s.ptr = NULL; \
        (v).value.s.len = 0; \
    } \
} while (0)

enum expr_opcode {
    OP_PUSH_I8          = 0x42,
    OP_PUSH_I16         = 0x57,
    OP_PUSH_I32         = 0x49,
    OP_PUSH_I64         = 0x4C,
    OP_PUSH_F64         = 0x46,
    OP_PUSH_STR         = 0x4D,
    OP_PUSH_VAR_SCALAR  = 0x48,
    OP_PUSH_VAR_ARRAY   = 0x76,

    OP_IDX_BEG = 0x56,
    OP_IDX_END = 0x29,

    OP_NEG = 0x52,
    OP_TO_STR = 0x73,
    OP_TO_NUM = 0x69,

    OP_MUL = 0x2A,
    OP_DIV = 0x2F,
    OP_MOD = 0x25,
    OP_ADD = 0x2B,
    OP_SUB = 0x2D,

    OP_LT   = 0x3C,
    OP_LTE  = 0x53,
    OP_GT   = 0x3E,
    OP_GTE  = 0x5A,
    OP_EQ   = 0x3D,
    OP_NEQ  = 0x21,

    OP_BIT_AND  = 0x41,
    OP_BIT_OR   = 0x4F,
    OP_BIT_XOR  = 0x5E,

    OP_LOG_AND  = 0x26,
    OP_LOG_OR   = 0x7C,

    OP_NOP  = 0x2C,
};

struct expr_value {
    enum {
        EXPR_NONE,
        EXPR_INT,
        EXPR_FLOAT,
        EXPR_STR,
    } type;
    union {
        int64_t i;
        double f;
        struct {
            uint32_t len;
            char *ptr; ///< NULL-terminated UTF-8
        } s;
    } value;
};

int eval_expr(const uint8_t *expr, const size_t len, struct expr_value *out);

#endif 