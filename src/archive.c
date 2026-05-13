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

#include "archive.h"

#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <SDL2/SDL.h>

#include "utils.h"
#include "libyuris.h"

typedef struct {
    fileEntry **sorted_files;
    size_t sorted_count;
    bool is_sorted;
} archiveManagerInternal;

//    _          _    _         
//   /_\  _ _ __| |_ (_)_ _____ 
//  / _ \| '_/ _| ' \| \ V / -_)
// /_/ \_\_| \__|_||_|_|\_/\___|

bool is_mpeg_archive(uint8_t file_start[4]) {
    // 00 00 01 BA 
    if (file_start[0] == 0x00 && file_start[1] == 0x00 && file_start[2] == 0x01 && file_start[3] == 0xBA)
        return true;
    return false;
}

int archive_load(archiveManager *manager, const char *path, archiveEntry **out_archive) {
    if (manager->count >= MAX_ARCHIVES) {
        errno = ENOMEM;
        return -1;
    } else if (!path) {
        errno = EINVAL;
        return -1;
    } else if (strlen(path) >= MAX_PATH_LEN) {
        errno = ENAMETOOLONG;
        return -1;
    }


    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if (!rw) {
        errno = ENOENT;
        return -1;
    }
    Sint64 file_size = SDL_RWseek(rw, 0, RW_SEEK_END);
    if (file_size < 0 || file_size > (Sint64)MANIFEST_HEADER_READ_SIZE) {
        file_size = MANIFEST_HEADER_READ_SIZE;
    }
    SDL_RWseek(rw, 0, RW_SEEK_SET);

    size_t to_read = (size_t)file_size;
    uint8_t *buf = malloc(to_read + 1);
    if (!buf) {
        SDL_RWclose(rw);
        errno = ENOMEM;
        return -1;
    }
    size_t read_bytes = SDL_RWread(rw, buf, 1, to_read);
    buf[read_bytes] = 0;
    SDL_RWclose(rw);

    if (read_bytes == 0) {
        free(buf);
        errno = EIO;
        return -1;
    } else if (is_mpeg_archive(buf)) {
        // TODO 
        WARN("%s appears to be an MPEG archive, not supported (yet)\n", path);
        free(buf);
        errno = ENOTSUP;
        return -1;
    }
    YurisManifest *manifest = yuris_get_manifest(buf, read_bytes);
    free(buf);
    if (!manifest || !manifest->num_files || !manifest->files) {
        if (manifest) yuris_free(manifest);
        errno = EBADMSG;
        return -1;
    }
    archiveType type = ASSET; // if any file has a .ybn extension the archive is probably a script archive
    for (size_t i = 0; i < manifest->num_files; i++) {
        YurisFile *yf = &manifest->files[i];
        if (strstr(yf->name, ".ybn")) {
            type = SCRIPT;
            break;
        }
    }
    archiveEntry entry = {
        .count = manifest->num_files,
        .version = manifest->version,
        .type = type,
    };
    strncpy(entry.path, path, MAX_PATH_LEN - 1);
    entry.path[MAX_PATH_LEN - 1] = '\0';


    // populate file entries from manifest
    entry.files = calloc(manifest->num_files, sizeof(fileEntry));
    if (!entry.files) {
        yuris_free(manifest);
        errno = ENOMEM;
        return -1;
    }
    for (size_t i = 0; i < entry.count; i++) {
        YurisFile *mf = &manifest->files[i];
        fileEntry *e = &entry.files[i];

        memcpy(&e->f, mf, sizeof(YurisFile));
    }
    manager->archives[manager->count] = entry;
    archiveEntry *archive = &manager->archives[manager->count]; // set back ptr to parent archive
    for (size_t i = 0; i < archive->count; i++) {
        archive->files[i].parent = archive;
    }
    if (out_archive) *out_archive = &manager->archives[manager->count];
    manager->count++;


    // Handling internal state for file indexing
    if (!manager->internal) {
        archiveManagerInternal *internal = calloc(1, sizeof(archiveManagerInternal));
        assert(internal); // the user's machine is probably fucked. Failing here would be a nightmare;
        manager->internal = internal;
    } else {
        // This never should really be triggered, why load assets after scripts? 
        archiveManagerInternal *internal = (archiveManagerInternal *)manager->internal;
        internal->is_sorted = false;
    }
    yuris_free(manifest);
    return 0;
}

void archive_manager_free(archiveManager *manager) {
    if (manager->internal) {
        archiveManagerInternal *internal = (archiveManagerInternal *)manager->internal;
        free(internal->sorted_files);
        internal->sorted_files = NULL;
        free(internal);
        manager->internal = NULL;
    }

    for (size_t i = 0; i < manager->count; i++) {
        free(manager->archives[i].files);
        manager->archives[i].files = NULL;
    }
    manager->count = 0;
}

