#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mine_presets.py -- Datamining-Werkzeug fuer die Referenz-Presets des
Techno-Kick-Builders (Phase "Parity/F1/F2").

Liest AUSSCHLIESSLICH (kein Schreiben ausserhalb von _private/mining):
  * Kick-Presets  (*.itkbpreset)   -> normalisierte 0..1-Werte (Params)
  * FX-Chain-Presets (*.xml)       -> reale Modul-Attribute

Erzeugt:
  * _private/mining/report.json           (vollstaendige Rohauswertung)
  * docs/specs/mining-report.md           (lesbare Zusammenfassung)

Nur Standardbibliothek. Keine Inhalte des Referenzprodukts werden in den
oeffentlichen Teil kopiert: es werden ausschliesslich Zahlen, Verteilungen und
abgeleitete Kandidaten-Zuordnungen ausgegeben -- keine Presetnamen, keine
Sample-Pfade, keine Handbuchtexte.

Aufruf:
  python tools/mine_presets.py              # Standardpfade
  python tools/mine_presets.py --json-only  # nur JSON schreiben
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import statistics
import sys
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

# --------------------------------------------------------------------------
# Pfade
# --------------------------------------------------------------------------

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

# Attribute, die keine numerischen Modulparameter sind (Struktur/Meta)
NON_NUMERIC_ATTRS = {"type", "ir", "name", "preset"}

# Struktur-Kinder, die eigene Datensaetze tragen
CHILD_TAGS = ("Band", "WNode", "Pt")


# --------------------------------------------------------------------------
# Hilfsfunktionen
# --------------------------------------------------------------------------

def fnum(s):
    """Robuste Float-Konvertierung; gibt None zurueck wenn nicht numerisch."""
    if s is None:
        return None
    try:
        v = float(s)
    except (TypeError, ValueError):
        return None
    if math.isnan(v) or math.isinf(v):
        return None
    return v


def is_numeric_str(s):
    return fnum(s) is not None


def quantiles(vals, qs=(0.0, 0.10, 0.25, 0.5, 0.75, 0.90, 1.0)):
    """Lineare Quantile (ohne numpy)."""
    if not vals:
        return {}
    vs = sorted(vals)
    n = len(vs)
    out = {}
    for q in qs:
        if n == 1:
            out["p%g" % (q * 100)] = vs[0]
            continue
        pos = q * (n - 1)
        lo = int(math.floor(pos))
        hi = int(math.ceil(pos))
        if lo == hi:
            out["p%g" % (q * 100)] = vs[lo]
        else:
            frac = pos - lo
            out["p%g" % (q * 100)] = vs[lo] * (1.0 - frac) + vs[hi] * frac
    return out


def spread(vals):
    """Kompakte Verteilungsbeschreibung."""
    if not vals:
        return {"n": 0}
    vs = sorted(vals)
    d = {
        "n": len(vs),
        "min": vs[0],
        "max": vs[-1],
        "median": statistics.median(vs),
        "mean": statistics.fmean(vs),
        "distinct": len(set(round(v, 9) for v in vs)),
        "quantiles": quantiles(vals),
    }
    try:
        d["stdev"] = statistics.pstdev(vs)
    except statistics.StatisticsError:
        d["stdev"] = 0.0
    return d


def top_values(vals, k=6):
    """Haeufigste Werte (auf 6 Nachkommastellen gerundet)."""
    c = Counter(round(v, 6) for v in vals)
    total = sum(c.values()) or 1
    return [
        {"value": v, "count": n, "share": round(n / total, 4)}
        for v, n in c.most_common(k)
    ]


def spearman(xs, ys):
    """Spearman-Rangkorrelation ohne externe Libs (Average-Ranks)."""
    n = len(xs)
    if n < 3:
        return None

    def ranks(a):
        order = sorted(range(n), key=lambda i: a[i])
        r = [0.0] * n
        i = 0
        while i < n:
            j = i
            while j + 1 < n and a[order[j + 1]] == a[order[i]]:
                j += 1
            avg = (i + j) / 2.0
            for k in range(i, j + 1):
                r[order[k]] = avg
            i = j + 1
        return r

    rx, ry = ranks(xs), ranks(ys)
    mx = sum(rx) / n
    my = sum(ry) / n
    num = sum((a - mx) * (b - my) for a, b in zip(rx, ry))
    den = math.sqrt(sum((a - mx) ** 2 for a in rx) * sum((b - my) ** 2 for b in ry))
    if den == 0:
        return None
    return num / den


def linfit(xs, ys):
    """Einfache lineare Regression y = a*x + b + Rueckgabe von R^2."""
    n = len(xs)
    if n < 2:
        return None
    mx = sum(xs) / n
    my = sum(ys) / n
    sxx = sum((x - mx) ** 2 for x in xs)
    if sxx == 0:
        return None
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    a = sxy / sxx
    b = my - a * mx
    ss_tot = sum((y - my) ** 2 for y in ys)
    ss_res = sum((y - (a * x + b)) ** 2 for x, y in zip(xs, ys))
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else 0.0
    return {"slope": a, "offset": b, "r2": r2}


