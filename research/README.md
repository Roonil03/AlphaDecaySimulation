# Research tools

These local tools call the native executable's version 1 `--run` JSON interface. They do not implement a second physics solver.

## Batch scheduler

Build with `cd research/go && go build -o scheduler .`. Run `scheduler -core /absolute/path/to/alpha-core -workers 4 -timeout 30s < ../astro/campaign.json > results.json`. Inputs contain a `cases` array with unique IDs and native requests. Output is an ordered array of IDs with either a native `response` or an `error`. A timeout or interrupt terminates active child processes. Individual failures remain in the result array; inspect them before using the data.

## Durable campaign runner (Linux)

Requires Elixir 1.14+, Erlang, `flock`, and a compiled Go scheduler. In `research/elixir`, run `mix deps.get`, then `sh campaign.sh /absolute/campaign-dir /absolute/manifest.json /absolute/scheduler /absolute/alpha-core`. Only one runner owns a campaign directory; the kernel releases its lock when the process exits. Pause by creating `campaign-dir/paused`, and resume by deleting that file; a running case finishes before pausing. Restart the same command after interruption. Successful results are committed by atomic rename under `results/`, and `events.jsonl` records starts, successes, failures and completion. The runner reconstructs a missing success event if interrupted between result commit and journal append. Inputs cannot change on resume; use a new directory for a different manifest. A failed case stops the runner and is retried on restart. Results are not a substitute for provenance or scientific validation.

## Analysis

Create a Python 3.10+ virtual environment, install `pip install -r research/python/requirements.txt`, and run `python research/python/fit.py research/python/data/example-v1.json --core /absolute/alpha-core --output fit.json --plot fit.png`. The sample is illustrative. Replace it with a versioned, sourced dataset and predeclare calibration/validation membership. The analysis fits only a common preformation factor in (0,1], holding assault frequency at 1e21 Hz, and calls the native core at unit preformation for every case. Validation cases do not affect the fitted factor. `python -m unittest` from `research/python` tests the split and normalization.

## Proofs and design

In `research/lean`, `lake exe cache get && lake build` uses the pinned Lean/mathlib versions. Proofs concern mathematical conditions, not the accuracy of floating-point calculations or nuclear-model assumptions. The Astro language sketch and separate runnable campaign are described in `astro/README.md`.
