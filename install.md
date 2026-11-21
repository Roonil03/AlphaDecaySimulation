# Installation and building

## Windows

Download `AlphaDecay-Windows` from a successful [Tests workflow](https://github.com/Roonil03/AlphaDecaySimulation/actions/workflows/ci.yml) run and extract the artifact [the files saved by that run]. You need access to this private repository and its workflow artifacts. Run `AlphaDecay.exe` directly; the native executable includes its catalog, report data and dependency notices. A working OpenGL 3.3 graphics driver is required for the viewer. `AlphaDecay.exe --self-test` verifies embedded numerical cases and `AlphaDecay.exe --credits` prints embedded notices.

For a source build install Git, CMake 3.21+, Rust/Cargo 1.75+ and Visual Studio with the C++ desktop workload, then:

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
.\build\Release\AlphaDecay.exe
```

## Linux

Download `AlphaDecay-Linux` from a successful [Tests workflow](https://github.com/Roonil03/AlphaDecaySimulation/actions/workflows/ci.yml) run. Its `AlphaDecay-linux.tar.gz` archive preserves the executable permission when extracted. The tested binary target is Ubuntu 24.04 x86-64 with glibc 2.39 or newer, X11, and OpenGL 3.3. Install X11/OpenGL runtime libraries and a suitable GPU or Mesa driver. In the extracted directory:

```sh
tar -xzf AlphaDecay-linux.tar.gz
./AlphaDecay --self-test
./AlphaDecay
```

For an Ubuntu 24.04 source build, install Git, CMake, a C++20 compiler, Rust/Cargo 1.75+ and graphics development libraries:

```sh
sudo apt-get install git cmake g++ libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/AlphaDecay
```

CMake fetches the pinned GLFW, Dear ImGui and nlohmann/json revisions on first configuration. The default build includes the Rust Numerov solver; `-DALPHA_ENABLE_NUMEROV=OFF` disables it. A graphical session is required to show the viewer; a headless `--run` or `--self-test` needs no display. An Xvfb display with Mesa can run `--render-preview` in a headless environment.

## Optional research and Python compatibility tools

Download the matching `AlphaDecay-research-Windows` or `AlphaDecay-research-Linux` artifact from the [Research tools workflow](https://github.com/Roonil03/AlphaDecaySimulation/actions/workflows/research.yml), or use the source tree. It contains the Go scheduler and research sources, but the native executable is distributed separately. Follow [research/README.md](research/README.md) for Python virtual environments, Elixir, Lean, examples and data provenance. To run the compatibility CLI from the repository root:

```sh
python -m observatory --core /absolute/path/to/AlphaDecay
python -m observatory --core /absolute/path/to/AlphaDecay --validate
python -m observatory --core /absolute/path/to/AlphaDecay --validate-holdout
```

On Windows use the full path to `AlphaDecay.exe`, or set `ALPHADECAY_CORE` to that path. `--validate` compares the 13-channel benchmark; `--validate-holdout` compares two channels separately. Both report the illustrative WKB baseline, Qi's empirical law and Ren's shell-corrected estimate. As the [holdout notes](observatory/HOLDOUT.md) explain, exclusion from Ren's half-life fit is conditional on its published selection. The tiny sample does not establish general accuracy or independence from Qi's fit, and its metrics do not propagate input uncertainties. The CLI's seeded survival samples describe independent decays for one channel. Domain limits and scientific caveats are documented in [native/README.md](native/README.md) and [the model notes](docs/external-model.md).
