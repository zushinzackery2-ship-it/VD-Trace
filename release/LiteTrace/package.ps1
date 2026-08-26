<#
.SYNOPSIS
    Package a LiteTrace release from an already-built tree.

.DESCRIPTION
    Collects the built LiteTrace.dll plus the example LiteTrace.ini and README
    from this folder into dist\LiteTrace-v<Version>\, then zips it. Run
    build-and-package.bat for a one-click "configure + build + package", or run
    this script directly if bin\release\LiteTrace.dll is already built.

.PARAMETER Version
    Release version string used for the output folder / zip name. Default 0.1.0.

.PARAMETER Configuration
    CMake configuration whose output to package. Default Release.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File package.ps1 -Version 0.1.0
#>
[CmdletBinding()]
param(
    [string]$Version = "0.1.0",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

# release\LiteTrace\ -> repository root is two levels up.
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..")

$dllSource = Join-Path $repoRoot "bin\release\LiteTrace.dll"
if (-not (Test-Path $dllSource))
{
    Write-Error "LiteTrace.dll not found at $dllSource. Build it first (build-and-package.bat, or build_release.bat)."
}

$stageName = "LiteTrace-v$Version"
$distRoot = Join-Path $repoRoot "dist"
$stageDir = Join-Path $distRoot $stageName

if (Test-Path $stageDir)
{
    Remove-Item -Recurse -Force $stageDir
}
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Copy-Item $dllSource (Join-Path $stageDir "LiteTrace.dll")
Copy-Item (Join-Path $scriptDir "LiteTrace.ini") (Join-Path $stageDir "LiteTrace.ini")
Copy-Item (Join-Path $scriptDir "README.md") (Join-Path $stageDir "README.md")

# Copy the matching public core DLL when present (LiteTrace links the core
# statically, so it is optional, but handy for side-by-side diagnostics).
$coreDll = Join-Path $repoRoot "bin\release\VDTrace.dll"
if (Test-Path $coreDll)
{
    Copy-Item $coreDll (Join-Path $stageDir "VDTrace.dll")
}

$zipPath = Join-Path $distRoot "$stageName.zip"
if (Test-Path $zipPath)
{
    Remove-Item -Force $zipPath
}
Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath

Write-Host ""
Write-Host "LiteTrace release packaged:"
Write-Host "  folder : $stageDir"
Write-Host "  zip    : $zipPath"
Get-ChildItem $stageDir | ForEach-Object { Write-Host "    - $($_.Name)" }
