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
// posix stuff, no winblows for now
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h> 

#include <SDL2/SDL.h>

#include "libyuris.h"
#include "archive.h"
#include "script_reader.h"

#ifdef YURIS_DEBUG
#include "debug.h"
#endif

// Config / Argparse //

struct config config = {
    .debug = false,

    #ifdef YURIS_DEBUG
    .script_info_id = -42, // detect that it's not set
    #endif
};

void show_help() {
    printf("Usage: yuris-player [options] <script.ypf OR game/pac>\n");
    printf("Options:\n");
    printf("  -h, --help           Show this help message\n");
    printf("  -t, --target PATH    Set the game target, either a script file or game asset directory (default: current directory)\n");
    printf("  -d, --debug          Enable debug mode\n");
    #ifdef YURIS_DEBUG
    printf("Debug Options:\n");
    printf("      --files          Show all virtual files in the loaded archives and exit\n");
    printf("      --symbols        Show symbol list from ysc.ybn to stdout and exit\n");
    printf("      --script-list    Show the list of scripts from yst_list.ybn and exit\n");
    printf("      --var-list       Show the list of variables from ysv.ybn and exit\n");
    printf("      --label-list     Show the list of labels from ysl.ybn and exit\n"); 
    printf("      --script-info=ID Show commands inside a script, all scripts if ID omitted\n");
    #endif
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

            #ifdef YURIS_DEBUG
            } else if (strcmp(arg, "--files") == 0) {
                config.show_files = true;
            } else if (strcmp(arg, "--symbols") == 0) {
                config.show_symbols = true;
            } else if (strcmp(arg, "--script-list") == 0) {
                config.show_script_list = true;
            } else if (strcmp(arg, "--var-list") == 0) {
                config.show_var_list = true;
            } else if (strcmp(arg, "--label-list") == 0) {
                config.show_label_list = true;
            } else if (strncmp(arg, "--script-info", 13) == 0) {
                config.script_info_id = -1; // default to all
                if (arg[13] == '=') {
                    char *id_str = arg + 14;
                    char *endptr;
                    long id = strtol(id_str, &endptr, 10);
                    if (*endptr != '\0' || id < 0) {
                        ERROR("Invalid script ID for --script-info: %s\n", id_str);
                        exit(EXIT_FAILURE);
                    }
                    config.script_info_id = (int)id;
                }
            #endif

            } else {
                ERROR("Unknown option: %s\n", arg);
                exit(EXIT_FAILURE);
            }
        } else {
            set_config_target(arg);
        }
    }
}


// Main & Init //

int load_script(archiveManager *manager, const char *vpath, void *out, int (*callback)(const uint8_t *data, size_t size, void *user)) {
    size_t out_len;
    uint8_t *script_data = archive_file_get(manager, vpath, &out_len);
    if (!script_data) {
        ERROR("Failed to load script '%s': %s\n", vpath, strerror(errno));
        return -1;
    }
    int result = callback(script_data, out_len, out);
    yuris_free(script_data);

    return result;
}

