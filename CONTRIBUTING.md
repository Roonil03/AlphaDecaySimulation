# Contributing

Thanks for helping improve Alpha Decay Planetarium. For a proposed change, explain the problem, the intended result and how you checked it. Keep each pull request focused enough to review. The [README](README.md) introduces the application and links to its model notes.

## Develop and check

The [installation guide](install.md) lists prerequisites and platform-specific commands. From the repository root, configure, build and run the native test suite:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

On Windows with a multi-configuration generator, `-C Release` selects the built configuration for tests. CMake fetches pinned dependencies on first configuration, and the default build needs Rust/Cargo for Numerov. The [Tests workflow](.github/workflows/ci.yml) runs these commands on Windows and Ubuntu and checks packaged executables. Run checks relevant to your change and report their results, including any you could not run. See [research tools](research/README.md) for optional component-specific checks.
