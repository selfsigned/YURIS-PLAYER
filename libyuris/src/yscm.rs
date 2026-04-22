use crate::{check_version, Error, Reader, Result};

pub const YSCM_MAGIC: u32 = u32::from_le_bytes(*b"YSCM");
const N_ERR_STR: usize = 37;

#[derive(Debug, Clone)]
pub struct MArg {
    pub name: String,
    pub typ: u8, // 0:Any, 1:Int, 2:Flt, 3:Str
    pub chk: u8,
}

#[derive(Debug, Clone)]
pub struct MCmd {
    pub name: String,
    pub args: Vec<MArg>,
}

pub struct KnownCmdCode {
    pub if_code: u8,
    pub else_code: u8,
    pub loop_code: u8,
    pub returncode: u8,
    pub word: u8,
}

impl KnownCmdCode {
    pub fn from_cmds(cmds: &[MCmd]) -> Self {
        let mut if_code = 0;
        let mut else_code = 0;
        let mut loop_code = 0;
        let mut returncode = 0;
        let mut word = 0;

        for (i, cmd) in cmds.iter().enumerate() {
            match cmd.name.as_str() {
                "IF" => if_code = i as u8,
                "ELSE" => else_code = i as u8,
                "LOOP" => loop_code = i as u8,
                "RETURNCODE" => returncode = i as u8,
                "WORD" => word = i as u8,
                _ => {}
            }
        }

        KnownCmdCode {
            if_code,
            else_code,
            loop_code,
            returncode,
            word,
        }
    }
}

pub struct Yscm {
    pub version: u32,
    pub cmds: Vec<MCmd>,
    pub errs: Vec<String>,
    pub b256: Vec<u8>,
    pub kcc: KnownCmdCode,
}

impl Yscm {
    pub fn parse(data: &[u8]) -> Result<Self> {
        let mut r = Reader::new(data);

        let magic = r.read_u32_le()?;
        if magic != YSCM_MAGIC {
            return Err(Error::BadMagic {
                expected: YSCM_MAGIC,
                got: magic,
            });
        }

        let version = r.read_u32_le()?;
        check_version(version)?;

        let ncmd = r.read_u32_le()?;
        let _pad = r.read_u32_le()?;

        let mut cmds = Vec::with_capacity(ncmd as usize);
        for _ in 0..ncmd {
            let name = r.read_cstr_cp932()?;
            let narg = r.read_u8()?;
            let mut args = Vec::with_capacity(narg as usize);
            for _ in 0..narg {
                let arg_name = r.read_cstr_cp932()?;
                let typ = r.read_u8()?;
                let chk = r.read_u8()?;
                args.push(MArg {
                    name: arg_name,
                    typ,
                    chk,
                });
            }
            cmds.push(MCmd { name, args });
        }

        let mut errs = Vec::with_capacity(N_ERR_STR);
        for _ in 0..N_ERR_STR {
            errs.push(r.read_cstr_cp932()?);
        }

        let b256 = r.read_bytes(256)?.to_vec();

        let kcc = KnownCmdCode::from_cmds(&cmds);

        r.assert_eof("yscm")?;

        Ok(Yscm {
            version,
            cmds,
            errs,
            b256,
            kcc,
        })
    }
}
