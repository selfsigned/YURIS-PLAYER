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

#include "expr.h"

#include <errno.h>
#include <math.h>

#include "utils.h"
#include "encoding.h"

#define PUSH(v) do { \
    if (sp >= EXPR_STACK_SIZE) goto fail_oob; \
    stack[sp++] = (v); \
} while (0)

#define POP(v) do { \
    if (sp == 0) goto fail_oob; \
    (v) = stack[--sp]; \
} while (0)



// Types //

static double to_float(struct expr_value val) {
    if (val.type == EXPR_FLOAT) return val.value.f;
    if (val.type == EXPR_INT) return (double)val.value.i;
    if (val.type == EXPR_STR) {
        char *endptr;
        double f = strtod(val.value.s.ptr, &endptr);
        if (endptr != val.value.s.ptr) return f;
    }
    return 0.0;
}

static int64_t to_int(struct expr_value val) {
    if (val.type == EXPR_INT) return val.value.i;
    if (val.type == EXPR_FLOAT) return (int64_t)val.value.f;
    if (val.type == EXPR_STR) {
        char *endptr;
        int64_t i = strtoll(val.value.s.ptr, &endptr, 10);
        if (endptr != val.value.s.ptr) return i;
    }
    return 0;
}

static size_t nbr_to_str(struct expr_value val, char *out) {
    if (val.type == EXPR_INT) {
        return snprintf(out, NUMBER_STR_BUF_SIZE, "%ld", val.value.i);
    } else if (val.type == EXPR_FLOAT) {
        return snprintf(out, NUMBER_STR_BUF_SIZE, "%f", val.value.f);
    }
    out[0] = '\0';
    return 0;
}

static struct expr_value str_to_nbr(struct expr_value val) {
    if (val.type != EXPR_STR) return val;

    char *endptr;
    int64_t i = strtoll(val.value.s.ptr, &endptr, 10);
    if (endptr != val.value.s.ptr && *endptr == '\0') {
        return INT_V(i);
    } else {
        double f = strtod(val.value.s.ptr, &endptr);
        if (endptr != val.value.s.ptr && *endptr == '\0')
            return FLT_V(f);
    }
    return INT_V(0);
}

// Operations //

// arithmetic
static struct expr_value op_add(struct expr_value a, struct expr_value b) {
    if (a.type == EXPR_STR || b.type == EXPR_STR) {
       char a_str_buf[NUMBER_STR_BUF_SIZE];
       char b_str_buf[NUMBER_STR_BUF_SIZE];
       const char *a_str = (a.type == EXPR_STR)
           ? a.value.s.ptr
           : (nbr_to_str(a, a_str_buf), a_str_buf);
       const char *b_str = (b.type == EXPR_STR)
           ? b.value.s.ptr
           : (nbr_to_str(b, b_str_buf), b_str_buf);
       const size_t a_len = (a.type == EXPR_STR) ? a.value.s.len : strlen(a_str);
       const size_t b_len = (b.type == EXPR_STR) ? b.value.s.len : strlen(b_str);

       size_t str_buf_size = a_len + b_len + 1;
       char *dest = malloc(str_buf_size);
       if (!dest) return STR_V(NULL, 0);
       snprintf(dest, str_buf_size, "%s%s", a_str, b_str);

       return (STR_V(dest, str_buf_size - 1));
    } else if (a.type != EXPR_INT || b.type != EXPR_INT)
        return FLT_V(to_float(a) + to_float(b));
    return INT_V(to_int(a) + to_int(b));
}
static struct expr_value op_mul(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return FLT_V(to_float(a) * to_float(b));
    return INT_V(to_int(a) * to_int(b));
}
static struct expr_value op_div(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return FLT_V(to_float(a) / to_float(b));
    return INT_V(to_int(b) != 0 ? to_int(a) / to_int(b) : 0);
}
static struct expr_value op_mod(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return FLT_V(fmod(to_float(a), to_float(b)));
    return INT_V(to_int(b) != 0 ? to_int(a) % to_int(b) : 0);
}
static struct expr_value op_sub(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return FLT_V(to_float(a) - to_float(b));
    return INT_V(to_int(a) - to_int(b));
}
static struct expr_value op_bit_and(struct expr_value a, struct expr_value b) {
    return INT_V(to_int(a) & to_int(b));
}
static struct expr_value op_bit_or(struct expr_value a, struct expr_value b) {
    return INT_V(to_int(a) | to_int(b));
}
static struct expr_value op_bit_xor(struct expr_value a, struct expr_value b) {
    return INT_V(to_int(a) ^ to_int(b));
}
// comparison
static struct expr_value op_lt(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return INT_V(to_float(a) < to_float(b));
    return INT_V(to_int(a) < to_int(b));
}
static struct expr_value op_lte(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return INT_V(to_float(a) <= to_float(b));
    return INT_V(to_int(a) <= to_int(b));
}
static struct expr_value op_gt(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return INT_V(to_float(a) > to_float(b));
    return INT_V(to_int(a) > to_int(b));
}
static struct expr_value op_gte(struct expr_value a, struct expr_value b) {
    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return INT_V(to_float(a) >= to_float(b));
    return INT_V(to_int(a) >= to_int(b));
}
static struct expr_value op_eq(struct expr_value a, struct expr_value b) {
    if (a.type == b.type) {
        switch (a.type) {
            case EXPR_INT:
                return INT_V(a.value.i == b.value.i);
            case EXPR_FLOAT:
                return INT_V(a.value.f == b.value.f);
            case EXPR_STR:
                return INT_V(a.value.s.len == b.value.s.len && strcmp(a.value.s.ptr, b.value.s.ptr) == 0);
        }
    }
    if (a.type == EXPR_STR) a = str_to_nbr(a);
    if (b.type == EXPR_STR) b = str_to_nbr(b);

