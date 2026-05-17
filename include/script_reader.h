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

#ifndef SCRIPT_READER_H
#define SCRIPT_READER_H

#include <stddef.h>
#include <inttypes.h>

// YSC //

#define MAX_YSC_COMMANDS 255
#define MAX_YSC_FUNCNAME_LEN 64
#define MAX_YSC_ARGS 128
#define MAX_YSC_ERRMSG 128
#define YSC_ERR_STR_COUNT 37

enum ysc_arg_type {
    YSC_ARG_ANY,
    YSC_ARG_INT,
    YSC_ARG_FLOAT,
    YSC_ARG_STR,
};

/// @brief Command definition (ysbin\ysc.ybn)
struct yuris_commands {
    char magic[4];  ///< "YSCM"
    uint32_t version;
    uint32_t command_count;
    uint32_t padding;

    struct ysc_command {
        char name[MAX_YSC_FUNCNAME_LEN];
        uint8_t arg_count;
        struct ysc_arg {
            char name[MAX_YSC_FUNCNAME_LEN];
            enum ysc_arg_type type;
            uint8_t chk;    // TODO purpose unknown
        } args[MAX_YSC_ARGS];
    } commands[MAX_YSC_COMMANDS];

    char errstr[YSC_ERR_STR_COUNT][MAX_YSC_ERRMSG]; // TODO actually CP932 encoded, probably includes ID as well
    uint8_t b256[256];      // TODO purpose unknown
};

/// @param data ptr to the YSC data to parse (incl header)
/// @param size size of the YSC data
/// @return 0 on success, negative ERRNO code on failure
int parse_ysc(const uint8_t *data, size_t size, struct yuris_commands *out);

// YSTL //

#define YSTL_MAX_SCRIPTS 1024
#define YSTL_MAX_PATH_LEN 256

/// @brief Script file entries (ysbin\yst_list.ybn)
struct yuris_script_list {
    char magic[4]; ///< "YSTL"
    uint32_t version;
    uint32_t script_count;
    struct ystl_script {
        uint32_t idx;
        uint32_t path_length;
        char path[YSTL_MAX_PATH_LEN*4]; ///< UTF-8 from SJIS and null-terminated
        uint64_t _modification_time; ///< actually FILETIME
        uint32_t variable_count; ///< 0xFFFFFFFF if non-applicable (engine spec)
        uint32_t label_count;
        uint32_t text_count;
    } scripts[YSTL_MAX_SCRIPTS];
};

/// @param data ptr to the YSTL data to parse
/// @param size size of the YSTL data
/// @return 0 on success, negative ERRNO code on failure
int parse_ystl(const uint8_t *data, size_t size, struct yuris_script_list *out);

// YSV //
#define MAX_YSV_ARR_DIMENSIONS 8

enum ysv_variable_scope {
    YSV_SCOPE_NONE,
    YSV_SCOPE_GLOBAL,
    YSV_SCOPE_STATIC, ///< var is local to a script
    YSV_SCOPE_FUNCTION, ///< var is local to a function
};

enum ysv_variable_type {
    YSV_NONE,
    YSV_INT,
    YSV_FLOAT,
    YSV_EXPR,
};

struct yuris_variables {
    char magic[4]; ///< "YSVR"
    uint32_t version;
    uint16_t variable_count;
    struct ysv_variable {
        enum ysv_variable_scope scope;
        uint8_t _global_or_user; ///< only in ~v255, un-needed
        uint16_t script_idx; ///< index into YSTL.bin that owns this (TODO: validate that assumption)
        uint16_t variable_idx;
        enum  ysv_variable_type type;
        uint8_t  dimension_size;
        uint32_t dimensions[8];  ///< at [0] for 1D arrays, 0 for non-arrays

        /// first variable
        union ysv_variable_value {
            int64_t int_val;
            double float_val;
            struct {
                uint16_t length;
                uint8_t *expr; ///<  expression argument for expression VM
            } expr_val;
        } initial_value;
    } *variables;

    // additions
    uint16_t max_variable_idx; ///< for sizing
    struct ysv_variable **lookup; ///< lookup table by actual variable index
};

/// @param data ptr to the YSV data to parse, free after use with free_ysv()
/// @param size size of the YSV data
/// @return 0 on success, negative ERRNO code on failure
int parse_ysv(const uint8_t *data, size_t size, struct yuris_variables *out);
void free_ysv(struct yuris_variables *ysv);


// YSL //

struct yuris_labels {
    char magic[4]; ///< "YSLB"
    uint32_t version;
    uint32_t label_count;

    struct ysl_label {
        char *name; ///< CP932 converted to NULL-terminated UTF-8, preceded by length(u8) in-file
        uint32_t id;
        uint32_t ip; // TODO offset or IP ? I think IP is cooler
        uint16_t script_idx; ///< index into YSTL.bin
        uint8_t if_lvl;
        uint8_t loop_lvl;
    } *labels;
};

/// @param data ptr to the YSL data to parse, free after use with free_ysl()
/// @param size size of the YSL data
/// @return 0 on success, negative ERRNO code on failure
int parse_ysl(const uint8_t *data, size_t size, struct yuris_labels *out);
void free_ysl(struct yuris_labels *ysl);

// YST (Script) //

#define MAX_YST_ARGS 255 /// TODO could maybe be lowered

enum yst_arg_type {
    YST_ARG_INT = 1,
    YST_ARG_FLT = 2,
    YST_ARG_STR = 3,
    YST_ARG_VREF_UNK = 4,   // base unknown + lvalue flag
    YST_ARG_VREF_INT   = 5,   ///< base=INT + lvalue flag
    YST_ARG_VREF_FLOAT = 6,   ///< base=FLOAT + lvalue flag
    YST_ARG_VREF_STR   = 7,   ///< base=STR + lvalue flag
    YST_ARG_MAX = 7,
};

enum yst_assign_type {
    YST_ASSIGN_EQ, ///< =
    YST_ASSIGN_ADD, ///< +=
    YST_ASSIGN_SUB, ///< -=
    YST_ASSIGN_MUL, ///< *=
    YST_ASSIGN_DIV, ///< /=
    YST_ASSIGN_MOD, ///< %=
    YST_ASSIGN_AND, ///< &=
    YST_ASSIGN_OR,  ///< |=
    YST_ASSIGN_XOR, ///< ^=
};

struct yuris_script {
    char magic[4]; ///< "YSTB"
    uint32_t version;
    uint32_t instruction_count;
    uint32_t instruction_size; ///< instruction_count * 4 (Different in v255?)
    uint32_t args_desc_size;
    uint32_t args_vals_size;
    uint32_t lines_size;

    uint32_t _padding;

    struct yst_command {
        uint32_t lno; ///< line number in compiled file?
        uint8_t code; ///< opcode
        uint8_t narg; ///< argument count
        uint16_t npar;  ///< GOSUB / RETURN parameter count

        struct yst_arg {
            uint16_t id;
            enum yst_arg_type type;
            enum yst_assign_type assign_type;
            uint32_t expr_len;
            uint8_t *expr; ///< STR will be CP932 encoded (sadly)
        } args[MAX_YST_ARGS];
    } *instructions;
};

/// @param data ptr to the script data to parse, free after use with free_yst()
/// @param size size of the script data
/// @return 0 on success, negative ERRNO code on failure
int parse_yst(const uint8_t *data, size_t size, struct yuris_script *out);
void free_yst(struct yuris_script *script);

#endif