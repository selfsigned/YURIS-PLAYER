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

#ifndef YURISPLAYER_H
#define YURISPLAYER_H

#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "utils.h" // posix stuff and log macros

// Config
struct config {
    char game_target[MAX_PATH_LEN];
    bool debug;

    #ifdef YURIS_DEBUG
    bool show_symbols;
    bool show_script_list;
    #endif
};
extern struct config config;

#endif



