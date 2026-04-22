use crate::{check_version, Error, Reader, Result};

pub const YSTL_MAGIC: u32 = u32::from_le_bytes(*b"YSTL");

#[derive(Debug, Clone)]
pub struct Scr {
    pub idx: u32,
    pub path: String,
    pub time: i64,
    pub nvar: i32,
    pub nlbl: i32,
    pub ntext: i32,
}

pub struct Ystl {
    pub version: u32,
    pub scrs: Vec<Scr>,
}

impl Ystl {
    pub fn parse(data: &[u8]) -> Result<Self> {
        let mut r = Reader::new(data);

        let magic = r.read_u32_le()?;
        if magic != YSTL_MAGIC {
            return Err(Error::BadMagic {
                expected: YSTL_MAGIC,
                got: magic,
            });
        }

        let version = r.read_u32_le()?;
        check_version(version)?;

        let nscr = r.read_u32_le()?;

        let mut scrs = Vec::with_capacity(nscr as usize);
        for i in 0..nscr {
            let scr = if version < 470 {
                Scr::parse_v200(&mut r, i)?
            } else {
                Scr::parse_v470(&mut r, i)?
            };
            scrs.push(scr);
        }

        r.assert_eof("ystl")?;

        Ok(Ystl { version, scrs })
    }
}

impl Scr {
    fn parse_v200(r: &mut Reader, _expected_idx: u32) -> Result<Self> {
        let idx = r.read_u32_le()?;
        let path_len = r.read_u32_le()?;
        let path = r.read_str_cp932(path_len as usize)?;
        let time = r.read_i64_le()?;
        let nvar = r.read_i32_le()?;
        let nlbl = r.read_i32_le()?;

        Ok(Scr {
            idx,
            path,
            time,
            nvar,
            nlbl,
            ntext: 0,
        })
    }

    fn parse_v470(r: &mut Reader, _expected_idx: u32) -> Result<Self> {
        let idx = r.read_u32_le()?;
        let path_len = r.read_u32_le()?;
        let path = r.read_str_cp932(path_len as usize)?;
        let time = r.read_i64_le()?;
        let nvar = r.read_i32_le()?;
        let nlbl = r.read_i32_le()?;
        let ntext = r.read_i32_le()?;

        Ok(Scr {
            idx,
            path,
            time,
            nvar,
            nlbl,
            ntext,
        })
    }
}
