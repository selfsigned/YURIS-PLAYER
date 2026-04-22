use crate::{Error, Reader, Result};

pub const INS_LIST: &[(u8, i8, &str)] = &[
    (0x2C, 0, "nop"),
    (0x48, 3, "var"),
    (0x76, 3, "arr"),
    (0x56, 3, "idxbeg"),
    (0x29, 1, "idxend"),
    (0x42, 1, "i8"),
    (0x57, 2, "i16"),
    (0x49, 4, "i32"),
    (0x4C, 8, "i64"),
    (0x46, 8, "f64"),
    (0x4D, -1, "str"),
    (0x73, 0, "$"),
    (0x69, 0, "@"),
    (0x52, 0, "neg"),
    (0x2A, 0, "*"),
    (0x2F, 0, "/"),
    (0x25, 0, "%"),
    (0x2B, 0, "+"),
    (0x2D, 0, "-"),
    (0x3C, 0, "<"),
    (0x53, 0, "<="),
    (0x3E, 0, ">"),
    (0x5A, 0, ">="),
    (0x3D, 0, "=="),
    (0x21, 0, "!="),
    (0x41, 0, "&"),
    (0x5E, 0, "^"),
    (0x4F, 0, "|"),
    (0x26, 0, "&&"),
    (0x7C, 0, "||"),
];

#[derive(Debug, Clone)]
pub enum Ins {
    Nop,
    I8(i8),
    I16(i16),
    I32(i32),
    I64(i64),
    F64(f64),
    Str(String),
    Var(u16, u8),
    Arr(u16, u8),
    IdxBeg(u16, u8),
    IdxEnd,
    Neg,
    ToStr,
    ToNum,
    Mul,
    Div,
    Mod,
    Add,
    Sub,
    Lt,
    Le,
    Gt,
    Ge,
    Eq,
    Ne,
    BitAnd,
    BitXor,
    BitOr,
    And,
    Or,
    Unknown(u8),
}

pub const OP_I8: u8 = 0x42;
pub const OP_I16: u8 = 0x57;
pub const OP_I32: u8 = 0x49;
pub const OP_I64: u8 = 0x4C;
pub const OP_F64: u8 = 0x46;
pub const OP_STR: u8 = 0x4D;
pub const OP_VAR: u8 = 0x48;
pub const OP_ARR: u8 = 0x76;
pub const OP_IDXBEG: u8 = 0x56;
pub const OP_IDXEND: u8 = 0x29;
pub const OP_NOP: u8 = 0x2C;
pub const OP_MUL: u8 = 0x2A;
pub const OP_DIV: u8 = 0x2F;
pub const OP_MOD: u8 = 0x25;
pub const OP_ADD: u8 = 0x2B;
pub const OP_SUB: u8 = 0x2D;
pub const OP_LT: u8 = 0x3C;
pub const OP_LE: u8 = 0x53;
pub const OP_GT: u8 = 0x3E;
pub const OP_GE: u8 = 0x5A;
pub const OP_EQ: u8 = 0x3D;
pub const OP_NE: u8 = 0x21;
pub const OP_BITAND: u8 = 0x41;
pub const OP_BITXOR: u8 = 0x5E;
pub const OP_BITOR: u8 = 0x4F;
pub const OP_AND: u8 = 0x26;
pub const OP_OR: u8 = 0x7C;
pub const OP_NEG: u8 = 0x52;
pub const OP_TOSTR: u8 = 0x73;
pub const OP_TONUM: u8 = 0x69;

fn ins_data_size(code: u8) -> i8 {
    for (op_code, exp_size, _name) in INS_LIST {
        if *op_code == code {
            return *exp_size;
        }
    }
    -1
}

