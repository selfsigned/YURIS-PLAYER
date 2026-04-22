use serde::Deserialize;
use std::path::Path;

#[derive(Debug, Deserialize)]
struct ExpectedYscm {
    version: u32,
    ncmd: usize,
    cmds: Vec<serde_json::Value>,
}

#[derive(Debug, Deserialize)]
struct ExpectedYsvr {
    version: u32,
    nvar: usize,
    vars: Vec<serde_json::Value>,
}

#[derive(Debug, Deserialize)]
struct ExpectedYslb {
    version: u32,
    nlbl: usize,
    labels: Vec<serde_json::Value>,
}

#[derive(Debug, Deserialize)]
struct ExpectedYstl {
    version: u32,
    nscr: usize,
    scripts: Vec<serde_json::Value>,
}

#[derive(Debug, Deserialize)]
struct ExpectedYstb {
    version: u32,
    key: u32,
    ncmd: usize,
    cmds: Vec<ExpectedCmd>,
}

#[derive(Debug, Deserialize)]
struct ExpectedCmd {
    off: usize,
    code: u8,
    narg: usize,
}

fn fixture_dir() -> std::path::PathBuf {
    let manifest_dir = env!("CARGO_MANIFEST_DIR");
    Path::new(manifest_dir).join("tests").join("fixtures")
}

fn load_expected<T: serde::de::DeserializeOwned>(path: &Path) -> T {
    let data = std::fs::read_to_string(path)
        .unwrap_or_else(|e| panic!("failed to read {}: {e}", path.display()));
    serde_json::from_str(&data)
        .unwrap_or_else(|e| panic!("failed to parse {}: {e}", path.display()))
}

fn validate_yscm(ysbin: &Path, expected_dir: &Path) {
    let expected: ExpectedYscm = load_expected(&expected_dir.join("expected_yscm.json"));
    let data = std::fs::read(ysbin.join("ysc.ybn")).expect("ysc.ybn");
    let yscm = yuris_decomp::Yscm::parse(&data).expect("YSCM parse");

    assert_eq!(yscm.version, expected.version);
    assert_eq!(yscm.cmds.len(), expected.ncmd);

    for (i, rc) in yscm.cmds.iter().enumerate() {
        let expected_name = expected.cmds[i].get("name").unwrap().as_str().unwrap();
        assert_eq!(rc.name, expected_name, "cmd[{i}] name mismatch");
    }
}

fn validate_ysvr(ysbin: &Path, expected_dir: &Path) {
    let expected: ExpectedYsvr = load_expected(&expected_dir.join("expected_ysvr.json"));
    let data = std::fs::read(ysbin.join("ysv.ybn")).expect("ysv.ybn");
    let ysvr = yuris_decomp::Ysvr::parse(&data).expect("YSVR parse");

    assert_eq!(ysvr.version, expected.version);
    assert_eq!(ysvr.vars.len(), expected.nvar);

    for (i, rv) in ysvr.vars.iter().enumerate() {
        let ev = &expected.vars[i];
        assert_eq!(
            rv.var_idx,
            ev.get("var_idx").unwrap().as_u64().unwrap() as u16,
            "var[{i}] var_idx mismatch"
        );
        assert_eq!(
            rv.typ,
            ev.get("typ").unwrap().as_u64().unwrap() as u8,
            "var[{i}] typ mismatch"
        );
        assert_eq!(
            rv.scope,
            ev.get("scope").unwrap().as_u64().unwrap() as u8,
            "var[{i}] scope mismatch"
        );
    }
}

fn validate_yslb(ysbin: &Path, expected_dir: &Path) {
    let expected: ExpectedYslb = load_expected(&expected_dir.join("expected_yslb.json"));
    let data = std::fs::read(ysbin.join("ysl.ybn")).expect("ysl.ybn");
    let yslb = yuris_decomp::Yslb::parse(&data).expect("YSLB parse");

    assert_eq!(yslb.version, expected.version);
    assert_eq!(yslb.lbls.len(), expected.nlbl);

    for (i, rl) in yslb.lbls.iter().enumerate() {
        let el = &expected.labels[i];
        assert_eq!(
            rl.name,
            el.get("name").unwrap().as_str().unwrap(),
            "lbl[{i}] name mismatch"
        );
        assert_eq!(
            rl.ip,
            el.get("ip").unwrap().as_u64().unwrap() as u32,
            "lbl[{i}] ip mismatch"
        );
    }
}

