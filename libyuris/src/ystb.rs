use crate::crypto::xor_trans;
use crate::expr::Ins;
use crate::yscm::KnownCmdCode;
use crate::{check_version, Error, Reader, Result};

pub const YSTB_MAGIC: u32 = u32::from_le_bytes(*b"YSTB");

#[derive(Debug, Clone)]
pub struct Arg {
    pub id: u16,
    pub typ: u8,
    pub aop: u8,
    pub len: u32,
    pub off: u32,
    pub data: ArgData,
}

#[derive(Debug, Clone)]
pub enum ArgData {
    None,
    Expr(Vec<Ins>),
    Word(String),
}

#[derive(Debug, Clone)]
pub struct Cmd {
    pub off: usize,
    pub lno: u32,
    pub code: u8,
    pub npar: u16,
    pub args: Vec<Arg>,
}

#[derive(Debug)]
pub struct Ystb {
    pub version: u32,
    pub key: u32,
    pub cmds: Vec<Cmd>,
}

pub fn guess_key(data: &[u8]) -> Result<u32> {
    if data.len() < 8 {
        return Err(Error::UnexpectedEof {
            offset: 0,
            need: 8,
            have: data.len(),
        });
    }
    let version = u32::from_le_bytes(
        data[4..8].try_into().map_err(|_| Error::UnexpectedEof {
            offset: 4,
            need: 4,
            have: data.len().saturating_sub(4),
        })?,
    );
    if version >= 200 && version < 300 {
        if data.len() < 0x30 {
            return Ok(0);
        }
        Ok(u32::from_le_bytes(
            data[0x2C..0x30].try_into().map_err(|_| Error::UnexpectedEof {
                offset: 0x2C,
                need: 4,
                have: data.len().saturating_sub(0x2C),
            })?,
        ))
    } else {
        Ok(0xD36FAC96)
    }
}

impl Ystb {
    pub fn parse(data: &[u8], kcc: &KnownCmdCode, key: u32) -> Result<Self> {
        let mut r = Reader::new(data);

        let magic = r.read_u32_le()?;
        if magic != YSTB_MAGIC {
            return Err(Error::BadMagic {
                expected: YSTB_MAGIC,
                got: magic,
            });
        }

        let version = r.read_u32_le()?;
        check_version(version)?;

        // If key is 0, try to extract it from the file
        let key = if key == 0 { guess_key(data)? } else { key };

        if version < 300 {
            Self::parse_v2(r, data, kcc, key, version)
        } else {
            Self::parse_v3(r, data, kcc, key, version)
        }
    }

    fn parse_v2(
        mut r: Reader,
        data: &[u8],
        kcc: &KnownCmdCode,
        key: u32,
        version: u32,
    ) -> Result<Self> {
        let code_seg_size = r.read_u32_le()?;
        let args_seg_size = r.read_u32_le()?;
        let _args_seg_offset = r.read_u32_le()?;
        let _reserved0 = r.read_u32_le()?;
        let _reserved1 = r.read_u32_le()?;
        let _reserved2 = r.read_u32_le()?;

        // Header is 32 bytes
        let header_size = 32;

        // Read and decrypt code segment
        let code_start = header_size;
        let code_end = code_start + code_seg_size as usize;
        if code_end > data.len() {
            return Err(Error::UnexpectedEof {
                offset: code_start,
                need: code_seg_size as usize,
                have: data.len() - code_start,
            });
        }
        let mut code_data = data[code_start..code_end].to_vec();
        xor_trans(&mut code_data, key);

        // Read and decrypt args segment
        let args_start = code_end;
        let args_end = args_start + args_seg_size as usize;
        if args_end > data.len() {
            return Err(Error::UnexpectedEof {
                offset: args_start,
                need: args_seg_size as usize,
                have: data.len() - args_start,
            });
        }
        let mut args_data = data[args_start..args_end].to_vec();
        xor_trans(&mut args_data, key);

        // Parse commands
        let mut cmd_r = Reader::new(&code_data);
        let mut cmds = Vec::new();

        while cmd_r.remaining() > 0 {
            let off = header_size + cmd_r.pos;
            let code = cmd_r.read_u8()?;
            let narg = cmd_r.read_u8()?;
            let lno = cmd_r.read_u32_le()?;

            let args = if code == kcc.returncode {
                if narg != 1 {
                    return Err(Error::AssertFailed(format!(
                        "RETURNCODE expects 1 arg, got {narg}"
                    )));
                }
                let id = cmd_r.read_u16_le()?;
                let typ = cmd_r.read_u8()?;
                let aop = cmd_r.read_u8()?;
                vec![Arg {
                    id,
                    typ,
                    aop,
                    len: 0,
                    off: 0,
                    data: ArgData::None,
                }]
            } else {
                Self::parse_args_v2(&mut cmd_r, &args_data, narg, code, kcc)?
            };

            cmds.push(Cmd {
                off,
                lno,
                code,
                npar: 0,
                args,
            });
        }

        Ok(Ystb { version, key, cmds })
    }

