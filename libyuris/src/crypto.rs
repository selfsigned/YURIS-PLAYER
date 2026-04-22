/// XOR decryption for YSTB files.
/// The key is applied 4 bytes at a time across each segment.

pub fn xor_trans(data: &mut [u8], key: u32) {
    let k = key.to_be_bytes(); // Python: key.to_bytes(4, 'big')
    let aligned = data.len() & !3;

    for i in (0..aligned).step_by(4) {
        data[i] ^= k[0];
        data[i + 1] ^= k[1];
        data[i + 2] ^= k[2];
        data[i + 3] ^= k[3];
    }

    let remainder = data.len() & 3;
    for j in 0..remainder {
        data[aligned + j] ^= k[j];
    }
}

/// In-place XOR returning a Vec (useful when you don't own the data).
pub fn xor_vec(data: &[u8], key: u32) -> Vec<u8> {
    let mut buf = data.to_vec();
    xor_trans(&mut buf, key);
    buf
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_xor_roundtrip() {
        let data = vec![0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04];
        let key = 0xD36FAC96;
        let mut buf = data.clone();
        xor_trans(&mut buf, key);
        // double XOR should restore
        xor_trans(&mut buf, key);
        assert_eq!(buf, data);
    }

    #[test]
    fn test_xor_nonaligned() {
        let data = vec![0x01, 0x02, 0x03, 0x04, 0x05];
        let key = 0x07B4024A;
        let mut buf = data.clone();
        xor_trans(&mut buf, key);
        xor_trans(&mut buf, key);
        assert_eq!(buf, data);
    }

    #[test]
    fn test_xor_matches_python() {
        // Verify our XOR matches the Python reference implementation.
        // xor_trans(bytearray([0]*8), 0xD36FAC96) in Python:
        //   key bytes big-endian: D3, 6F, AC, 96
        //   [0xD3, 0x6F, 0xAC, 0x96, 0xD3, 0x6F, 0xAC, 0x96]
        let mut buf = vec![0u8; 8];
        xor_trans(&mut buf, 0xD36FAC96);
        assert_eq!(buf, vec![0xD3, 0x6F, 0xAC, 0x96, 0xD3, 0x6F, 0xAC, 0x96]);
    }
}