# --------------------------------------------------------------------------
# Einlesen: Kick-Presets
# --------------------------------------------------------------------------

def read_kick_preset(path: Path):
    """Liest ein .itkbpreset. Gibt dict zurueck (ohne Presetnamen/Pfade)."""
    tree = ET.parse(str(path))
    root = tree.getroot()

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

    # FX-Snapshot (reale Einheiten) innerhalb des Presets
    chain_el = root.find(".//fxSnap/FxChain")
    modules = []
    if chain_el is not None:
        modules = read_fx_chain(chain_el)

    return {
        "schemaVersion": root.get("schemaVersion"),
        "appVersion": root.get("appVersion"),
        "genreId": root.get("genreId"),
        "slots": slots,
        "params": params,
        "modules": modules,
        "hasFxSnap": chain_el is not None,
    }


# --------------------------------------------------------------------------
# Einlesen: FX-Ketten
# --------------------------------------------------------------------------

def read_fx_chain(chain_el):
    """Liest eine <FxChain>-Element in eine Modulliste (reale Einheiten)."""
    out = []
    for idx, m in enumerate(chain_el.findall("Module")):
        attrs = []
        numeric = {}
        for k, v in m.attrib.items():
            attrs.append(k)
            if k in NON_NUMERIC_ATTRS:
                continue
            fv = fnum(v)
            if fv is not None:
                numeric[k] = fv
        children = defaultdict(list)
        for c in m:
            if c.tag in CHILD_TAGS:
                children[c.tag].append(dict(c.attrib))
        out.append({
            "pos": idx,
            "type": m.get("type"),
            "attr_order": attrs,
            "attrs": numeric,
            "children": dict(children),
            "child_counts": {t: len(children.get(t, [])) for t in CHILD_TAGS},
        })
    return out


def read_fx_chain_file(path: Path):
    tree = ET.parse(str(path))
    root = tree.getroot()
    chain_el = root.find(".//FxChain")
    if chain_el is None:
        # Fallback: Wurzel selbst ist die Kette
        chain_el = root if root.tag == "FxChain" else None
    if chain_el is None:
        return None
    return {
        "bypassAll": chain_el.get("bypassAll"),
        "modules": read_fx_chain(chain_el),
    }


# --------------------------------------------------------------------------
# Auswertung 1: Parameterverteilung
# --------------------------------------------------------------------------

def analyse_params(presets):
    """Verteilung je Parameter-ID."""
    by_id = defaultdict(list)
    for pr in presets:
        for pid, v in pr["params"].items():
            by_id[pid].append(v)

    result = {}
    for pid, vals in by_id.items():
        d = spread(vals)
        d["top_values"] = top_values(vals)
        d["at_zero"] = sum(1 for v in vals if abs(v) < 1e-12)
        d["at_one"] = sum(1 for v in vals if abs(v - 1.0) < 1e-12)
        d["at_half"] = sum(1 for v in vals if abs(v - 0.5) < 1e-6)
        result[pid] = d
    return dict(sorted(result.items()))


def param_groups():
    """Gruppiert Parameter-IDs nach Klasse (slot/global/fx)."""
    groups = {"slot": [], "global": [], "fx": []}
    for n in range(SLOT_COUNT):
        for suf in SLOT_PARAM_SUFFIXES:
            groups["slot"].append("l%d_%s" % (n, suf))
    groups["global"] = list(GLOBAL_IDS)
    for n in range(FX_SLOTS):
        for m in range(FX_PARAMS):
            groups["fx"].append("fx%d_p%d" % (n, m))
    return groups


def group_rollup(param_stats):
    """Fasst Parameter nach Suffix zusammen: lN_vol ueber alle N."""
    g = param_groups()
    roll = {}
    for key in ("slot", "global", "fx"):
        for pid in g[key]:
            if pid not in param_stats:
                continue
            if key == "slot":
                base = re.sub(r"^l\d+_", "", pid)
            elif key == "fx":
                base = re.sub(r"^fx\d+_", "", pid)
            else:
                base = pid
            roll.setdefault(key, {}).setdefault(base, []).append(pid)

    out = {}
    for key, d in roll.items():
        out[key] = {}
        for base, ids in d.items():
            allv = []
            for pid in ids:
                st = param_stats[pid]
                for _ in range(st["n"]):
                    pass
            # Sammelstatistik aus den Einzelstatistiken
            mins = [param_stats[i]["min"] for i in ids]
            maxs = [param_stats[i]["max"] for i in ids]
            meds = [param_stats[i]["median"] for i in ids]
            out[key][base] = {
                "ids": len(ids),
                "n": sum(param_stats[i]["n"] for i in ids),
                "min": min(mins),
                "max": max(maxs),
                "median_range": [min(meds), max(meds)],
            }
    return out


# --------------------------------------------------------------------------
# Auswertung 2: FX-Modulattribute (aus den 76 Chains + 44 Snapshots)
# --------------------------------------------------------------------------

