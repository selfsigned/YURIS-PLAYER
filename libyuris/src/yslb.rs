use crate::{check_version, Error, Reader, Result};

pub const YSLB_MAGIC: u32 = u32::from_le_bytes(*b"YSLB");

#[derive(Debug, Clone)]
pub struct Lbl {
    pub name: String,
    pub id: u32,
    pub ip: u32, // V200-300: offset, V300+: index
    pub scr_idx: u16,
    pub if_lvl: u8,
    pub loop_lvl: u8,
}

pub struct Yslb {
    pub version: u32,
    pub lbls: Vec<Lbl>,
}

impl Yslb {
    pub fn parse(data: &[u8]) -> Result<Self> {
        let mut r = Reader::new(data);

        let magic = r.read_u32_le()?;
        if magic != YSLB_MAGIC {
            return Err(Error::BadMagic {
                expected: YSLB_MAGIC,
                got: magic,
            });
        }

        let version = r.read_u32_le()?;
        check_version(version)?;

        let nlbl = r.read_u32_le()?;

        // Skip 256 * 4 bytes of padding (from Python: r.idx += 4 * 256)
        r.skip(4 * 256)?;

        let mut lbls = Vec::with_capacity(nlbl as usize);
        for _ in 0..nlbl {
            let name_len = r.read_u8()?;
            let name = r.read_str_cp932(name_len as usize)?;

            let id = r.read_u32_le()?;
            let ip = r.read_u32_le()?;
            let scr_idx = r.read_u16_le()?;
            let if_lvl = r.read_u8()?;
            let loop_lvl = r.read_u8()?;

            lbls.push(Lbl {
                name,
                id,
                ip,
                scr_idx,
                if_lvl,
                loop_lvl,
            });
        }

        r.assert_eof("yslb")?;

        Ok(Yslb { version, lbls })
    }
}
