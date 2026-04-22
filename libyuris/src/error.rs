use std::str::Utf8Error;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("unexpected eof at offset {offset}, need {need} bytes, have {have}")]
    UnexpectedEof {
        offset: usize,
        need: usize,
        have: usize,
    },

    #[error("bad magic: expected {expected:#010x}, got {got:#010x}")]
    BadMagic { expected: u32, got: u32 },

    #[error("unsupported version: {0}")]
    UnsupportedVersion(u32),

    #[error("invalid utf-8 at offset {offset}: {source}")]
    InvalidUtf8 { offset: usize, source: Utf8Error },

    #[error("invalid cp932 at offset {offset}")]
    InvalidCp932 { offset: usize },

    #[error("assertion failed: {0}")]
    AssertFailed(String),

    #[error("invalid instruction opcode: {0:#04x}")]
    InvalidOpcode(u8),

    #[error("type mismatch: {0}")]
    TypeMismatch(String),

    #[error("undefined variable: index {0}")]
    UndefinedVariable(usize),

    #[error("trailing data: {0} bytes after parse")]
    TrailingData(usize),

    #[error("io error: {0}")]
    Io(#[from] std::io::Error),

    #[error("hash mismatch for {context}: expected {expected:#010x}, got {actual:#010x}")]
    HashMismatch {
        context: String,
        expected: u32,
        actual: u32,
    },

    #[error("YPF decompression error: {0}")]
    Decompress(String),
}

pub type Result<T> = std::result::Result<T, Error>;