//  ___ _ _        
// | __(_) |___ 
// | _|| | / -_|
// |_| |_|_\___/

static int archive_file_compare(const void *a, const void *b) {
    const fileEntry *const *pa = (const fileEntry *const *)a;
    const fileEntry *const *pb = (const fileEntry *const *)b;
    const fileEntry *fa = (pa) ? *pa : NULL;
    const fileEntry *fb = (pb) ? *pb : NULL;

    if (!fa && !fb) return 0;
    if (!fa) return -1;
    if (!fb) return 1;
    return strcmp(fa->f.name, fb->f.name);
}

static void archive_file_build_idx(archiveManager *manager) {
    archiveManagerInternal *internal = (archiveManagerInternal *)manager->internal;
    size_t total_files = 0;
    for (size_t i = 0; i < manager->count; i++) {
        total_files += manager->archives[i].count;
    }

    if (internal->sorted_files) {
        free(internal->sorted_files);
        internal->sorted_files = NULL;
    }

    internal->sorted_files = calloc(total_files, sizeof(fileEntry *)); // Could be used for Dos attacks maybe
    assert(internal->sorted_files); // a working index is non-negotiable

    size_t idx = 0;
    for (size_t i = 0; i < manager->count; i++) {
        archiveEntry *archive = &manager->archives[i];
        for (size_t j = 0; j < archive->count; j++) {
            internal->sorted_files[idx++] = &archive->files[j];
        }
    }
    qsort(internal->sorted_files, total_files, sizeof(fileEntry *), archive_file_compare);
    internal->sorted_count = total_files;
    internal->is_sorted = true;

    INFO("Built file index with %zu files\n", total_files);
}


fileEntry *archive_file_find(archiveManager *manager, const char *target) {
    if (!target) return NULL;
    if (strlen(target) >= YURIS_NAME_LEN) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    if (!manager->internal) {
        errno = ENOENT; // should've been called AFTER loading an archive
        return NULL;
    }
    archiveManagerInternal *internal = (archiveManagerInternal *)manager->internal;
    if (!internal->is_sorted) {
        archive_file_build_idx(manager);
    }

    fileEntry key = {0};
    strncpy(key.f.name, target, YURIS_NAME_LEN - 1);
    key.f.name[YURIS_NAME_LEN - 1] = '\0';
    fileEntry *key_ptr = &key;

    fileEntry **result = bsearch(&key_ptr, internal->sorted_files, internal->sorted_count, sizeof(fileEntry *), archive_file_compare);
    return result ? *result : NULL;
}

bool archive_file_exists(archiveManager *manager, const char *target) {
    fileEntry *result = archive_file_find(manager, target);
    return result != NULL; 
}

uint8_t *archive_file_get(archiveManager *manager, const char *target, size_t *out_len) {
    if (out_len) *out_len = 0;
    
    fileEntry *entry = archive_file_find(manager, target);
    if (!entry) {
        errno = ENOENT;
        return NULL;
    }
    YurisFile *file = &entry->f;
    if (file->stored_size == 0) {
        if (out_len) *out_len = 0;
        return NULL;
    }

    archiveEntry *archive = entry->parent;
    uint8_t *buf = calloc(1, file->stored_size);
    if (!buf) {
        errno = ENOMEM;
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromFile(archive->path, "rb");
    if (!rw) {
        free(buf);
        errno = ENOENT;
        return NULL;
    }
    DEBUG("archive_file_get: path=%s offset=%lu stored=%u\n", archive->path, file->offset, file->stored_size);
    if (file->offset > (uint64_t)INT64_MAX) {
        SDL_RWclose(rw);
        free(buf);
        errno = EINVAL;
        return NULL;
    }
    Sint64 seek_res = SDL_RWseek(rw, (Sint64)file->offset, RW_SEEK_SET);
    if (seek_res < 0 || seek_res != (Sint64)file->offset) {
        SDL_RWclose(rw);
        free(buf);
        errno = EIO;
        return NULL;
    }
    size_t read_bytes = SDL_RWread(rw, buf, 1, file->stored_size);
    SDL_RWclose(rw);
    if (read_bytes != file->stored_size) {
        free(buf);
        errno = EIO;
        return NULL;
    }

    uint8_t *decrypted_data = yuris_file_read(buf, file->stored_size, &archive->version, file, out_len);
    free(buf);
    if (!decrypted_data) {
        if (out_len) *out_len = 0;
        errno = EINVAL;
        return NULL;
    }
    return decrypted_data;
}
