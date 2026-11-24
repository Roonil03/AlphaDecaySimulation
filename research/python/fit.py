"""Fit one common preformation factor using the native core as prediction authority."""
import argparse
import json
import math
import subprocess
from pathlib import Path

ASSAULT_HZ = 1e21  # fixed normalization; only preformation is fitted


def load_data(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if (not isinstance(data, dict) or type(data.get("schema_version")) is not int or
            data["schema_version"] != 1 or not isinstance(data.get("dataset_version"), str) or
            not data["dataset_version"].strip()):
        raise ValueError("versioned dataset schema required")
    rows = data.get("cases")
    if not isinstance(rows, list) or not all(isinstance(row, dict) for row in rows):
        raise ValueError("dataset cases must be an array of objects")
    ids = [row.get("id") for row in rows]
    if not all(isinstance(case_id, str) and case_id.strip() for case_id in ids) or len(ids) != len(set(ids)):
        raise ValueError("case IDs must be unique and nonempty")
    for row in rows:
        if row.get("split") not in ("calibration", "validation"):
            raise ValueError("split must be calibration or validation")
        value = row.get("log10_half_life_s")
        if type(value) not in (int, float) or not math.isfinite(value):
            raise ValueError("observed log half-life must be finite")
        if not isinstance(row.get("request"), dict):
            raise ValueError("case request must be an object")
    if not any(row["split"] == "calibration" for row in rows):
        raise ValueError("at least one calibration case required")
    return data


def native_prediction(core, request):
    allowed = {"schema_version", "parent_z", "parent_a", "q_mev", "l", "radius_fm",
               "preformation", "assault_hz", "research_model", "parent_binding_mev",
               "potential_model", "centrifugal_convention", "nuclear_depth_mev",
               "nuclear_radius_fm", "diffuseness_fm", "charge_radius_fm"}
    extra = set(request) - allowed
    if extra:
        raise ValueError("calibration supports only single spherical channels; unsupported fields: " + ", ".join(sorted(extra)))
    request = dict(request, schema_version=1, assault_hz=ASSAULT_HZ, preformation=1.0)
    proc = subprocess.run([core, "--run"], input=json.dumps(request), text=True,
                          capture_output=True, check=True, timeout=30)
    result = json.loads(proc.stdout)
    if not isinstance(result, dict) or type(result.get("schema_version")) is not int or result["schema_version"] != 1:
        raise ValueError("unsupported native result version")
    value = result.get("log10_half_life_s")
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError("native prediction must be finite")
    return value


def analyze(data, predict):
    rows = [dict(row, baseline_log10_half_life_s=predict(row["request"])) for row in data["cases"]]
    residuals = [r["log10_half_life_s"] - r["baseline_log10_half_life_s"]
                 for r in rows if r["split"] == "calibration"]
    # T = T_baseline / preformation; constrain the factor to (0, 1].
    shift = max(0.0, sum(residuals) / len(residuals))
    preformation = 10.0 ** (-shift)
    if preformation == 0:
        raise ValueError("fitted preformation underflows floating point")
    for row in rows:
        row["predicted_log10_half_life_s"] = row["baseline_log10_half_life_s"] + shift
        row["residual"] = row["log10_half_life_s"] - row["predicted_log10_half_life_s"]
    return {"schema_version": 1, "dataset_version": data["dataset_version"],
            **{key: data[key] for key in ("source", "description") if key in data},
            "assault_hz": ASSAULT_HZ, "preformation": preformation,
            "calibration_ids": [r["id"] for r in rows if r["split"] == "calibration"], "cases": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset"); parser.add_argument("--core", required=True)
    parser.add_argument("--output", required=True); parser.add_argument("--plot")
    args = parser.parse_args()
    result = analyze(load_data(args.dataset), lambda req: native_prediction(args.core, req))
    Path(args.output).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    if args.plot:
        import matplotlib.pyplot as plt
        for split in ("calibration", "validation"):
            rows = [r for r in result["cases"] if r["split"] == split]
            plt.scatter([r["log10_half_life_s"] for r in rows],
                        [r["predicted_log10_half_life_s"] for r in rows], label=split)
        plt.xlabel("Observed log10 half-life (s)"); plt.ylabel("Predicted log10 half-life (s)")
        plt.legend(); plt.tight_layout(); plt.savefig(args.plot); plt.close()

if __name__ == "__main__":
    main()

