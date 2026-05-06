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

#include "debug.h"

#include <string.h>
#include <stdio.h>

void debug_show_ysc_command(const struct ysc_command *cmd) {
    if (!cmd) return;
    if (cmd->arg_count == 0) printf("%s\n", cmd->name);

    for (uint8_t i = 0; i < cmd->arg_count; ++i) {
        const struct ysc_arg *arg = &cmd->args[i];

        char type_str[8] = {0};
        switch (arg->type) {
            case YSC_ARG_ANY: strcpy(type_str, "ANY"); break;
            case YSC_ARG_INT: strcpy(type_str, "INT"); break;
            case YSC_ARG_FLOAT: strcpy(type_str, "FLOAT"); break;
            case YSC_ARG_STR: strcpy(type_str, "STR"); break;
            default: strcpy(type_str, "UNKNOWN"); break;
        }

        if (!arg->name[0]) printf("%s: %s\n", cmd->name, type_str);
        else printf("%s.%s: %s\n", cmd->name, arg->name, type_str);
    }
}

void debug_show_ysc_commands(const struct yuris_commands *ysc) {
    for (uint32_t i = 0; i < ysc->command_count; ++i)
        debug_show_ysc_command(&ysc->commands[i]);
}

void debug_show_ystl_script(const struct ystl_script *script) {
    if (!script) return;
    printf("[%u] %s (vars: %u, labels: %u, text: %u)\n", script->idx, script->path, script->variable_count, script->label_count, script->text_count);
}

void debug_show_ystl_scripts(const struct yuris_script_list *ystl) {
    if (!ystl) return;
    for (uint32_t i = 0; i < ystl->script_count; ++i)
        debug_show_ystl_script(&ystl->scripts[i]);
}

void debug_show_ysv_variable(const struct ysv_variable *var, const struct yuris_script_list *ystl) {
    if (!var || !ystl) return;

    char scope_str[9] = {0};
    switch (var->scope) {
        case YSV_SCOPE_NONE: strcpy(scope_str, "NONE"); break;
        case YSV_SCOPE_GLOBAL: strcpy(scope_str, "GLOBAL"); break;
        case YSV_SCOPE_STATIC: strcpy(scope_str, "STATIC"); break;
        case YSV_SCOPE_FUNCTION: strcpy(scope_str, "FUNCTION"); break;
        default: strcpy(scope_str, "UNKNOWN"); break;
    }

    char type_str[8] = {0};
    switch (var->type) {
        case YSV_NONE: strcpy(type_str, "NONE"); break;
        case YSV_INT: strcpy(type_str, "INT"); break;
        case YSV_FLOAT: strcpy(type_str, "FLOAT"); break;
        case YSV_EXPR: strcpy(type_str, "EXPR"); break;
        default: strcpy(type_str, "UNKNOWN"); break;
    }

    printf("%u\t%s\t%s\t%u\t", var->variable_idx, scope_str, type_str, var->dimension_size);
    switch (var->type) {
        case YSC_ARG_ANY:
            printf("N/A\t");
            break;
        case YSC_ARG_INT:
            printf("%ld\t", var->initial_value.int_val);
            break;
        case YSC_ARG_FLOAT:
            printf("%f\t", var->initial_value.float_val);
            break;
        case YSC_ARG_STR:
            for (uint16_t i = 0; i < var->initial_value.expr_val.length; ++i)
                printf("%02X ", var->initial_value.expr_val.expr[i]);
            printf("\t");
            break;
        default:
            printf("UNKNOWN\t");
            break;
    }
    printf("%u\t%s\n", var->script_idx, (var->script_idx < ystl->script_count) ? ystl->scripts[var->script_idx].path : "UNKNOWN");
}

void debug_show_ysv_variables(const struct yuris_variables *ysv, const struct yuris_script_list *ystl) {
    printf("var_idx\tscope\ttype\tdim\tinit\tscript_idx\tscript_path\n");
    if (!ysv || !ystl) return;
    for (uint16_t i = 0; i < ysv->variable_count; ++i)
        debug_show_ysv_variable(&ysv->variables[i], ystl);
}

void debug_show_ysl_label(const struct ysl_label *label, const struct yuris_script_list *ystl) {
    if (!label) return;
    printf("%u\t%u\t%u\t%u\t%u\t%s\t%s\n",
        label->id,
        label->script_idx,
        label->ip,
        label->if_lvl,
        label->loop_lvl,
        label->name,
        (label->script_idx < ystl->script_count) ? ystl->scripts[label->script_idx].path : "UNKNOWN");
}

void debug_show_ysl_labels(const struct yuris_labels *ysl, const struct yuris_script_list *ystl)
{
    printf("id\tscript_idx\tip\tif_lvl\tloop_lvl\tname\tscript_path\n");
    if (!ysl || !ystl) return;
    for (uint32_t i = 0; i < ysl->label_count; ++i)
        debug_show_ysl_label(&ysl->labels[i], ystl);
}