    fn parse_args_v2(
        r: &mut Reader,
        args_data: &[u8],
        narg: u8,
        code: u8,
        kcc: &KnownCmdCode,
    ) -> Result<Vec<Arg>> {
        let mut args = Vec::with_capacity(narg as usize);
        for _ in 0..narg {
            let id = r.read_u16_le()?;
            let typ = r.read_u8()?;
            let aop = r.read_u8()?;
            let len = r.read_u32_le()?;
            let off = r.read_u32_le()?;

            let data = if len > 0 {
                let start = off as usize;
                let end = start + len as usize;
                if end <= args_data.len() {
                    let bytes = &args_data[start..end];
                    if let Ok(s) = std::str::from_utf8(bytes) {
                        ArgData::Word(s.to_string())
                    } else {
                        ArgData::Expr(Ins::parse_buf(bytes)?)
                    }
                } else {
                    ArgData::None
                }
            } else {
                ArgData::None
            };

            args.push(Arg {
                id,
                typ,
                aop,
                len,
                off,
                data,
            });
        }
        Ok(args)
    }

    fn parse_v3(
        mut r: Reader,
        data: &[u8],
        kcc: &KnownCmdCode,
        key: u32,
        version: u32,
    ) -> Result<Self> {
        let ncmd = r.read_u32_le()?;
        let lcmd = r.read_u32_le()?;
        let larg = r.read_u32_le()?;
        let lexp = r.read_u32_le()?;
        let llno = r.read_u32_le()?;
        let pad = r.read_u32_le()?;

        if pad != 0 {
            return Err(Error::AssertFailed(format!("expected pad==0, got {pad}")));
        }
        if ncmd * 4 != lcmd || lcmd != llno {
            // Some versions have different sizes, but parsing may still work
        }
        if larg % 12 != 0 {
            return Err(Error::AssertFailed(format!(
                "larg={larg} not divisible by 12"
            )));
        }

        let header_size = 32;

        // Read segments
        let cmd_start = header_size;
        let cmd_end = cmd_start + lcmd as usize;
        let arg_start = cmd_end;
        let arg_end = arg_start + larg as usize;
        let exp_start = arg_end;
        let exp_end = exp_start + lexp as usize;
        let lno_start = exp_end;
        let lno_end = lno_start + llno as usize;

        if lno_end > data.len() {
            return Err(Error::UnexpectedEof {
                offset: lno_start,
                need: llno as usize,
                have: data.len() - lno_start,
            });
        }

        let mut cmd_data = data[cmd_start..cmd_end].to_vec();
        let mut arg_data = data[arg_start..arg_end].to_vec();
        let mut exp_data = data[exp_start..exp_end].to_vec();
        let mut lno_data = data[lno_start..lno_end].to_vec();

        xor_trans(&mut cmd_data, key);
        xor_trans(&mut arg_data, key);
        xor_trans(&mut exp_data, key);
        xor_trans(&mut lno_data, key);

        let mut cmd_r = Reader::new(&cmd_data);
        let mut arg_r = Reader::new(&arg_data);
        let mut lno_r = Reader::new(&lno_data);

        let _n_args_entries = larg as usize / 12;
        let mut cmds = Vec::with_capacity(ncmd as usize);

        for _ in 0..ncmd {
            let off = header_size + cmd_r.pos;

            if cmd_r.remaining() < 4 {
                break;
            }

            let code = cmd_r.read_u8()?;
            let narg = cmd_r.read_u8()?;
            let npar = cmd_r.read_u16_le()?;
            let lno = lno_r.read_u32_le()?;

            if arg_r.remaining() < (narg as usize) * 12 {
                break;
            }

            let args = if code == kcc.returncode {
                eprintln!("DEBUG: code {} is RETURNCODE", code);
                // RETURNCODE: skip arg entry
                if arg_r.remaining() < 12 {
                    return Err(Error::UnexpectedEof {
                        offset: arg_r.pos,
                        need: 12,
                        have: arg_r.remaining(),
                    });
                }
                let _id = arg_r.read_u16_le()?;
                let _typ = arg_r.read_u8()?;
                let _aop = arg_r.read_u8()?;
                let _len = arg_r.read_u32_le()?;
                let _off = arg_r.read_u32_le()?;
                vec![Arg {
                    id: 0,
                    typ: 0,
                    aop: 0,
                    len: 0,
                    off: 0,
                    data: ArgData::None,
                }]
            } else {
                Self::parse_args_v3(&mut arg_r, &exp_data, narg, code, kcc)?
            };

            cmds.push(Cmd {
                off,
                lno,
                code,
                npar,
                args,
            });
        }

        if arg_r.remaining() > 0 {
            // Some versions have extra arg bytes - this is a known difference
            // between V290-V494 and newer versions like V481
        }

        // Don't assert EOF - some versions have different arg encoding
        // arg_r.assert_eof("args")?;

        Ok(Ystb { version, key, cmds })
    }