    if (a.type != EXPR_INT || b.type != EXPR_INT)
        return INT_V(to_float(a) == to_float(b));
    return INT_V(a.value.i == b.value.i);
}
static struct expr_value op_neq(struct expr_value a, struct expr_value b) {
    return INT_V(!op_eq(a, b).value.i);
}
static struct expr_value op_log_and(struct expr_value a, struct expr_value b) {
    return INT_V(to_int(a) && to_int(b));
}
static struct expr_value op_log_or(struct expr_value a, struct expr_value b) {
    return INT_V(to_int(a) || to_int(b));
}
static struct expr_value(*const op_table[])(struct expr_value, struct expr_value) = {
    // arithmetic
    [OP_ADD] = op_add,
    [OP_MUL] = op_mul,
    [OP_DIV] = op_div,
    [OP_MOD] = op_mod,
    [OP_SUB] = op_sub,
    [OP_BIT_OR] = op_bit_or,
    [OP_BIT_AND] = op_bit_and,
    [OP_BIT_XOR] = op_bit_xor,
    // comparison
    [OP_LT] = op_lt,
    [OP_GT] = op_gt,
    [OP_LTE] = op_lte,
    [OP_GTE] = op_gte,
    [OP_NEQ] = op_neq,
    [OP_EQ] = op_eq,
    [OP_LOG_OR] = op_log_or,
    [OP_LOG_AND] = op_log_and,
};

// expr eval //

