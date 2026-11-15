# Alpha-decay observatory

A reproducible alpha-decay demonstration connecting a barrier calculation, published empirical estimates and a seeded population simulation. The default is Po-212; selected ground-state channels from Sm, Po, Rn and Ra and custom nuclei are supported.

## Run

Install Python 3 and Docker with Compose, then:

```sh
python -m unittest discover -s tests -p 'test_*.py' -v
docker compose up --build -d
python -m unittest discover -s tests -p 'integration.py' -v
python -m observatory
python -m observatory --isotope Ra-224
python -m observatory --z 84 --a 212 --q 8.9542 --binding 1655.7716856
python -m observatory --validate
# When finished:
docker compose down
```

Open the interactive 3D viewer at [http://127.0.0.1:8090](http://127.0.0.1:8090); see [viewer documentation](services/viewer/README.md). The Python CLI has no third-party dependencies; `--help` describes options, including `--model baseline`, `--population`, `--seed`, and `--url`. For custom inputs, `--binding` is optional total parent nuclear binding energy in MeV. Calculations retry transient startup failures for up to 60 seconds per request. A fixed seed reproduces the sample for a given implementation, but random streams are not promised across versions.

The central output contains all available predictions. By default survival uses the shell estimate when available, otherwise Qi's estimate, otherwise the WKB baseline. `selected_model` identifies which was used. Simulation times are multiples of the selected predicted half-life (clamped for extreme values). The ±0.1 MeV sweep is a sensitivity illustration, not an experimental uncertainty or fitted confidence interval. For a custom Q the sweep must remain within the kernel's allowed energy range.

All predictions concern the specified **ground-state channel**. Its partial half-life is the lifetime this channel would have if acting alone. The survival curve assumes that single constant-rate channel, independent nuclei and no production; where competing channels exist it is not the physical total-parent survival curve, which requires the sum of all decay rates. This is not a detector simulation.

## Benchmark and sources

[observatory/isotopes.json](observatory/isotopes.json) is the canonical catalog. Q values and most parent half-lives are from [Cai et al. (2020), Table VII](https://doi.org/10.1103/PhysRevC.101.054304), using their underlying evaluated data; ground-state branch fractions are recorded separately with source links. Partial half-life equals total half-life divided by the absolute ground-state alpha branch fraction. The Ra-220 fraction is an approximation from the 1% excited-state branch in Cai Table VIII; Sm and Po-212 branches are approximated as 100%. Po-212 uses the later [Belli et al. (2021)](https://doi.org/10.1140/epja/s10050-021-00510-y) 295.1 ns measurement. Sm-146 uses the [2024 direct measurement](https://www.nature.com/articles/s41598-024-64104-6) of 92.0 Ma, in place of the older disputed value. Uncertainties and the small approximation in branch fractions are not propagated into the reported errors.

Total parent binding energies are AME2020 binding energy per nucleon multiplied by A, in MeV, from the [official unrounded mass table](https://www-nds.iaea.org/amdc/ame2020/mass_1.mas20.txt); the selected entries are not marked extrapolated there. The Q values are total transition energies, not emitted alpha kinetic energies.

`python -m observatory --validate` reports each log10(prediction/partial-half-life) and mean absolute and root-mean-square errors in log10 seconds, with denominators, for all available estimates. [observatory/validation.json](observatory/validation.json) records a reproducible run. Positive error means the predicted half-life is too long. WKB uses l=0, radius 9 fm, preformation 0.1 and assault frequency 10²¹ s⁻¹ for every channel, illustrative assumptions rather than fitted parameters.

The research estimate uses the alpha-only universal decay law of [Qi et al.](https://doi.org/10.1103/PhysRevLett.103.072501), fitted to 139 even-even alpha transitions with Z=78–108. Sm (Z=62) and custom values outside that range are extrapolations. The shell estimate implements [Ren et al. (2026)](https://cpc.ihep.ac.cn/article/doi/10.1088/1674-1137/ae167b), a global fit to 550 alpha decays using measured binding energies. Both are empirical, and their original fitting data may overlap these catalog nuclei: this comparison is not an independent out-of-sample validation. No coefficients are fitted by this project. Neither estimate is available for odd parent Z or A, or nonzero l. The simplified WKB baseline omits nuclear structure, deformation and uncertainty; models can be worse for individual nuclei even when aggregate errors improve.

Compose builds kernel, ensemble and gateway services. The gateway exposes `POST /v1/barrier` and `POST /v1/ensemble`; each service has `GET /health`. See [services documentation](services/README.md) for contracts and domain limits. The viewer is illustrative, not a new decay theory or first simulation.
