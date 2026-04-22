#!/usr/bin/env python3
"""Generate expected parse outputs from the Python reference implementation.

Usage:
    python tests/generate_expected.py <ysbin_dir> <output_dir>

    ysbin_dir:   directory containing .ybn files (ysv.ybn, ysl.ybn, ysc.ybn, etc.)
    output_dir:  where to write expected_*.json files (e.g. tests/fixtures/v255/expected/)

The Python reference is the source of truth. Rust tests compare against these outputs.
"""
import json
import sys
import os
import io

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'existing_projects', 'yuris_decompiler'))
from yurislib.fileformat import YPF, Rdr, YSVR, YSLB, YSCM, YSTB, YSTL, CP932, KnownCmdCode

KEY_200 = 0x07B4024A
KEY_300 = 0xD36FAC96


def key_for_version(v):
    return KEY_200 if v < 290 else KEY_300


def parse_ysvr(data):
    ysvr = YSVR(Rdr(data, CP932))
    return {
        'version': ysvr.ver,
        'nvar': len(ysvr.vars),
        'vars': [{
            'scope': v.scope, 'g_ext': v.g_ext, 'scr_idx': v.scr_idx,
            'var_idx': v.var_idx, 'typ': v.typ, 'dim': v.dim
        } for v in ysvr.vars]
    }


def parse_yslb(data):
    yslb = YSLB(Rdr(data, CP932))
    return {
        'version': yslb.ver,
        'nlbl': len(yslb.lbls),
        'labels': [{
            'name': l.name, 'id': l.id, 'ip': l.ip, 'scr_idx': l.scr_idx,
            'if_lvl': l.if_lvl, 'loop_lvl': l.loop_lvl
        } for l in yslb.lbls]
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
        'scripts': [{
            'idx': s.idx, 'path': s.path, 'time': s.time,
            'nvar': s.nvar, 'nlbl': s.nlbl, 'ntext': s.ntext
        } for s in ystl.scrs]
    }


def parse_ystb(data, kcc, key):
    f = io.BytesIO(data)
    ystb = YSTB(f, kcc, key, encoding=CP932)
    return {
        'version': ystb.ver,
        'key': ystb.key,
        'ncmd': len(ystb.cmds),
        'cmds': [{
            'off': c.off, 'code': c.code, 'narg': len(c.args)
        } for c in ystb.cmds]
    }


def save_json(obj, path):
    with open(path, 'w') as f:
        json.dump(obj, f, indent=2)


def process_dir(ysbin_dir, out_dir):
    os.makedirs(out_dir, exist_ok=True)

    yscm_path = os.path.join(ysbin_dir, 'ysc.ybn')
    if not os.path.exists(yscm_path):
        print(f"ERROR: {yscm_path} not found")
        return False

    yscm_data = open(yscm_path, 'rb').read()
    yscm = YSCM(Rdr(yscm_data, CP932))
    kcc = KnownCmdCode(yscm)
    key = key_for_version(yscm.ver)

    save_json(parse_yscm(yscm_data), os.path.join(out_dir, 'expected_yscm.json'))
    print(f"  yscm: {len(yscm.cmds)} cmds")

    for filename, out_name, parser in [
        ('ysv.ybn', 'ysvr', parse_ysvr),
        ('ysl.ybn', 'yslb', parse_yslb),
        ('yst_list.ybn', 'ystl', parse_ystl),
    ]:
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
                print(f"  {stem}: ok")

    return True


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    ysbin_dir = sys.argv[1]
    out_dir = sys.argv[2]

    print(f"Processing {ysbin_dir} -> {out_dir}")
    if process_dir(ysbin_dir, out_dir):
        print("Done.")
    else:
        print("Failed.")
        sys.exit(1)


if __name__ == '__main__':
    main()
