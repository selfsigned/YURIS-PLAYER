
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

#ifndef DEBUG_H
#define DEBUG_H

#include "archive.h"
#include "script_reader.h"

/// @brief show all virtual files in the loaded archives and exit
void debug_show_files(const archiveManager *manager);

/// @brief show ysc command w/ args and signatures
void debug_show_ysc_command(const struct ysc_command *cmd);
/// @brief show all ysc commands
void debug_show_ysc_commands(const struct yuris_commands *ysc);

/// @brief show all the scripts in the script list
void debug_show_ystl_scripts(const struct yuris_script_list *ystl);
void debug_show_ystl_script(const struct ystl_script *script);

/// @brief show all variables in the variable list
/// @param ystl used to resolve script names
void debug_show_ysv_variables(const struct yuris_variables *ysv, const struct yuris_script_list *ystl);
void debug_show_ysv_variable(const struct ysv_variable *var, const struct yuris_script_list *ystl);

/// @brief show all labels
/// @param ystl used to resolve script names
void debug_show_ysl_labels(const struct yuris_labels *ysl, const struct yuris_script_list *ystl);
void debug_show_ysl_label(const struct ysl_label *label, const struct yuris_script_list *ystl);

/// @brief show info about a script
/// @param script the script to show info about
/// @param script_info the script's info from yst_list.ybn, used to show path
/// @param ysv used to show variable info
void debug_show_yst(const struct yuris_script *script, const struct ystl_script *info, const struct yuris_commands *ysc);


#endif 