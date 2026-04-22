use crate::{check_version, Error, Result};
use flate2::read::ZlibDecoder;
use std::io::Read;

pub const YPF_MAGIC: u32 = 0x00465059;

const NL_SWAPS: [(u8, u8); 9] = [
    (6, 53),
    (9, 11),
    (12, 16),
    (13, 19),
    (21, 27),
    (28, 30),
    (32, 35),
    (38, 41),
    (44, 47),
];

const fn build_nl_trans(swaps: &[(u8, u8); 9], extra: &[(u8, u8); 3]) -> [u8; 256] {
    let mut trans: [u8; 256] = [0; 256];
    let mut i = 0;
    while i < 256 {
        trans[i] = i as u8;
        i += 1;
    }
    let mut idx = 0;
    while idx < swaps.len() {
        let (a, b) = swaps[idx];
        trans[a as usize] = b;
        trans[b as usize] = a;
        idx += 1;
    }
    let mut idx = 0;
    while idx < extra.len() {
        let (a, b) = extra[idx];
        trans[a as usize] = b;
        trans[b as usize] = a;
        idx += 1;
    }
    trans
}

const fn build_name_xor(xor_val: u8) -> [u8; 256] {
    let mut xor = [0u8; 256];
    let mut i = 0;
    while i < 256 {
        xor[i] = (i as u8) ^ xor_val;
        i += 1;
    }
    xor
}

const NL_TRANS_V000: [u8; 256] = build_nl_trans(&NL_SWAPS, &[(3, 72), (17, 25), (46, 50)]);
const NL_TRANS_V500: [u8; 256] = build_nl_trans(&NL_SWAPS, &[(3, 10), (17, 24), (20, 46)]);
const NAME_XOR_V000: [u8; 256] = build_name_xor(0xff);
const NAME_XOR_V290: [u8; 256] = build_name_xor(0xff ^ 0x40);
const NAME_XOR_V500: [u8; 256] = build_name_xor(0xff ^ 0x36);

fn verify_crc32(data: &[u8], expected: u32) -> Result<u32> {
    let actual = crc32fast::hash(data);
    if actual != expected {
        return Err(Error::HashMismatch {
            context: "file".to_string(),
            expected,
            actual,
        });
    }
    Ok(actual)
}

fn verify_adler(data: &[u8], expected: u32) -> Result<u32> {
    let actual = adler32_slice(data);
    if actual != expected {
        return Err(Error::HashMismatch {
            context: "file".to_string(),
            expected,
            actual,
        });
    }
    Ok(actual)
}

