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

Run `AlphaDecay --credits` to display the embedded dependency license notices. Linux executables require system OpenGL and X11 runtime libraries. Dear ImGui's built-in font is compiled into the executable.
