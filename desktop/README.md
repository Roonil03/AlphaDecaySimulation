# Alpha Decay Planetarium

A native, offline Windows WPF app. It embeds the canonical `observatory/isotopes.json` at build time; rebuilding after catalog changes updates the executable. It does not use a server or download dependencies at runtime.

From the repository root, build on Windows with .NET Framework 4.x and its WPF reference assemblies:

```powershell
powershell -ExecutionPolicy Bypass -File desktop/build.ps1
```

The output is `desktop/AlphaDecayPlanetarium.exe`. Use `-OutputPath C:\path\AlphaDecay.exe` to choose another destination. Launch the EXE normally. The runtime requires Windows and .NET Framework 4.x with WPF.

The app supports `AlphaDecayPlanetarium.exe --self-test` (exit code 0 on success, nonzero on failure). It checks the kernel's published Qi and Ren fixtures, analytic Coulomb integrals including the extreme endpoint, model eligibility, and barrier behavior. `AlphaDecayPlanetarium.exe --render-preview path.png` renders the actual initial WPF window to a PNG and exits; this can be used for visual smoke checks on a Windows desktop. Its exit code is nonzero on render failure.

Select a reference or edit the inputs and calculate. Drag the scene to orbit, scroll to zoom, or focus the scene and use arrow keys. Playback and the slider use selected-model half-lives; 180 independently sampled lifetimes use seed 42 and remain fixed when rewinding. Dots are illustrative population members, not particles within a single nucleus. The rings show the specified boundary and outer turning point on a common scale; their radii have the physical ratio R/b. The simulated positions and sphere sizes are illustrative.

Predictions and residuals are log10 seconds and log10(prediction/measured partial half-life), respectively. WKB uses the same sharp-boundary Coulomb plus centrifugal integral, constants, 4096-panel quadrature, preformation and assault convention as the canonical kernel. Qi and Ren are available only for even-even ℓ=0 inputs; Ren additionally requires supplied parent binding energy. Qi outside Z=78–108 extrapolates. Neither parity nor these numerical ranges establish that a channel is a ground-state decay or physically realistic. The published fits may overlap catalog entries. The baseline is a simplified external-barrier model, and none of the models gives a prediction uncertainty. Consult `services/kernel/README.md` for formulas, provenance and limitations.

Supported input bounds match the kernel: Z 3–130, A 5–400, A−Z ≥2, ℓ 0–100, Q 0.01–100 MeV, boundary 0.1–100 fm, preformation (0,1], assault frequency 1e-300–1e30 s⁻¹, and optional binding energy (0,10000] MeV. These bounds do not certify physical applicability.