int main(int argc, char *argv[]) {
    archiveManager manager = {0};
    archiveEntry *archive = NULL;
    YurisVersion *yuris_version = NULL;

    parse_arguments(argc, argv);
    if (config.debug)
        SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    #ifdef YURIS_DEBUG
    if (config.show_files ||
        config.show_script_list ||
        config.show_var_list ||
        config.show_label_list ||
        config.script_info_id >= -1)
        setvbuf(stdout, NULL, _IOFBF, 64 * 1024);
#endif

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

    #ifdef YURIS_DEBUG
    if (config.show_files) {
        debug_show_files(&manager);
        archive_manager_free(&manager);
        return EXIT_SUCCESS;
    }
    #endif

    struct yuris_commands ysc = {0};
    struct yuris_script_list ystl = {0};
    struct yuris_variables ysv = {0};
    struct yuris_labels ysl = {0};

    // Command list //
    if (load_script(&manager, "ysbin\\ysc.ybn", &ysc, (int (*)(const uint8_t *, size_t, void *))parse_ysc)) {
        if (errno < 0) ERROR("Failed to load ysc.ybn: %s\n", strerror(-errno));
        goto fail;
    }
    INFO("YSC.bin (v%u) contains %u commands\n", ysc.version, ysc.command_count);

    #ifdef YURIS_DEBUG
    if (config.show_symbols) {
        debug_show_ysc_commands(&ysc);
        goto success;
    }
    #endif

    // Script list //
    if (load_script(&manager, "ysbin\\yst_list.ybn", &ystl, (int (*)(const uint8_t *, size_t, void *))parse_ystl)) {
        if (errno < 0) ERROR("Failed to load yst_list.ybn: %s\n", strerror(-errno));
        goto fail;
    }
    INFO("YSTL.bin (v%u) contains %u scripts\n", ystl.version, ystl.script_count);

    #ifdef YURIS_DEBUG
    if (config.show_script_list) {
        debug_show_ystl_scripts(&ystl);
        goto success;
    }
    #endif

    // Variable list //
    if (load_script(&manager, "ysbin\\ysv.ybn", &ysv, (int (*)(const uint8_t *, size_t, void *))parse_ysv)) {
        if (errno < 0) ERROR("Failed to load ysv.ybn: %s\n", strerror(-errno));
        goto fail;
    }
    INFO("YSV.bin (v%u) contains %u variables\n", ysv.version, ysv.variable_count);

    #ifdef YURIS_DEBUG
    if (config.show_var_list) {
        debug_show_ysv_variables(&ysv, &ystl);
        goto success;
    }
    #endif


    // Label list //
    if (load_script(&manager, "ysbin\\ysl.ybn", &ysl, (int (*)(const uint8_t *, size_t, void *))parse_ysl)) {
        if (errno < 0) ERROR("Failed to load ysl.ybn: %s\n", strerror(-errno));
        goto fail;
    }
    INFO("YSL.bin (v%u) contains %u labels\n", ysl.version, ysl.label_count);

    #ifdef YURIS_DEBUG
    if (config.show_label_list) {
        debug_show_ysl_labels(&ysl, &ystl);
        goto success;
    }

    if (config.script_info_id >= 0) {
        char path[MAX_PATH_LEN] = {0};
        snprintf(path, sizeof(path), "ysbin\\yst%05u.ybn", ystl.scripts[config.script_info_id].idx);

        if (ystl.scripts[config.script_info_id].variable_count == UINT32_MAX) {
            ERROR("Script '%s' does not have script data\n", ystl.scripts[config.script_info_id].path);
            goto fail;
        }

        struct yuris_script script = {0};
        if (load_script(&manager, path, &script, (int (*)(const uint8_t *, size_t, void *))parse_yst)) {
            ERROR("Failed to load script '%s': %s\n", path, strerror(errno));
            goto fail;
        }
        debug_show_yst(&script, &ystl.scripts[config.script_info_id], &ysc);
        free_yst(&script);
        goto success;
    } else if (config.script_info_id == -1) {
        /// TODO mapping here is wrong, ystl doesn't resolve to ybn like this
        for (uint32_t i = 0; i < ystl.script_count; ++i) {
            if (ystl.scripts[i].variable_count == UINT32_MAX) continue;

            struct yuris_script script = {0};
            char path[MAX_PATH_LEN] = {0};
            snprintf(path, sizeof(path), "ysbin\\yst%05u.ybn", ystl.scripts[i].idx);

            if (load_script(&manager, path, &script, (int (*)(const uint8_t *, size_t, void *))parse_yst)) {
                ERROR("Failed to load script '%s': %s\n", path, strerror(errno));
                continue;
            }
            debug_show_yst(&script, &ystl.scripts[i], &ysc);
            free_yst(&script);
        }
        goto success;
    }
    #endif


    success:
        archive_manager_free(&manager);
        free_ysv(&ysv);
        free_ysl(&ysl);
        return 0;

    fail:
        archive_manager_free(&manager);
        free_ysv(&ysv);
        free_ysl(&ysl);
        exit(EXIT_FAILURE);
}
