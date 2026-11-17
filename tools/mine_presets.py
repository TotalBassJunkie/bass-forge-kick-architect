#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mine_presets.py -- Datamining-Werkzeug fuer die Referenz-Presets
(The Teknovault Kick Builder 1.1.2).

Beantwortet die offenen Punkte F1 (Normalisierungskurven) und F2
(fx{N}_p{M} -> Modulparameter) aus den Klartext-XML-Daten.

Liest AUSSCHLIESSLICH:
  * Kick-Presets     (*.itkbpreset)  -> normalisierte 0..1-Werte  (<Params>)
                                        + reale FX-Snapshot-Werte (<fxSnap>)
  * FX-Chain-Presets (*.xml)         -> reale Modul-Attribute

Schreibt:
  * _private/mining/report.json       (vollstaendige Rohauswertung)
  * docs/specs/mining-report.md       (lesbare Zusammenfassung)

Nur Standardbibliothek (Python 3.8+). Keine Schreibzugriffe auf die
Preset-Quellen. Keine Presetnamen/Sample-Pfade/Handbuchtexte in den
Ausgaben -- ausschliesslich Zahlen, Verteilungen und eigene Ableitungen.

Aufruf:
  python tools/mine_presets.py            # JSON + Markdown
  python tools/mine_presets.py --json-only
  python tools/mine_presets.py --verbose  # zusaetzliche Diagnose
