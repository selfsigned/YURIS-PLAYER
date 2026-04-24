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
#define MAX_YSC_FUNCNAME_LEN 32 
#define MAX_YSC_ARGS 128
#define MAX_YSC_ERRMSG 128
#define YSC_ERR_STR_COUNT 37

enum ysc_arg_type {
    YSC_ARG_ANY,
    YSC_ARG_INT,
    YSC_ARG_FLOAT,
    YSC_ARG_STR,
};

/// @brief Command definition 
struct yuris_commands {
    char magic[4];  // "YSCM"
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

#endif