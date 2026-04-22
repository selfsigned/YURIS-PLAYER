#!/usr/bin/env python3
"""Extract test fixtures from example .ypf files and generate expected outputs.

Usage:
    python tests/extract_fixtures.py

Extracts v255.ypf and v494.ypf into tests/fixtures/ and generates expected_*.json
for each parsed .ybn file.
"""
import json
import sys
import os
import io
import struct

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'existing_projects', 'yuris_decompiler'))

# Patch murmurhash2 before importing — use a stub that raises for non-trivial use
class _StubModule:
    @staticmethod
    def murmurhash2(data, seed=0):
        return 0
sys.modules['murmurhash2'] = _StubModule()

from yurislib.fileformat import (
    YPF, Rdr, YSVR, YSLB, YSCM, YSTB, YSTL, CP932, KnownCmdCode,
    nohash, NLTransV000, NLTransV500, NameXorV000, NameXorV290, NameXorV500,
)

KEY_200 = 0x07B4024A
KEY_300 = 0xD36FAC96


def key_for_version(v):
    return KEY_200 if v < 290 else KEY_300


def parse_ysvr(data):
    ysvr = YSVR(Rdr(data, CP932))
    return {
        'version': ysvr.ver,
        'nvar': len(ysvr.vars),
        'vars': [{'scope': v.scope, 'g_ext': v.g_ext, 'scr_idx': v.scr_idx,
                  'var_idx': v.var_idx, 'typ': v.typ, 'dim': v.dim} for v in ysvr.vars]
    }


def parse_yslb(data):
    yslb = YSLB(Rdr(data, CP932))
    return {
        'version': yslb.ver,
        'nlbl': len(yslb.lbls),
        'labels': [{'name': l.name, 'id': l.id, 'ip': l.ip, 'scr_idx': l.scr_idx,
                     'if_lvl': l.if_lvl, 'loop_lvl': l.loop_lvl} for l in yslb.lbls]
    }


def parse_yscm(data):
    yscm = YSCM(Rdr(data, CP932))
    return {
        'version': yscm.ver,
        'ncmd': len(yscm.cmds),
        'cmds': [{'name': c.name, 'narg': len(c.args)} for c in yscm.cmds]
    }


def parse_ystl(data):
    ystl = YSTL(Rdr(data, CP932))
    return {
        'version': ystl.ver,
        'nscr': len(ystl.scrs),
        'scripts': [{'idx': s.idx, 'path': s.path, 'time': s.time,
                      'nvar': s.nvar, 'nlbl': s.nlbl, 'ntext': s.ntext} for s in ystl.scrs]
    }


def parse_ystb(data, kcc, key):
    f = io.BytesIO(data)
    ystb = YSTB(f, kcc, key, encoding=CP932)
    return {
        'version': ystb.ver,
        'key': ystb.key,
        'ncmd': len(ystb.cmds),
        'cmds': [{'off': c.off, 'code': c.code, 'narg': len(c.args)} for c in ystb.cmds]
    }


def save_json(obj, path):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=2)


def process_extracted(ysbin_dir, out_dir):
    """Process an already-extracted ysbin directory."""
    os.makedirs(out_dir, exist_ok=True)

    yscm_path = os.path.join(ysbin_dir, 'ysc.ybn')
    if not os.path.exists(yscm_path):
        print(f"  ERROR: {yscm_path} not found")
        return False

    yscm_data = open(yscm_path, 'rb').read()
    yscm = YSCM(Rdr(yscm_data, CP932))
    kcc = KnownCmdCode(yscm)
    key = key_for_version(yscm.ver)

    save_json(parse_yscm(yscm_data), os.path.join(out_dir, 'expected_yscm.json'))
    print(f"  yscm: {len(yscm.cmds)} cmds")

    # Map filename stems to output names matching Rust module names
    file_map = [
        ('ysv.ybn', 'ysvr', parse_ysvr),
        ('ysl.ybn', 'yslb', parse_yslb),
        ('yst_list.ybn', 'ystl', parse_ystl),
    ]
    for filename, out_name, parser in file_map:
        path = os.path.join(ysbin_dir, filename)
        if os.path.exists(path):
            data = open(path, 'rb').read()
            save_json(parser(data), os.path.join(out_dir, f'expected_{out_name}.json'))
            print(f"  {out_name}: ok")

    ystl_path = os.path.join(ysbin_dir, 'yst_list.ybn')
    if os.path.exists(ystl_path):
        ystl = YSTL(Rdr(open(ystl_path, 'rb').read(), CP932))
        for scr in ystl.scrs:
            ystb_name = f'yst{scr.idx:05d}.ybn'
            ystb_path = os.path.join(ysbin_dir, ystb_name)
            if os.path.exists(ystb_path):
                data = open(ystb_path, 'rb').read()
                stem = ystb_name.replace('.ybn', '')
                save_json(parse_ystb(data, kcc, key), os.path.join(out_dir, f'expected_{stem}.json'))
                print(f"  {stem}: {parse_ystb(data, kcc, key)['ncmd']} cmds")
    return True


def extract_ypf(ypf_path, dst_dir):
    """Extract YPF archive, using no-hash for compatibility."""
    with open(ypf_path, 'rb') as f:
        # Use nohash for both to avoid murmurhash2 dependency
        ver_bytes = f.read(8)
        ver = struct.unpack_from('<I', ver_bytes, 4)[0]
        f.seek(0)
        ypf = YPF(f, hash_name_file=(nohash, nohash))
    ypf.extract(dst_dir)
    return ypf.ver


def main():
    base = os.path.join(os.path.dirname(__file__), '..')
    example_dir = os.path.join(base, 'existing_projects', 'yuris_decompiler', 'example-files')
    fixture_dir = os.path.join(base, 'tests', 'fixtures')

    for name in ['v255', 'v494']:
        ypf_path = os.path.join(example_dir, f'{name}.ypf')
        if not os.path.exists(ypf_path):
            print(f"SKIP {name}: {ypf_path} not found")
            continue

        extract_dir = os.path.join(fixture_dir, name)
        ysbin_dir = os.path.join(extract_dir, 'ysbin')
        expected_dir = os.path.join(extract_dir, 'expected')

        print(f"=== {name} ===")

        if not os.path.exists(ysbin_dir):
            print(f"  Extracting {name}.ypf...")
            extract_ypf(ypf_path, extract_dir)

        if os.path.exists(ysbin_dir):
            print(f"  Generating expected outputs...")
            process_extracted(ysbin_dir, expected_dir)
        else:
            print(f"  ERROR: ysbin dir not found after extraction")

    print("\nDone. Run 'cargo test' to validate.")


if __name__ == '__main__':
    main()
