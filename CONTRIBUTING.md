# Contributing

For a proposed change, explain the problem, the intended result and how you checked it. Keep each pull request focused enough to review. The [README](README.md) introduces the application and links to its model notes.

## Develop and check

The [installation guide](install.md) lists prerequisites and platform-specific commands. From the repository root, configure, build and run the native test suite:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

On Windows with a multi-configuration generator, `-C Release` selects the built configuration for tests. CMake fetches pinned dependencies on first configuration, and the default build needs Rust/Cargo for Numerov. The [Tests workflow](.github/workflows/ci.yml) runs these commands on Windows and Ubuntu and checks packaged executables. Run checks relevant to your change and report their results, including any you could not run. See [research tools](research/README.md) for optional component-specific checks.

## Scientific changes

For a change to a model, data value or scientific claim, link the primary research paper, the original report of the work. Identify the equation, table or other precise location that supports each change. Give units, input ranges, assumptions and any conversion from the source. Distinguish a measured value, an upper limit and a theoretical prediction. If a paper alone does not establish a data value, also link the relevant evaluation or dataset and identify its entry.

Explain how you checked the change against an independent reference. Include reference cases, expected results, tolerances and sources. State limitations and cases outside the model's scope. Add or update tests and source notes where needed. Agreement for selected cases does not establish general physical accuracy. The [model notes](docs/spherical-models.md) show how to record sources and limitations.

For a code, documentation or other change that does not alter a scientific claim, cite the relevant issue or documentation instead. A research paper is not required for such a fix.

## Submit a pull request

Describe the behavior before and after the change. Link the issue or supporting sources, list the checks you ran, and identify any remaining limitations. Include representative inputs and outputs when they help a reviewer reproduce the result. Follow the [security reporting instructions](SECURITY.md) for vulnerabilities. Keep secrets and vulnerability details out of public issues and pull requests.