int eval_expr(const uint8_t *expr, const size_t len, struct expr_value *out) {
    if (!expr || !out) return -EINVAL;
    size_t ret = 0;

    struct expr_value stack[EXPR_STACK_SIZE] = {0};
    size_t offset = 0, sp = 0;

    while (offset < len) {
        uint8_t opcode;
        uint16_t arg_len;
        if (!read_u8(expr, &offset, len, &opcode)) goto fail_read;
        if (!read_u16(expr, &offset, len, &arg_len)) goto fail_read;

        switch (opcode) {
            case OP_PUSH_I8: {
                uint8_t val;
                if (!read_u8(expr, &offset, len, &val)) goto fail_read;
                PUSH(INT_V((int8_t)val));
                break;
            }
            case OP_PUSH_I16: {
                uint16_t val;
                if (!read_u16(expr, &offset, len, &val)) goto fail_read;
                PUSH(INT_V((int16_t)val));
                break;
            }
            case OP_PUSH_I32: {
                uint32_t val;
                if (!read_u32(expr, &offset, len, &val)) goto fail_read;
                PUSH(INT_V((int32_t)val));
                break;
            }
            case OP_PUSH_I64: {
                uint64_t val;
                if (!read_u64(expr, &offset, len, &val)) goto fail_read;
                PUSH(INT_V((int64_t)val));
                break;
            }
            case OP_PUSH_F64: {
                uint64_t val;
                if (!read_u64(expr, &offset, len, &val)) goto fail_read;
                double f;
                memcpy(&f, &val, sizeof(double));
                PUSH(FLT_V(f));
                break;
            }
            case OP_PUSH_STR: {
                char *str = malloc(arg_len + 1);
                if (!str) goto fail_alloc;
                memcpy(str, expr + offset, arg_len);
                str[arg_len] = '\0';

                size_t data_len = arg_len;
                char *utf8_str = str;
                char q = utf8_str[0];
                if (data_len >= 2 && (q == '"' || q == '\'') && utf8_str[data_len - 1] == q) {
                    utf8_str[data_len - 1] = '\0';
                    utf8_str++;
                    data_len -= 2;
                }
                if (data_len == 0) {
                    char *empty_str = strdup("");
                    if (!empty_str) goto fail_alloc;
                    free(str);
                    offset += arg_len;
                    PUSH(STR_V(empty_str, 0));
                    break;
                }

                size_t out_len = 0;
                utf8_str = cp932_to_utf8(utf8_str, data_len, &out_len);
                free(str);
                if (!utf8_str || out_len == 0) goto fail_encode;

                for (size_t i = 0; i < out_len; ++i) {
                    if (utf8_str[i] == '\\' && i + 1 < out_len) {
                        char code = utf8_str[i + 1];
                        switch (code) {
                            case '\\': utf8_str[i] = '\\'; break;
                            case 'n': utf8_str[i] = '\n'; break;
                            case 't': utf8_str[i] = '\t'; break;
                            default: continue;
                        }
                        memmove(&utf8_str[i + 1], &utf8_str[i + 2], out_len - (i + 1));
                        out_len--;
                    }
                }

                offset += arg_len;
                PUSH(STR_V(utf8_str, out_len));
                break;
            }

            case OP_PUSH_VAR_SCALAR:
            case OP_PUSH_VAR_ARRAY:
            case OP_IDX_BEG:
            case OP_IDX_END: {
                WARN("0x%02X opcode not implemented, skipping\n", opcode);
                offset += arg_len;
                break;
            }

            case OP_ADD:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
            case OP_SUB:
            case OP_BIT_AND:
            case OP_BIT_OR:
            case OP_BIT_XOR:
            case OP_LT:
            case OP_LTE:
            case OP_GT:
            case OP_GTE:
            case OP_EQ:
            case OP_NEQ:
            case OP_LOG_AND:
            case OP_LOG_OR: {
                if (sp < 2) goto fail_read;
                struct expr_value b, a;
                POP(b);
                POP(a);

                PUSH(op_table[opcode](a, b));
                FREE_EXPR(a);
                FREE_EXPR(b);
                break;
            }

            case OP_NEG: {
                if (sp < 1) goto fail_read;
                struct expr_value val;
                POP(val);

                if (val.type == EXPR_INT)
                    PUSH(INT_V(-to_int(val)));
                else if (val.type == EXPR_FLOAT)
                    PUSH(FLT_V(-to_float(val)));
                else
                    PUSH(INT_V(-to_int(val)));

                FREE_EXPR(val);
                break;
            }
            case OP_TO_STR: {
                if (sp < 1) goto fail_read;
                struct expr_value val;
                POP(val);

                if (val.type == EXPR_STR) {
                    PUSH(val);
                    break;
                }

                char num_str_buf[NUMBER_STR_BUF_SIZE];
                size_t num_str_len = nbr_to_str(val, num_str_buf);
                char *num_str = strdup(num_str_buf);
                if (!num_str) goto fail_alloc;

                PUSH(STR_V(num_str, num_str_len));
                break;
            }
            case OP_TO_NUM: {
                if (sp < 1) goto fail_read;
                struct expr_value val;
                POP(val);
                PUSH(str_to_nbr(val));
                FREE_EXPR(val);
                break;
            }


            case OP_NOP:
                break;
            default:
                offset += arg_len;
                WARN("Unknown opcode in expression: 0x%02X\n", opcode);
        }
    }

    *out = stack[sp > 0 ? --sp : 0];
    goto cleanup;

    fail_read:
        ERROR("Unexpected end of expression data\n");
        ret = -EINVAL;
        goto cleanup;
    fail_oob:
        ERROR("Expression stack under/overflow\n");
        ret = -EOVERFLOW;
        goto cleanup;
    fail_alloc:
        ERROR("Failed to allocate memory for expression string\n");
        ret = -ENOMEM;
        goto cleanup;
    fail_encode:
        ERROR("Failed to convert expression string to UTF-8\n");
        ret = -EINVAL;
        goto cleanup;
    cleanup:
        for (size_t i = 0; i < sp; ++i)
            FREE_EXPR(stack[i]);

    return ret;
}