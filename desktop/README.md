# Alpha Decay Planetarium

A native, offline Windows WPF app. It embeds the canonical `observatory/isotopes.json` at build time; rebuilding after catalog changes updates the executable. It does not use a server or download dependencies at runtime.

From the repository root, build on Windows with .NET Framework 4.x and its WPF reference assemblies:

```powershell
powershell -ExecutionPolicy Bypass -File desktop/build.ps1
```

The output is `desktop/AlphaDecayPlanetarium.exe`. Use `-OutputPath C:\path\AlphaDecay.exe` to choose another destination. Launch the EXE normally. The runtime requires Windows and .NET Framework 4.x with WPF.

The app supports `AlphaDecayPlanetarium.exe --self-test` (exit code 0 on success, nonzero on failure). It loads the embedded catalog, legacy report and holdout report, compares all 15 channels with the offline physics, checks the kernel's published Qi and Ren fixtures and analytic Coulomb integrals, and exercises the actual WPF reference, input invalidation and playback state transitions. Both Dy-154 and Os-186 holdouts and their uncertainty metadata are required. No adjacent data files are read at runtime.

`AlphaDecayPlanetarium.exe --render-preview path.png [catalog-id [width height]]` renders the actual WPF window and exits. For reproducible holdout and minimum-size checks use `--render-preview dy.png Dy-154` and `--render-preview os-small.png Os-186 1050 700`. Omit the ID to preview Po-212. Exit code 0 means success, 1 is a render failure, and 2 is an invalid ID or size.

Select a reference or edit the inputs and calculate. Drag the scene to orbit, scroll to zoom, or focus the scene and use arrow keys. Playback and the slider use selected-model half-lives; 180 independently sampled lifetimes use seed 42 and remain fixed when rewinding. Dots are illustrative population members, not particles within a single nucleus. The separate barrier view shows one nucleus and the specified boundary and outer turning point on a common scale; their radii have the physical ratio R/b. The simulated population positions and sphere sizes are illustrative.

Predictions use suitable physical time units; residuals are explicitly labeled Δ = log10(prediction/measured partial half-life). WKB uses the same sharp-boundary Coulomb plus centrifugal integral, constants, 4096-panel quadrature, preformation and assault convention as the canonical kernel. Qi and Ren are available only for even-even ℓ=0 inputs; Ren additionally requires supplied parent binding energy. Qi outside Z=78–108 extrapolates. Neither parity nor these numerical ranges establish that a channel is a ground-state decay or physically realistic. The published fits may overlap catalog entries. The baseline is a simplified external-barrier model, and none of the models gives a prediction uncertainty. Consult `services/kernel/README.md` for formulas, provenance and limitations.

Catalog selection restores the recorded transition's angular momentum (currently ℓ=0). Editing inputs clears prior reference, geometry and population output until recalculated. An unavailable selected model disables playback and clears the scene; selecting an available model recalculates. Only an exact Z/A/Q/ℓ match receives the catalog measurement. The population follows one specified channel; competing branches are excluded, so its survival is not generally the total parent survival.

Supported input bounds match the kernel: Z 3–130, A 5–400, A−Z ≥2, ℓ 0–100, Q 0.01–100 MeV, boundary 0.1–100 fm, preformation (0,1], assault frequency 1e-300–1e30 s⁻¹, and optional binding energy (0,10000] MeV. These bounds do not certify physical applicability.
