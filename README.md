# Alpha Decay Planetarium

Explore alpha-decay barrier estimates in a desktop application. The models use illustrative assumptions; their output is not an experimental uncertainty estimate.

## Download and run

Download the `AlphaDecay-Windows` or `AlphaDecay-Linux` artifact from a successful [Tests workflow run](https://github.com/Roonil03/AlphaDecaySimulation/actions/workflows/ci.yml). You need access to this repository and its workflow artifacts. On Windows, extract the artifact and run `AlphaDecay.exe`. On Linux, extract the artifact, then extract `AlphaDecay-linux.tar.gz` and run `./AlphaDecay`. The Linux binary targets Ubuntu 24.04 x86-64 with glibc 2.39 or newer, X11 and OpenGL 3.3. Both viewers need an OpenGL 3.3 graphics driver. See [installation](install.md) for source builds and platform details.

Choose an isotope in the local 3D viewer and calculate its decay estimate. Drag the scene to orbit and scroll to zoom. The population view draws independently sampled nuclei with illustrative positions and sizes. **Single nucleus / barrier view** shows the inner boundary in blue and the outer turning radius as a gold wireframe. Deformed shapes show the nuclear surface; their barrier and turning surfaces are schematic. The potential plot and half-life change with the selected model and inputs. Treat the result as a model calculation, not a measured lifetime.

## Command-line example

The same C++ core also accepts one JSON [structured text] request through standard input. For example, in PowerShell from the folder containing the Windows executable:

```powershell
'{"schema_version":1,"parent_z":84,"parent_a":212,"q_mev":8.9542,"l":0,"radius_fm":9.0,"preformation":0.1,"assault_hz":1e21}' | .\AlphaDecay.exe --run
```

The response includes `log10_half_life_s`, the base-10 logarithm of the estimated half-life in seconds. An invalid request returns a JSON error on standard error and a nonzero exit status. `AlphaDecay --self-test` checks embedded reference calculations. The [native guide](native/README.md) describes the schema [request format], limits and status values.

## Models and sources

The viewer and command line share the native core. The external Coulomb plus centrifugal WKB [semiclassical tunneling approximation] model, finite spherical well and Woods-Saxon models are described in the [external model notes](docs/external-model.md) and [spherical model notes](docs/spherical-models.md), with equations and research sources. See [deformed channels](docs/deformed-channels.md) for orientation and daughter-channel assumptions and the [solver notes](rust/numerov/README.md) for Numerov comparisons and parameter uncertainty. Qi and Ren estimates are separate empirical comparisons. Agreement with selected measurements does not establish independent validation. Catalog measured values are alpha partial half-lives where stated.

The optional [research tools and setup guide](research/README.md) cover a scheduler, campaign supervisor, calibration tools and proofs. Go workers and Python analysis tools call the native executable for physics predictions; the Lean proofs concern model properties. The [Research tools workflow](https://github.com/Roonil03/AlphaDecaySimulation/actions/workflows/research.yml) provides `AlphaDecay-research-Windows` and `AlphaDecay-research-Linux` bundles. The [Python compatibility client instructions](install.md#optional-research-and-python-compatibility-tools) show catalog comparisons with `--validate` and a separate two-channel comparison with `--validate-holdout`. These compare the illustrative WKB baseline, Qi's empirical law and Ren's shell-corrected estimate. The [holdout assumptions and sources](observatory/HOLDOUT.md) explain why exclusion from Ren's half-life fit is conditional on its published selection; this tiny sample does not establish general accuracy or independence from Qi's fit.

See [contributing](CONTRIBUTING.md) to propose changes, [security](SECURITY.md) to report a vulnerability, and [credits](CREDITS.md) for dependency notices.
