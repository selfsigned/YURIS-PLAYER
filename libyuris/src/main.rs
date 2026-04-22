use std::fs;
use std::path::PathBuf;
use yuris_decomp::{key_for_version, ArgData, Result, Ypf, Yscm, Yslb, Ystb, Ystl, Ysvr};

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: yuris_decomp <command> [options]");
        eprintln!("commands:");
        eprintln!("  extract <input.ypf> <output_dir>      - Extract YPF archive");
        eprintln!("  decompile <ysbin_dir> [-o <output_dir>] [-v]  - Decompile scripts");
        std::process::exit(1);
    }

    let cmd = &args[1];
    match cmd.as_str() {
        "extract" => {
            if args.len() != 4 {
                eprintln!("usage: yuris_decomp extract <input.ypf> <output_dir>");
                std::process::exit(1);
            }
            let input = PathBuf::from(&args[2]);
            let output_dir = PathBuf::from(&args[3]);

            let data = fs::read(&input)?;
            let ypf = Ypf::parse(&data)?;

            println!("YPF: version={}, files={}", ypf.version, ypf.entries.len());

            for (name, file_data) in ypf.extract() {
                let clean_name = name.replace('\\', "/");
                let out_path = output_dir.join(&clean_name);
                if let Some(parent) = out_path.parent() {
                    fs::create_dir_all(parent)?;
                }
                fs::write(&out_path, &file_data)?;
                println!("  extracted: {}", clean_name);
            }
        }
        "decompile" => {
            let mut dir = None;
            let mut output_dir = None;
            let mut verbose = false;

            let mut i = 2;
            while i < args.len() {
                match args[i].as_str() {
                    "-o" => {
                        i += 1;
                        if i < args.len() {
                            output_dir = Some(PathBuf::from(&args[i]));
                        }
                    }
                    "-v" => verbose = true,
                    _ if dir.is_none() => dir = Some(PathBuf::from(&args[i])),
                    _ => {}
                }
                i += 1;
            }

            let dir = match dir {
                Some(d) => d,
                None => {
                    eprintln!("usage: yuris_decomp decompile <ysbin_dir> [-o <output_dir>] [-v]");
                    std::process::exit(1);
                }
            };

            let actual_dir = if dir.join("ysv.ybn").exists() {
                dir.clone()
            } else if dir.join("ysbin").exists() && dir.join("ysbin").join("ysv.ybn").exists() {
                dir.join("ysbin")
            } else {
                eprintln!("Error: ysv.ybn not found in directory");
                std::process::exit(1);
            };

            if verbose {
                println!("Using directory: {:?}", actual_dir);
            }
            let ysvr_data = fs::read(actual_dir.join("ysv.ybn"))?;
            let yslb_data = fs::read(actual_dir.join("ysl.ybn"))?;
            let yscm_data = fs::read(actual_dir.join("ysc.ybn"))?;
            let ystl_data = fs::read(actual_dir.join("yst_list.ybn"))?;

            let yscm = Yscm::parse(&yscm_data)?;
            let ysvr = Ysvr::parse(&ysvr_data)?;
            let yslb = Yslb::parse(&yslb_data)?;
            let ystl = Ystl::parse(&ystl_data)?;

            if verbose {
                println!("YSVR: version={}, vars={}", ysvr.version, ysvr.vars.len());
                println!("YSLB: version={}, labels={}", yslb.version, yslb.lbls.len());
                println!("YSCM: version={}, cmds={}", yscm.version, yscm.cmds.len());
                println!(
                    "YSTL: version={}, scripts={}",
                    ystl.version,
                    ystl.scrs.len()
                );
            }

            let key = key_for_version(ysvr.version);
            if verbose {
                println!("Using key: {key:#010x}");
            }

            for scr in &ystl.scrs {
                let filename = format!("yst{:05}.ybn", scr.idx);
                let path = actual_dir.join(&filename);
                if !path.exists() {
                    if verbose {
                        println!("[{}] {} - file not found", scr.idx, scr.path);
                    }
                    continue;
                }
                let data = fs::read(&path)?;
                match Ystb::parse(&data, &yscm.kcc, key) {
                    Ok(ystb) => {
                        if verbose {
                            println!(
                                "[{}] {} - version={}, cmds={}",
                                scr.idx,
                                scr.path,
                                ystb.version,
                                ystb.cmds.len()
                            );
                        }

                        if let Some(ref out_base) = output_dir {
                            let output_path = out_base.join(&scr.path);
                            if let Some(parent) = output_path.parent() {
                                fs::create_dir_all(parent)?;
                            }

                            let mut content = String::new();
                            content.push_str(&format!(
                                "YSTB ver={} key={:x} ncmd={}\n",
                                ystb.version,
                                key,
                                ystb.cmds.len()
                            ));

                            for (i, cmd) in ystb.cmds.iter().enumerate() {
                                let cmd_name = yscm
                                    .cmds
                                    .get(cmd.code as usize)
                                    .map(|c| c.name.as_str())
                                    .unwrap_or("UNKNOWN");
                                content.push_str(&format!(
                                    "[{}] off={} npar={} {}:{}\n",
                                    i, cmd.off, cmd.npar, cmd.code, cmd_name
                                ));

                                for (j, arg) in cmd.args.iter().enumerate() {
                                    let arg_repr = match &arg.data {
                                        ArgData::None => "None".to_string(),
                                        ArgData::Word(s) => format!("\"{}\"", s),
                                        ArgData::Expr(ins) => format!("{:?}", ins),
                                    };
                                    content.push_str(&format!(
                                        "- [{}] id={} typ={} {}\n",
                                        j, arg.id, arg.typ, arg_repr
                                    ));
                                }
                            }

                            if let Err(e) = fs::write(&output_path, &content) {
                                eprintln!("Error writing {}: {}", output_path.display(), e);
                            } else if verbose {
                                println!("  wrote: {}", scr.path);
                            }
                        }
                    }
                    Err(e) => {
                        eprintln!("[{}] {} - ERROR: {}", scr.idx, scr.path, e);
                        break;
                    }
                }
            }
        }
        _ => {
            eprintln!("unknown command: {}", cmd);
            eprintln!("commands: extract, decompile");
            std::process::exit(1);
        }
    }

    Ok(())
}