def analyse_fx_attrs(fx_chains, preset_snaps):
    """Attribut-Inventar je Modultyp: Name, Reihenfolge, Wertebereiche."""
    per_type = defaultdict(lambda: {
        "attr_first_seen": [],
        "attr_counts": Counter(),
        "attr_order_votes": defaultdict(Counter),
        "attr_values": defaultdict(list),
        "occurrences": 0,
        "children": Counter(),
        "child_attr_values": defaultdict(list),
        "chains_with_type": [],
    })

    sources = []
    for name, chain in fx_chains.items():
        sources.append(("chain", name, chain))
    for name, snap in preset_snaps.items():
        sources.append(("snapshot", name, snap))

    for kind, name, chain in sources:
        if chain is None:
            continue
        types_here = []
        for mod in chain["modules"]:
            t = mod["type"]
            if t is None:
                continue
            rec = per_type[t]
            rec["occurrences"] += 1
            types_here.append(t)
            for i, (k, v) in enumerate(mod["attrs"].items()):
                if k not in rec["attr_first_seen"]:
                    rec["attr_first_seen"].append(k)
                rec["attr_counts"][k] += 1
                rec["attr_order_votes"][k][i] += 1
                rec["attr_values"][k].append(v)
            for ctag, items in mod["children"].items():
                rec["children"][ctag] += len(items)
                for it in items:
                    for ck, cv in it.items():
                        fv = fnum(cv)
                        if fv is not None:
                            rec["child_attr_values"]["%s.%s" % (ctag, ck)].append(fv)
        if types_here:
            per_type_key = ",".join(types_here)
            for t in set(types_here):
                per_type[t]["chains_with_type"].append({"source": kind, "shape": per_type_key})

    out = {}
    for t, rec in sorted(per_type.items()):
        # kanonische Attributreihenfolge = haeufigste Position
        order = sorted(
            rec["attr_counts"].keys(),
            key=lambda k: (-rec["attr_counts"][k],
                           rec["attr_order_votes"][k].most_common(1)[0][0]),
        )
        # Positionen, die in allen Vorkommen stabil sind
        stable = []
        for k in order:
            pos = rec["attr_order_votes"][k].most_common(1)[0][0]
            share = rec["attr_order_votes"][k].most_common(1)[0][1] / rec["attr_counts"][k]
            stable.append({"attr": k, "pos": pos, "stability": round(share, 4)})

        attrs = {}
        for k in order:
            vals = rec["attr_values"][k]
            d = spread(vals)
            d["top_values"] = top_values(vals, 5)
            d["distinct"] = len(set(round(v, 9) for v in vals))
            attrs[k] = d
        child_stats = {}
        for k, vals in rec["child_attr_values"].items():
            d = spread(vals)
            d["top_values"] = top_values(vals, 5)
            child_stats[k] = d

        out[t] = {
            "occurrences": rec["occurrences"],
            "attr_order": order,
            "attr_positions": stable,
            "attrs": attrs,
            "child_tag_counts": dict(rec["children"]),
            "child_attrs": child_stats,
            "chain_shapes": per_type[t]["chains_with_type"][:40],
        }
    return out


# --------------------------------------------------------------------------
# Auswertung 3: Kreuztabelle Preset -> FX-Kette + fx-Werte (Kern von F2)
# --------------------------------------------------------------------------

def analyse_crosstab(presets):
    """Fuer jedes Preset: Module je Kettenposition (Typ + reale Attribute)
    neben den zugehoerigen fx{N}_p{M}-Werten."""
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
                "module_attrs": mod["attrs"] if mod else None,
                "module_attr_order": mod["attr_order"] if mod else None,
                "child_counts": mod["child_counts"] if mod else None,
                "p": p,
                "p_active": [v for v in p if v is not None and v != 0.0],
            })
        rows.append({
            "preset_key": pr["preset_key"],
            "n_modules": len(mods),
            "chain_types": chain_types,
            "chain_shape": ",".join(t for t in chain_types if t),
            "slots": slots,
        })

    # Aggregation: welche Kettenform (Modultypfolge) kommt wie oft vor
    shapes = Counter(r["chain_shape"] for r in rows)
    # je Kettenposition: welche Modultypen
    pos_types = defaultdict(Counter)
    for r in rows:
        for i, t in enumerate(r["chain_types"]):
            if t:
                pos_types[i][t] += 1

    return {
        "rows": rows,
        "chain_shapes": [
            {"shape": s, "count": n} for s, n in shapes.most_common()
        ],
        "position_type_counts": {
            str(p): dict(c.most_common()) for p, c in sorted(pos_types.items())
        },
        "n_presets": len(rows),
    }


# --------------------------------------------------------------------------
# Auswertung 4: Kandidaten-Zuordnung Modulattribut <-> fxN_pM
# --------------------------------------------------------------------------