fn validate_ystl(ysbin: &Path, expected_dir: &Path) {
    let expected: ExpectedYstl = load_expected(&expected_dir.join("expected_ystl.json"));
    let data = std::fs::read(ysbin.join("yst_list.ybn")).expect("yst_list.ybn");
    let ystl = yuris_decomp::Ystl::parse(&data).expect("YSTL parse");

    assert_eq!(ystl.version, expected.version);
    assert_eq!(ystl.scrs.len(), expected.nscr);

    for (i, rs) in ystl.scrs.iter().enumerate() {
        let es = &expected.scripts[i];
        assert_eq!(
            rs.idx,
            es.get("idx").unwrap().as_u64().unwrap() as u32,
            "scr[{i}] idx mismatch"
        );
        assert_eq!(
            rs.path,
            es.get("path").unwrap().as_str().unwrap(),
            "scr[{i}] path mismatch"
        );
    }
}

fn validate_ystb(ysbin: &Path, expected_dir: &Path, script_idx: u32, key: u32) {
    let ystb_name = format!("yst{script_idx:05}");
    let expected_path = expected_dir.join(format!("expected_{ystb_name}.json"));
    if !expected_path.exists() {
        return;
    }
    let expected: ExpectedYstb = load_expected(&expected_path);
    let ystb_path = ysbin.join(format!("{ystb_name}.ybn"));
    let data = std::fs::read(&ystb_path).unwrap_or_else(|e| panic!("{}: {e}", ystb_path.display()));

    let yscm_data = std::fs::read(ysbin.join("ysc.ybn")).expect("ysc.ybn");
    let yscm = yuris_decomp::Yscm::parse(&yscm_data).expect("YSCM parse");

    let ystb = yuris_decomp::Ystb::parse(&data, &yscm.kcc, key)
        .unwrap_or_else(|e| panic!("YSTB {ystb_name} parse: {e}"));

    assert_eq!(
        ystb.version, expected.version,
        "YSTB {ystb_name} version: rust={}, python={}",
        ystb.version, expected.version
    );
    assert_eq!(
        ystb.cmds.len(),
        expected.ncmd,
        "YSTB {ystb_name} cmd count: rust={}, python={}",
        ystb.cmds.len(),
        expected.ncmd
    );

    for (i, rc) in ystb.cmds.iter().enumerate() {
        let ec = &expected.cmds[i];
        assert_eq!(
            rc.code, ec.code,
            "YSTB {ystb_name} cmd[{i}] code: rust={}, python={}",
            rc.code, ec.code
        );
        assert_eq!(
            rc.args.len(),
            ec.narg,
            "YSTB {ystb_name} cmd[{i}] narg: rust={}, python={}",
            rc.args.len(),
            ec.narg
        );
    }
}

fn validate_fixture(name: &str) {
    let base = fixture_dir().join(name);
    let ysbin = base.join("ysbin");
    let expected = base.join("expected");

    if !ysbin.exists() || !expected.exists() {
        eprintln!("SKIP {name}: fixtures not found (run tests/generate_expected.py first)");
        return;
    }

    println!("=== Validating {name} ===");

    if expected.join("expected_yscm.json").exists() {
        validate_yscm(&ysbin, &expected);
        println!("  yscm: OK");
    }
    if expected.join("expected_ysvr.json").exists() {
        validate_ysvr(&ysbin, &expected);
        println!("  ysvr: OK");
    }
    if expected.join("expected_yslb.json").exists() {
        validate_yslb(&ysbin, &expected);
        println!("  yslb: OK");
    }
    if expected.join("expected_ystl.json").exists() {
        validate_ystl(&ysbin, &expected);
        println!("  ystl: OK");
    }

    if expected.join("expected_ystl.json").exists() {
        let ystl_data = std::fs::read(ysbin.join("yst_list.ybn")).unwrap();
        let ystl = yuris_decomp::Ystl::parse(&ystl_data).unwrap();
        let key = yuris_decomp::key_for_version(ystl.version);
        let mut validated = 0;
        for scr in &ystl.scrs {
            let exp_path = expected.join(format!("expected_yst{:05}.json", scr.idx));
            if exp_path.exists() {
                validate_ystb(&ysbin, &expected, scr.idx, key);
                validated += 1;
            }
        }
        println!("  ystb: {} scripts validated", validated);
    }
}

#[test]
fn test_validate_v255() {
    validate_fixture("v255");
}

#[test]
fn test_validate_v494() {
    validate_fixture("v494");
}
