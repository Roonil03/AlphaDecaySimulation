# Windows installation

## Run the executable

Use 64-bit Windows with .NET Framework 4.x and WPF enabled (normally included with Windows). The application is one executable with the isotope catalog embedded; no browser, network connection, Docker, Python, or running service is needed. Download the `AlphaDecay-windows` artifact from a successful GitHub Actions run for the desired commit, extract it, and double-click `AlphaDecay.exe`. Alternatively, from PowerShell in the extracted folder:

```powershell
.\AlphaDecay.exe
```

Select an isotope or enter a custom channel and choose **Calculate channel**. Drag to orbit, scroll to zoom, and use the playback controls and time slider to explore the population. The visualization and model predictions are illustrative; see the scientific limitations in [README.md](README.md).

## Build from source

On 64-bit Windows, PowerShell, the .NET Framework 4.x compiler and WPF reference assemblies are required. The compiler is expected at `%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe`, with WPF assemblies in its `WPF` subdirectory. From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\desktop\build.ps1 -OutputPath .\dist\AlphaDecay.exe
.\dist\AlphaDecay.exe
```

The build embeds `observatory/isotopes.json`; rebuild after catalog changes. With no `-OutputPath`, the script writes `desktop\AlphaDecayPlanetarium.exe`. Build outputs are not source files and should not be committed.

## Verify

The executable runs an offline calculation self-test without opening its window. In PowerShell, wait for the Windows GUI process and check its exit code:

```powershell
$p = Start-Process -FilePath .\dist\AlphaDecay.exe -ArgumentList '--self-test' -Wait -PassThru
$p.ExitCode
```

A result of `0` means the self-test passed; a nonzero result means it failed. For a downloaded artifact, replace the path with the extracted executable's path. This checks selected calculations, not a complete scientific validation.

## Troubleshooting

- If Windows reports a missing or disabled .NET Framework, enable or install .NET Framework 4.x including WPF, then retry.
- If the build cannot find `csc.exe` or WPF assemblies, check the paths above and use a 64-bit Windows installation with those components available.
- If the executable does not start, extract it from the downloaded archive first and check Windows security prompts and system requirements. Download artifacts only from a trusted run.
- If a calculation is rejected, check the input ranges in [the kernel documentation](services/kernel/README.md); the domain bounds do not guarantee physical applicability.