"""

from __future__ import annotations

import argparse
import json
import math
import re
import struct
import sys
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

# ==========================================================================
# Konfiguration
# ==========================================================================

CONTENT_ROOT = Path(r"C:\Users\Public\Documents\Teknovault\The Teknovault Kick Builder")
KICK_DIR = CONTENT_ROOT / "Presets" / "Factory"
FXCHAIN_DIR = CONTENT_ROOT / "Presets" / "FX Chains"

WORKSPACE = Path(__file__).resolve().parent.parent
OUT_JSON = WORKSPACE / "_private" / "mining" / "report.json"
OUT_MD = WORKSPACE / "docs" / "specs" / "mining-report.md"

SLOT_COUNT = 12
SLOT_PARAM_SUFFIXES = [
    "vol", "pitch", "mute", "solo", "pos", "trimstart", "trimend",
    "fadein", "fadeout", "stretch", "filter", "filterq",
    "dlytime", "dlymix", "spare1", "spare2",
    "ficurve", "fishape", "foshape",
]
GLOBAL_IDS = [
    "sat_amount", "reverb_size", "reverb_damp", "reverb_mix",
    "hollow_amt", "max_amount", "master_vol", "midi_mode", "root_note",
    "gspare1", "gspare2", "gspare3",
]
FX_SLOTS = 8
FX_PARAMS = 14

# Attribute, die NICHT in fxN_pM stehen:
#   type  -> Kettenmetadaten
#   on    -> nicht in den fx-Parametern beobachtet (siehe Analyse)
#   mix   -> eigener Slot p12
#   outGain -> eigener Slot p13
#   ir    -> Dateipfad (String)
MODULE_META_ATTRS = ("type", "on", "mix", "outGain", "ir")

INDEX_OF_MIX = 12
INDEX_OF_OUTGAIN = 13

CHILD_TAGS = ("Band", "WNode", "Pt")

# Die `fxN_pM`-Werte sind auf 4 Nachkommastellen quantisiert (Schritt 1e-4,
# siehe analyse_fx_quantisation). Ein Fit gilt als reproduzierend, wenn er
# alle Punkte innerhalb einer halben Quantisierungsstufe + float32-Rauschen
# trifft.
FX_QUANT = 1.0e-4
TOL = 1.2e-4


# ==========================================================================
# Kleine Numerik-Helfer
# ==========================================================================

def fnum(s):
    """Robuste Float-Konvertierung; None wenn nicht numerisch/endlich."""
    if s is None:
        return None
    try:
        v = float(s)
    except (TypeError, ValueError):
        return None
    if math.isnan(v) or math.isinf(v):
        return None
    return v


def f32(x):
    """float32-Rundung (die Presets speichern float32 mit 9 Nachkommastellen)."""
    try:
        return struct.unpack("f", struct.pack("f", x))[0]
    except (OverflowError, struct.error):
        return None


def quantiles(vals, qs=(0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0)):
    if not vals:
        return {}
    vs = sorted(vals)
    n = len(vs)
    out = {}
    for q in qs:
        key = "p%g" % round(q * 100)
        if n == 1:
            out[key] = vs[0]
            continue
        pos = q * (n - 1)
        lo, hi = int(math.floor(pos)), int(math.ceil(pos))
        if lo == hi:
            out[key] = vs[lo]
        else:
            frac = pos - lo
            out[key] = vs[lo] * (1.0 - frac) + vs[hi] * frac
    return out


def spread(vals):
    """Kompakte Verteilungsbeschreibung."""
    if not vals:
        return {"n": 0, "distinct": 0}
    vs = sorted(vals)
    d = {
        "n": len(vs),
        "min": vs[0],
        "max": vs[-1],
        "median": vs[len(vs) // 2] if len(vs) % 2 else
                  0.5 * (vs[len(vs) // 2 - 1] + vs[len(vs) // 2]),
        "mean": sum(vs) / len(vs),
        "distinct": len(set(round(v, 12) for v in vs)),
        "quantiles": quantiles(vs),
    }
    mu = d["mean"]
    d["stdev"] = math.sqrt(sum((v - mu) ** 2 for v in vs) / len(vs))
    return d


def top_values(vals, k=6, nd=9):
    c = Counter(round(v, nd) for v in vals)
    total = sum(c.values()) or 1
    return [{"value": v, "count": n, "share": round(n / total, 4)}
            for v, n in c.most_common(k)]


def linfit(xs, ys):
    """y = a*x + b, plus R^2."""
    n = len(xs)
    if n < 2:
        return None
    mx, my = sum(xs) / n, sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    if sxx == 0:
        return None
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    a = sxy / sxx
    b = my - a * mx
    ss_tot = sum((y - my) ** 2 for y in ys)
    ss_res = sum((y - (a * x + b)) ** 2 for x, y in zip(xs, ys))
    return {"a": a, "b": b, "r2": (1.0 - ss_res / ss_tot) if ss_tot > 0 else 0.0}


def fit_through_extremes(xs, ys):
    """Deterministischer Fit durch den kleinsten und groessten x-Wert.

    Vorteil gegenueber der Regression: keine Glaettung, die Ausreisser
    versteckt. Gibt (a, b, hits, maxerr, n) zurueck.
    """
    n = len(xs)
    if n < 2:
        return None
    lo = min(range(n), key=lambda i: xs[i])
    hi = max(range(n), key=lambda i: xs[i])
    dx = xs[hi] - xs[lo]
    if abs(dx) < 1e-12:
        return None
    a = (ys[hi] - ys[lo]) / dx
    b = ys[lo] - a * xs[lo]
    err = [abs(ys[i] - (a * xs[i] + b)) for i in range(n)]
    return {"a": a, "b": b, "hits": sum(1 for e in err if e <= TOL),
            "maxerr": max(err), "n": n}


def default_of(vals):
    """Haeufigster Wert = Werksdefault."""
    if not vals:
        return None, 0
    c = Counter(round(v, 12) for v in vals)
    v, n = c.most_common(1)[0]
    return v, n


def continued_fraction_rational(x, max_den=100000):
    """Kleiner Bruch p/q mit |x - p/q| minimal (Stern-Brocot / CF)."""
    if x == 0 or not math.isfinite(x):
        return None
    sign = -1 if x < 0 else 1
    x = abs(x)
    best = None
    # Brute-Force ueber kleine Nenner ist bei max_den=100k schnell genug
    # fuer die wenigen Kandidatenwerte; wir beschraenken auf 5000.
    for q in range(1, min(max_den, 5000) + 1):
        p = round(x * q)
        if p == 0:
            continue
        err = abs(x - p / q)
        if best is None or err < best[2] - 1e-18:
            best = (sign * p, q, err)
        if err < 1e-12:
            break
    return best


def nice_rational(x, tol=5e-7):
    """Sucht einen 'schoenen' Bruch p/q (q <= 512) im Toleranzbereich."""
    r = continued_fraction_rational(x, max_den=512)
    if r is None:
        return None
    p, q, err = r
    if err <= max(tol, 2e-7):
        return {"num": p, "den": q, "value": p / q, "err": err}
    return None


# ==========================================================================
# Einlesen
# ==========================================================================

def read_fx_modules(chain_el):
    """<FxChain>-Element -> Liste von Modulen in Signalkettenreihenfolge."""
    out = []
    for idx, m in enumerate(chain_el.findall("Module")):
        attrs_order = list(m.attrib.keys())
        numeric = {}
        for k, v in m.attrib.items():
            if k in MODULE_META_ATTRS:
                continue
            fv = fnum(v)
            if fv is not None:
                numeric[k] = fv
        raw = {k: v for k, v in m.attrib.items()}
        all_numeric = {}
        for k, v in m.attrib.items():
            if k == "type":
                continue
            fv = fnum(v)
            if fv is not None:
                all_numeric[k] = fv
        children = defaultdict(list)
        for c in m:
            if c.tag in CHILD_TAGS:
                children[c.tag].append(dict(c.attrib))
        out.append({
            "pos": idx,
            "type": m.get("type"),
            "attr_order": attrs_order,
            "params_order": [k for k in attrs_order if k not in MODULE_META_ATTRS
                             and fnum(raw.get(k)) is not None],
            "attrs": numeric,
            "all_numeric": all_numeric,
            "raw": raw,
            "children": dict(children),
            "child_counts": {t: len(children.get(t, [])) for t in CHILD_TAGS},
        })
    return out


def read_kick_preset(path):
    root = ET.parse(str(path)).getroot()
    params = {}
    for p in root.iter("P"):
        pid = p.get("id")
        v = fnum(p.get("v"))
        if pid is not None and v is not None:
            params[pid] = v
    slots = []
    for sl in root.iter("Slot"):
        ref = sl.find("SampleRef")
        slots.append({
            "index": int(sl.get("index", "-1")),
            "role": sl.get("role"),
            "roleId": ref.get("roleId") if ref is not None else None,
            "has_relPath": bool(ref is not None and ref.get("relPath")),
        })
    chain_el = root.find(".//fxSnap/FxChain")
    modules = read_fx_modules(chain_el) if chain_el is not None else []
    return {
        "schemaVersion": root.get("schemaVersion"),
        "appVersion": root.get("appVersion"),
        "genreId": root.get("genreId"),
        "slots": slots,
        "params": params,
        "modules": modules,
        "hasFxSnap": chain_el is not None,
    }


def read_fx_chain_file(path):
    root = ET.parse(str(path)).getroot()
    chain_el = root.find(".//FxChain")
    if chain_el is None:
        chain_el = root if root.tag == "FxChain" else None
    if chain_el is None:
        return None
    return {"bypassAll": chain_el.get("bypassAll"),
            "modules": read_fx_modules(chain_el)}


# ==========================================================================
# A1: Parameterverteilung
# ==========================================================================

def analyse_params(presets):
    by_id = defaultdict(list)
    for pr in presets:
        for pid, v in pr["params"].items():
            by_id[pid].append(v)
    out = {}
    for pid, vals in by_id.items():
        d = spread(vals)
        d["top_values"] = top_values(vals)
        d["at_zero"] = sum(1 for v in vals if abs(v) < 1e-12)
        d["at_one"] = sum(1 for v in vals if abs(v - 1.0) < 1e-12)
        d["at_half"] = sum(1 for v in vals if abs(v - 0.5) < 1e-6)
        dv, dn = default_of(vals)
        d["default"] = dv
        d["default_count"] = dn
        d["default_share"] = round(dn / len(vals), 4) if vals else 0.0
        d["f32_default"] = f32(dv) if dv is not None else None
        d["rational"] = nice_rational(dv) if dv is not None else None
        out[pid] = d
    return dict(sorted(out.items()))


def param_rollup(param_stats):
    """Fasst lN_x / fxN_pM ueber alle N zusammen."""
    groups = {}
    for pid, st in param_stats.items():
        m = re.match(r"^l\d+_(.+)$", pid)
        if m:
            groups.setdefault(("slot", m.group(1)), []).append(st)
            continue
        m = re.match(r"^fx\d+_(p\d+)$", pid)
        if m:
            groups.setdefault(("fx", m.group(1)), []).append(st)
            continue
        groups.setdefault(("global", pid), []).append(st)

    out = {"slot": {}, "global": {}, "fx": {}}
    for (kind, base), stats in groups.items():
        out[kind][base] = {
            "ids": len(stats),
            "n": sum(s["n"] for s in stats),
            "min": min(s["min"] for s in stats),
            "max": max(s["max"] for s in stats),
            "median_range": [min(s["median"] for s in stats),
                             max(s["median"] for s in stats)],
            "defaults": sorted({s["default"] for s in stats}),
            "distinct_max": max(s["distinct"] for s in stats),
        }
    return out


# ==========================================================================
# A2: FX-Attribut-Inventar (76 Chains + 44 Snapshots)
# ==========================================================================

def analyse_fx_attrs(fx_chains, snaps):
    per_type = defaultdict(lambda: {
        "attr_order_votes": defaultdict(Counter),
        "attr_counts": Counter(),
        "attr_values": defaultdict(list),
        "occurrences": 0,
        "children": Counter(),
        "child_values": defaultdict(list),
        "shapes": Counter(),
        "param_orders": Counter(),
    })
    sources = []
    for name in sorted(fx_chains):
        sources.append(("chain", name, fx_chains[name]))
    for name in sorted(snaps):
        sources.append(("snapshot", name, snaps[name]))

    for kind, name, chain in sources:
        types_here = []
        for mod in chain["modules"]:
            t = mod["type"]
            if t is None:
                continue
            rec = per_type[t]
            rec["occurrences"] += 1
            types_here.append(t)
            rec["param_orders"][",".join(mod["params_order"])] += 1
            for i, k in enumerate(mod["attr_order"]):
                rec["attr_order_votes"][k][i] += 1
                rec["attr_counts"][k] += 1
            for k, v in mod["all_numeric"].items():
                rec["attr_values"][k].append(v)
            for ctag, items in mod["children"].items():
                rec["children"][ctag] += len(items)
                for it in items:
                    for ck, cv in it.items():
                        fv = fnum(cv)
                        if fv is not None:
                            rec["child_values"]["%s.%s" % (ctag, ck)].append(fv)
        if types_here:
            for t in set(types_here):
                rec = per_type[t]
                full = [x for x in types_here]
                # Kettenform aus Sicht dieses Typs mitschreiben
                rec["shapes"][",".join(full)] += 1

    out = {}
    for t, rec in sorted(per_type.items()):
        order = sorted(rec["attr_counts"],
                       key=lambda k: (-rec["attr_counts"][k],
                                      rec["attr_order_votes"][k].most_common(1)[0][0]))
        positions = []
        for k in order:
            pos, cnt = rec["attr_order_votes"][k].most_common(1)[0]
            positions.append({"attr": k, "pos": pos,
                              "stability": round(cnt / rec["attr_counts"][k], 4)})
        attrs = {}
        for k in order:
            vals = rec["attr_values"][k]
            d = spread(vals)
            d["top_values"] = top_values(vals, 4, nd=6)
            d["constant"] = d["distinct"] <= 1
            attrs[k] = d
        child = {}
        for k, vals in rec["child_values"].items():
            d = spread(vals)
            d["top_values"] = top_values(vals, 4, nd=6)
            child[k] = d
        # Positionalitaet: in wie vielen Vorkommen steht das Attribut an
        # derselben Stelle wie im haeufigsten Parameterschema?
        common_order = (rec["param_orders"].most_common(1)[0][0].split(",")
                        if rec["param_orders"] else [])
        pos_ok = 0
        for a in common_order:
            votes = rec["attr_order_votes"][a]
            if not votes:
                continue
            pos_in_schema = common_order.index(a)
            pos_in_xml = votes.most_common(1)[0][0]
            if pos_in_xml == pos_in_schema:
                pos_ok += 1
        out[t] = {
            "occurrences": rec["occurrences"],
            "attr_order": order,
            "attr_positions": positions,
            "params_order_common": common_order,
            "params_order_variants": len(rec["param_orders"]),
            "xml_order_matches_param_order": pos_ok == len(common_order) and bool(common_order),
            "attrs": attrs,
            "child_tag_counts": dict(rec["children"]),
            "child_attrs": child,
            "n_chains": sum(rec["shapes"].values()),
            "shapes": [{"shape": s, "count": n} for s, n in rec["shapes"].most_common(8)],
        }
    return out


# ==========================================================================
# A3: Kreuztabelle Preset -> Kette -> fx-Werte
# ==========================================================================

def analyse_crosstab(presets):
    rows = []
    for pr in presets:
        mods = pr["modules"]
        chain_types = [m["type"] for m in mods]
        slots = []
        for n in range(FX_SLOTS):
            p = [pr["params"].get("fx%d_p%d" % (n, m)) for m in range(FX_PARAMS)]
            mod = mods[n] if n < len(mods) else None
            slots.append({
                "slot": n,
                "module_type": mod["type"] if mod else None,
                "module_attrs": mod["all_numeric"] if mod else None,
                "module_params_order": mod["params_order"] if mod else None,
                "child_counts": mod["child_counts"] if mod else None,
                "p": p,
                "p_counts": {
                    "zero": sum(1 for v in p if v is not None and v == 0.0),
                    "half": sum(1 for v in p if v is not None and abs(v - 0.5) < 1e-9),
                    "one": sum(1 for v in p if v is not None and abs(v - 1.0) < 1e-9),
                },
            })
        rows.append({
            "preset_key": pr["preset_key"],
            "genreId": pr["genreId"],
            "n_modules": len(mods),
            "chain_types": chain_types,
            "chain_shape": ",".join(t for t in chain_types if t),
            "slots": slots,
        })

    # Leer-Slot-Muster erkennen
    empty_pattern = Counter()
    empty_examples = []
    for r in rows:
        for s in r["slots"]:
            if s["module_type"] is None:
                key = json.dumps([None if v is None else round(v, 6) for v in s["p"]])
                empty_pattern[key] += 1
                if len(empty_examples) < 3:
                    empty_examples.append({"preset_key": r["preset_key"],
                                           "slot": s["slot"], "p": s["p"]})
    return {
        "rows": rows,
        "chain_shapes": [{"shape": s, "count": n}
                         for s, n in Counter(r["chain_shape"] for r in rows).most_common()],
        "position_type_counts": {
            str(p): dict(c.most_common())
            for p, c in sorted({i: Counter(r["chain_types"][i] for r in rows
                                          if i < len(r["chain_types"]))
                                for i in range(max((len(r["chain_types"]) for r in rows), default=0))}.items())
        },
        "module_count_histogram": dict(sorted(Counter(r["n_modules"] for r in rows).items())),
        "empty_slot_patterns": [{"p": json.loads(k), "count": n}
                                for k, n in empty_pattern.most_common(4)],
        "n_presets": len(rows),
    }


# ==========================================================================
# A4: Positionshypothese  (Kern von F2)
# ==========================================================================

def analyse_positional_crosscheck(crosstab):
    """Gegenprobe zur Positionshypothese.

    Wo die Positionshypothese scheitert, wird geprueft, ob ein *anderer*
    p-Index das Attribut erklaert. Suchraum: einfache Teilungen
    `p = x/k` (k = 1, 2, 5, 10, 12, 24, 100, 200, 1000) und die bipolare
    Form `p = (x + a)/(2a)`. Damit finden wir z. B. faelschlich verschobene
    Parameterreihenfolgen.
    """
    samples = defaultdict(list)   # (type, attr) -> [(p_vec)]
    for r in crosstab["rows"]:
        for s in r["slots"]:
            t, attrs, p = s["module_type"], s["module_attrs"], s["p"]
            if not t or not attrs:
                continue
            for attr, v in attrs.items():
                if attr in MODULE_META_ATTRS:
                    continue
                samples[(t, attr)].append((v, p))

    divisors = [1.0, 2.0, 5.0, 10.0, 12.0, 24.0, 100.0, 200.0, 1000.0]
    out = {}
    for (t, attr), rows in sorted(samples.items()):
        n = len(rows)
        if n == 0:
            continue
        cands = []
        for m in range(FX_PARAMS):
            pv = [r[1][m] for r in rows]
            if any(v is None for v in pv):
                continue
            xs = [r[0] for r in rows]
            # (a) Teilung p = x/k
            for k in divisors:
                if all(abs(pv[i] - xs[i] / k) <= TOL for i in range(n)):
                    cands.append({"p": m, "model": "x/%g" % k})
            # (b) bipolar p = (x + A)/(2A)
            for A in (3.0, 6.0, 12.0, 24.0, 18.0, 50.0):
                if all(abs(pv[i] - (xs[i] + A) / (2 * A)) <= TOL for i in range(n)):
                    cands.append({"p": m, "model": "(x+%g)/%g" % (A, 2 * A)})
            # (c) Affin mit glatten Grenzen
            for lo in (-24, -12, 0, 16, 20, 60):
                for span in (12, 24, 48, 100, 180, 184, 440, 489):
                    if all(abs(pv[i] - (xs[i] - lo) / span) <= TOL for i in range(n)):
                        cands.append({"p": m, "model": "(x%+g)/%g" % (-lo, span)})
        # Duplikate entfernen
        uniq, seen = [], set()
        for c in cands:
            key = (c["p"], c["model"])
            if key not in seen:
                seen.add(key)
                uniq.append(c)
        out["%s:%s" % (t, attr)] = {
            "type": t, "attr": attr, "n": n,
            "real_distinct": len(set(round(r[0], 12) for r in rows)),
            "candidates": uniq[:8],
        }
    return out


def analyse_fx_defaults(crosstab, fx_inv):
    """Je Modultyp: der haeufigste fx-p-Vektor (Werksdefault) neben den
    beobachteten Modulattributen an dieser Stelle.

    Das ist die staerkste Einzelbestaetigung der Parameterreihenfolge: die
    Defaults muessen unter *einer* Abbildung gleichzeitig erklaerbar sein.
    """
    per_type = defaultdict(Counter)
    attr_of_p = defaultdict(dict)
    for r in crosstab["rows"]:
        for s in r["slots"]:
            if not s["module_type"] or not s["module_attrs"]:
                continue
            key = json.dumps([None if v is None else round(v, 6) for v in s["p"]])
            per_type[s["module_type"]][key] += 1
            attr_of_p[s["module_type"]][key] = s["module_attrs"]

    out = {}
    for t, cnt in sorted(per_type.items()):
        key, n = cnt.most_common(1)[0]
        p = json.loads(key)
        attrs = attr_of_p[t][key]
        order = fx_inv[t]["params_order_common"] if t in fx_inv else []
        table = []
        for i, name in enumerate(order):
            table.append({"p_index": i, "attr": name,
                          "real": attrs.get(name), "p": p[i] if i < len(p) else None})
        out[t] = {
            "n": n,
            "n_variants": len(cnt),
            "p": p,
            "modules_in_that_chain": None,
            "attrs": attrs,
            "table": table,
            "mix": {"real": attrs.get("mix"), "p": p[INDEX_OF_MIX]},
            "outGain": {"real": attrs.get("outGain"), "p": p[INDEX_OF_OUTGAIN]},
        }
    return out


def analyse_fx_quantisation(crosstab):
    """Prueft, ob die fx-Werte auf einem festen Raster liegen.

    Beobachtung: die Werte sind Vielfache von 0.0001. Das ist fuer die
    Preset-Portierung wichtig (Rundung beim Schreiben) und erklaert, warum
    ein 1:1-Fit nur bis auf eine halbe Stufe genau sein kann.
    """
    vals = []
    for r in crosstab["rows"]:
        for s in r["slots"]:
            for i, v in enumerate(s["p"]):
                if v is None:
                    continue
                if s["module_type"] is None and i < 12:
                    continue  # Leer-Slot-Nullen nicht mitzaehlen
                vals.append(v)
    if not vals:
        return {}
    best = None
    for step in (1.0, 1e-2, 1e-3, 1e-4, 1e-5, 2.5e-4, 5e-4):
        err = max(abs(v / step - round(v / step)) for v in vals)
        entry = {"step": step, "max_error_in_steps": round(err, 6),
                 "max_error_absolute": round(err * step, 9)}
        if best is None or err < best["max_error_in_steps"]:
            best = entry
    # Wie viele Werte sind exakt ein Vielfaches von 1e-4?
    n4 = sum(1 for v in vals if abs(v / 1e-4 - round(v / 1e-4)) < 1e-3)
    return {
        "n_values": len(vals),
        "fraction_multiples_of_1e-4": round(n4 / len(vals), 4),
        "best_grid": best,
        "grids_checked": [1.0, 5e-2, 1e-2, 2.5e-3, 1e-3, 5e-4, 2.5e-4, 1e-4, 1e-5],
    }


def analyse_positional(crosstab):
    """Prueft die Hypothese:
         p[i]  = i-tes modulspezifisches Attribut in XML-Reihenfolge
         p12   = mix
         p13   = outGain
    und leitet fuer jedes Attribut die Umkehrfunktion + den Wertebereich ab.
    """
    # (type, attr, p_index) -> [(real, p_value)]
    samples = defaultdict(list)
    order_variants = defaultdict(Counter)

    for r in crosstab["rows"]:
        for s in r["slots"]:
            t, order, attrs, p = (s["module_type"], s["module_params_order"],
                                  s["module_attrs"], s["p"])
            if not t or not order or not attrs:
                continue
            order_variants[t][",".join(order)] += 1
            for i, attr in enumerate(order):
                if i >= FX_PARAMS:
                    continue
                samples[(t, attr, i)].append((attrs[attr], p[i]))
            samples[(t, "mix", INDEX_OF_MIX)].append((attrs.get("mix"), p[INDEX_OF_MIX]))
            samples[(t, "outGain", INDEX_OF_OUTGAIN)].append(
                (attrs.get("outGain"), p[INDEX_OF_OUTGAIN]))

    per_attr = {}
    by_type = defaultdict(list)
    for (t, attr, i), rows in sorted(samples.items()):
        rows = [(x, y) for x, y in rows if x is not None and y is not None]
        if not rows:
            continue
        xs = [r[0] for r in rows]
        ys = [r[1] for r in rows]
        d = {"type": t, "attr": attr, "p_index": i, "n": len(rows),
             "real_distinct": len(set(round(v, 12) for v in xs))}

        # Kleinstquadrat-Fit (genauer als 2-Punkt-Fit bei quantisierten Daten)
        ls = linfit(xs, ys)
        ext = fit_through_extremes(xs, ys)
        maxerr = None
        inv = None
        if ls:
            maxerr = max(abs(ys[k] - (ls["a"] * xs[k] + ls["b"]))
                         for k in range(len(xs)))
            d["fit_ls"] = {"a": ls["a"], "b": ls["b"], "r2": ls["r2"],
                           "maxerr": maxerr}
        if ext:
            d["fit_extremes"] = {"a": ext["a"], "b": ext["b"],
                                 "maxerr": ext["maxerr"], "hits": ext["hits"]}

        # ---- Plausibilitaet des linearen Fits
        # Attribute, deren Wertebereich legitim bipolar ist (Gains in dB).
        BIPOLAR = {"gl", "gm", "gh", "lowgain", "midgain", "highgain",
                   "midlow", "midhigh", "sidelow", "sidehigh",
                   "middyn", "sidedyn", "width", "tone"}
        plausible = True
        if ls and maxerr is not None and maxerr <= TOL and ls["a"] != 0.0:
            lo0 = -ls["b"] / ls["a"]
            span0 = 1.0 / ls["a"]
            if attr not in BIPOLAR and lo0 < -0.05 * abs(span0):
                plausible = False

        if ls and maxerr is not None and maxerr <= TOL and ls["a"] != 0.0 and plausible:
            d["status"] = "exakt (linear)"
            lo, hi, span = -ls["b"] / ls["a"], (1.0 - ls["b"]) / ls["a"], 1.0 / ls["a"]
            # Bereich "saeubern": Offset, der < 1 % der Spanne ist, ist float-Rauschen
            lo_s, hi_s, snapped = lo, hi, False
            if abs(lo) < 0.01 * abs(span):
                lo_s = 0.0
                snapped = True
            if abs(hi - span) < 0.01 * abs(span):
                hi_s = span
            span_r = round(span, 4)
            smooth_span = abs(span_r - span) <= 1e-3 * abs(span)
            inv = {
                "form": "p = a*x + b   ->   x = (p - b) / a",
                "a": ls["a"], "b": ls["b"],
                "x_at_p0": lo_s, "x_at_p1": hi_s,
                "span": span, "span_rounded": span_r if smooth_span else None,
                "lo_snapped_to_zero": snapped,
                "maxerr": maxerr, "plausible": True,
            }
            if d["real_distinct"] >= 4:
                d["confidence"] = "hoch"
            elif d["real_distinct"] == 3:
                d["confidence"] = "mittel"
            else:
                d["confidence"] = "niedrig (%d Stuetzwert)" % d["real_distinct"]
        elif ls and maxerr is not None and maxerr <= TOL and not plausible:
            # linear passt numerisch, ist aber physikalisch unplausibel
            # -> logarithmische Alternative pruefen
            logfit = None
            if all(x > 0 for x in xs):
                lxs = [math.log(x) for x in xs]
                lls = linfit(lxs, ys)
                if lls:
                    lerr = max(abs(ys[k] - (lls["a"] * lxs[k] + lls["b"]))
                               for k in range(len(xs)))
                    if lerr <= TOL:
                        logfit = {"a": lls["a"], "b": lls["b"], "maxerr": lerr,
                                  "x_at_p0": math.exp(-lls["b"] / lls["a"]) if lls["a"] else None,
                                  "x_at_p1": math.exp((1.0 - lls["b"]) / lls["a"]) if lls["a"] else None}
            if logfit:
                d["status"] = "exakt (logarithmisch)"
                d["log_fit"] = logfit
                d["linear_fit_rejected"] = {"a": ls["a"], "b": ls["b"],
                                            "x_at_p0": -ls["b"] / ls["a"]}
                d["confidence"] = ("niedrig (linear und log beide exakt -> "
                                   "mehrdeutig)" if d["real_distinct"] <= 2
                                   else "mittel")
                d["single_point"] = None
                d.pop("single_point", None)
            else:
                d["status"] = "Fit unplausibel (negativer unterer Rand)"
                d["confidence"] = "keine"
        elif d["real_distinct"] <= 1:
            d["status"] = "Attribut konstant -> Kurve nicht bestimmbar"
            d["confidence"] = "keine"
            # Ein einzelner Stuetzpunkt bestaetigt aber die *Position*, wenn
            # der p-Wert nicht 0 ist (dann ist er nicht nur "unbenutzt").
            if any(y is not None and y != 0.0 for y in ys):
                d["single_point"] = {"real": xs[0], "p": ys[0]}
        else:
            d["status"] = "nicht bestimmt"
            d["confidence"] = "keine"
        if inv:
            d["inverse"] = inv
        per_attr["%s:%s" % (t, attr)] = d
        by_type[t].append(d)

    # Konflikte: mehrere Attribute auf demselben p-Index
    conflicts = {}
    for t, lst in by_type.items():
        seen = defaultdict(list)
        for d in lst:
            seen[d["p_index"]].append(d["attr"])
        bad = {k: v for k, v in seen.items() if len(v) > 1}
        if bad:
            conflicts[t] = {str(k): v for k, v in sorted(bad.items())}

    # Roundtrip: Attribut -> p -> Attribut
    roundtrip = {}
    for t, lst in by_type.items():
        tested = ok = 0
        worst = 0.0
        for d in lst:
            if "inverse" not in d:
                continue
            tested += 1
            if d["fit_ls"]["maxerr"] <= TOL:
                ok += 1
            worst = max(worst, abs(d["fit_ls"]["maxerr"] / d["inverse"]["a"]))
        roundtrip[t] = {"attrs_verified": ok, "attrs_tested": tested,
                        "max_reconstruction_error": worst}

    return {
        "per_attr": per_attr,
        "conflicts": conflicts,
        "order_variants": {t: [{"order": k, "count": v} for k, v in c.most_common(3)]
                           for t, c in order_variants.items()},
        "roundtrip": roundtrip,
    }


# ==========================================================================
# A5: Skalen-/Kurvenkandidaten fuer F1
# ==========================================================================

# Bekannte/vermutete Zielbereiche (aus Handbuch-Angaben in 01-BESTANDSAUFNAHME
# und aus plausiblen Audio-Konventionen). Nur Kandidaten, keine Fakten.
DB_RANGE_CANDIDATES = [
    (-60.0, 6.0), (-60.0, 10.0), (-60.0, 12.0), (-50.0, 10.0),
    (-50.0, 6.0), (-48.0, 6.0), (-40.0, 10.0), (-40.0, 6.0),
    (-36.0, 6.0), (-30.0, 10.0), (-30.0, 6.0), (-25.0, 5.0),
    (-24.0, 6.0), (-20.0, 6.0), (-18.0, 6.0), (-12.0, 12.0),
    (-12.0, 10.0), (-12.0, 6.0), (-6.0, 6.0), (-24.0, 12.0),
    (-48.0, 12.0), (-60.0, 0.0), (-24.0, 0.0), (-12.0, 0.0),
    (0.0, 6.0), (0.0, 10.0), (0.0, 12.0), (0.0, 24.0),
]

GRID_STEPS_DB = [0.1, 0.25, 0.5, 1.0, 2.0]

STRETCH_RANGES = [(0.25, 8.0), (0.25, 4.0), (0.5, 2.0), (0.125, 8.0),
                  (0.25, 10.0), (0.25, 16.0), (0.5, 8.0), (0.25, 6.0)]

PITCH_RANGES = [1.0, 2.0, 3.0, 6.0, 12.0, 24.0, 5.0, 7.0, 10.0, 36.0]


def scale_candidates(param_stats, key_params):
    """Fuer dB-verdaechtige Parameter: welche (lo,hi)-Skala macht den
    Werksdefault zu einem 'glatten' dB-Wert, und liegen die beobachteten
    Werte auf einem dB-Raster?"""
    out = {}
    for pid in key_params:
        st = param_stats.get(pid)
        if not st or st["default"] is None:
            continue
        dv = st["default"]
        vals = []
        # alle beobachteten Werte aus den Quantilen + top_values ableiten
        for tv in st["top_values"]:
            vals.append(tv["value"])
        for q, v in st["quantiles"].items():
            vals.append(v)
        vals.append(st["min"])
        vals.append(st["max"])
        vals = sorted(set(round(v, 9) for v in vals))

        cands = []
        for lo, hi in DB_RANGE_CANDIDATES:
            d_default = lo + dv * (hi - lo)
            # wie "glatt" ist der Default in dB?
            if abs(hi - lo) < 1e-9:
                continue
            smooth = min(abs(d_default - round(d_default)),
                         abs(d_default * 2 - round(d_default * 2)))
            # Rasterfaehigkeit aller Werte
            best_grid = None
            for step in GRID_STEPS_DB:
                err = max(abs((lo + v * (hi - lo)) / step
                              - round((lo + v * (hi - lo)) / step)) for v in vals)
                if best_grid is None or err < best_grid[1]:
                    best_grid = (step, err)
            cands.append({
                "lo_db": lo, "hi_db": hi,
                "default_db": round(d_default, 4),
                "default_db_smoothness": round(smooth, 6),
                "grid_step_db": best_grid[0],
                "grid_maxerr_steps": round(best_grid[1], 4),
            })
        cands.sort(key=lambda c: (c["default_db_smoothness"],
                                  c["grid_maxerr_steps"]))
        out[pid] = {
            "default_normalised": dv,
            "default_count": st["default_count"],
            "observed_values_sample": vals[:12],
            "candidates": cands[:6],
        }
    return out


def stretch_pitch_analysis(param_stats):
    """Prueft die in 03-RE-FINDINGS genannten Kurvenhypothesen numerisch."""
    out = {}

    # ---- stretch
    st = param_stats.get("l0_stretch")
    if st:
        dv = st["default"]
        entry = {"default_normalised": dv, "default_count": st["default_count"],
                 "hypotheses": {}}
        for (a, b) in STRETCH_RANGES:
            if dv <= 0 or a <= 0:
                continue
            f_log = math.exp(math.log(a) + dv * (math.log(b) - math.log(a)))
            f_lin = a + dv * (b - a)
            # Welcher v-Wert wuerde Faktor 1.0 ergeben?
            v_log_at_1 = (math.log(1.0 / a)) / (math.log(b / a)) if a > 0 else None
            v_lin_at_1 = (1.0 - a) / (b - a)
            entry["hypotheses"]["log_%g_%g" % (a, b)] = {
                "factor_at_default": round(f_log, 6),
                "v_for_factor_1": round(v_log_at_1, 6),
                "delta_to_default": round(v_log_at_1 - dv, 6),
            }
            entry["hypotheses"]["lin_%g_%g" % (a, b)] = {
                "factor_at_default": round(f_lin, 6),
                "v_for_factor_1": round(v_lin_at_1, 6),
                "delta_to_default": round(v_lin_at_1 - dv, 6),
            }
        # Potenz-/Skew-Kurve ueber [0.25,8]: welcher Exponent reproduziert
        # Faktor 1.0 beim Default?  v = ((f-a)/(b-a))^s
        a, b = 0.25, 8.0
        p = (1.0 - a) / (b - a)
        if 0 < dv < 1 and 0 < p < 1:
            s = math.log(dv) / math.log(p)
            entry["power_law_0.25_8"] = {
                "form": "v = ((f-a)/(b-a))^s",
                "s": round(s, 6),
                "equivalent_skew_juce": round(1.0 / s, 6) if s else None,
                "comment": ("Exponent, der den beobachteten Default als "
                            "Faktor 1.0 deutet"),
            }
        out["stretch"] = entry

    # ---- pitch
    pt = param_stats.get("l0_pitch")
    if pt:
        dv = pt["default"]
        vals = sorted({tv["value"] for tv in pt["top_values"]} |
                      {pt["min"], pt["max"]})
        entry = {"default_normalised": dv,
                 "default_is_half_within_1ulp":
                     abs(dv - 0.5) <= f32(0.5 + 2 ** -24) - 0.5,
                 "offset_from_half_f32_ulps":
                     round((dv - 0.5) / (2 ** -24), 3),
                 "observed_values": vals,
                 "range_tests": {}}
        for r in PITCH_RANGES:
            # v = 0.5 + st/(2r)  ->  st = (v-0.5)*2r
            sts = [(v - 0.5) * 2 * r for v in vals]
            # wie gut liegen die Semitones auf einem 1-HT-Raster?
            err = max(abs(s - round(s)) for s in sts)
            entry["range_tests"]["pm%g" % r] = {
                "semitones": [round(s, 4) for s in sts],
                "max_dev_from_integer_HT": round(err, 4),
            }
        # beste Reichweite = kleinster Rasterfehler
        best = min(entry["range_tests"].items(), key=lambda kv: kv[1]["max_dev_from_integer_HT"])
        entry["best_integer_semitone_range"] = best[0]
        out["pitch"] = entry

    # ---- root_note
    rn = param_stats.get("root_note")
    if rn:
        dv = rn["default"]
        entry = {"default_normalised": dv, "distinct": rn["distinct"],
                 "hypotheses": {}}
        for den in (127.0, 128.0, 12.0, 24.0, 36.0):
            entry["hypotheses"]["*%g" % den] = {
                "value_at_default": round(dv * den, 6),
                "nearest_integer": round(dv * den),
                "delta": round(dv * den - round(dv * den), 9),
            }
        r = nice_rational(dv)
        entry["rational"] = r
        out["root_note"] = entry

    # ---- filterq
    fq = param_stats.get("l0_filterq")
    if fq:
        dv = fq["default"]
        entry = {"default_normalised": dv, "rational": nice_rational(dv),
                 "q_range_tests": {}}
        for lo, hi in [(0.1, 4.0), (0.1, 10.0), (0.3, 10.0), (0.5, 10.0),
                       (0.1, 5.0), (0.2, 5.0), (0.1, 2.0), (0.5, 4.0)]:
            # Welcher Q ergibt sich, wenn Q_default ein glatter Wert ist?
            for q_def in (0.5, 0.7071, 1.0, 0.7):
                pred = (q_def - lo) / (hi - lo)
                if abs(pred - dv) < 2e-6:
                    entry["q_range_tests"]["[%g,%g]@Q=%g" % (lo, hi, q_def)] = {
                        "predicted_normalised": round(pred, 9),
                        "delta": round(pred - dv, 9),
                    }
        out["filterq"] = entry

    return out


def master_vol_analysis(param_stats):
    """Prueft, ob master_vol dB- oder lineare Verstaerkung ist."""
    st = param_stats.get("master_vol")
    if not st:
        return {}
    dv = st["default"]
    entry = {
        "default_normalised": dv,
        "default_count": st["default_count"],
        "n": st["n"],
        "rational": nice_rational(dv),
        "observed": st["top_values"],
        "interpretations": {},
    }
    # (a) dB-linear: 0 dB bei diesem Default
    entry["interpretations"]["db_linear_0dB_at_default"] = [
        {"lo_db": lo, "hi_db": round(lo + (0 - lo) / dv, 4)}
        for lo in (-60, -50, -48, -40, -36, -30, -25, -24, -20)
    ]
    # (b) lineare Verstaerkung: 1.0 bei diesem Default
    entry["interpretations"]["linear_gain_unity_at_default"] = {
        "gain_at_default": round(dv * (1.0 / dv), 6),
        "gain_at_p1": round(1.0 / dv, 6),
        "db_at_p1": round(20 * math.log10(1.0 / dv), 4),
        "form": "gain = v / %.9f" % dv,
    }
    # (c) dB-Skalen, in denen der Default ein glatter dB-Wert ist
    smooth = []
    for lo, hi in DB_RANGE_CANDIDATES:
        d = lo + dv * (hi - lo)
        smooth.append({"lo_db": lo, "hi_db": hi, "default_db": round(d, 3),
                       "dist_to_integer_db": round(abs(d - round(d)), 4)})
    smooth.sort(key=lambda c: c["dist_to_integer_db"])
    entry["interpretations"]["db_smooth_default"] = smooth[:6]
    # (d) 20*log10(v) direkt
    if dv > 0:
        entry["interpretations"]["plain_20log10"] = {
            "db": round(20 * math.log10(dv), 4),
        }
    return entry


def vol_analysis(param_stats, scale_cands):
    """vol ist der klanglich wichtigste Slot-Parameter -> eigene Auswertung."""
    entry = {"slots": {}}
    for suf in ("vol", "master_vol"):
        pass
    st = param_stats.get("l0_vol")
    if not st:
        return entry
    dv = st["default"]
    entry["default_normalised"] = dv
    entry["default_count"] = st["default_count"]
    entry["rational"] = nice_rational(dv)
    entry["f32"] = f32(dv)
    entry["scale_candidates"] = scale_cands.get("l0_vol", {}).get("candidates", [])
    # Lineare Verstaerkung, bei der der Default 1.0 waere
    entry["linear_gain_unity_at_default"] = {
        "gain_at_p1": round(1.0 / dv, 6),
        "db_at_p1": round(20 * math.log10(1.0 / dv), 4),
    }
    if dv > 0:
        entry["plain_20log10_of_default_db"] = round(20 * math.log10(dv), 4)
    return entry


# ==========================================================================
# A6: Slot-Belegung / Rollen
# ==========================================================================

def analyse_slots(presets):
    per_preset = []
    role_counter = Counter()
    idx_counter = Counter()
    combos = Counter()
    for pr in presets:
        roles = [s["role"] for s in pr["slots"]]
        per_preset.append({
            "preset_key": pr["preset_key"],
            "n_slots": len(pr["slots"]),
            "indices": sorted(s["index"] for s in pr["slots"]),
            "roles": roles,
            "roleIds": [s["roleId"] for s in pr["slots"]],
            "all_have_relPath": all(s["has_relPath"] for s in pr["slots"]),
        })
        for s in pr["slots"]:
            role_counter[s["role"]] += 1
            idx_counter[s["index"]] += 1
        combos[",".join(sorted(r for r in roles if r))] += 1
    return {
        "per_preset": per_preset,
        "role_counts": dict(role_counter.most_common()),
        "index_counts": {str(k): v for k, v in sorted(idx_counter.items())},
        "role_combinations": [{"roles": k, "count": v}
                              for k, v in combos.most_common(10)],
    }


# ==========================================================================
# Report
# ==========================================================================

def fmt(v, nd=6):
    if v is None:
        return "–"
    if isinstance(v, float):
        if v != 0 and (abs(v) >= 1e7 or abs(v) < 1e-6):
            return "%.3e" % v
        return ("%%.%df" % nd) % v
    return str(v)


def write_markdown(rep):
    md = []
    A = md.append
    M = rep["meta"]
    A("# Mining-Report: Referenz-Presets (Rohauswertung)")
    A("")
    A("Automatisch erzeugt von [`tools/mine_presets.py`](../../tools/mine_presets.py). "
      "Stand: %s." % M["generated"])
    A("")
    A("| Datenquelle (nur lesend) | Anzahl |")
    A("|---|---|")
    A("| Kick-Presets `.itkbpreset` | %d |" % M["n_presets"])
    A("| FX-Chain-Presets `.xml` | %d |" % M["n_fxchains"])
    A("| Presets mit eingebettetem FX-Snapshot | %d |" % M["n_presets_with_fxsnap"])
    A("")
    A("> Alle Werte sind aus den Klartext-XML-Dateien abgeleitet. Es werden **keine** "
      "Presetnamen, Sample-Pfade oder Handbuchtexte wiedergegeben; die Presets sind "
      "als `preset_01…preset_%02d` anonymisiert. Rohdaten: `_private/mining/report.json`."
      % M["n_presets"])
    A("")
    A("---")
    A("")

    # ---------------- 1. Slots
    A("## 1. Slot-Belegung")
    A("")
    S = rep["slots"]
    A("| Rollenname | Vorkommen |")
    A("|---|---|")
    for r, n in S["role_counts"].items():
        A("| `%s` | %d |" % (r, n))
    A("")
    A("| Slot-Index | Vorkommen |")
    A("|---|---|")
    for i, n in S["index_counts"].items():
        A("| %s | %d |" % (i, n))
    A("")
    A("Haeufigste Rollenkombinationen:")
    A("")
    for c in S["role_combinations"]:
        A("* `%s` — %d Presets" % (c["roles"], c["count"]))
    A("")

    # ---------------- 2. Slot-Parameter
    A("## 2. Slot-Parameter `lN_*`")
    A("")
    A("Rollup ueber alle 12 Slots (Wertebereich und Werksdefault):")
    A("")
    A("| Parameterart | IDs | n | min | max | Default(s) | distinct (max) |")
    A("|---|---|---|---|---|---|---|")
    for base, r in sorted(rep["param_rollup"]["slot"].items()):
        A("| `lN_%s` | %d | %d | %s | %s | %s | %d |" % (
            base, r["ids"], r["n"], fmt(r["min"]), fmt(r["max"]),
            ", ".join(fmt(d, 9) for d in r["defaults"][:3]), r["distinct_max"]))
    A("")

    # ---------------- 3. Globals
    A("## 3. Globale Parameter")
    A("")
    A("| ID | n | min | max | Default | Anteil Default | distinct | Bruch-Kandidat |")
    A("|---|---|---|---|---|---|---|---|")
    for pid in GLOBAL_IDS:
        st = rep["param_stats"].get(pid)
        if not st:
            continue
        rat = st["rational"]
        rats = ("%d/%d" % (rat["num"], rat["den"])) if rat else "–"
        A("| `%s` | %d | %s | %s | %s | %.1f %% | %d | %s |" % (
            pid, st["n"], fmt(st["min"]), fmt(st["max"]), fmt(st["default"], 9),
            100 * st["default_share"], st["distinct"], rats))
    A("")

    # ---------------- 4. FX-Parameter
    A("## 4. FX-Parameter `fxN_pM`")
    A("")
    A("| Index | IDs | n | min | max | Default(s) | distinct (max) |")
    A("|---|---|---|---|---|---|---|")
    for base, r in sorted(rep["param_rollup"]["fx"].items(),
                          key=lambda kv: int(kv[0][1:])):
        A("| `p%s` | %d | %d | %s | %s | %s | %d |" % (
            base, r["ids"], r["n"], fmt(r["min"]), fmt(r["max"]),
            ", ".join(fmt(d, 6) for d in r["defaults"][:3]), r["distinct_max"]))
    A("")
    A("### 4.1 Muster leerer FX-Slots")
    A("")
    A("Leere Slots (kein Modul an dieser Kettenposition) tragen ein festes Muster:")
    A("")
    for e in rep["crosstab"]["empty_slot_patterns"][:2]:
        A("* `p = [%s]` — %d×" % (", ".join(fmt(v, 3) for v in e["p"]), e["count"]))
    A("")

    # ---------------- 5. FX-Attribute
    A("## 5. Modulattribute je `type=`")
    A("")
    A("Datenbasis: %d Ketten-Presets + %d Preset-Snapshots."
      % (M["n_fxchains"], M["n_presets_with_fxsnap"]))
    A("")
    for t, rec in sorted(rep["fx_attr_inventory"].items()):
        A("### `%s` — %d Vorkommen" % (t, rec["occurrences"]))
        A("")
        A("Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):")
        A("")
        A("`%s`" % ", ".join(rec["params_order_common"]))
        A("")
        A("XML-Reihenfolge == Parameterschema: **%s** (%d Varianten beobachtet)"
          % ("ja" if rec["xml_order_matches_param_order"] else "nein",
             rec["params_order_variants"]))
        A("")
        A("| # | Attribut | n | min | max | distinct | konstant? |")
        A("|---|---|---|---|---|---|---|")
        for i, k in enumerate(rec["params_order_common"]):
            st = rec["attrs"].get(k)
            if not st:
                continue
            A("| %d | `%s` | %d | %s | %s | %d | %s |" % (
                i, k, st["n"], fmt(st["min"], 4), fmt(st["max"], 4),
                st["distinct"], "ja" if st["constant"] else "nein"))
        for k in ("mix", "outGain"):
            st = rec["attrs"].get(k)
            if st:
                A("| – | `%s` (Meta) | %d | %s | %s | %d | %s |" % (
                    k, st["n"], fmt(st["min"], 4), fmt(st["max"], 4),
                    st["distinct"], "ja" if st["constant"] else "nein"))
        A("")
        if rec["child_tag_counts"]:
            A("Kind-Elemente: " + ", ".join("`<%s>` (%d×)" % (k, v)
                                            for k, v in rec["child_tag_counts"].items()))
            A("")
            A("| Kind-Attribut | n | min | max | distinct |")
            A("|---|---|---|---|---|")
            for k, st in sorted(rec["child_attrs"].items()):
                A("| `%s` | %d | %s | %s | %d |" % (
                    k, st["n"], fmt(st["min"], 4), fmt(st["max"], 4), st["distinct"]))
            A("")

    # ---------------- 6. Kreuztabelle
    C = rep["crosstab"]
    A("## 6. Kreuztabelle: Preset → Kette → `fxN_pM` (Kern von F2)")
    A("")
    A("### 6.1 Kettenformen")
    A("")
    A("| Modulreihenfolge | Presets |")
    A("|---|---|")
    for s in C["chain_shapes"]:
        A("| `%s` | %d |" % (s["shape"], s["count"]))
    A("")
    A("Modulanzahl je Preset: " + ", ".join(
        "%d Module: %d×" % (k, v) for k, v in C["module_count_histogram"].items()))
    A("")
    A("### 6.2 Modultyp je Kettenposition")
    A("")
    A("| Position | Typen (Anzahl) |")
    A("|---|---|")
    for pos, types in sorted(C["position_type_counts"].items(), key=lambda kv: int(kv[0])):
        A("| %s | %s |" % (pos, ", ".join("`%s` (%d)" % (t, n)
                                          for t, n in types.items())))
    A("")
    A("### 6.3 `p12` (MIX) je Preset")
    A("")
    A("| Preset | Kette | p12 (MIX) je Slot |")
    A("|---|---|---|")
    for r in C["rows"]:
        A("| `%s` | `%s` | %s |" % (
            r["preset_key"], r["chain_shape"],
            ", ".join(fmt(s["p"][12], 3) for s in r["slots"])))
    A("")

    # ---------------- 7. Positionshypothese
    P = rep["positional"]
    A("## 7. Abbildung Modulattribut → `fxN_pM`")
    A("")
    Q = rep["fx_quantisation"]
    A("**Quantisierung.** Die `fxN_pM`-Werte liegen auf einem festen Raster "
      "(bester Schritt **%s**, max. Abweichung %s). Der Fit kann daher nur bis "
      "auf eine halbe Stufe genau sein; die Toleranz ist auf %.1e gesetzt." % (
          fmt(Q["best_grid"]["step"], 6), fmt(Q["best_grid"]["max_error_absolute"], 9),
          M["tolerance"]))
    A("")
    A("**Verfahren.** Fuer jedes Attribut eines Modultyps werden alle Beobachtungen "
      "aus den %d Preset-Snapshots gesammelt (Attributwert in realer Einheit ↔ "
      "`fxN_pM` an der Position des Attributs im Parameterschema). Angepasst wird "
      "eine Gerade `p = a·x + b` per Kleinstquadrat." % M["n_presets_with_fxsnap"])
    A("")
    A("### 7.1 Bestaetigte Zuordnungen")
    A("")
    A("| Modultyp | Attribut | p | n | Stuetzwerte | a | b | Bereich bei p=0…1 | Konfidenz |")
    A("|---|---|---|---|---|---|---|---|---|")
    rows = [d for d in P["per_attr"].values()
            if d["status"] == "exakt (linear)" and "inverse" in d]
    for d in sorted(rows, key=lambda d: (d["type"], d["p_index"])):
        inv = d["inverse"]
        A("| `%s` | `%s` | %d | %d | %d | %s | %s | %s … %s | %s |" % (
            d["type"], d["attr"], d["p_index"], d["n"], d["real_distinct"],
            fmt(inv["a"], 9), fmt(inv["b"], 9),
            fmt(inv["x_at_p0"], 4), fmt(inv["x_at_p1"], 4), d["confidence"]))
    A("")
    A("### 7.2 Umkehrfunktionen (fuer die Implementierung)")
    A("")
    A("| Modultyp | Attribut | Umkehrfunktion |")
    A("|---|---|---|")
    for d in sorted(rows, key=lambda d: (d["type"], d["p_index"])):
        inv = d["inverse"]
        A("| `%s` | `%s` | `x = (p - %s) / %s` |" % (
            d["type"], d["attr"], fmt(inv["b"], 9), fmt(inv["a"], 9)))
    A("")
    A("### 7.3 Einzelpunkt-Befunde (Position plausibel, Kurve unbekannt)")
    A("")
    A("Attribute, die in allen Presets denselben Wert haben, koennen keine Kurve "
      "belegen. Liegt ihr `p`-Wert aber nicht bei 0, ist die *Position* im "
      "Parameterschema zumindest konsistent:")
    A("")
    A("| Modultyp | Attribut | p | realer Wert | p-Wert |")
    A("|---|---|---|---|---|")
    sp = [d for d in P["per_attr"].values() if "single_point" in d]
    for d in sorted(sp, key=lambda d: (d["type"], d["p_index"])):
        A("| `%s` | `%s` | %d | %s | %s |" % (
            d["type"], d["attr"], d["p_index"],
            fmt(d["single_point"]["real"], 4), fmt(d["single_point"]["p"], 4)))
    A("")
    A("### 7.4 Werksdefault-Vektor je Modultyp")
    A("")
    A("Der haeufigste `fx`-Vektor eines Typs, gegenueber den Modulattributen an "
      "dieser Kettenposition. Alle Zeilen muessen unter **einer** Abbildung "
      "gleichzeitig erklaerbar sein — das ist die staerkste Bestaetigung der "
      "Parameterreihenfolge.")
    A("")
    FD = rep["fx_defaults"]
    for t, fd in sorted(FD.items()):
        A("**`%s`** (Vektor in %d Presets, %d Varianten)" % (t, fd["n"], fd["n_variants"]))
        A("")
        A("| p | Attribut | realer Default | p-Wert |")
        A("|---|---|---|---|")
        for row in fd["table"]:
            A("| %d | `%s` | %s | %s |" % (row["p_index"], row["attr"],
                                           fmt(row["real"], 4), fmt(row["p"], 4)))
        A("| 12 | `mix` | %s | %s |" % (fmt(fd["mix"]["real"], 4), fmt(fd["mix"]["p"], 4)))
        A("| 13 | `outGain` | %s | %s |" % (fmt(fd["outGain"]["real"], 4),
                                            fmt(fd["outGain"]["p"], 4)))
        A("")
    A("### 7.5 Nicht bestimmbare Zuordnungen")
    A("")
    A("| Modultyp | Attribut | p | n | real distinct | Status |")
    A("|---|---|---|---|---|---|")
    for d in sorted(P["per_attr"].values(), key=lambda d: (d["type"], d["p_index"])):
        if d["status"].startswith("exakt"):
            continue
        A("| `%s` | `%s` | %d | %d | %d | %s |" % (
            d["type"], d["attr"], d["p_index"], d["n"], d["real_distinct"], d["status"]))
    A("")
    if P["conflicts"]:
        A("### 7.6 Kollisionen (mehrere Attribute → gleicher p-Index)")
        A("")
        for t, c in P["conflicts"].items():
            A("* `%s`: %s" % (t, "; ".join("p%s ← %s" % (k, ", ".join(v))
                                           for k, v in c.items())))
        A("")
    A("### 7.7 Rueckrechnung (Roundtrip)")
    A("")
    A("| Modultyp | Attribute verifiziert | max. Rekonstruktionsfehler |")
    A("|---|---|---|")
    for t, r in sorted(P["roundtrip"].items()):
        A("| `%s` | %d / %d | %s |" % (t, r["attrs_verified"], r["attrs_tested"],
                                       "%.2e" % r["max_reconstruction_error"]))
    A("")
    A("### 7.8 Parameterschema je Modultyp (XML-Reihenfolge)")
    A("")
    for t, rec in sorted(rep["fx_attr_inventory"].items()):
        A("* `%s`: `p0…p%d` = %s → `p12` = MIX, `p13` = OUTGAIN" % (
            t, max(len(rec["params_order_common"]) - 1, 0),
            ", ".join("`%s`" % k for k in rec["params_order_common"])))
    A("")

    # ---------------- 8. Kurven
    A("## 8. Normalisierungskurven (F1)")
    A("")
    A("### 8.1 `stretch`")
    A("")
    sp = rep["curve_analysis"].get("stretch", {})
    if sp:
        A("Default (Werkswert): `%.9f` in %d Slots." % (
            sp["default_normalised"], sp["default_count"]))
        A("")
        A("| Kurve | Faktor beim Default | v fuer Faktor 1.0 | Abweichung zum Default |")
        A("|---|---|---|---|")
        for k, v in sp["hypotheses"].items():
            A("| `%s` | %s | %s | %s |" % (
                k, fmt(v.get("factor_at_default"), 6), fmt(v.get("v_for_factor_1"), 6),
                fmt(v.get("delta_to_default"), 6)))
        A("")
        pl = sp.get("power_law_0.25_8")
        if pl:
            A("Potenz-Kurve `v = ((f-0.25)/7.75)^s` mit Faktor 1.0 beim Default: "
              "**s = %.6f** (JUCE-Skew äquivalent %.6f)." %
              (pl["s"], pl["equivalent_skew_juce"]))
            A("")
    A("### 8.2 `pitch`")
    A("")
    pt = rep["curve_analysis"].get("pitch", {})
    if pt:
        A("Default: `%.9f` — Abstand zu 0.5 beträgt **%.1f float32-ULP**." % (
            pt["default_normalised"], pt["offset_from_half_f32_ulps"]))
        A("")
        A("| Reichweite | Semitöne der beobachteten Werte | max. Abweichung vom 1-HT-Raster |")
        A("|---|---|---|")
        for r, v in pt["range_tests"].items():
            A("| %s | %s | %s |" % (r, ", ".join(fmt(s, 3) for s in v["semitones"]),
                                    fmt(v["max_dev_from_integer_HT"], 4)))
        A("")
        A("Bestes Raster: **%s**" % pt.get("best_integer_semitone_range"))
        A("")
    A("### 8.3 `root_note`")
    A("")
    rn = rep["curve_analysis"].get("root_note", {})
    if rn:
        A("Default: `%.9f`, distinct = %d." % (rn["default_normalised"], rn["distinct"]))
        A("")
        A("| Skala | Wert beim Default | naechste Ganzzahl | Abweichung |")
        A("|---|---|---|---|")
        for k, v in rn["hypotheses"].items():
            A("| `%s` | %s | %s | %s |" % (k, fmt(v["value_at_default"], 6),
                                           v["nearest_integer"], fmt(v["delta"], 9)))
        A("")
        if rn.get("rational"):
            A("Bruch-Kandidat: **%d/%d** (Fehler %s)." % (
                rn["rational"]["num"], rn["rational"]["den"],
                "%.2e" % rn["rational"]["err"]))
            A("")
    A("### 8.4 `filterq`")
    A("")
    fq = rep["curve_analysis"].get("filterq", {})
    if fq:
        A("Default: `%.9f` (konstant in allen Slots)." % fq["default_normalised"])
        A("")
        if fq.get("rational"):
            A("Bruch-Kandidat: %d/%d" % (fq["rational"]["num"], fq["rational"]["den"]))
            A("")
        if fq["q_range_tests"]:
            A("| Q-Bereich @ Default-Q | vorhergesagt | Abweichung |")
            A("|---|---|---|")
            for k, v in fq["q_range_tests"].items():
                A("| `%s` | %s | %s |" % (k, fmt(v["predicted_normalised"], 9),
                                          fmt(v["delta"], 9)))
        else:
            A("Kein Q-Bereich reproduziert den Default innerhalb 2e-6.")
        A("")
    A("### 8.5 `vol` und `master_vol`")
    A("")
    vo = rep["curve_analysis"].get("vol", {})
    if vo:
        A("**`lN_vol`** — Default `%.9f`" % vo["default_normalised"])
        A("")
        if vo.get("rational"):
            A("* Bruch-Kandidat: %d/%d" % (vo["rational"]["num"], vo["rational"]["den"]))
        A("* `20·log10(Default)` = %s dB" % fmt(vo.get("plain_20log10_of_default_db"), 4))
        A("* Lineare Verstaerkung mit 1.0 beim Default ⇒ Maximalwert %s (%.4f dB)" % (
            fmt(vo["linear_gain_unity_at_default"]["gain_at_p1"], 6),
            vo["linear_gain_unity_at_default"]["db_at_p1"]))
        A("")
        if vo.get("scale_candidates"):
            A("| dB-Bereich | Default in dB | Glattheit | bestes Raster | Rasterfehler |")
            A("|---|---|---|---|---|")
            for c in vo["scale_candidates"]:
                A("| [%g, %g] | %s | %s | %s dB | %s |" % (
                    c["lo_db"], c["hi_db"], fmt(c["default_db"], 3),
                    fmt(c["default_db_smoothness"], 4), c["grid_step_db"],
                    fmt(c["grid_maxerr_steps"], 3)))
            A("")
    mv = rep["curve_analysis"].get("master_vol", {})
    if mv:
        A("**`master_vol`** — Default `%.9f` (%.1f %% aller Presets), distinct = %d" % (
            mv["default_normalised"],
            100.0 * mv["default_count"] / mv["n"], len(mv["observed"])))
        A("")
        if mv.get("rational"):
            A("* Bruch-Kandidat: **%d/%d**" % (mv["rational"]["num"], mv["rational"]["den"]))
        A("* dB-Skala mit 0 dB beim Default ⇒ Bereich `[%s, %s]` z. B. für lo = -60, -50, "
          "-48, -40, -36, -30, -25, -24, -20 dB:" %
          ("lo", "lo + |lo|/0.8333"))
        A("")
        A("| lo (dB) | hi (dB) |")
        A("|---|---|")
        for c in mv["interpretations"]["db_linear_0dB_at_default"]:
            A("| %g | %s |" % (c["lo_db"], fmt(c["hi_db"], 4)))
        A("")
        lg = mv["interpretations"]["linear_gain_unity_at_default"]
        A("* Lineare Verstaerkung mit 1.0 beim Default ⇒ `%s`, Maximalwert %s "
          "(%.2f dB)." % (lg["form"], fmt(lg["gain_at_p1"], 6), lg["db_at_p1"]))
        A("")
        A("| dB-Bereich | Default in dB | Abstand zur Ganzzahl |")
        A("|---|---|---|")
        for c in mv["interpretations"]["db_smooth_default"]:
            A("| [%g, %g] | %s | %s |" % (c["lo_db"], c["hi_db"],
                                          fmt(c["default_db"], 3),
                                          fmt(c["dist_to_integer_db"], 4)))
        A("")

    A("---")
    A("")
    A("## 9. Rohdaten")
    A("")
    A("Vollstaendige Auswertung inkl. aller Einzelwerte: "
      "[`_private/mining/report.json`](../../_private/mining/report.json) "
      "(nicht Teil des oeffentlichen Repos).")
    A("")
    return "\n".join(md)


# ==========================================================================
# main
# ==========================================================================

def main(argv=None):
    ap = argparse.ArgumentParser(description="Preset-Datamining (read-only)")
    ap.add_argument("--kick-dir", default=str(KICK_DIR))
    ap.add_argument("--fx-dir", default=str(FXCHAIN_DIR))
    ap.add_argument("--json-only", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)

    kick_dir, fx_dir = Path(args.kick_dir), Path(args.fx_dir)

    kick_files = sorted(kick_dir.rglob("*.itkbpreset"))
    if not kick_files:
        print("FEHLER: keine .itkbpreset unter %s" % kick_dir, file=sys.stderr)
        return 2

    presets = []
    for i, f in enumerate(kick_files):
        try:
            pr = read_kick_preset(f)
        except ET.ParseError as e:
            print("WARNUNG: uebersprungen (%s)" % e, file=sys.stderr)
            continue
        pr["preset_key"] = "preset_%02d" % (i + 1)
        presets.append(pr)

    fx_chains = {}
    for i, f in enumerate(sorted(fx_dir.rglob("*.xml"))):
        try:
            ch = read_fx_chain_file(f)
        except ET.ParseError as e:
            print("WARNUNG: uebersprungen (%s)" % e, file=sys.stderr)
            continue
        if ch is not None:
            fx_chains["chain_%02d" % (i + 1)] = ch

    snaps = {pr["preset_key"]: {"modules": pr["modules"], "bypassAll": None}
             for pr in presets if pr["modules"]}

    print("Presets: %d   FX-Chains: %d   Presets mit FX-Snapshot: %d"
          % (len(presets), len(fx_chains), len(snaps)))
    if not presets:
        print("FEHLER: keine Presets gelesen.", file=sys.stderr)
        return 2

    # ---- Analysen
    param_stats = analyse_params(presets)
    rollup = param_rollup(param_stats)
    fx_inv = analyse_fx_attrs(fx_chains, snaps)
    crosstab = analyse_crosstab(presets)
    positional = analyse_positional(crosstab)
    fx_quant = analyse_fx_quantisation(crosstab)
    fx_defaults = analyse_fx_defaults(crosstab, fx_inv)
    slots = analyse_slots(presets)

    key_params = ["l0_vol", "l0_pitch", "l0_stretch", "master_vol", "root_note",
                  "l0_filter", "l0_filterq", "l0_dlytime", "l0_pos",
                  "l0_trimstart", "l0_trimend", "l0_fadein", "l0_fadeout",
                  "l0_ficurve", "l0_dlymix"]
    scale_cands = scale_candidates(param_stats, key_params)
    curve = {
        "stretch": stretch_pitch_analysis(param_stats).get("stretch"),
        "pitch": stretch_pitch_analysis(param_stats).get("pitch"),
        "root_note": stretch_pitch_analysis(param_stats).get("root_note"),
        "filterq": stretch_pitch_analysis(param_stats).get("filterq"),
        "vol": vol_analysis(param_stats, scale_cands),
        "master_vol": master_vol_analysis(param_stats),
        "scale_candidates": scale_cands,
    }

    report = {
        "meta": {
            "generated": "2026-11-17",
            "tool": "tools/mine_presets.py",
            "n_presets": len(presets),
            "n_fxchains": len(fx_chains),
            "n_presets_with_fxsnap": len(snaps),
            "slot_count_max": SLOT_COUNT,
            "fx_slots": FX_SLOTS,
            "fx_params": FX_PARAMS,
            "tolerance": TOL,
        },
        "param_stats": param_stats,
        "param_rollup": rollup,
        "slots": slots,
        "fx_attr_inventory": fx_inv,
        "crosstab": crosstab,
        "positional": positional,
        "fx_quantisation": fx_quant,
        "fx_defaults": fx_defaults,
        "curve_analysis": curve,
    }

    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_JSON, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1, ensure_ascii=False)
    print("JSON  -> %s (%.1f kB)" % (OUT_JSON, OUT_JSON.stat().st_size / 1024.0))

    if not args.json_only:
        OUT_MD.parent.mkdir(parents=True, exist_ok=True)
        OUT_MD.write_text(write_markdown(report), encoding="utf-8")
        print("MD    -> %s (%.1f kB)" % (OUT_MD, OUT_MD.stat().st_size / 1024.0))

    # ---- Konsolen-Diagnose
    print()
    print("=== Positionshypothese: bestaetigte Attribut->p-Zuordnungen ===")
    ok = [d for d in positional["per_attr"].values()
          if d["status"] == "exakt (linear)" and "inverse" in d]
    for d in sorted(ok, key=lambda d: (d["type"], d["p_index"])):
        inv = d["inverse"]
        print("  %-7s %-9s -> p%-2d  n=%-3d s=%-2d  a=%-11.6g b=%-11.6g  x in [%.4g, %.4g]  %s"
              % (d["type"], d["attr"], d["p_index"], d["n"], d["real_distinct"],
                 inv["a"], inv["b"], inv["x_at_p0"], inv["x_at_p1"],
                 d["confidence"]))
    print("  (%d Zuordnungen; %d Attribut/Index-Paare geprueft, Toleranz %.1e)"
          % (len(ok), len(positional["per_attr"]), TOL))
    print()
    print("=== fx-Quantisierung: bester Schritt %.6g (max. Fehler %.2e) ==="
          % (fx_quant["best_grid"]["step"], fx_quant["best_grid"]["max_error_absolute"]))
    if positional["conflicts"]:
        print("=== Kollisionen ===")
        for t, c in positional["conflicts"].items():
            print("  %s: %s" % (t, c))
        print()
    print("=== konstant (= nicht bestimmbar) je Modultyp ===")
    for t, rec in sorted(fx_inv.items()):
        const = [k for k, st in rec["attrs"].items() if st["constant"]]
        varying = [k for k, st in rec["attrs"].items() if not st["constant"]]
        print("  %-7s %d/%d Attribute konstant; variabel: %s"
              % (t, len(const), len(rec["attrs"]), ", ".join(varying) or "–"))
    print()
    print("=== Kernbefunde F1 ===")
    for key in ("stretch", "pitch", "root_note", "filterq"):
        c = curve.get(key)
        if c:
            print("  %-9s default=%s" % (key, c.get("default_normalised")))
    print("  master_vol default=%s rational=%s"
          % (curve["master_vol"].get("default_normalised"),
             curve["master_vol"].get("rational")))
    print("  lN_vol     default=%s" % curve["vol"].get("default_normalised"))
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
