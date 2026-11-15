# Alpha-decay observatory

A small, reproducible research demonstration connecting a barrier calculation with a population survival simulation. A Po-212 benchmark shows the model's log discrepancy against a measured half-life, the change under a ±0.1 MeV energy sweep, and expected and seeded sampled survival for the central prediction. The sweep is an illustrative model sensitivity study, **not** the experimental uncertainty in Q or a fitted uncertainty interval.

## Run

Install Python 3 and Docker with the Compose plugin. From the repository root:

```sh
python -m venv .venv
# Windows PowerShell: .venv\Scripts\Activate.ps1
# macOS/Linux: source .venv/bin/activate
python -m unittest discover -s tests -p 'test_*.py' -v
docker compose up --build -d
python -m unittest discover -s tests -p 'integration.py' -v
python -m observatory
# When finished:
docker compose down
```

The client uses only Python's standard library; no global packages are required. It retries health checks and transient backend startup failures for up to 60 seconds per request. `python -m observatory --help` shows options for the gateway URL, initial population (1–100,000) and signed 64-bit integer random seed. JSON output records all inputs, the seed, energy sweep, measured half-life, log10(model/measurement), times in seconds, and expected and sampled survivors. A fixed seed reproduces the sample for a given implementation; random streams are not promised across implementations or versions. The integration check exercises the direct services and the gateway, a Coulomb turning-point reference, energy monotonicity, the independent exponential survival formula, and repeatability.

Compose builds `services/kernel`, `services/ensemble` and `services/gateway`. Their host ports 8081, 8082 and 8080 bind only to loopback; the gateway reaches the other services over the Compose network. Each offers `GET /health`. The gateway routes `POST /v1/barrier` and `POST /v1/ensemble`. The former takes `parent_z`, `parent_a`, `q_mev`, `l`, `radius_fm`, `preformation`, and `assault_hz`, and returns log10 half-life (seconds), log10 penetrability, and outer turning point (fm). The latter takes `log10_half_life_s`, sorted nonnegative `times_s`, positive integer `population`, and integer `seed`, returning expected and sampled survivor arrays.

## Benchmark and interpretation

The ground-state Po-212 → Pb-208 + α example uses Qα = 8.95420 MeV from the [2020 ENSDF evaluation](https://www.nndc.bnl.gov/ensnds/212/Po/adopted.pdf), which quotes 8.95420(11) MeV. This is the total decay energy, not the emitted alpha's kinetic energy. We compare with the 295.1(4) ns experimental result of [Belli et al. (2021)](https://doi.org/10.1140/epja/s10050-021-00510-y). These are independently sourced values; the former evaluation predates the latter measurement. The example uses l=0, radius 9 fm, preformation 0.1, and assault frequency 10²¹ s⁻¹ as transparent illustrative assumptions, not fitted or independently measured Po-212 parameters. Changes in these choices affect the comparison. `log10_ratio_to_measured` is zero for exact agreement, positive for a longer predicted half-life and negative for a shorter one. The survival calculation assumes independent, constant-rate decays with no production or competing channels; it does not represent a detector measurement.

The barrier is a simplified semiclassical model, not a precision nuclear-structure calculation. In particular, a fixed preformation factor and sharp radius omit nuclear structure, deformation and their uncertainty; the Q sweep varies only energy and does not propagate correlated parameter uncertainties. Recent work such as [Qi et al. (2026), arXiv preprint](https://arxiv.org/abs/2511.14705v2), studies structure-dependent preformation and deformation. Alpha-decay numerical tools predate this project, for example [PRESCOLD (1997)](https://doi.org/10.17632/8yshfryyyz.1). This project's distinctive contribution is the compact diagnostic workflow pairing a measured benchmark and energy sensitivity with a reproducible survival demonstration, not a claim of a new decay theory or first simulation.
