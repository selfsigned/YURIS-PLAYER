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

#ifndef VM_H
#define VM_H

#include <stdint.h>
#include "libyuris.h"
#include "script_reader.h"
#include "expr.h"

typedef struct {
    YurisVersion *version;

    // Parsed definitions
    struct yuris_script_list *ystl;
    struct yuris_commands *ysc;
    struct yuris_variables *ysv;
    struct yuris_labels *ysl;

    // Runtime
    struct {
        struct expr_value *values;
        uint16_t max_idx;
    } vars;
} yuris_vm;


/// @brief Init VM variables from YSV.bin, needs initialized ysv
/// @return 0 on success, negative ERRNO code on failure
int vm_init_vars(yuris_vm *vm);

// Variable access //
int vm_var_get(yuris_vm *vm, uint16_t idx, struct expr_value *out);
int vm_var_set(yuris_vm *vm, uint16_t idx, struct expr_value val);
struct ysv_variable *vm_var_def(yuris_vm *vm, uint16_t idx); ///< get variable definition from YSV

/// @brief free resources owned by the VM
void free_vm(yuris_vm *vm);

#endif
