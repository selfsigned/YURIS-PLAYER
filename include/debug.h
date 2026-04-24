
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

#include "script_reader.h"

/// @brief show ysc command w/ args and signatures
void debug_show_ysc_command(const struct ysc_command *cmd);
/// @brief show all ysc commands
void debug_show_ysc_commands(const struct yuris_commands *ysc);

#endif 