def analyse_bridge(crosstab):
    """Statistische Kandidaten fuer die Abbildung Attribut -> p-Index.

    Fuer jeden Modultyp werden alle (Attributwert real, p-Index-Wert)
    Paare ueber alle Presets gesammelt und je Paar Spearman-Rangkorrelation
    und Lin-/Log-Fit berechnet. Das liefert Kandidaten, keine Beweise.
    """
    # (type) -> attr -> [ (real_value, p_value_0, ..., p_value_13) ]
    samples = defaultdict(list)
    for r in crosstab["rows"]:
        for s in r["slots"]:
            t = s["module_type"]
            if not t or not s["module_attrs"]:
                continue
            p = s["p"]
            for attr, rv in s["module_attrs"].items():
                if attr in NON_NUMERIC_ATTRS:
                    continue
                samples[(t, attr)].append((rv, p))

    out = {}
    for (t, attr), rows in sorted(samples.items()):
        reals = [r[0] for r in rows]
        n = len(rows)
        entry = {
            "type": t,
            "attr": attr,
            "n": n,
            "real_distinct": len(set(round(v, 9) for v in reals)),
            "real": spread(reals),
            "candidates": [],
        }
        if n < 4 or entry["real_distinct"] < 2:
            # konstant -> allein aus Werten nicht identifizierbar
            entry["note"] = "Attribut (nahezu) konstant in den Presets -> nicht trennbar"
        for m in range(FX_PARAMS):
            pv = [r[1][m] for r in rows]
            if any(v is None for v in pv):
                continue
            if len(set(round(v, 9) for v in pv)) < 2:
                continue
            rho = spearman(reals, pv)
            if rho is None:
                continue
            # R^2 fuer p = a*real+b und fuer p = a*log(real)+b
            lin = linfit(reals, pv)
            r2_lin = lin["r2"] if lin else -1.0
            r2_log = -1.0
            pos = all(v > 0 for v in reals)
            if pos:
                lg = linfit([math.log(v) for v in reals], pv)
                r2_log = lg["r2"] if lg else -1.0
            entry["candidates"].append({
                "p": m,
                "rho": round(rho, 4),
                "r2_lin": round(r2_lin, 4),
                "r2_log": round(r2_log, 4),
                "verified_frac": round(
                    sum(1 for i in range(n)
                        if abs(pv[i] - (_LIN[p[1]][m](reals[i]))) < 0.002
                        for p in [rows]) / n, 4) if False else None,
            })
        entry["candidates"].sort(key=lambda c: (-abs(c["rho"]), -max(c["r2_lin"], c["r2_log"])))
        entry["candidates"] = entry["candidates"][:4]
        out["%s:%s" % (t, attr)] = entry
    return out


_LIN = {}  # Platzhalter, nicht genutzt


def analyse_bridge_exact(crosstab):
    """Praezisere Bruecke: prueft fuer jedes (type, attr, p) Paar, ob eine
    *lineare* oder *logarithmische* Abbildung existiert, die ALLE Beobachtungen
    mit Toleranz reproduziert. Ausreisser werden gezaehlt, nicht versteckt.
    """
    samples = defaultdict(list)
    for r in crosstab["rows"]:
        for s in r["slots"]:
            t = s["module_type"]
            if not t or not s["module_attrs"]:
                continue
            for attr, rv in s["module_attrs"].items():
                if attr in NON_NUMERIC_ATTRS:
                    continue
                samples[(t, attr)].append((rv, s["p"]))

    out = {}
    for (t, attr), rows in sorted(samples.items()):
        reals = [r[0] for r in rows]
        n = len(rows)
        if n < 3:
            continue
        best = []
        for m in range(FX_PARAMS):
            pv = [r[1][m] for r in rows]
            if any(v is None for v in pv):
                continue
            if len(set(round(v, 9) for v in pv)) < 2:
                # konstanter p-Wert: pruefe ob er zu einer Konstanten passt
                continue
            # Fit auf den beiden Extrempunkten -> teste alle
            lo = min(range(n), key=lambda i: reals[i])
            hi = max(range(n), key=lambda i: reals[i])
            if abs(reals[hi] - reals[lo]) < 1e-12:
                continue
            a = (pv[hi] - pv[lo]) / (reals[hi] - reals[lo])
            b = pv[lo] - a * reals[lo]
            err = [abs(pv[i] - (a * reals[i] + b)) for i in range(n)]
            hits = sum(1 for e in err if e < 0.0015)
            best.append({
                "p": m, "hits": hits, "n": n,
                "hit_frac": round(hits / n, 4),
                "max_err": round(max(err), 6),
                "model": "lin", "a": a, "b": b,
            })
        # logarithmisch
        if all(v > 0 for v in reals):
            for m in range(FX_PARAMS):
                pv = [r[1][m] for r in rows]
                if any(v is None for v in pv):
                    continue
                if len(set(round(v, 9) for v in pv)) < 2:
                    continue
                lg = [math.log(v) for v in reals]
                lo = min(range(n), key=lambda i: lg[i])
                hi = max(range(n), key=lambda i: lg[i])
                if abs(lg[hi] - lg[lo]) < 1e-12:
                    continue
                a = (pv[hi] - pv[lo]) / (lg[hi] - lg[lo])
                b = pv[lo] - a * lg[lo]
                err = [abs(pv[i] - (a * lg[i] + b)) for i in range(n)]
                hits = sum(1 for e in err if e < 0.0015)
                best.append({
                    "p": m, "hits": hits, "n": n,
                    "hit_frac": round(hits / n, 4),
                    "max_err": round(max(err), 6),
                    "model": "log", "a": a, "b": b,
                })
        best.sort(key=lambda c: (-c["hit_frac"], c["max_err"]))
        ded = []
        seen = set()
        for c in best:
            key = (c["p"], c["model"])
            if key in seen:
                continue
            seen.add(key)
            ded.append(c)
        out["%s:%s" % (t, attr)] = {
            "type": t, "attr": attr, "n": n,
            "real_distinct": len(set(round(v, 9) for v in reals)),
            "matches": ded[:5],
        }
    return out


