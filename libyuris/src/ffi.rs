use crate::crypto::xor_vec;
use crate::ypf;
use std::collections::HashMap;
use std::ptr;
use std::sync::{Mutex, OnceLock};

struct AbiState {
    manifests: HashMap<usize, ManifestAlloc>,
    buffers: HashMap<usize, usize>,
}

struct ManifestAlloc {
    num_files: usize,
    files_ptr: *mut YurisFile,
    names: Vec<String>,
}

unsafe impl Send for ManifestAlloc {}
unsafe impl Sync for ManifestAlloc {}

static STATE: OnceLock<Mutex<AbiState>> = OnceLock::new();

fn state() -> &'static Mutex<AbiState> {
    STATE.get_or_init(|| {
        Mutex::new(AbiState {
            manifests: HashMap::new(),
            buffers: HashMap::new(),
        })
    })
}

fn with_state<T, F>(f: F) -> Option<T>
where
    F: FnOnce(&mut AbiState) -> Option<T>,
{
    let guard = match state().lock() {
        Ok(g) => g,
        Err(poisoned) => poisoned.into_inner(),
    };
    let mut state = guard;
    f(&mut state)
}

#[no_mangle]
pub extern "C" fn yuris_decrypt(
    data: *const u8,
    len: usize,
    version: *const YurisVersion,
    out_len: *mut usize,
) -> *mut u8 {
    if data.is_null() || len == 0 || version.is_null() || out_len.is_null() {
        return ptr::null_mut();
    }

    let input = unsafe { std::slice::from_raw_parts(data, len) };
    let version = unsafe { &*version };

    let key = crate::key_for_version(version.version);
    let decrypted = xor_vec(input, key);

    let len_usize = decrypted.len();
    let mut boxed: Box<[u8]> = decrypted.into_boxed_slice();
    let ptr_val = boxed.as_mut_ptr();

    if with_state(|s| {
        s.buffers.insert(ptr_val as usize, len_usize);
        Some(())
    }).is_none() {
        unsafe { *out_len = 0; }
        return ptr::null_mut();
    }

    std::mem::forget(boxed);
    unsafe { *out_len = len_usize; }
    ptr_val
}

fn probe_version(data: &[u8]) -> Option<YurisVersion> {
    if data.len() < 8 {
        return None;
    }

    let magic = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
    let version = u32::from_le_bytes([data[4], data[5], data[6], data[7]]);

    if crate::check_version(version).is_err() {
        return None;
    }

    let xor_key = if magic == crate::ystb::YSTB_MAGIC {
        match crate::ystb::guess_key(data) {
            Ok(0) => crate::key_for_version(version),
            Ok(k) => k,
            Err(_) => crate::key_for_version(version),
        }
    } else if matches!(
        magic,
        crate::ysvr::YSVR_MAGIC
            | crate::yslb::YSLB_MAGIC
            | crate::yscm::YSCM_MAGIC
            | crate::ystl::YSTL_MAGIC
            | crate::ypf::YPF_MAGIC
    ) {
        crate::key_for_version(version)
    } else {
        return None;
    };

    let instr_len = if version < 290 { 12 } else { 4 };

    Some(YurisVersion {
        version,
        xor_key,
        instr_len,
    })
}

