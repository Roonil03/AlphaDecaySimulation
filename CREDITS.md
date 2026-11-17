# Third-party credits

The native executable embeds its dependency notices; run `AlphaDecay --credits` to read them. Dear ImGui's default ProggyClean font is included in the executable and has its own MIT notice, reproduced below and embedded in `--credits`.

ProggyClean.ttf
Copyright (c) 2004, 2005 Tristan Grimmer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## Historical web viewer

The retired web viewer used [ln](https://github.com/fogleman/ln), version `v0.0.0-20170223135521-12e6c6e74459` (commit `12e6c6e74459`), to construct spheres, perform 3D camera projection and hidden-line removal, and generate SVG polylines. The current desktop application does not include ln; its historical attribution and MIT license are retained below.

Copyright (C) 2016 Michael Fogleman

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Dependencies and related work

The native build pins [GLFW 3.4](https://github.com/glfw/glfw) (zlib license), [Dear ImGui 1.91.8](https://github.com/ocornut/imgui) (MIT), and [nlohmann/json 3.11.3](https://github.com/nlohmann/json) (MIT). GLFW's bundled OpenGL loader includes Khronos material; its notices and the ProggyClean font license are embedded in `AlphaDecay --credits`. Their exact source revisions are in `CMakeLists.txt`.

Optional research tooling uses Go 1.22 or newer (BSD-style), Elixir 1.14 or newer and Erlang/OTP (Apache-2.0), [Jason 1.4.5](https://github.com/michalmuskala/jason) (Apache-2.0), Python 3 (PSF license), and [Matplotlib 3.10.7](https://matplotlib.org/) (Matplotlib license). The proofs pin Lean 4.15.0 and Mathlib v4.15.0 (Apache-2.0); transitive proof and Elixir dependencies are pinned in their lock files. The Astro 0.1.15a sketch uses the syntax of the experimental [Astro language](https://github.com/astrolang/astro) (Apache-2.0); Astro is not a runtime dependency.

Related scientific and educational work includes [ALPHAD](https://github.com/IAEA-NSDDNetwork/ALPHAD), [AlphaHF](https://github.com/IAEA-NSDDNetwork/AlphaHF), HFBTHO and HFODD nuclear-structure programs, [PhET Alpha Decay](https://github.com/phetsims/alpha-decay), [NuDat](https://www.nndc.bnl.gov/nudat3/), and [radioactivedecay](https://github.com/radioactivedecay/radioactivedecay). These projects provide context and inspiration; they are not bundled as dependencies. Individual catalog and model references are linked in the source data and scientific notes.
