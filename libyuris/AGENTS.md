# AGENTS.md

## What This Is

A Rust-based Yu-Ris visual novel engine decompiler. Yu-Ris (.yps/.ybn files) is a Japanese visual novel scripting engine. This tool decrypts, parses, and decompiles Yu-Ris game files back to readable script form.

**Primary goal**: Rust library with WASM as first-class target for browser-based decompilation.

## Project Structure

```
src/
  lib.rs      — crate root, re-exports all modules
  error.rs    — error types (thiserror)
  reader.rs   — binary reader with LE primitives + CP932 decoding (encoding_rs)
  crypto.rs   — XOR decryption (YSTB file segments)
  expr.rs     — expression VM opcodes + instruction parser
  ystb.rs     — compiled script parser (the core)
  ysvr.rs     — variable definitions (YSVR magic)
  yslb.rs     — label definitions (YSLB magic)
  yscm.rs     — command definitions (YSCM magic)
  ystl.rs     — script file list (YSTL magic)
tests/
  validate.rs             — validation harness (Rust vs Python JSON)
  generate_expected.py    — generates expected_*.json from Python reference
  fixtures/               — test fixtures (generated, gitignored)
existing_projects/  — reference implementations (DO NOT MODIFY)
  yuris_decompiler/ — Python reference (primary, complete pipeline)
  RxYuris/          — C++ reference (secondary, Windows-specific)
```

## External References

- `VNTranslationTools_Notes.md` — format specs from arcusmaximus's VNTranslationTools, documents the binary layout of each .ybn file type and the expression VM opcodes. Use this as the definitive reference for byte-level format details.
- `existing_projects/RxYuris/README.md` — C++ tool set by the same author, documents the supported version ranges and tool capabilities. Useful for cross-referencing the C++ parsing logic against the Python reference.

## Version Handling

Yu-Ris has version ranges with different formats:
- **V200-V289**: Old format, 12-byte instructions, XOR key `0x07B4024A`
- **V290-V501**: New format, 4-byte instructions, XOR key `0xD36FAC96`
- Version-specific logic branches on `version < 300` or `version < 470` etc.

Always use `check_version()` at parse entry points.

## Coding Guidelines

- **No comments unless asked** — code should be self-documenting
- **Mirror the Python reference** — the Python implementation in `existing_projects/yuris_decompiler/yurislib/` is the source of truth. Match its logic exactly.
- **Error handling**: use `crate::Error` variants, never `unwrap()` in library code
- **Binary parsing**: use `crate::Reader`, never raw pointer casts or manual offset math
- **WASM compat**: avoid `std::fs` in lib code, accept `&[u8]` slices instead of paths
- **No external crypto libs**: XOR is hand-rolled (it's trivial), keep it that way
- **Test naming**: `test_<module>_<what>` e.g. `test_xor_roundtrip`

## Validation Pipeline

1. Run `python tests/generate_expected.py <ysbin_dir> <output_dir>` to regenerate expected outputs
2. Rust tests in `tests/validate.rs` parse the same fixtures → compare against expected JSON
3. `cargo test` must pass before any commit

## Key Constants

```rust
KEY_200: u32 = 0x07B4024A  // V200-V289
KEY_300: u32 = 0xD36FAC96  // V290-V501
YSTB_MAGIC = b"YSTB"
YSVR_MAGIC = b"YSVR"
YSLB_MAGIC = b"YSLB"
YSCM_MAGIC = b"YSCM"
YSTL_MAGIC = b"YSTL"
```

## Adding New File Format Support

1. Create `src/<format>.rs`
2. Add `pub mod <format>;` to `lib.rs`
3. Define magic constant, struct, and `parse(data: &[u8]) -> Result<T>`
4. Write unit tests with synthetic data
5. Write validation tests against Python reference output