#[no_mangle]
pub extern "C" fn yuris_get_manifest(data: *const u8, len: usize) -> *mut YurisManifest {
    if data.is_null() || len == 0 {
        return ptr::null_mut();
    }

    let input = unsafe { std::slice::from_raw_parts(data, len) };

    let version = match probe_version(input) {
        Some(v) => v,
        None => return ptr::null_mut(),
    };

    let entries = match ypf::Ypf::parse_index(input) {
        Ok(e) => e,
        Err(_) => return ptr::null_mut(),
    };

    let num_files = entries.len();
    let mut names: Vec<String> = Vec::with_capacity(num_files);

    let mut files_vec: Vec<YurisFile> = Vec::with_capacity(num_files);
    for entry in entries {
        let name_bytes = entry.name.as_bytes();
        let mut name_arr = [0u8; 64];
        let copy_len = name_bytes.len().min(63);
        name_arr[..copy_len].copy_from_slice(&name_bytes[..copy_len]);

        files_vec.push(YurisFile {
            name: name_arr,
            offset: entry.offset as u64,
            stored_size: entry.compressed_len,
            unpacked_size: entry.uncompressed_len,
            compressed: if entry.compressed { 1 } else { 0 },
            hash: entry.hash,
        });
        names.push(entry.name);
    }

    let files_ptr = files_vec.as_mut_ptr();
    std::mem::forget(files_vec);

    let manifest = Box::new(YurisManifest {
        version,
        num_files: num_files as u32,
        files: files_ptr,
    });
    let manifest_ptr = Box::into_raw(manifest);

    let alloc = ManifestAlloc {
        num_files,
        files_ptr,
        names,
    };

    if with_state(|s| {
        s.manifests.insert(manifest_ptr as usize, alloc);
        Some(())
    }).is_none() {
        unsafe {
            drop(Box::from_raw(manifest_ptr));
        }
        return ptr::null_mut();
    }

    manifest_ptr
}

#[no_mangle]
pub extern "C" fn yuris_file_read(
    file_data: *const u8,
    file_len: usize,
    version: *const YurisVersion,
    file: *const YurisFile,
    out_len: *mut usize,
) -> *mut u8 {
    if file_data.is_null() || version.is_null() || file.is_null() {
        return ptr::null_mut();
    }

    let version = unsafe { &*version };
    let file = unsafe { &*file };

    if file_len != file.stored_size as usize {
        if !out_len.is_null() {
            unsafe { *out_len = 0; }
        }
        return ptr::null_mut();
    }

    let data = unsafe { std::slice::from_raw_parts(file_data, file_len) };

    let decoded = match ypf::decode_entry(
        data,
        version.version,
        file.compressed != 0,
        file.stored_size,
        file.unpacked_size,
        0,
        file.hash,
    ) {
        Ok(d) => d,
        Err(_) => {
            if !out_len.is_null() {
                unsafe { *out_len = 0; }
            }
            return ptr::null_mut();
        }
    };

    let len_usize = decoded.len();
    let mut boxed: Box<[u8]> = decoded.into_boxed_slice();
    let ptr_val = boxed.as_mut_ptr();

    if with_state(|s| {
        s.buffers.insert(ptr_val as usize, len_usize);
        Some(())
    }).is_none() {
        if !out_len.is_null() {
            unsafe { *out_len = 0; }
        }
        return ptr::null_mut();
    }

    std::mem::forget(boxed);
    if !out_len.is_null() {
        unsafe { *out_len = len_usize; }
    }
    ptr_val
}

#[no_mangle]
pub extern "C" fn yuris_free(ptr: *mut libc::c_void) {
    if ptr.is_null() {
        return;
    }

    let ptr_val = ptr as usize;

    if with_state(|s| {
        if let Some(alloc) = s.manifests.remove(&ptr_val) {
            unsafe {
                let files_vec = Vec::from_raw_parts(alloc.files_ptr, alloc.num_files, alloc.num_files);
                drop(files_vec);
                drop(Box::from_raw(ptr_val as *mut YurisManifest));
            }
            Some(true)
        } else {
            None
        }
    }).is_some() {
        return;
    }

    if with_state(|s| {
        if let Some(len) = s.buffers.remove(&ptr_val) {
            unsafe {
                drop(Box::from_raw(std::slice::from_raw_parts_mut(ptr_val as *mut u8, len)));
            }
            Some(true)
        } else {
            None
        }
    }).is_some() {
        return;
    }
}

#[repr(C)]
pub struct YurisVersion {
    pub version: u32,
    pub xor_key: u32,
    pub instr_len: u8,
}

#[repr(C)]
pub struct YurisFile {
    pub name: [u8; 64],
    pub offset: u64,
    pub stored_size: u32,
    pub unpacked_size: u32,
    pub compressed: u8,
    pub hash: u32,
}

