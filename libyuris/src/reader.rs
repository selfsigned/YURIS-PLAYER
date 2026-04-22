use crate::{Error, Result};
use std::convert::TryInto;

pub struct Reader<'a> {
    buf: &'a [u8],
    pub pos: usize,
}

impl<'a> Reader<'a> {
    pub fn new(buf: &'a [u8]) -> Self {
        Self { buf, pos: 0 }
    }

    pub fn remaining(&self) -> usize {
        self.buf.len().saturating_sub(self.pos)
    }

    pub fn slice(&self) -> &'a [u8] {
        &self.buf[self.pos..]
    }

    pub fn read_bytes(&mut self, n: usize) -> Result<&'a [u8]> {
        let end = self.pos.checked_add(n).ok_or(Error::UnexpectedEof {
            offset: self.pos,
            need: n,
            have: self.buf.len() - self.pos,
        })?;
        if end > self.buf.len() {
            return Err(Error::UnexpectedEof {
                offset: self.pos,
                need: n,
                have: self.buf.len() - self.pos,
            });
        }
        let slice = &self.buf[self.pos..end];
        self.pos = end;
        Ok(slice)
    }

    pub fn read_u8(&mut self) -> Result<u8> {
        let b = self.read_bytes(1)?;
        Ok(b[0])
    }

    pub fn read_u16_le(&mut self) -> Result<u16> {
        let b = self.read_bytes(2)?;
        Ok(u16::from_le_bytes(b.try_into().expect("read_bytes guarantees 2 bytes")))
    }

    pub fn read_u32_le(&mut self) -> Result<u32> {
        let b = self.read_bytes(4)?;
        Ok(u32::from_le_bytes(b.try_into().expect("read_bytes guarantees 4 bytes")))
    }

    pub fn read_i32_le(&mut self) -> Result<i32> {
        let b = self.read_bytes(4)?;
        Ok(i32::from_le_bytes(b.try_into().expect("read_bytes guarantees 4 bytes")))
    }

    pub fn read_i64_le(&mut self) -> Result<i64> {
        let b = self.read_bytes(8)?;
        Ok(i64::from_le_bytes(b.try_into().expect("read_bytes guarantees 8 bytes")))
    }

    pub fn read_u64_le(&mut self) -> Result<u64> {
        let b = self.read_bytes(8)?;
        Ok(u64::from_le_bytes(b.try_into().expect("read_bytes guarantees 8 bytes")))
    }

    pub fn read_f64_le(&mut self) -> Result<f64> {
        let b = self.read_bytes(8)?;
        Ok(f64::from_le_bytes(b.try_into().expect("read_bytes guarantees 8 bytes")))
    }

    pub fn read_cstr(&mut self) -> Result<&'a [u8]> {
        let start = self.pos;
        while self.pos < self.buf.len() {
            if self.buf[self.pos] == 0 {
                let s = &self.buf[start..self.pos];
                self.pos += 1;
                return Ok(s);
            }
            self.pos += 1;
        }
        Err(Error::UnexpectedEof {
            offset: start,
            need: 1,
            have: 0,
        })
    }

    pub fn read_cstr_utf8(&mut self) -> Result<&'a str> {
        let bytes = self.read_cstr()?;
        std::str::from_utf8(bytes).map_err(|e| Error::InvalidUtf8 {
            offset: self.pos - bytes.len(),
            source: e,
        })
    }

    pub fn read_cstr_cp932(&mut self) -> Result<String> {
        let bytes = self.read_cstr()?;
        decode_cp932(bytes)
    }

    pub fn read_str_cp932(&mut self, n: usize) -> Result<String> {
        let bytes = self.read_bytes(n)?;
        decode_cp932(bytes)
    }

    pub fn skip(&mut self, n: usize) -> Result<()> {
        self.read_bytes(n)?;
        Ok(())
    }

    pub fn assert_eof(&self, _context: &str) -> Result<()> {
        if self.pos != self.buf.len() {
            return Err(Error::TrailingData(self.buf.len() - self.pos));
        }
        Ok(())
    }
}

fn decode_cp932(bytes: &[u8]) -> Result<String> {
    let (decoded, _, had_errors) = encoding_rs::SHIFT_JIS.decode(bytes);
    if had_errors {
        // Fallback: return what we can, the python impl also has fallbacks
        // For now just use the decoded result even with errors
    }
    Ok(decoded.into_owned())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_read_past_eof() {
        let data = vec![0x00; 2];
        let mut r = Reader::new(&data);
        let result = r.read_u32_le();
        assert!(result.is_err());
        match result {
            Err(Error::UnexpectedEof { offset, need, have }) => {
                assert_eq!(offset, 0);
                assert_eq!(need, 4);
                assert_eq!(have, 2);
            }
            _ => panic!("expected UnexpectedEof"),
        }
    }

    #[test]
    fn test_read_u16_ok() {
        let data = vec![0x34, 0x12];
        let mut r = Reader::new(&data);
        let result = r.read_u16_le();
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 0x1234);
    }

    #[test]
    fn test_read_u64_ok() {
        let data = 0x0123456789ABCDEFu64.to_le_bytes().to_vec();
        let mut r = Reader::new(&data);
        let result = r.read_u64_le();
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 0x0123456789ABCDEF);
    }
}