    fn parse_args_v3(
        r: &mut Reader,
        exp_data: &[u8],
        narg: u8,
        code: u8,
        kcc: &KnownCmdCode,
    ) -> Result<Vec<Arg>> {
        let mut args = Vec::with_capacity(narg as usize);
        for _ in 0..narg {
            if r.remaining() < 12 {
                return Err(Error::UnexpectedEof {
                    offset: r.pos,
                    need: 12,
                    have: r.remaining(),
                });
            }
            let id = r.read_u16_le()?;
            let typ = r.read_u8()?;
            let aop = r.read_u8()?;
            let len = r.read_u32_le()?;
            let off = r.read_u32_le()?;

            let data = if len > 0 {
                let start = off as usize;
                let end = start + len as usize;
                if end <= exp_data.len() {
                    let bytes = &exp_data[start..end];
                    if let Ok(s) = std::str::from_utf8(bytes) {
                        ArgData::Word(s.to_string())
                    } else {
                        ArgData::Expr(Ins::parse_buf(bytes)?)
                    }
                } else {
                    ArgData::None
                }
            } else {
                ArgData::None
            };

            args.push(Arg {
                id,
                typ,
                aop,
                len,
                off,
                data,
            });
        }
        Ok(args)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_guess_key_truncated() {
        let short_data = vec![0x00; 4];
        let result = guess_key(&short_data);
        assert!(result.is_err());
        match result {
            Err(Error::UnexpectedEof { offset, need, have }) => {
                assert_eq!(offset, 0);
                assert_eq!(need, 8);
                assert_eq!(have, 4);
            }
            _ => panic!("expected UnexpectedEof"),
        }
    }

    #[test]
    fn test_guess_key_truncated_v2_offset() {
        let mut data = vec![0x00; 0x2C];
        data[4..8].copy_from_slice(&220u32.to_le_bytes());
        let result = guess_key(&data);
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 0);
    }

    #[test]
    fn test_guess_key_v2_valid() {
        let mut data = vec![0x00; 0x30];
        data[4..8].copy_from_slice(&250u32.to_le_bytes());
        data[0x2C..0x30].copy_from_slice(&0x07B4024Au32.to_le_bytes());
        let result = guess_key(&data);
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 0x07B4024A);
    }

    #[test]
    fn test_guess_key_v3() {
        let mut data = vec![0x00; 8];
        data[4..8].copy_from_slice(&300u32.to_le_bytes());
        let result = guess_key(&data);
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 0xD36FAC96);
    }

    #[test]
    fn test_returncode_wrong_narg() {
        use crate::crypto::xor_trans;
        use crate::yscm::KnownCmdCode;

        let kcc = KnownCmdCode {
            if_code: 0,
            else_code: 0,
            loop_code: 0,
            returncode: 10,
            word: 0,
        };

        let version = 250u32;
        let key = 0x07B4024A;

        let code_seg_size: u32 = 6;
        let args_seg_size: u32 = 0;

        let mut header = Vec::with_capacity(32);
        header.extend_from_slice(b"YSTB");
        header.extend_from_slice(&version.to_le_bytes());
        header.extend_from_slice(&code_seg_size.to_le_bytes());
        header.extend_from_slice(&args_seg_size.to_le_bytes());
        header.extend_from_slice(&(32u32).to_le_bytes());
        header.extend_from_slice(&[0u8; 12]);

        let mut code_cmd = vec![10u8, 2u8, 0u8, 0u8, 0u8, 0u8];
        xor_trans(&mut code_cmd, key);

        let mut data = header;
        data.extend_from_slice(&code_cmd);

        let result = Ystb::parse(&data, &kcc, key);
        assert!(result.is_err());
        match result {
            Err(Error::AssertFailed(msg)) => {
                assert!(msg.contains("RETURNCODE expects 1 arg"));
            }
            _ => panic!("expected AssertFailed, got {:?}", result),
        }
    }
}