#[repr(C)]
pub struct YurisManifest {
    pub version: YurisVersion,
    pub num_files: u32,
    pub files: *mut YurisFile,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_probe_version_valid_ystb_v300() {
        let data = b"YSTB\x2c\x01\x00\x00".to_vec();
        let result = probe_version(&data);
        assert!(result.is_some());
        let v = result.unwrap();
        assert_eq!(v.version, 300);
        assert_eq!(v.xor_key, crate::KEY_300);
        assert_eq!(v.instr_len, 4);
    }

    #[test]
    fn test_probe_version_valid_ystb_v200() {
        let data = b"YSTB\xc8\x00\x00\x00".to_vec();
        let result = probe_version(&data);
        assert!(result.is_some());
        let v = result.unwrap();
        assert_eq!(v.version, 200);
        assert_eq!(v.xor_key, crate::KEY_200);
        assert_eq!(v.instr_len, 12);
    }

    #[test]
    fn test_probe_version_valid_ypf_v300() {
        let data = b"YPF\x00\x2c\x01\x00\x00".to_vec();
        let result = probe_version(&data);
        assert!(result.is_some());
        let v = result.unwrap();
        assert_eq!(v.version, 300);
        assert_eq!(v.xor_key, crate::KEY_300);
        assert_eq!(v.instr_len, 4);
    }

    #[test]
    fn test_probe_version_unknown_magic() {
        let data = b"XXXX\x2c\x01\x00\x00".to_vec();
        assert!(probe_version(&data).is_none());
    }

    #[test]
    fn test_probe_version_short_length() {
        let data = b"YSTB\x2c".to_vec();
        assert!(probe_version(&data).is_none());
    }

    #[test]
    fn test_probe_version_invalid_version() {
        let data = b"YSTB\xff\xff\xff\xff".to_vec();
        assert!(probe_version(&data).is_none());
    }

    #[test]
    fn test_get_manifest_with_version_metadata() {
        let mut data = vec![0u8; 32];
        data[0..4].copy_from_slice(b"YPF\x00");
        data[4..8].copy_from_slice(&300u32.to_le_bytes());
        data[8..12].copy_from_slice(&0u32.to_le_bytes());
        data[12..16].copy_from_slice(&0u32.to_le_bytes());

        let manifest = unsafe { yuris_get_manifest(data.as_ptr(), data.len()) };
        assert!(!manifest.is_null());

        unsafe {
            assert_eq!((*manifest).version.version, 300);
            assert_eq!((*manifest).version.xor_key, crate::KEY_300);
            assert_eq!((*manifest).version.instr_len, 4);
            assert_eq!((*manifest).num_files, 0);
            yuris_free(manifest as *mut libc::c_void);
        }
    }

    #[test]
    fn test_get_manifest_null_data() {
        assert!(yuris_get_manifest(std::ptr::null(), 0).is_null());
    }

    #[test]
    fn test_get_manifest_short_length() {
        let data = b"YPF".to_vec();
        assert!(yuris_get_manifest(data.as_ptr(), data.len()).is_null());
    }

    #[test]
    fn test_get_manifest_bad_magic() {
        let mut data = vec![0u8; 32];
        data[0..4].copy_from_slice(b"XXXX");
        data[4..8].copy_from_slice(&300u32.to_le_bytes());
        assert!(yuris_get_manifest(data.as_ptr(), data.len()).is_null());
    }

    #[test]
    fn test_file_read_uncompressed_with_nonzero_offset() {
        let raw_data = b"Hello, World! This is test data for uncompressed read.";

        let version = YurisVersion {
            version: 300,
            xor_key: crate::KEY_300,
            instr_len: 4,
        };

        let file = YurisFile {
            name: [0u8; 64],
            offset: 100,
            stored_size: raw_data.len() as u32,
            unpacked_size: raw_data.len() as u32,
            compressed: 0,
            hash: 0,
        };

        let mut out_len: usize = 0;
        let result = yuris_file_read(
            raw_data.as_ptr(),
            raw_data.len(),
            &version,
            &file,
            &mut out_len,
        );

        assert!(!result.is_null());
        assert_eq!(out_len, raw_data.len());

        let decoded = unsafe { std::slice::from_raw_parts(result, out_len) };
        assert_eq!(decoded, raw_data);

        yuris_free(result as *mut libc::c_void);
    }

