param(
    [Parameter(Mandatory=$true)][string]$CampaignDir,
    [Parameter(Mandatory=$true)][string]$Manifest,
    [Parameter(Mandatory=$true)][string]$Scheduler,
    [Parameter(Mandatory=$true)][string]$Core,
    [int]$Workers = 1
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $CampaignDir | Out-Null
$lockPath = Join-Path $CampaignDir 'owner.lock'
try {
    $lock = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
} catch { throw "Campaign is already owned or lock cannot be opened: $lockPath" }
try {
    & mix run run.exs $CampaignDir $Manifest $Scheduler $Core $Workers
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally { $lock.Dispose() }


