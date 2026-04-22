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

#include "yurisplayer.h"

#include <string.h>
#include <errno.h>
#include <SDL2/SDL.h>
// posix stuff, no winblows for now
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h> 

#include <SDL2/SDL.h>

#include "libyuris.h"
#include "archive.h"

// Main stuff

struct config config = {
    .debug = false
};

void show_help() {
    printf("Usage: yuris-player [options] <script.ypf OR game/pac>\n");
    printf("Options:\n");
    printf("  -h, --help       Show this help message\n");
    printf("  -t, --target PATH  Set the game target, either a script file or game asset directory (default: current directory)\n");
    printf("  -d, --debug      Enable debug mode\n");
}

void set_config_target(const char *path) {
    if (strlen(path) >= NAME_MAX) {
        ERROR("Target is too long (max %d characters)\n", NAME_MAX - 1);
        exit(EXIT_FAILURE);
    }
    struct stat s;
    if (stat(path, &s) != 0) {
        ERROR("Target does not exist: %s\n", path);
        exit(EXIT_FAILURE);
    }
    strncpy(config.game_target, path, MAX_PATH_LEN - 1);
    config.game_target[MAX_PATH_LEN - 1] = '\0';
}

/// @brief remove trailing slash
void normalize_path(char *path) {
    size_t len = strlen(path);
    if (len > 1 && path[len - 1] == '/')
        path[len - 1] = '\0';
}

void parse_arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++ ) {
        char *arg = argv[i];
        if (arg[0] == '-') {
            if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
                show_help();
                exit(EXIT_SUCCESS);
            } else if (!strcmp(arg, "-t") || !strcmp(arg, "--target")) {
                if (i + 1 < argc) {
                    set_config_target(argv[++i]);
                } else {
                    ERROR("Error: --target option requires an argument\n");
                    exit(EXIT_FAILURE);
                }

            } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--debug") == 0) {
                config.debug = true;
            } else {
                ERROR("Unknown option: %s\n", arg);
                exit(EXIT_FAILURE);
            }
        } else {
            set_config_target(arg);
        }
    }
}


int main(int argc, char *argv[]) {
    archiveManager manager = {0};
    archiveEntry *archive = NULL;
    YurisVersion *yuris_version = NULL;

    parse_arguments(argc, argv);
    if (config.debug) {
        SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
    }

    // default to current dir
    if (!config.game_target[0]) {
        char *sdl_base_path = SDL_GetBasePath();
        set_config_target(sdl_base_path);
        free(sdl_base_path);
    }

    // load assets
    struct stat s;
    if (!stat(config.game_target, &s) && S_ISDIR(s.st_mode)) {
        DIR *dir = opendir(config.game_target);
        if (!dir) {
            ERROR("Error opening '%s': %s\n", config.game_target, strerror(errno));
            exit(EXIT_FAILURE);
        }
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".ypf")) {
                // path concat
                char path[MAX_PATH_LEN];
                size_t target_len = strlen(config.game_target);
                size_t entry_len = strlen(entry->d_name);
                if (target_len + 1 + entry_len + 1 > sizeof(path)) {
                    ERROR("Archive path is too long, skipping: %s/%s\n", config.game_target, entry->d_name);
                    continue;
                }
                memcpy(path, config.game_target, target_len);
                path[target_len] = '/';
                memcpy(path + target_len + 1, entry->d_name, entry_len + 1);

                // archive load
                int result = archive_load(&manager, path, &archive);
                if (result == 0) {
                    INFO("Loaded archive: %s\n", path);
                    if (!yuris_version) {
                        yuris_version = &archive->version;
                    } else if (archive->version.version != yuris_version->version) {
                        WARN("Version mismatch: %s has version %u, expected %u\n", path, archive->version.version, yuris_version->version);
                        if (archive->type == SCRIPT) {
                            yuris_version = &archive->version;
                        }
                    }
                } else if (result == 1) {
                    ERROR("Archive not found: %s\n", path);
                } else
                    ERROR("Couldn't load '%s': %s\n", path, strerror(errno));
            }
        }
        closedir(dir);
    } else {
        int result = archive_load(&manager, config.game_target, &archive); // load archive directly, useful for testing
        if (result == 0) {
            INFO("Loaded archive: %s\n", config.game_target);
            yuris_version = &archive->version;
        } else if (result == 1) {
            ERROR("Archive not found: %s\n", config.game_target);
            return EXIT_FAILURE;
        } else {
            ERROR("Couldn't load '%s': %s\n", config.game_target, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    if (!yuris_version) {
        ERROR("No valid archives loaded, exiting\n");
        return EXIT_FAILURE;
    }
    INFO("Detected YU-RIS Version: %u XOR key: 0x%08X instruction len: %u\n", yuris_version->version, yuris_version->xor_key, yuris_version->instr_len);

    // test call
    char *testfile = "ysbin\\ysc.ybn";
    bool file_exists = archive_file_exists(&manager, testfile);
    if (file_exists) {
        size_t out_len;
        uint8_t *data = archive_file_get(&manager, testfile, &out_len);

        if (data && out_len > 4 && !strncmp((char *)data, "YSCM", 4)) {
            INFO("Successfully got file data for %s\n", testfile);
            yuris_free(data);
        } else
            ERROR("Failed to get file data for %s: %s\n", testfile, strerror(errno));
    }

    if (yuris_version->version < 300) {
        ERROR("Unsupported YU-RIS version: %u\n", yuris_version->version);
        return EXIT_FAILURE;
    }


    // TODO:
    // [load script files] 
    // [ run VM loop]
    archive_manager_free(&manager);
    return 0;
}