    #[test]
    fn test_file_read_compressed_with_nonzero_offset() {
        use std::io::Write;

        let raw_data = b"Compressed test data.";
        let mut compressed = Vec::new();
        {
            let mut encoder = flate2::write::ZlibEncoder::new(&mut compressed, flate2::Compression::default());
            encoder.write_all(raw_data).unwrap();
            encoder.finish().unwrap();
        }

        let version = YurisVersion {
            version: 300,
            xor_key: crate::KEY_300,
            instr_len: 4,
        };

        let file = YurisFile {
            name: [0u8; 64],
            offset: 500,
            stored_size: compressed.len() as u32,
            unpacked_size: raw_data.len() as u32,
            compressed: 1,
            hash: 0,
        };

        let mut out_len: usize = 0;
        let result = yuris_file_read(
            compressed.as_ptr(),
            compressed.len(),
            &version,
            &file,
            &mut out_len,
        );

        assert!(!result.is_null());
        assert_eq!(out_len, raw_data.len());

        let decompressed = unsafe { std::slice::from_raw_parts(result, out_len) };
        assert_eq!(decompressed, raw_data);

        yuris_free(result as *mut libc::c_void);
    }

    #[test]
    fn test_file_read_out_len_null_success() {
        let raw_data = b"Null out_len test data.";

        let version = YurisVersion {
            version: 300,
            xor_key: crate::KEY_300,
            instr_len: 4,
        };

        let file = YurisFile {
            name: [0u8; 64],
            offset: 0,
            stored_size: raw_data.len() as u32,
            unpacked_size: raw_data.len() as u32,
            compressed: 0,
            hash: 0,
        };

        let result = yuris_file_read(
            raw_data.as_ptr(),
            raw_data.len(),
            &version,
            &file,
            std::ptr::null_mut(),
        );

        assert!(!result.is_null());

        let decoded = unsafe { std::slice::from_raw_parts(result, raw_data.len()) };
        assert_eq!(decoded, raw_data);

        yuris_free(result as *mut libc::c_void);
    }

    #[test]
    fn test_file_read_out_len_zeroed_on_failure() {
        let version = YurisVersion {
            version: 300,
            xor_key: crate::KEY_300,
            instr_len: 4,
        };

        let file = YurisFile {
            name: [0u8; 64],
            offset: 0,
            stored_size: 100,
            unpacked_size: 50,
            compressed: 0,
            hash: 0,
        };

        let bad_data = vec![0u8; 50];

        let mut out_len: usize = 999;
        let result = yuris_file_read(
            bad_data.as_ptr(),
            bad_data.len(),
            &version,
            &file,
            &mut out_len,
        );

        assert!(result.is_null());
        assert_eq!(out_len, 0);
    }

    #[test]
    fn test_file_read_null_ptrs() {
        let mut out_len: usize = 0;

        assert!(yuris_file_read(
            std::ptr::null(),
            0,
            std::ptr::null(),
            std::ptr::null(),
            &mut out_len
        ).is_null());

        let version = YurisVersion {
            version: 300,
            xor_key: crate::KEY_300,
            instr_len: 4,
        };

        assert!(yuris_file_read(
            std::ptr::null(),
            0,
            &version,
            std::ptr::null(),
            &mut out_len
        ).is_null());
    }

    #[test]
    fn test_file_read_file_len_mismatch() {
        let version = YurisVersion {
            version: 300,
            xor_key: crate::KEY_300,
            instr_len: 4,
        };

        let file = YurisFile {
            name: [0u8; 64],
            offset: 0,
            stored_size: 100,
            unpacked_size: 100,
            compressed: 0,
            hash: 0,
        };

        let data = vec![0u8; 50];

        let mut out_len: usize = 999;
        let result = yuris_file_read(
            data.as_ptr(),
            data.len(),
            &version,
            &file,
            &mut out_len,
        );

        assert!(result.is_null());
        assert_eq!(out_len, 0);
    }
}