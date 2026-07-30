# SPDX-License-Identifier: GPL-2.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ObsSourceRoot,

    [Parameter(Mandatory = $false)]
    [string]$VirtualCameraName = "OpenLiveBridge Virtual Camera"
)

$ErrorActionPreference = "Stop"

$targetFile = Get-ChildItem -Path $ObsSourceRoot -Recurse -File -Filter "virtualcam-module.cpp" |
    Select-Object -First 1

if (-not $targetFile) {
    throw "virtualcam-module.cpp was not found under $ObsSourceRoot"
}

$content = Get-Content -LiteralPath $targetFile.FullName -Raw
$content = $content -replace 'L"OBS Virtual Camera"', ('L"' + $VirtualCameraName + '"')
[System.IO.File]::WriteAllText(
    $targetFile.FullName,
    $content,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Updated virtual camera name in $($targetFile.FullName)"