fn adler32_slice(data: &[u8]) -> u32 {
    let mut a: u32 = 1;
    let mut b: u32 = 0;
    const MOD_ADLER: u32 = 65521;
    for &byte in data {
        a = (a + u32::from(byte)) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    b << 16 | a
}

fn verify_murmur(data: &[u8], expected: u32) -> Result<u32> {
    let actual = murmur2::murmur2(data, 0);
    if actual != expected {
        return Err(Error::HashMismatch {
            context: "file".to_string(),
            expected,
            actual,
        });
    }
    Ok(actual)
}

fn transform_name_size(size: u8, nl_trans: &[u8; 256]) -> usize {
    nl_trans[(size ^ 0xff) as usize] as usize
}

fn transform_name_bytes(bytes: &[u8], xor_trans: &[u8; 256]) -> Vec<u8> {
    bytes.iter().map(|b| xor_trans[*b as usize]).collect()
}

fn decode_cp932(bytes: &[u8]) -> Result<String> {
    let (decoded, _, _) = encoding_rs::SHIFT_JIS.decode(bytes);
    Ok(decoded.into_owned())
}

#[derive(Debug, Clone)]
pub struct YpfEntry {
    pub name: String,
    pub kind: u8,
    pub compressed: bool,
    pub uncompressed_len: u32,
    pub compressed_len: u32,
    pub offset: u64,
    pub hash: u32,
    pub data: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct YpfIndexEntry {
    pub name: String,
    pub kind: u8,
    pub compressed: bool,
    pub uncompressed_len: u32,
    pub compressed_len: u32,
    pub offset: u64,
    pub hash: u32,
}

#[derive(Debug)]
pub struct Ypf {
    pub version: u32,
    pub entries: Vec<YpfEntry>,
}

impl Ypf {
    pub fn parse_index(data: &[u8]) -> Result<Vec<YpfIndexEntry>> {
        if data.len() < 32 {
            return Err(Error::UnexpectedEof {
                offset: 0,
                need: 32,
                have: data.len(),
            });
        }

        let magic = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
        if magic != YPF_MAGIC {
            return Err(Error::BadMagic {
                expected: YPF_MAGIC,
                got: magic,
            });
        }

        let version = u32::from_le_bytes([data[4], data[5], data[6], data[7]]);
        check_version(version)?;

        let nent = u32::from_le_bytes([data[8], data[9], data[10], data[11]]);
        let lhdr = u32::from_le_bytes([data[12], data[13], data[14], data[15]]);

        let reserved = &data[16..32];
        if !reserved.iter().all(|&b| b == 0) {
            return Err(Error::AssertFailed("reserved bytes not zero".to_string()));
        }

        let nl_trans = if version == 500 {
            &NL_TRANS_V500
        } else {
            &NL_TRANS_V000
        };

        let xor_trans = match version {
            290 => &NAME_XOR_V290,
            500 => &NAME_XOR_V500,
            _ => &NAME_XOR_V000,
        };

        let header_size = if version >= 300 { lhdr } else { lhdr + 32 };

        let entry_count = nent as usize;
        let start_pos = 32;
        let mut pos = start_pos;
        let mut entries = Vec::with_capacity(entry_count);

        for _ in 0..entry_count {
            if pos + 5 > data.len() {
                return Err(Error::UnexpectedEof {
                    offset: pos,
                    need: 5,
                    have: data.len() - pos,
                });
            }

            let _name_hash =
                u32::from_le_bytes([data[pos], data[pos + 1], data[pos + 2], data[pos + 3]]);
            let name_size = data[pos + 4];
            pos += 5;

            let transformed_size = transform_name_size(name_size, nl_trans);
            if pos + transformed_size > data.len() {
                return Err(Error::UnexpectedEof {
                    offset: pos,
                    need: transformed_size,
                    have: data.len() - pos,
                });
            }

            let name_bytes = &data[pos..pos + transformed_size];
            pos += transformed_size;

            let transformed_name = transform_name_bytes(name_bytes, xor_trans);
            let name = decode_cp932(&transformed_name)?;

            if version >= 480 {
                if pos + 22 > data.len() {
                    return Err(Error::UnexpectedEof {
                        offset: pos,
                        need: 22,
                        have: data.len() - pos,
                    });
                }
                let kind = data[pos];
                let comp = data[pos + 1];
                let ul = u32::from_le_bytes([
                    data[pos + 2],
                    data[pos + 3],
                    data[pos + 4],
                    data[pos + 5],
                ]);
                let cl = u32::from_le_bytes([
                    data[pos + 6],
                    data[pos + 7],
                    data[pos + 8],
                    data[pos + 9],
                ]);
                let offset = u64::from_le_bytes([
                    data[pos + 10],
                    data[pos + 11],
                    data[pos + 12],
                    data[pos + 13],
                    data[pos + 14],
                    data[pos + 15],
                    data[pos + 16],
                    data[pos + 17],
                ]);
                let hash = u32::from_le_bytes([
                    data[pos + 18],
                    data[pos + 19],
                    data[pos + 20],
                    data[pos + 21],
                ]);
                pos += 22;

                entries.push(YpfIndexEntry {
                    name,
                    kind,
                    compressed: comp != 0,
                    uncompressed_len: ul,
                    compressed_len: cl,
                    offset: offset as u64,
                    hash,
                });
            } else {
                if pos + 18 > data.len() {
                    return Err(Error::UnexpectedEof {
                        offset: pos,
                        need: 18,
                        have: data.len() - pos,
                    });
                }
                let kind = data[pos];
                let comp = data[pos + 1];
                let ul = u32::from_le_bytes([
                    data[pos + 2],
                    data[pos + 3],
                    data[pos + 4],
                    data[pos + 5],
                ]);
                let cl = u32::from_le_bytes([
                    data[pos + 6],
                    data[pos + 7],
                    data[pos + 8],
                    data[pos + 9],
                ]);
                let offset = u32::from_le_bytes([
                    data[pos + 10],
                    data[pos + 11],
                    data[pos + 12],
                    data[pos + 13],
                ]);
                let hash = u32::from_le_bytes([
                    data[pos + 14],
                    data[pos + 15],
                    data[pos + 16],
                    data[pos + 17],
                ]);
                pos += 18;

                entries.push(YpfIndexEntry {
                    name,
                    kind,
                    compressed: comp != 0,
                    uncompressed_len: ul,
                    compressed_len: cl,
                    offset: offset as u64,
                    hash,
                });
            }
        }

        if pos != header_size as usize && pos < header_size as usize {
            return Err(Error::AssertFailed(format!(
                "header size: expect={}, actual={}",
                header_size, pos
            )));
        }

        Ok(entries)
    }

    pub fn parse(data: &[u8]) -> Result<Self> {
        if data.len() < 32 {
            return Err(Error::UnexpectedEof {
                offset: 0,
                need: 32,
                have: data.len(),
            });
        }

        let magic = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
        if magic != YPF_MAGIC {
            return Err(Error::BadMagic {
                expected: YPF_MAGIC,
                got: magic,
            });
        }

        let version = u32::from_le_bytes([data[4], data[5], data[6], data[7]]);
        check_version(version)?;

        let nent = u32::from_le_bytes([data[8], data[9], data[10], data[11]]);
        let lhdr = u32::from_le_bytes([data[12], data[13], data[14], data[15]]);

        if data.len() < 32 {
            return Err(Error::UnexpectedEof {
                offset: 0,
                need: 32,
                have: data.len(),
            });
        }

        let reserved = &data[16..32];
        if !reserved.iter().all(|&b| b == 0) {
            return Err(Error::AssertFailed("reserved bytes not zero".to_string()));
        }

        let nl_trans = if version == 500 {
            &NL_TRANS_V500
        } else {
            &NL_TRANS_V000
        };

        let xor_trans = match version {
            290 => &NAME_XOR_V290,
            500 => &NAME_XOR_V500,
            _ => &NAME_XOR_V000,
        };

        let header_size = if version >= 300 { lhdr } else { lhdr + 32 };

        let entry_count = nent as usize;
        let start_pos = 32;
        let mut pos = start_pos;
        let mut entries = Vec::with_capacity(entry_count);

        for _ in 0..entry_count {
            if pos + 5 > data.len() {
                return Err(Error::UnexpectedEof {
                    offset: pos,
                    need: 5,
                    have: data.len() - pos,
                });
            }

            let _name_hash =
                u32::from_le_bytes([data[pos], data[pos + 1], data[pos + 2], data[pos + 3]]);
            let name_size = data[pos + 4];
            pos += 5;

            let transformed_size = transform_name_size(name_size, nl_trans);
            if pos + transformed_size > data.len() {
                return Err(Error::UnexpectedEof {
                    offset: pos,
                    need: transformed_size,
                    have: data.len() - pos,
                });
            }

            let name_bytes = &data[pos..pos + transformed_size];
            pos += transformed_size;

            let transformed_name = transform_name_bytes(name_bytes, xor_trans);
            let name = decode_cp932(&transformed_name)?;

            if version >= 480 {
                if pos + 22 > data.len() {
                    return Err(Error::UnexpectedEof {
                        offset: pos,
                        need: 22,
                        have: data.len() - pos,
                    });
                }
                let kind = data[pos];
                let comp = data[pos + 1];
                let ul = u32::from_le_bytes([
                    data[pos + 2],
                    data[pos + 3],
                    data[pos + 4],
                    data[pos + 5],
                ]);
                let cl = u32::from_le_bytes([
                    data[pos + 6],
                    data[pos + 7],
                    data[pos + 8],
                    data[pos + 9],
                ]);
                let offset = u64::from_le_bytes([
                    data[pos + 10],
                    data[pos + 11],
                    data[pos + 12],
                    data[pos + 13],
                    data[pos + 14],
                    data[pos + 15],
                    data[pos + 16],
                    data[pos + 17],
                ]);
                let hash = u32::from_le_bytes([
                    data[pos + 18],
                    data[pos + 19],
                    data[pos + 20],
                    data[pos + 21],
                ]);
                pos += 22;

                entries.push(YpfEntry {
                    name,
                    kind,
                    compressed: comp != 0,
                    uncompressed_len: ul,
                    compressed_len: cl,
                    offset: offset as u64,
                    hash,
                    data: Vec::new(),
                });
            } else {
                if pos + 18 > data.len() {
                    return Err(Error::UnexpectedEof {
                        offset: pos,
                        need: 18,
                        have: data.len() - pos,
                    });
                }
                let kind = data[pos];
                let comp = data[pos + 1];
                let ul = u32::from_le_bytes([
                    data[pos + 2],
                    data[pos + 3],
                    data[pos + 4],
                    data[pos + 5],
                ]);
                let cl = u32::from_le_bytes([
                    data[pos + 6],
                    data[pos + 7],
                    data[pos + 8],
                    data[pos + 9],
                ]);
                let offset = u32::from_le_bytes([
                    data[pos + 10],
                    data[pos + 11],
                    data[pos + 12],
                    data[pos + 13],
                ]);
                let hash = u32::from_le_bytes([
                    data[pos + 14],
                    data[pos + 15],
                    data[pos + 16],
                    data[pos + 17],
                ]);
                pos += 18;

                entries.push(YpfEntry {
                    name,
                    kind,
                    compressed: comp != 0,
                    uncompressed_len: ul,
                    compressed_len: cl,
                    offset: offset as u64,
                    hash,
                    data: Vec::new(),
                });
            }
        }

        if pos != header_size as usize && pos < header_size as usize {
            return Err(Error::AssertFailed(format!(
                "header size: expect={}, actual={}",
                header_size, pos
            )));
        }

        for entry in entries.iter_mut() {
            let offset = entry.offset as usize;
            let cl = entry.compressed_len as usize;

            if offset + cl > data.len() {
                return Err(Error::UnexpectedEof {
                    offset,
                    need: cl,
                    have: data.len() - offset,
                });
            }

            let compressed_data = &data[offset..offset + cl];

            let file_data = if entry.compressed {
                let mut decoder = ZlibDecoder::new(compressed_data);
                let mut decompressed = Vec::new();
                decoder
                    .read_to_end(&mut decompressed)
                    .map_err(|e| Error::Decompress(e.to_string()))?;

                if decompressed.len() != entry.uncompressed_len as usize {
                    return Err(Error::Decompress(format!(
                        "decompressed size mismatch: expect={}, actual={}",
                        entry.uncompressed_len,
                        decompressed.len()
                    )));
                }

                decompressed
            } else {
                compressed_data.to_vec()
            };

            let hash_input = if entry.compressed {
                compressed_data
            } else {
                &file_data
            };

            match version {
                v if v < 265 => {}
                v if v < 470 => {
                    if entry.hash != 0 {
                        let _ = verify_adler(hash_input, entry.hash);
                    }
                }
                _ => {
                    if entry.hash != 0 {
                        let _ = verify_murmur(hash_input, entry.hash);
                    }
                }
            }

            entry.data = file_data;
        }

        Ok(Self { version, entries })
    }

    pub fn extract(&self) -> Vec<(String, Vec<u8>)> {
        self.entries
            .iter()
            .map(|e| (e.name.clone(), e.data.clone()))
            .collect()
    }
}

pub fn decode_entry(
    archive_data: &[u8],
    version: u32,
    compressed: bool,
    compressed_len: u32,
    uncompressed_len: u32,
    offset: u64,
    hash: u32,
) -> Result<Vec<u8>> {
    let offset = offset as usize;
    let compressed_len = compressed_len as usize;

    if offset + compressed_len > archive_data.len() {
        return Err(Error::UnexpectedEof {
            offset,
            need: compressed_len,
            have: archive_data.len() - offset,
        });
    }

    let compressed_data = &archive_data[offset..offset + compressed_len];

    let file_data = if compressed {
        let mut decoder = ZlibDecoder::new(compressed_data);
        let mut decompressed = Vec::new();
        decoder
            .read_to_end(&mut decompressed)
            .map_err(|e| Error::Decompress(e.to_string()))?;

        if decompressed.len() != uncompressed_len as usize {
            return Err(Error::Decompress(format!(
                "decompressed size mismatch: expect={}, actual={}",
                uncompressed_len,
                decompressed.len()
            )));
        }

        decompressed
    } else {
        compressed_data.to_vec()
    };

    let hash_input = if compressed { compressed_data } else { &file_data };

    match version {
        v if v < 265 => {}
        v if v < 470 => {
            if hash != 0 {
                let _ = verify_adler(hash_input, hash);
            }
        }
        _ => {
            if hash != 0 {
                let _ = verify_murmur(hash_input, hash);
            }
        }
    }

    Ok(file_data)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ypf_offset_size_threshold() {
        assert!(NL_TRANS_V000[18] == 18);
        assert!(NL_TRANS_V000[237] == 237);
        assert!(NL_TRANS_V000[255] == 255);
    }
}
