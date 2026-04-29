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
    for (uint32_t i = 0; i < ystl->scripts_count; ++i)
        debug_show_ystl_script(&ystl->scripts[i]);
}