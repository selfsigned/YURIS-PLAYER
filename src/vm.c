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

#include "vm.h"

#include <errno.h>
#include "expr.h"
#include "utils.h"

// Vars //

int vm_init_vars(yuris_vm *vm) {
    if (!vm || !vm->ysv) return -EINVAL;
    struct yuris_variables *ysv = vm->ysv;

    vm->vars.max_idx = 0;
    vm->vars.values = calloc(ysv->max_variable_idx + 1, sizeof(struct expr_value));

    uint16_t initialized_count = 0;
    for (uint16_t i = 0; i < ysv->variable_count; ++i) {
        struct ysv_variable *var = &ysv->variables[i];
        uint16_t idx = var->variable_idx;

        if (idx > vm->vars.max_idx)
            vm->vars.max_idx = idx;

        switch(var->type) {
            case YSV_INT:
                vm->vars.values[idx] = INT_V(var->initial_value.int_val);
                break;
            case YSV_FLOAT:
                vm->vars.values[idx] = FLT_V(var->initial_value.float_val);
                break;
            case YSV_EXPR:
                struct expr_value val = {0};
                if (eval_expr(var->initial_value.expr_val.expr, var->initial_value.expr_val.length, &val))
                    WARN("Failed to evaluate initial value for variable idx %u: %s\n", idx, strerror(-errno));
                vm->vars.values[idx] = val;
                break;
            default:
                vm->vars.values[idx] = (struct expr_value){0};
                break;
        }
    }

    return 0;
}

int vm_var_get(yuris_vm *vm, uint16_t idx, struct expr_value *out) {
    if (!vm || !out || idx > vm->vars.max_idx) return -EINVAL;
    *out = vm->vars.values[idx];
    return 0;
}

int vm_var_set(yuris_vm *vm, uint16_t idx, struct expr_value val) {
    if (!vm || idx > vm->vars.max_idx) return -EINVAL;
    FREE_EXPR(vm->vars.values[idx]);
    vm->vars.values[idx] = val;
    return 0;
}

struct ysv_variable *vm_var_def(yuris_vm *vm, uint16_t idx) {
    if (!vm || idx > vm->vars.max_idx) return NULL;
    return &vm->ysv->variables[idx];
}

// VM //

void free_vm(yuris_vm *vm) {
    if (!vm) return;

    if (vm->ysc) {
        yuris_free(vm->ysc);
        vm->ysc = NULL;
    }
    if (vm->ysl) {
        free_ysl(vm->ysl);
        vm->ysl = NULL;
    }
    if (vm->ysv) {
        free_ysv(vm->ysv);
        vm->ysv = NULL;
    }
    if (vm->vars.values) {
        for (uint16_t i = 0; i <= vm->vars.max_idx; ++i)
            FREE_EXPR(vm->vars.values[i]);
        free(vm->vars.values);
        vm->vars.values = NULL;
    }
}