# --------------------------------------------------------------------------
# Auswertung 5: Normalisierungskurven-Hypothesen fuer 0..1-Parameter
# --------------------------------------------------------------------------

CURVE_TESTS = [
    ("linear0_1", lambda v: v),
    ("percent", lambda v: v * 100.0),
    ("bipolar_pm1", lambda v: v * 2.0 - 1.0),
    ("bipolar_pm10", lambda v: (v * 2.0 - 1.0) * 10.0),
    ("bipolar_pm12", lambda v: (v * 2.0 - 1.0) * 12.0),
    ("bipolar_pm24", lambda v: (v * 2.0 - 1.0) * 24.0),
    ("db_20log10", lambda v: 20.0 * math.log10(v) if v > 0 else None),
    ("db_20log10_center", lambda v: 20.0 * math.log10(v * 2.0) if v > 0 else None),
]

# Bekannte Zielbereiche fuer Plausibilitaetspruefung
PLAUSIBLE_RANGES = {
    "vol": [(-24.0, 10.0), (-12.0, 12.0), (-60.0, 0.0), (-40.0, 6.0)],
    "pitch": [(-24.0, 24.0), (-12.0, 12.0)],
    "stretch": [(0.25, 8.0), (0.25, 4.0), (0.5, 2.0)],
}


def hypothesise_curves(param_stats, presets):
    """Prueft fuer ausgewaehlte Parameter typische Abbildungen."""
    out = {}
    interesting = ["master_vol", "root_note", "sat_amount", "reverb_size",
                   "reverb_damp", "reverb_mix", "hollow_amt", "max_amount",
                   "midi_mode"]
    for n in range(SLOT_COUNT):
        for suf in ("vol", "pitch", "stretch", "filter", "filterq",
                    "dlytime", "dlymix", "pos", "fadein", "fadeout",
                    "trimstart", "trimend", "ficurve", "fishape", "foshape"):
            interesting.append("l%d_%s" % (n, suf))

    for pid in interesting:
        st = param_stats.get(pid)
        if not st:
            continue
        mn, mx, med = st["min"], st["max"], st["median"]
        entry = {
            "n": st["n"], "min": mn, "max": mx, "median": med,
            "distinct": st["distinct"],
            "top_values": st["top_values"][:5],
            "range_tests": {},
        }
        # Reichweite der Werte
        entry["covers_full_range"] = (mn <= 0.001 and mx >= 0.999)
        entry["centered_on_half"] = abs(med - 0.5) < 1e-3
        # Symmetrie um 0.5 pruefen (nur bei bipolarer Skala sinnvoll)
        if st["n"] >= 4:
            entry["symmetry_about_0.5"] = round(
                abs((mx - 0.5) - (0.5 - mn)), 6)
        # Bei 0.5 gemappte Werte zaehlen
        entry["share_at_0.5"] = round(st["at_half"] / st["n"], 4) if st["n"] else 0
        entry["share_at_0"] = round(st["at_zero"] / st["n"], 4) if st["n"] else 0
        entry["share_at_1"] = round(st["at_one"] / st["n"], 4) if st["n"] else 0
        out[pid] = entry
    return out


def check_simple_ratios(param_stats):
    """Prueft die in 03-RE-FINDINGS genannten Verhaeltnis-Hypothesen."""
    tests = []
    def get(pid):
        st = param_stats.get(pid)
        return st["median"] if st else None

    # root_note: 36/127 = 0.2834645669
    rn = param_stats.get("root_note")
    if rn:
        r = 36.0 / 127.0
        tests.append({
            "param": "root_note",
            "observed_median": rn["median"],
            "observed_values": rn["top_values"],
            "hypothesis": "MIDI-Note n / 127 (n=36 -> C1)",
            "predicted_36_127": r,
            "delta": abs(rn["median"] - r),
            "match": abs(rn["median"] - r) < 1e-5,
        })
        # alle distinct Werte gegen n/127 testen
        for tv in rn["top_values"]:
            v = tv["value"]
            n_est = v * 127.0
            tests[-1].setdefault("top_value_note_estimates", []).append(
                {"v": v, "n_127": round(n_est, 4), "count": tv["count"]})

    # master_vol: 0.833333313 -> ?
    mv = param_stats.get("master_vol")
    if mv:
        tests.append({
            "param": "master_vol",
            "observed_median": mv["median"],
            "observed_values": mv["top_values"],
            "ratio_5_6": mv["median"] / (5.0 / 6.0),
            "db_20log10": 20 * math.log10(mv["median"]) if mv["median"] > 0 else None,
            "hypothesis": "linear 0..1.2 (0.8333 = 1/1.2) oder 0..1.2 Bereich",
            "as_gain_1_over": (1.0 / mv["median"]) if mv["median"] else None,
        })

    # stretch: 0.374989420
    st = param_stats.get("l0_stretch")
    if st:
        v = st["median"]
        tests.append({
            "param": "l0_stretch",
            "observed_median": v,
            "hypotheses": {
                "log_0.25_8": round(math.exp(math.log(0.25) + v * (math.log(8) - math.log(0.25))), 6),
                "log_0.25_4": round(math.exp(math.log(0.25) + v * (math.log(4) - math.log(0.25))), 6),
                "lin_0.25_8": round(0.25 + v * (8 - 0.25), 6),
            },
            "note": "1.0 als Zielwert pruefen: bei welchem v liegt Faktor 1.0?",
        })
    return tests


