# Windows installation

## Run the executable

Use 64-bit Windows with .NET Framework 4.x and WPF enabled (normally included with Windows). The application is one executable with the isotope catalog and both scientific comparison reports embedded; no adjacent data files, browser, network connection, Docker, Python, or running service are needed. Download the `AlphaDecay-windows` artifact from a successful [Tests workflow run](https://github.com/Roonil03/AlphaDecaySimulation/actions/workflows/ci.yml) for the desired commit, extract it, and double-click `AlphaDecay.exe`. Access to the private repository is required to download its artifacts. Alternatively, from PowerShell in the extracted folder:

```powershell
.\AlphaDecay.exe
```

Select an isotope or enter a custom channel and choose **Calculate channel**. Drag to orbit, scroll to zoom, and use the playback controls and time slider to explore the population. The visualization and model predictions are illustrative; see the scientific limitations in [README.md](README.md).

To inspect a holdout offline, choose **Dy-154** or **Os-186** in **Reference isotope**, then select **Ren shell** under **Playback model**. The measured-reference panel pairs the measured partial half-life with its uncertainty and identifies the holdout. These two channels are reported separately from the 13 legacy comparison channels. See [the holdout provenance and limitations](observatory/HOLDOUT.md); exclusion from Ren's half-life fit does not establish independence from all empirical inputs or from Qi's fit.

## Build from source

On 64-bit Windows, PowerShell, the .NET Framework 4.x compiler and WPF reference assemblies are required. The compiler is expected at `%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe`, with WPF assemblies in its `WPF` subdirectory. From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\desktop\build.ps1 -OutputPath .\dist\AlphaDecay.exe
.\dist\AlphaDecay.exe
```

The default output is `dist\AlphaDecay.exe`. The build embeds `observatory/isotopes.json`, `observatory/validation.json` and `observatory/holdout-validation.json`; rebuild after those data change. Build outputs are not source files and should not be committed.

## Verify

The executable runs an offline calculation self-test without opening its window. In PowerShell, wait for the Windows GUI process and check its exit code:

```powershell
$p = Start-Process -FilePath .\dist\AlphaDecay.exe -ArgumentList '--self-test' -Wait -PassThru
$p.ExitCode
```

A result of `0` means the self-test passed; a nonzero result means it failed. For a downloaded artifact, replace the path with the extracted executable's path. The self-test reads the embedded catalog and both reports, compares all 15 channels against the offline numerical implementation, and checks WPF reference and playback transitions under invariant and comma-decimal display cultures. These checks do not establish general scientific accuracy. CI downloads the uploaded single-EXE artifact into a directory containing only that executable and runs the same self-test there.

The optional service-backed CLI retains `python -m observatory --validate` for the 13 legacy channels and `python -m observatory --validate-holdout` for the two holdouts. It is separate from the offline executable and requires the services described in [README.md](README.md).

## Troubleshooting

- If Windows reports a missing or disabled .NET Framework, enable or install .NET Framework 4.x including WPF, then retry.
- If the build cannot find `csc.exe` or WPF assemblies, check the paths above and use a 64-bit Windows installation with those components available.
- If the executable does not start, extract it from the downloaded archive first and check Windows security prompts and system requirements. Download artifacts only from a trusted run.
- If a calculation is rejected, check the input ranges in [the kernel documentation](services/kernel/README.md); the domain bounds do not guarantee physical applicability.
