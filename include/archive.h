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
#ifndef ARCHIVE_H
#define ARCHIVE_H

/// @file archive.h
/// @brief Archive loading and file retrival, no caching (yet?)

#include <stdbool.h>

#include "libyuris.h"
#include "utils.h"

#define MAX_ARCHIVES 32
#define MANIFEST_HEADER_READ_SIZE 1048576 // 1MiB should be enough for manifest header + index region

typedef enum {
    ASSET,
    SCRIPT,
    MPEG,
} archiveType;

typedef struct archiveEntry archiveEntry;

typedef struct {
    YurisFile f;
    archiveEntry *parent;
} fileEntry;

typedef struct archiveEntry {
    char path[MAX_PATH_LEN];
    YurisVersion version;
    size_t count;
    archiveType type;
    fileEntry *files;
} archiveEntry;

typedef struct {
    size_t count;
    archiveEntry archives[MAX_ARCHIVES];
    void *internal;
} archiveManager;

/// @brief Add an archive to the manager
/// @param out_archive optional pointer to receive the newly loaded archive entry
/// @return 0 on success, -1 on error (check errno)
int archive_load(archiveManager *manager, const char *path, archiveEntry **out_archive);
void archive_manager_free(archiveManager *manager);

/// @brief Lookup a file in the the archives contained in manager
/// @param path Yuris-style path, e.g. "ysbin\\ysc.ybn"
fileEntry *archive_file_find(archiveManager *manager, const char *path);

/// @param path Yuris-style path, e.g. "ysbin\\ysc.ybn"
bool archive_file_exists(archiveManager *manager, const char *path);

/// @brief Get decrypted file content from archive
/// @param out_len nbr of bytes read, optional
/// @param path Yuris-style path, e.g. "ysbin\\ysc.ybn"
/// @return NULL on error (check errno), call yuris_free() on the returned buffer when done
uint8_t *archive_file_get(archiveManager *manager, const char *path, size_t *out_len);

#endif