# --------------------------------------------------------------------------
# Markdown-Report
# --------------------------------------------------------------------------

def fmt(v, nd=6):
    if v is None:
        return "–"
    if isinstance(v, float):
        if abs(v) >= 1e6 or (v != 0 and abs(v) < 1e-6):
            return "%.3e" % v
        return ("%%.%df" % nd) % v
    return str(v)


def write_markdown(report):
    md = []
    A = md.append
    A("# Mining-Report: Referenz-Presets (Rohauswertung)")
    A("")
    A("**Erzeugt von:** `tools/mine_presets.py` (Stand %s)" % report["meta"]["generated"])
    A("**Datenbasis (nur lesend erfasst):**")
    A("")
    A("| Quelle | Anzahl |")
    A("|---|---|")
    A("| Kick-Presets `.itkbpreset` | %d |" % report["meta"]["n_presets"])
    A("| FX-Chain-Presets `.xml` | %d |" % report["meta"]["n_fxchains"])
    A("| Presets mit eingebettetem FX-Snapshot | %d |" % report["meta"]["n_presets_with_fxsnap"])
    A("")
    A("Alle Werte sind aus den Klartext-XML-Dateien abgeleitet. Es werden "
      "**keine** Presetnamen, Sample-Pfade oder Handbuchtexte wiedergegeben.")
    A("")
    A("---")
    A("")

    # --- Kennzahlen
    A("## 1. Kennzahlen der Kick-Presets")
    A("")
    P = report["param_stats"]
    A("| Kennzahl | Wert |")
    A("|---|---|")
    A("| Parameter-IDs insgesamt | %d |" % len(P))
    A("| davon `lN_*` (Slot) | %d |" % len([k for k in P if re.match(r"^l\d+_", k)]))
    A("| davon global | %d |" % len([k for k in P if k in GLOBAL_IDS]))
    A("| davon `fxN_pM` | %d |" % len([k for k in P if re.match(r"^fx\d+_p", k)]))
    A("| Presets | %d |" % report["meta"]["n_presets"])
    A("")
    A("Slot-Belegung: siehe JSON (`slots_per_preset`). Kettenformen: Abschnitt 4.")
    A("")

    # --- Slot-Grundwerte
    A("### 1.1 Rollup: Slots ueber alle N")
    A("")
    A("Fuer jede Slot-Parameterart der beobachtete Wertebereich ueber alle 12 Slots "
      "und alle 44 Presets:")
    A("")
    A("| Parameterart | n | min | max | Median-Bereich |")
    A("|---|---|---|---|---|")
    for base, r in sorted(report["param_rollup"]["slot"].items()):
        A("| `%s` | %d | %s | %s | %s … %s |" % (
            base, r["n"], fmt(r["min"]), fmt(r["max"]),
            fmt(r["median_range"][0]), fmt(r["median_range"][1])))
    A("")

    # --- Globals
    A("### 1.2 Globale Parameter")
    A("")
    A("| ID | n | min | max | median | distinct | haeufigste Werte |")
    A("|---|---|---|---|---|---|---|")
    for pid in GLOBAL_IDS:
        st = P.get(pid)
        if not st:
            continue
        tv = ", ".join("%s (%d×)" % (fmt(t["value"]), t["count"]) for t in st["top_values"][:4])
        A("| `%s` | %d | %s | %s | %s | %d | %s |" % (
            pid, st["n"], fmt(st["min"]), fmt(st["max"]), fmt(st["median"]),
            st["distinct"], tv))
    A("")

    # --- FX-Parameter
    A("## 2. FX-Parameter `fxN_pM` (normalisiert)")
    A("")
    A("### 2.1 Rollup ueber alle Slots")
    A("")
    A("| Index | n | min | max | Median-Bereich |")
    A("|---|---|---|---|---|")
    for base, r in sorted(report["param_rollup"]["fx"].items(),
                          key=lambda kv: int(kv[0][1:])):
        A("| `p%s` | %d | %s | %s | %s … %s |" % (
            base, r["n"], fmt(r["min"]), fmt(r["max"]),
            fmt(r["median_range"][0]), fmt(r["median_range"][1])))
    A("")
    A("### 2.2 Wert pro Slot-Index (Mittel ueber p0..p10)")
    A("")
    A("Aktive FX-Slots: siehe Abschnitt 4. Leere Slots sind an einem Muster aus "
      "`p12=1.0`, `p13=0.5` und sonst 0 erkennbar.")
    A("")

    # --- FX Attribut-Inventar
    A("## 3. Modulattribute je `type=` (aus 76 Chains + 44 Snapshots)")
    A("")
    for t, rec in sorted(report["fx_attr_inventory"].items()):
        A("### `%s` — %d Vorkommen" % (t, rec["occurrences"]))
        A("")
        A("Attributreihenfolge (kanonisch, stabilste Position zuerst):")
        A("")
        A("| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |")
        A("|---|---|---|---|---|---|---|---|")
        for a in rec["attr_positions"]:
            st = rec["attrs"][a["attr"]]
            A("| %d | `%s` | %d | %.2f | %d | %s | %s | %d |" % (
                a["pos"], a["attr"], a["pos"], a["stability"], st["n"],
                fmt(st["min"]), fmt(st["max"]), st["distinct"]))
        A("")
        if rec["child_tag_counts"]:
            A("Kind-Elemente: " + ", ".join(
                "`<%s>` (%d×)" % (k, v) for k, v in rec["child_tag_counts"].items()))
            A("")
        if rec["child_attrs"]:
            A("| Kind-Attribut | n | min | max | distinct |")
            A("|---|---|---|---|---|")
            for k, st in sorted(rec["child_attrs"].items()):
                A("| `%s` | %d | %s | %s | %d |" % (
                    k, st["n"], fmt(st["min"]), fmt(st["max"]), st["distinct"]))
            A("")

    # --- Kreuztabelle
    A("## 4. Kreuztabelle: Preset → FX-Kette (Kern von F2)")
    A("")
    A("### 4.1 Verteilung der Kettenformen")
    A("")
    A("| Modulreihenfolge | Presets |")
    A("|---|---|")
    for s in report["crosstab"]["chain_shapes"]:
        A("| `%s` | %d |" % (s["shape"], s["count"]))
    A("")
    A("### 4.2 Modultyp je Kettenposition")
    A("")
    A("| Position | Typen (Anzahl) |")
    A("|---|---|")
    for pos, types in sorted(report["crosstab"]["position_type_counts"].items(),
                             key=lambda kv: int(kv[0])):
        A("| %s | %s |" % (pos, ", ".join(
            "`%s` (%d)" % (t, n) for t, n in types.items())))
    A("")
    A("### 4.3 Details je Preset")
    A("")
    A("Vollstaendig im JSON (`crosstab.rows`). Kompakt hier:")
    A("")
    A("| Preset | Kette | p12 je Slot |")
    A("|---|---|---|")
    for r in report["crosstab"]["rows"]:
        p12 = ", ".join(fmt(s["p"][12], 3) for s in r["slots"])
        A("| `%s` | `%s` | %s |" % (r["preset_key"], r["chain_shape"], p12))
    A("")

    # --- Bridge
    A("## 5. Kandidaten: Modulattribut → `fxN_pM`")
    A("")
    A("Verfahren: fuer jede Kombination (Modultyp, Attribut, p-Index) werden alle "
      "Beobachtungen aus den 44 Preset-Snapshots gesammelt. Ein Kandidat gilt als "
      "**exakt reproduzierend**, wenn eine lineare **oder** logarithmische Abbildung "
      "alle Punkte mit Toleranz 0.0015 trifft.")
    A("")
    A("| Modultyp | Attribut | n | distinct(real) | Kandidaten (p, Trefferquote, Modell) |")
    A("|---|---|---|---|---|")
    for key, rec in sorted(report["bridge_exact"].items()):
        cands = []
        for m in rec["matches"][:3]:
            if m["hit_frac"] > 0:
                cands.append("p%d: %.0f%% %s" % (m["p"], m["hit_frac"] * 100, m["model"]))
        if not cands:
            cands = ["keine"]
        A("| `%s` | `%s` | %d | %d | %s |" % (
            rec["type"], rec["attr"], rec["n"], rec["real_distinct"], "; ".join(cands)))
    A("")

    # --- Kurvenhypothesen
    A("## 6. Normalisierungskurven: Pruefungen")
    A("")
    A("### 6.1 Verhaeltnis-Hypothesen")
    A("")
    for t in report["ratio_tests"]:
        A("**`%s`**" % t["param"])
        A("")
        A("```json")
        A(json.dumps(t, indent=2, ensure_ascii=False))
        A("```")
        A("")
    A("### 6.2 Wertebereichs-/Symmetriepruefung")
    A("")
    A("| Parameter | n | min | max | median | distinct | voll 0..1 | Median=0.5 | Anteil 0.5 | Symmetrie um 0.5 |")
    A("|---|---|---|---|---|---|---|---|---|---|")
    for pid, e in report["curve_hypotheses"].items():
        A("| `%s` | %d | %s | %s | %s | %d | %s | %s | %s | %s |" % (
            pid, e["n"], fmt(e["min"]), fmt(e["max"]), fmt(e["median"]),
            e["distinct"], "ja" if e["covers_full_range"] else "nein",
            "ja" if e["centered_on_half"] else "nein",
            e["share_at_0.5"], fmt(e.get("symmetry_about_0.5"), 4)))
    A("")
    A("---")
    A("")
    A("## 7. Rohdaten")
    A("")
    A("Vollstaendige Auswertung: `_private/mining/report.json` (nicht Teil des "
      "oeffentlichen Repos).")
    A("")
    return "\n".join(md)


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="Preset-Datamining (read-only)")
    ap.add_argument("--kick-dir", default=str(KICK_DIR))
    ap.add_argument("--fx-dir", default=str(FXCHAIN_DIR))
    ap.add_argument("--json-only", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)

    kick_dir = Path(args.kick_dir)
    fx_dir = Path(args.fx_dir)

    # ---- Kick-Presets
    kick_files = sorted(kick_dir.rglob("*.itkbpreset"))
    if not kick_files:
        print("FEHLER: keine .itkbpreset unter %s" % kick_dir, file=sys.stderr)
        return 2

    presets = []
    for i, f in enumerate(kick_files):
        try:
            pr = read_kick_preset(f)
        except ET.ParseError as e:
            print("WARNUNG: %s nicht parsebar: %s" % (f.name, e), file=sys.stderr)
            continue
        # anonymisierter Schluessel (kein Presetname im Report)
        pr["preset_key"] = "preset_%02d" % (i + 1)
        presets.append(pr)

    # ---- FX-Chains
    fx_files = sorted(fx_dir.rglob("*.xml"))
    fx_chains = {}
    for i, f in enumerate(fx_files):
        try:
            ch = read_fx_chain_file(f)
        except ET.ParseError as e:
            print("WARNUNG: %s nicht parsebar: %s" % (f.name, e), file=sys.stderr)
            continue
        if ch is not None:
            fx_chains["chain_%02d" % (i + 1)] = ch

    # ---- Snapshots aus den Presets
    snaps = {}
    for pr in presets:
        if pr["modules"]:
            snaps[pr["preset_key"]] = {"modules": pr["modules"], "bypassAll": None}

    print("Presets: %d   FX-Chains: %d   Presets mit FX-Snapshot: %d"
          % (len(presets), len(fx_chains), len(snaps)))

    # ---- Auswertungen
    param_stats = analyse_params(presets)
    rollup = group_rollup(param_stats)
    fx_inv = analyse_fx_attrs(fx_chains, snaps)
    crosstab = analyse_crosstab(presets)
    bridge_exact = analyse_bridge_exact(crosstab)
    curves = hypothesise_curves(param_stats, presets)
    ratios = check_simple_ratios(param_stats)

    slots_per_preset = [len(p["slots"]) for p in presets]

    report = {
        "meta": {
            "generated": "2026-11-17",
            "n_presets": len(presets),
            "n_fxchains": len(fx_chains),
            "n_presets_with_fxsnap": len(snaps),
            "slot_count_max": SLOT_COUNT,
            "fx_slots": FX_SLOTS,
            "fx_params": FX_PARAMS,
            "tool": "tools/mine_presets.py",
        },
        "slots_per_preset": {
            "min": min(slots_per_preset) if slots_per_preset else 0,
            "max": max(slots_per_preset) if slots_per_preset else 0,
            "histogram": dict(sorted(Counter(slots_per_preset).items())),
        },
        "role_counts": dict(Counter(
            s["role"] for p in presets for s in p["slots"]
        ).most_common()),
        "param_stats": param_stats,
        "param_rollup": rollup,
        "fx_attr_inventory": fx_inv,
        "crosstab": crosstab,
        "bridge_exact": bridge_exact,
        "curve_hypotheses": curves,
        "ratio_tests": ratios,
    }

    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_JSON, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1, ensure_ascii=False, sort_keys=False)
    print("JSON  -> %s (%.1f kB)" % (OUT_JSON, OUT_JSON.stat().st_size / 1024.0))

    if not args.json_only:
        OUT_MD.parent.mkdir(parents=True, exist_ok=True)
        md = write_markdown(report)
        with open(OUT_MD, "w", encoding="utf-8") as fh:
            fh.write(md)
        print("MD    -> %s (%.1f kB)" % (OUT_MD, OUT_MD.stat().st_size / 1024.0))

    # ---- Konsolen-Zusammenfassung
    print()
    print("=== Parameter mit voller 0..1-Abdeckung ===")
    full = [k for k, e in curves.items() if e["covers_full_range"]]
    print("  %d von %d geprueften Parametern" % (len(full), len(curves)))
    print()
    print("=== Top-Bruecken-Kandidaten (Trefferquote 100%%) ===")
    exact = []
    for key, rec in bridge_exact.items():
        for m in rec["matches"]:
            if m["hit_frac"] >= 0.999 and m["n"] >= 4:
                exact.append((rec["type"], rec["attr"], m["p"], m["model"], m["n"]))
                break
    for t, a, p, model, n in sorted(exact):
        print("  %-7s %-10s -> p%-2d  (%s, n=%d)" % (t, a, p, model, n))
    print()
    print("=== fx-Parameter, die in allen Presets konstant sind ===")
    for m in range(FX_PARAMS):
        pid = "fx0_p%d" % m
        st = param_stats.get(pid)
        if st and st["distinct"] == 1:
            print("  p%-2d konstant = %s" % (m, fmt(st["min"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
