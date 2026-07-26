// laneAN: measure the "84-byte cap" in funclet byte-signature pairing.
//
// Uses objdiff's own COFF reader (so symbol sizes are exactly what the diff sees).
// Modes:
//   basesyms  <repo>          -- dump every Code symbol in every base obj (TSV)
//   pairscan  <repo>          -- per-unit: target fn_ symbols vs base symbols by masked sig
use std::{collections::BTreeMap, path::PathBuf};

use objdiff_core::{
    diff::{self, DiffObjConfig, DiffSide},
    obj::{Object, SectionKind, SymbolFlag, SymbolKind},
};

fn is_funclet_like(name: &str) -> bool {
    if let Some(rest) = name.strip_prefix("__unwind$") {
        return rest.chars().all(|c| c.is_ascii_digit());
    }
    if let Some(rest) = name.strip_prefix("__catch$") {
        return rest.chars().all(|c| c.is_ascii_digit());
    }
    if name.starts_with("__unwind__merged_") {
        return true;
    }
    if let Some(rest) = name.strip_prefix("fn_") {
        return rest.len() == 8 && rest.chars().all(|c| c.is_ascii_hexdigit());
    }
    if name.starts_with("??__E") || name.starts_with("??__F") {
        return true;
    }
    false
}

fn is_fn_anon(name: &str) -> bool {
    match name.strip_prefix("fn_") {
        Some(rest) => rest.len() == 8 && rest.chars().all(|c| c.is_ascii_hexdigit()),
        None => false,
    }
}

/// Verbatim copy of objdiff-core's `funclet_signature` (pub(crate) there).
fn funclet_signature(obj: &Object, sym_idx: usize) -> Option<Vec<u8>> {
    let symbol = obj.symbols.get(sym_idx)?;
    if symbol.size == 0 {
        return None;
    }
    let section = obj.sections.get(symbol.section?)?;
    let start = symbol.address.checked_sub(section.address)? as usize;
    let end = start.checked_add(symbol.size as usize)?;
    let raw = section.data.get(start..end)?;
    let mut bytes = raw.to_vec();
    let sym_start_abs = symbol.address;
    let sym_end_abs = sym_start_abs + symbol.size;
    for reloc in &section.relocations {
        if reloc.address < sym_start_abs || reloc.address >= sym_end_abs {
            continue;
        }
        let off = (reloc.address - sym_start_abs) as usize;
        let end_off = (off + 4).min(bytes.len());
        for b in &mut bytes[off..end_off] {
            *b = 0;
        }
    }
    Some(bytes)
}

/// Ordered reloc-target-name descriptors for a symbol (the `named_symbol_signature` half
/// that `funclet_signature` throws away).
fn reloc_names(obj: &Object, sym_idx: usize) -> Option<Vec<(u64, String, i64)>> {
    let symbol = obj.symbols.get(sym_idx)?;
    let section = obj.sections.get(symbol.section?)?;
    let s = symbol.address;
    let e = s + symbol.size;
    let mut v = Vec::new();
    for reloc in &section.relocations {
        if reloc.address < s || reloc.address >= e {
            continue;
        }
        let t = obj.symbols.get(reloc.target_symbol)?;
        v.push((reloc.address - s, t.name.clone(), reloc.addend));
    }
    v.sort_by_key(|x| x.0);
    Some(v)
}

fn code_syms(obj: &Object) -> Vec<usize> {
    (0..obj.symbols.len())
        .filter(|&i| {
            let s = &obj.symbols[i];
            if s.size == 0 || s.flags.contains(SymbolFlag::Ignored) {
                return false;
            }
            if s.kind == SymbolKind::Section {
                return false;
            }
            match s.section {
                Some(si) => obj.sections[si].kind == SectionKind::Code,
                None => false,
            }
        })
        .collect()
}

struct Unit {
    name: String,
    target: Option<PathBuf>,
    base: Option<PathBuf>,
}

fn load_units(repo: &PathBuf) -> Vec<Unit> {
    let txt = std::fs::read_to_string(repo.join("objdiff.json")).unwrap();
    let v: serde_json::Value = serde_json::from_str(&txt).unwrap();
    v["units"]
        .as_array()
        .unwrap()
        .iter()
        .map(|u| Unit {
            name: u["name"].as_str().unwrap_or("").to_string(),
            target: u["target_path"].as_str().map(|p| repo.join(p)),
            base: u["base_path"].as_str().map(|p| repo.join(p)),
        })
        .collect()
}

