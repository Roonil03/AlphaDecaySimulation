"""Fit one common preformation factor using the native core as prediction authority."""
import argparse
import csv
import json
import math
import subprocess
from pathlib import Path

ASSAULT_HZ = 1e21  # fixed normalization; only preformation is fitted


def load_data(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not data.get("dataset_version"):
        raise ValueError("versioned dataset schema required")
    rows = data["cases"]
    ids = [row["id"] for row in rows]
    if len(ids) != len(set(ids)) or not all(ids):
        raise ValueError("case IDs must be unique and nonempty")
    for row in rows:
        if row["split"] not in ("calibration", "validation"):
            raise ValueError("split must be calibration or validation")
        if not math.isfinite(row["log10_half_life_s"]):
            raise ValueError("observed log half-life must be finite")
    if not any(row["split"] == "calibration" for row in rows):
        raise ValueError("at least one calibration case required")
    return data


def native_prediction(core, request):
    request = dict(request, schema_version=1, assault_hz=ASSAULT_HZ, preformation=1.0)
    proc = subprocess.run([core, "--run"], input=json.dumps(request), text=True,
                          capture_output=True, check=True, timeout=30)
    result = json.loads(proc.stdout)
    value = result["log10_half_life_s"]
    if not math.isfinite(value):
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
