use crate::expr::Ins;
use crate::{check_version, Error, Reader, Result};

pub const YSVR_MAGIC: u32 = u32::from_le_bytes(*b"YSVR");
const VAR_USR_MIN: u32 = 1000;

#[derive(Debug, Clone)]
pub struct Var {
    pub scope: u8, // 1:G, 2:S, 3:F
    pub g_ext: u8, // 0:Sys, 1-3:Usr
    pub scr_idx: u16,
    pub var_idx: u16,
    pub typ: u8, // 0:none, 1:Int, 2:Flt, 3:Str
    pub dim: Vec<u32>,
    pub initv: InitVal,
}

#[derive(Debug, Clone)]
pub enum InitVal {
    None,
    Int(i64),
    Float(f64),
    Expr(Vec<Ins>),
}

pub struct Ysvr {
    pub version: u32,
    pub vars: Vec<Var>,
}

impl Ysvr {
    pub fn parse(data: &[u8]) -> Result<Self> {
        let mut r = Reader::new(data);

        let magic = r.read_u32_le()?;
        if magic != YSVR_MAGIC {
            return Err(Error::BadMagic {
                expected: YSVR_MAGIC,
                got: magic,
            });
        }

        let version = r.read_u32_le()?;
        check_version(version)?;

        let nvar = r.read_u16_le()?;

        let mut vars = Vec::with_capacity(nvar as usize);
        for _ in 0..nvar {
            let var = if version < 481 {
                Var::parse_v000(&mut r)?
            } else {
                Var::parse_v481(&mut r)?
            };
            vars.push(var);
        }

        r.assert_eof("ysvr")?;

        Ok(Ysvr { version, vars })
    }
}

impl Var {
    fn parse_v000(r: &mut Reader) -> Result<Self> {
        let scope = r.read_u8()?;
        let scr_idx = r.read_u16_le()?;
        let var_idx = r.read_u16_le()?;
        let typ = r.read_u8()?;
        let ndim = r.read_u8()?;

        let g_ext = if var_idx < VAR_USR_MIN as u16 { 0 } else { 1 };
        let dim = Self::read_dims(r, ndim)?;
        let initv = Self::read_initv(r, typ)?;

        Ok(Var {
            scope,
            g_ext,
            scr_idx,
            var_idx,
            typ,
            dim,
            initv,
        })
    }

    fn parse_v481(r: &mut Reader) -> Result<Self> {
        let scope = r.read_u8()?;
        let g_ext = r.read_u8()?;
        let scr_idx = r.read_u16_le()?;
        let var_idx = r.read_u16_le()?;
        let typ = r.read_u8()?;
        let ndim = r.read_u8()?;

        let dim = Self::read_dims(r, ndim)?;
        let initv = Self::read_initv(r, typ)?;

        Ok(Var {
            scope,
            g_ext,
            scr_idx,
            var_idx,
            typ,
            dim,
            initv,
        })
    }

    fn read_dims(r: &mut Reader, ndim: u8) -> Result<Vec<u32>> {
        let mut dims = Vec::with_capacity(ndim as usize);
        for _ in 0..ndim {
            dims.push(r.read_u32_le()?);
        }
        Ok(dims)
    }

    fn read_initv(r: &mut Reader, typ: u8) -> Result<InitVal> {
        match typ {
            0 => Ok(InitVal::None),
            1 => {
                let v = r.read_i64_le()?;
                Ok(InitVal::Int(v))
            }
            2 => {
                let v = r.read_f64_le()?;
                Ok(InitVal::Float(v))
            }
            3 => {
                let len = r.read_u16_le()?;
                let data = r.read_bytes(len as usize)?;
                let insns = Ins::parse_buf(data)?;
                Ok(InitVal::Expr(insns))
            }
            _ => Err(Error::AssertFailed(format!("unknown var type: {typ}"))),
        }
    }
}