fn cfg() -> DiffObjConfig {
    DiffObjConfig {
        function_reloc_diffs: diff::FunctionRelocDiffs::None,
        combine_data_sections: true,
        combine_text_sections: true,
        ppc_calculate_pool_relocations: false,
        ..Default::default()
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let mode = args.get(1).cloned().unwrap_or_default();
    let repo = PathBuf::from(args.get(2).cloned().unwrap_or_else(|| ".".into()));
    let units = load_units(&repo);
    let c = cfg();

    match mode.as_str() {
        // TSV: unit  obj  name  size  kind  funclet_like  local
        "basesyms" => {
            let mut seen = std::collections::BTreeSet::new();
            println!("unit\tobj\tname\tsize\tkind\tfunclet_like");
            for u in &units {
                let Some(bp) = &u.base else { continue };
                if !bp.exists() || !seen.insert(bp.clone()) {
                    continue;
                }
                let Ok(obj) = objdiff_core::obj::read::read(bp, &c, DiffSide::Base) else {
                    eprintln!("read fail {bp:?}");
                    continue;
                };
                for i in code_syms(&obj) {
                    let s = &obj.symbols[i];
                    println!(
                        "{}\t{}\t{}\t{}\t{:?}\t{}",
                        u.name,
                        bp.display(),
                        s.name,
                        s.size,
                        s.kind,
                        is_funclet_like(&s.name) as u8
                    );
                }
            }
        }
        // For each unit with both sides: for every anonymous target fn_ symbol, report
        // how many base Code symbols have an equal masked signature, split by whether the
        // base symbol is funclet-like (currently eligible) or not (only eligible if lifted).
        // TSV: unit  tgt_name  tgt_size  n_tgt_same_sig  n_base_eq_funcletlike  n_base_eq_other
        //      best_base_name  reloc_names_equal
        "pairscan" => {
            println!(
                "unit\ttgt\tsize\tn_tgt_same_sig\tn_base_fl\tn_base_other\tbase_name\treloc_eq"
            );
            for u in &units {
                let (Some(tp), Some(bp)) = (&u.target, &u.base) else { continue };
                if !tp.exists() || !bp.exists() {
                    continue;
                }
                let (Ok(tobj), Ok(bobj)) = (
                    objdiff_core::obj::read::read(tp, &c, DiffSide::Target),
                    objdiff_core::obj::read::read(bp, &c, DiffSide::Base),
                ) else {
                    continue;
                };
                // base index by signature
                let mut base_by_sig: BTreeMap<Vec<u8>, Vec<usize>> = BTreeMap::new();
                for i in code_syms(&bobj) {
                    if let Some(sig) = funclet_signature(&bobj, i) {
                        base_by_sig.entry(sig).or_default().push(i);
                    }
                }
                let mut tgt_by_sig: BTreeMap<Vec<u8>, usize> = BTreeMap::new();
                let tcode = code_syms(&tobj);
                for &i in &tcode {
                    if let Some(sig) = funclet_signature(&tobj, i) {
                        *tgt_by_sig.entry(sig).or_default() += 1;
                    }
                }
                for &i in &tcode {
                    let s = &tobj.symbols[i];
                    if !is_fn_anon(&s.name) {
                        continue;
                    }
                    let Some(sig) = funclet_signature(&tobj, i) else { continue };
                    let Some(cands) = base_by_sig.get(&sig) else { continue };
                    let mut n_fl = 0usize;
                    let mut n_other = 0usize;
                    let mut best: Option<usize> = None;
                    for &bi in cands {
                        if is_funclet_like(&bobj.symbols[bi].name) {
                            n_fl += 1;
                        } else {
                            n_other += 1;
                            if best.is_none() {
                                best = Some(bi);
                            }
                        }
                    }
                    let bname = best.map(|bi| bobj.symbols[bi].name.clone()).unwrap_or_default();
                    let reloc_eq = match best {
                        Some(bi) => {
                            let a = reloc_names(&tobj, i).unwrap_or_default();
                            let b = reloc_names(&bobj, bi).unwrap_or_default();
                            (a == b) as u8
                        }
                        None => 2,
                    };
                    println!(
                        "{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                        u.name,
                        s.name,
                        s.size,
                        tgt_by_sig.get(&sig).copied().unwrap_or(0),
                        n_fl,
                        n_other,
                        bname,
                        reloc_eq
                    );
                }
            }
        }
        // TSV of every target Code symbol size (for context) in units that have a base obj.
        "tgtsyms" => {
            println!("unit\tname\tsize");
            for u in &units {
                let (Some(tp), Some(bp)) = (&u.target, &u.base) else { continue };
                if !tp.exists() || !bp.exists() {
                    continue;
                }
                let Ok(tobj) = objdiff_core::obj::read::read(tp, &c, DiffSide::Target) else {
                    continue;
                };
                for i in code_syms(&tobj) {
                    let s = &tobj.symbols[i];
                    println!("{}\t{}\t{}", u.name, s.name, s.size);
                }
            }
        }
        _ => eprintln!("usage: capscan <basesyms|pairscan|tgtsyms> <repo>"),
    }
}
