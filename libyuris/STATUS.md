# YURIS-DECOMP Status

## Project Overview

Rust-based Yu-Ris visual novel engine decompiler. Yu-Ris (.yps/.ybn files) is a Japanese visual novel scripting engine. This tool decrypts, parses, and decompiles Yu-Ris game files back to readable script form.

**Goal**: Rust library with WASM as first-class target for browser-based decompilation.

## Current Status: WORKING

All tests pass:
- 3 crypto tests (XOR roundtrip, non-aligned, matches Python)
- 2 validation tests (v255 and v494)

### Validation Results

| Version | Files | Validated | Coverage |
|---------|-------|-----------|----------|
| v255 (V200-V289) | 98 .ybn files | 96 scripts | 98% |
| v494 (V290-V501) | 138 .ybn files | 136 scripts | 99% |

## What Works

### Core Modules
- ✅ `lib.rs` - crate root, exports all modules
- ✅ `error.rs` - Error enum (thiserror)
- ✅ `reader.rs` - BinaryReader with LE primitives + CP932 decoding
- ✅ `crypto.rs` - XOR decryption (YSTB file segments)
- ✅ `expr.rs` - Expression VM opcodes/parsing
- ✅ `ystb.rs` - Compiled script parser (V2 and V3)
- ✅ `ysvr.rs` - Variable definitions
- ✅ `yslb.rs` - Label definitions  
- ✅ `yscm.rs` - Command definitions + KnownCmdCode
- ✅ `ystl.rs` - Script file list
- ✅ `main.rs` - CLI

### File Formats Supported

| Magic | Format | Versions | Status |
|-------|--------|----------|--------|
| YSTB | Compiled script | V200-V501 | ✅ Working |
| YSVR | Variables | V200-V501 | ✅ Working |
| YSLB | Labels | V200-V501 | ✅ Working |
| YSCM | Commands | V200-V501 | ✅ Working |
| YSTL | Script index | V200-V501 | ✅ Working |

### Expression VM

- V2 format (V200-V289): ✅ Working
- V3 format (V290-V501): ✅ Working with warnings

Known opcodes:
- 0x2C NOP, 0x48 VAR, 0x76 ARR, 0x56 IDXBEG, 0x29 IDXEND
- 0x42 I8, 0x57 I16, 0x49 I32, 0x4C I64, 0x46 F64, 0x4D STR
- 0x73 ToStr, 0x69 ToNum, 0x52 NEG
- 0x2A MUL, 0x2F DIV, 0x25 MOD, 0x2B ADD, 0x2D SUB
- 0x3C LT, 0x53 LE, 0x3E GT, 0x5A GE, 0x3D EQ, 0x21 NE
- 0x41 BITAND, 0x5E BITXOR, 0x4F BITOR, 0x26 AND, 0x7C OR

Unknown opcodes (warning logged, skipped gracefully):
- 0x01, 0x03, 0x06, 0x33, 0xB0, 0xFF

### XOR Keys

- V200 format: `0x07B4024A` ✅
- V300 format: `0xD36FAC96` ✅

## Known Issues

1. **Unknown V3 opcodes** - Some V3 expression opcodes are unknown (see list above). Currently logged as warnings and skipped.

2. **Error recovery** - `parse_buf()` silently returns partial results on parse errors instead of failing completely.

3. **Limited validation** - Tests only check command count and basic fields, not actual expression output.

## What's Missing

### High Priority
- [ ] Complete unknown opcode list for V3
- [ ] Better expression output validation

### Medium Priority
- [ ] WASM target support (`wasm32-unknown-unknown`)
- [ ] More comprehensive test coverage
- [ ] Better error messages
- [ ] TypeScript interpreter (future goal)

### Nice to Have
- [ ] Documentation
- [ ] CLI improvements (more output formats)
- [ ] Performance optimization

## Reference Implementations

- `existing_projects/yuris_decompiler/yurislib/` - Python reference (primary, complete pipeline)
- `existing_projects/yuris_decompiler/yuris_note.md` - Additional format notes with details on WORD args, RETURNCODE, IF/ELSE/LOOP special handling
- `existing_projects/RxYuris/` - C++ reference (secondary, Windows-specific)
- `VNTranslationTools_Notes.md` - Format specs documenting binary layout

## Building & Testing

```bash
# Build
cargo build

# Test
cargo test

# Run CLI
cargo run -- --help
```

## Next Steps

1. Fix WORD arg handling for proper string detection
2. Add complete V3 opcode support
3. Enable WASM target
4. Add validation for expression output