impl Ins {
    pub fn parse(r: &mut Reader) -> Result<Self> {
        let code = r.read_u8()?;
        let size = r.read_u16_le()?;

        if code == 0 {
            return Ok(Ins::Nop);
        }

        let expected = ins_data_size(code);
        if expected > 0 && size != expected as u16 {
            return Err(Error::AssertFailed(format!(
                "opcode {:#04x}: expected size {}, got {}",
                code, expected, size
            )));
        }

        match code {
            OP_NOP => Ok(Ins::Nop),
            OP_I8 => {
                let b = r.read_bytes(size as usize)?;
                Ok(Ins::I8(b[0] as i8))
            }
            OP_I16 => {
                let b = r.read_bytes(size as usize)?;
                let v = i16::from_le_bytes([b[0], b[1]]);
                Ok(Ins::I16(v))
            }
            OP_I32 => {
                let v = r.read_i32_le()?;
                Ok(Ins::I32(v))
            }
            OP_I64 => {
                let v = r.read_i64_le()?;
                Ok(Ins::I64(v))
            }
            OP_F64 => {
                let v = r.read_f64_le()?;
                Ok(Ins::F64(v))
            }
            OP_STR => {
                let s = r.read_str_cp932(size as usize)?;
                Ok(Ins::Str(s))
            }
            OP_VAR => {
                let b = r.read_bytes(size as usize)?;
                let raw = i32::from_le_bytes([b[0], b[1], b[2], 0]);
                let idx = (raw >> 8) as u16;
                let tyq = (raw & 0xFF) as u8;
                Ok(Ins::Var(idx, tyq))
            }
            OP_ARR => {
                let b = r.read_bytes(size as usize)?;
                let raw = i32::from_le_bytes([b[0], b[1], b[2], 0]);
                let idx = (raw >> 8) as u16;
                let tyq = (raw & 0xFF) as u8;
                Ok(Ins::Arr(idx, tyq))
            }
            OP_IDXBEG => {
                let b = r.read_bytes(size as usize)?;
                let raw = i32::from_le_bytes([b[0], b[1], b[2], 0]);
                let idx = (raw >> 8) as u16;
                let tyq = (raw & 0xFF) as u8;
                Ok(Ins::IdxBeg(idx, tyq))
            }
            OP_IDXEND => Ok(Ins::IdxEnd),
            OP_NEG => Ok(Ins::Neg),
            OP_TOSTR => Ok(Ins::ToStr),
            OP_TONUM => Ok(Ins::ToNum),
            OP_MUL => Ok(Ins::Mul),
            OP_DIV => Ok(Ins::Div),
            OP_MOD => Ok(Ins::Mod),
            OP_ADD => Ok(Ins::Add),
            OP_SUB => Ok(Ins::Sub),
            OP_LT => Ok(Ins::Lt),
            OP_LE => Ok(Ins::Le),
            OP_GT => Ok(Ins::Gt),
            OP_GE => Ok(Ins::Ge),
            OP_EQ => Ok(Ins::Eq),
            OP_NE => Ok(Ins::Ne),
            OP_BITAND => Ok(Ins::BitAnd),
            OP_BITXOR => Ok(Ins::BitXor),
            OP_BITOR => Ok(Ins::BitOr),
            OP_AND => Ok(Ins::And),
            OP_OR => Ok(Ins::Or),
            _ => {
                eprintln!(
                    "Warning: unknown opcode {:#04x} at offset {}, size={}",
                    code,
                    r.pos - 3,
                    size
                );
                let data_len = size as usize;
                if data_len > 0 && r.remaining() >= data_len {
                    let _ = r.read_bytes(data_len);
                }
                Ok(Ins::Unknown(code))
            }
        }
    }

    pub fn parse_buf(data: &[u8]) -> Result<Vec<Ins>> {
        if data.is_empty() {
            return Ok(Vec::new());
        }
        let mut r = Reader::new(data);
        let mut insns = Vec::new();
        while r.remaining() >= 3 {
            match Ins::parse(&mut r) {
                Ok(ins) => insns.push(ins),
                Err(_) => break,
            }
        }
        Ok(insns)
    }
}
