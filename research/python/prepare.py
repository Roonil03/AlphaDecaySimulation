"""Convert a pre-split CSV to a versioned calibration dataset."""
import argparse
import csv
import json
from pathlib import Path
import tempfile
from fit import load_data

def prepare(source, version, citation):
    if not version or not citation:
        raise ValueError("dataset version and source citation are required")
    with open(source, newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))
    cases = [{"id": row["id"], "split": row["split"],
              "log10_half_life_s": float(row["log10_half_life_s"]),
              "request": {"parent_z": int(row["parent_z"]), "parent_a": int(row["parent_a"]),
                          "q_mev": float(row["q_mev"]), "l": int(row["l"]),
                          "radius_fm": float(row["radius_fm"])}} for row in rows]
    return {"schema_version": 1, "dataset_version": version, "source": citation, "cases": cases}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv")
    parser.add_argument("--version", required=True)
    parser.add_argument("--source", required=True, help="citation or source URL")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    data = prepare(args.csv, args.version, args.source)
    with tempfile.TemporaryDirectory() as temp:
        check = Path(temp) / "dataset.json"
        check.write_text(json.dumps(data), encoding="utf-8")
        load_data(check)
    Path(args.output).write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
