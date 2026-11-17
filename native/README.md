# Native Alpha Decay Planetarium

The C++20 core is the authoritative native implementation of the spherical external Coulomb plus centrifugal WKB model and the Qi and Ren comparisons. The GLFW/OpenGL 3.3/ImGui application calls that same library; it needs no server or adjacent data files. The legacy services remain available during the migration.

Build with CMake 3.21+, a C++20 compiler, Git and the platform OpenGL development libraries. CMake fetches GLFW 3.4, Dear ImGui 1.91.8 and nlohmann/json 3.11.3 at the immutable commits recorded in `CMakeLists.txt`. On Linux, X11 and its Xrandr, Xinerama, Xcursor and Xi development packages are needed. On Windows use a Visual Studio C++ toolchain.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run `AlphaDecay` for the viewer, `AlphaDecay --self-test` for embedded numerical reference checks, or `AlphaDecay --render-preview preview.png Dy-154 1050 700` to render a deterministic frame. The preview requires a working OpenGL display (for example Xvfb with Mesa on Linux). All catalog and fixture data are embedded at build time.

`AlphaDecay --run` reads a single JSON request from standard input and writes a single JSON response to standard output. Errors are JSON on standard error and return a nonzero exit status. Example:

```json
{"schema_version":1,"parent_z":84,"parent_a":212,"q_mev":8.9542,"l":0,"radius_fm":9.0,"preformation":0.1,"assault_hz":1e21,"research_model":true,"parent_binding_mev":1655.7716856}
```

The result contains `schema_version`, `log10_half_life_s`, `log10_penetrability`, and `outer_turning_fm`. `research_log10_half_life_s` and `shell_log10_half_life_s` are present only when applicable. Unknown fields and unsupported versions are rejected. See the kernel README for model definitions, bounds and limitations. Measured partial half-lives apply only to an exact catalog Z/A/Q/l match. The viewer's population illustrates independent survival for one channel, excluding competing branches. The models do not estimate prediction uncertainty.

The fields `schema_version`, `parent_z`, `parent_a`, and `l` require JSON integers within the C++ `int` range; floating-point values, including `1.0`, are rejected before conversion. The supported physical bounds still apply.

Run `AlphaDecay --credits` to display the embedded dependency license notices, including the ProggyClean font copyright and MIT license. Release packages also include `CREDITS.md`. Linux executables require system OpenGL and X11 runtime libraries. Dear ImGui's built-in font is compiled into the executable.

<<<<<<< HEAD
Numerov is built by default and requires Rust/Cargo 1.75 or later. Configure with `-DALPHA_ENABLE_NUMEROV=OFF` only for builds that do not need the independent solver. Run the registered `numerov-abi` and `numerov-rust` CTest cases to verify the C interface and solver.
=======
## Spherical potential alternatives

The default `potential_model` is `external`, the original Coulomb plus physical
centrifugal exterior model. Set it to `square_well` or `woods_saxon` to use a
finite charged sphere and an attractive nuclear potential. For example:

```json
{"schema_version":1,"parent_z":52,"parent_a":104,"q_mev":5.23,"l":0,"radius_fm":9,"preformation":0.1,"assault_hz":1e21,"potential_model":"woods_saxon","nuclear_depth_mev":100,"nuclear_radius_fm":9,"diffuseness_fm":0.65,"charge_radius_fm":9,"centrifugal_convention":"physical"}
```

The nuclear depth defaults to 100 MeV, the nuclear and charge radii default
to `radius_fm`, and diffuseness defaults to 0.65 fm. These are adjustable
illustrative assumptions, not fits. `centrifugal_convention` is `physical`
(default) or `langer_wkb`; the latter is a named WKB approximation. Alternative
responses contain `status`, `message`, `action`, inner and outer turning points,
`log10_penetrability` and `log10_half_life_s`; unavailable numbers are JSON
null. `zero_preformation` has a null half-life, and `closed_barrier` reports
zero Q. The solver reports `above_barrier`, `no_pocket`, `unsupported_topology`
or `numerical_error` instead of inventing a tunneling lifetime. For regular
inputs `external_reference` contains the original external-barrier calculation;
its Qi/Ren estimates are independent comparisons. With zero preformation or Q,
the external reference is omitted. Unsupported parameter values return a JSON
error and nonzero exit status. The viewer exposes the same potential parameters
and playback uses the selected model; sources and historical limits are in the
expandable provenance section.

The spherical solver supports nuclear well depths `0 < nuclear_depth_mev <=
10000` MeV. Larger depths are rejected before topology classification: their
Woods-Saxon tails can extend beyond the validated nuclear-region scan. This
numerical limit is not a fitted physical constraint. Both CLI and viewer accept
Q = 0 and 0 < Q < 0.01 MeV for the spherical models; the external, Qi and Ren
comparisons are unavailable there because those inputs lie outside the legacy
model's supported energy domain.

For deterministic screenshots, append an optional `external`, `square_well`,
or `woods_saxon` argument after the preview width and height. See
[the scientific notes](../docs/spherical-models.md) for model assumptions,
Te-104 source distinctions and numerical scope.
>>>>>>> potentials


For a spherical `--run` request, optional `numerov` accepts `lead_radius_fm`, `match_radius_fm`, and `step_fm`. The response gives convergence status, transmission, the step and exterior differences in decades, and the difference from WKB. The physical centrifugal term is used even if WKB uses Langer. A half-life is shown only for a converged calculation and uses the request's same preformation and assault frequency. The constant inner lead and its radius are explicit model assumptions; this transmission is not a resonance width. Unsupported or unconverged Numerov cases retain the WKB response.

Optional `uncertainty` accepts nonnegative integer `seed`, integer `sample_count` (1 to 1,000,000), four `standard_deviations`, or a symmetric positive-semidefinite 4-by-4 `covariance` array. Parameter order is Q, nuclear depth, nuclear radius, diffuseness, in the units above. Supplying no covariance assumes independence and is reported. Quantiles describe only the supplied parameter distribution, conditional on valid accepted draws, not experimental, model, or numerical uncertainty. No experimental uncertainty is inferred automatically. The viewer exposes these inputs and runs analysis in a cancellable background job. Numerical bounds and reference fixtures are documented in [the solver notes](../rust/numerov/README.md).
