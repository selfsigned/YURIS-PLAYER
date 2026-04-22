#ifndef LIBYURIS_H
#define LIBYURIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define YURIS_NAME_LEN 64

/// @file Headers for the vibslopped libyuris FFI api, the

/**
 * @brief Yu-Ris version metadata for archive configuration.
 */
typedef struct {
    uint32_t version;
    uint32_t xor_key;
    uint8_t instr_len;
} YurisVersion;

/**
 * @brief Single file entry in a Yu-Ris archive.
 *
 * The offset field points into the archive for retrieval via yuris_file_read().
 * Caller owns the returned manifest; must be freed via yuris_free().
 */
typedef struct {
    char name[YURIS_NAME_LEN];
    uint64_t offset;
    uint32_t stored_size;
    uint32_t unpacked_size;
    uint8_t compressed;
    uint32_t hash;
} YurisFile;

/**
 * @brief Parsed file index from archive header+index region.
 *
 * Populated by yuris_get_manifest().
 * Caller owns returned manifest; must be freed via yuris_free().
 */
typedef struct {
    YurisVersion version;
    uint32_t num_files;
    YurisFile *files;
} YurisManifest;

/**
 * @brief Decrypt raw XOR-encoded data using version-derived key.
 *
 * Stateless: key is derived from version->version, no global state.
 * Caller owns returned buffer; must be freed via yuris_free().
 * Does NOT decompress; assumes input is XOR-encoded raw bytes.
 * @param data XOR-encoded bytes.
 * @param len Number of bytes at data.
 * @param version Used to derive XOR key.
 * @param out_len Receives decrypted size.
 * @return Newly allocated buffer, or NULL on error.
 */
uint8_t* yuris_decrypt(const uint8_t *data, size_t len, const YurisVersion *version, size_t *out_len);

/**
 * @brief Parse archive header+index to build file manifest.
 *
 * Reads header/index only (32-byte header + variable-size index), not file data.
 * @param data Bytes containing the YPF header+index (NOT the full archive).
 * @param len Length of data.
 * @return Newly allocated manifest, or NULL on error.
 */
YurisManifest* yuris_get_manifest(const uint8_t *data, size_t len);

/**
 * @brief Decode a single file entry (decompress + decrypt).
 *
 * If file->compressed == 1: zlib-compressed, decompresses before XOR.
 * If file->compressed == 0: raw XOR decryption only.
 * Hash validation is version-dependent: V<265 skip, V<470 use adler32, V>=470 use murmur2.
 * @param file_data Raw bytes at file->offset (file_len should equal file->stored_size).
 * @param file_len Length of file_data.
 * @param version For XOR key derivation and hash validation.
 * @param file File metadata from manifest.
 * @param out_len Receives decoded size.
 * @return Newly allocated buffer, or NULL on error.
 */
uint8_t* yuris_file_read(
    const uint8_t *file_data,
    size_t file_len,
    const YurisVersion *version,
    const YurisFile *file,
    size_t *out_len
);

/**
 * @brief Free memory allocated by libyuris FFI.
 *
 * No-op for NULL. Do NOT mix with malloc/free; use only for yuris function return values.
 * @param ptr Pointer returned by a libyuris function.
 */
void yuris_free(void *ptr);

#endif