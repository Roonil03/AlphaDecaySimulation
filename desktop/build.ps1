param([string]$OutputPath = (Join-Path $PSScriptRoot 'AlphaDecayPlanetarium.exe'))
$ErrorActionPreference = 'Stop'
$framework = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$csc = Join-Path $framework 'csc.exe'
$wpf = Join-Path $framework 'WPF'
$catalog = Join-Path $PSScriptRoot '..\observatory\isotopes.json'
$validation = Join-Path $PSScriptRoot '..\observatory\validation.json'
$holdout = Join-Path $PSScriptRoot '..\observatory\holdout-validation.json'
foreach ($dataPath in @($catalog, $validation, $holdout)) {
    if (!(Test-Path -LiteralPath $dataPath)) { throw "Canonical data is missing: $dataPath" }
}
$destination = [IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
$compilerArguments = @(
    '/nologo', '/target:winexe', '/optimize+', '/platform:anycpu', "/out:$destination",
    "/resource:$catalog,Isotopes.json",
    "/resource:$validation,Validation.json",
    "/resource:$holdout,HoldoutValidation.json",
    "/reference:$wpf\PresentationCore.dll",
    "/reference:$wpf\PresentationFramework.dll",
    "/reference:$wpf\WindowsBase.dll",
    "/reference:$framework\System.Xaml.dll",
    "/reference:$framework\System.Web.Extensions.dll",
    (Join-Path $PSScriptRoot 'Planetarium.cs')
)
& $csc @compilerArguments
if ($LASTEXITCODE -ne 0) { throw "Compiler failed ($LASTEXITCODE)." }
Write-Host "Built $destination"
