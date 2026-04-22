pub mod crypto;
pub mod error;
pub mod expr;
pub mod ffi;
pub mod reader;
pub mod ypf;
pub mod yscm;
pub mod yslb;
pub mod ystb;
pub mod ystl;
pub mod ysvr;

pub use error::{Error, Result};
pub use reader::Reader;
pub use ypf::{Ypf, YpfEntry, YpfIndexEntry, decode_entry};
pub use yscm::{MArg, MCmd, Yscm};
pub use yslb::Yslb;
pub use ystb::{Arg, ArgData, Cmd, Ystb};
pub use ystl::Ystl;
pub use ysvr::Ysvr;

pub const VERSION_MIN: u32 = 200;
pub const VERSION_MAX: u32 = 501;

pub const KEY_200: u32 = 0x07B4024A;
pub const KEY_300: u32 = 0xD36FAC96;

pub fn key_for_version(version: u32) -> u32 {
    if version < 290 {
        KEY_200
    } else {
        KEY_300
    }
}

pub fn check_version(version: u32) -> Result<()> {
    if version < VERSION_MIN || version >= VERSION_MAX {
        Err(Error::UnsupportedVersion(version))
    } else {
        Ok(())
    }
}
