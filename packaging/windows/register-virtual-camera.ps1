# SPDX-License-Identifier: GPL-2.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ObsRuntimeRoot,

    [Parameter(Mandatory = $false)]
    [string]$VirtualCameraName = "OpenLiveBridge Virtual Camera",

    [Parameter(Mandatory = $false)]
    [string]$VirtualCameraClsid = "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}",

    [switch]$Unregister
)

$ErrorActionPreference = "Stop"

function Test-IsAdmin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Normalize-Clsid {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Clsid
    )

    $value = $Clsid.Trim()
    if ($value.StartsWith('{')) {
        $value = $value.Substring(1)
    }
    if ($value.EndsWith('}')) {
        $value = $value.Substring(0, $value.Length - 1)
    }

    if ($value -notmatch '^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$') {
        throw "Invalid CLSID: $Clsid"
    }

    return '{' + $value.ToLowerInvariant() + '}'
}

function Normalize-Path {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
}

function Get-RegistryString {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SubKey,

        [Parameter(Mandatory = $false)]
        [AllowEmptyString()]
        [string]$ValueName,

        [Parameter(Mandatory = $true)]
        [Microsoft.Win32.RegistryView]$RegistryView
    )

    $baseKey = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
        [Microsoft.Win32.RegistryHive]::LocalMachine,
        $RegistryView)

    try {
        $key = $baseKey.OpenSubKey($SubKey)
        if (-not $key) {
            return $null
        }

        try {
            $registryValueName = if ([string]::IsNullOrEmpty($ValueName)) { $null } else { $ValueName }
            $value = $key.GetValue($registryValueName, $null)
            if ($null -eq $value) {
                return $null
            }

            return [string]$value
        } finally {
            $key.Dispose()
        }
    } finally {
        $baseKey.Dispose()
    }
}

function Get-VirtualCameraRegistration {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Clsid,

        [Parameter(Mandatory = $true)]
        [Microsoft.Win32.RegistryView]$RegistryView
    )

    $normalizedClsid = Normalize-Clsid -Clsid $Clsid
    $comKey = "SOFTWARE\Classes\CLSID\$normalizedClsid"
    $inprocKey = "$comKey\InprocServer32"
    $categoryKey = "SOFTWARE\Classes\CLSID\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\Instance\$normalizedClsid"

    return [pscustomobject]@{
        Clsid = $normalizedClsid
        ComName = Get-RegistryString -SubKey $comKey -ValueName "" -RegistryView $RegistryView
        ModulePath = Get-RegistryString -SubKey $inprocKey -ValueName "" -RegistryView $RegistryView
        FriendlyName = Get-RegistryString -SubKey $categoryKey -ValueName "FriendlyName" -RegistryView $RegistryView
    }
}

function Assert-Registered {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Clsid,

        [Parameter(Mandatory = $true)]
        [string]$ViewLabel,

        [Parameter(Mandatory = $true)]
        [Microsoft.Win32.RegistryView]$RegistryView,

        [Parameter(Mandatory = $true)]
        [string]$ExpectedModulePath,

        [Parameter(Mandatory = $true)]
        [string]$ExpectedFriendlyName
    )

    $registration = Get-VirtualCameraRegistration -Clsid $Clsid -RegistryView $RegistryView
    if ([string]::IsNullOrWhiteSpace($registration.ComName) -or [string]::IsNullOrWhiteSpace($registration.ModulePath)) {
        throw "$ViewLabel virtual camera registration was not found after install: $($registration.Clsid)"
    }

    if ((Normalize-Path -Path $registration.ModulePath) -ne (Normalize-Path -Path $ExpectedModulePath)) {
        throw "Virtual camera module path mismatch. Expected '$ExpectedModulePath', got '$($registration.ModulePath)'"
    }

    if ([string]::IsNullOrWhiteSpace($registration.FriendlyName)) {
        throw "$ViewLabel virtual camera friendly name was not found after install: $($registration.Clsid)"
    }

    if ($registration.FriendlyName -ne $ExpectedFriendlyName) {
        throw "Virtual camera friendly name mismatch. Expected '$ExpectedFriendlyName', got '$($registration.FriendlyName)'"
    }

    return $registration
}

function Assert-Unregistered {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Clsid,

        [Parameter(Mandatory = $true)]
        [string]$ViewLabel,

        [Parameter(Mandatory = $true)]
        [Microsoft.Win32.RegistryView]$RegistryView
    )

    $registration = Get-VirtualCameraRegistration -Clsid $Clsid -RegistryView $RegistryView
    if (-not [string]::IsNullOrWhiteSpace($registration.ComName) -or
        -not [string]::IsNullOrWhiteSpace($registration.ModulePath) -or
        -not [string]::IsNullOrWhiteSpace($registration.FriendlyName)) {
        throw "$ViewLabel virtual camera registration still exists after uninstall: $($registration.Clsid)"
    }
}

function Invoke-RegSvr32 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RegSvr32Path,

        [Parameter(Mandatory = $true)]
        [string]$ModulePath,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $ModulePath)) {
        throw "$Label virtual camera module was not found: $ModulePath"
    }

    $arguments = if ($Unregister) {
        @("/u", "/s", $ModulePath)
    } else {
        @("/i", "/s", $ModulePath)
    }

    $action = if ($Unregister) { "Unregistering" } else { "Registering" }
    Write-Host "$action $Label virtual camera module: $ModulePath"
    $process = Start-Process -FilePath $RegSvr32Path -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne 0) {
        throw "regsvr32 failed for $Label module with exit code $($process.ExitCode)"
    }
}

function Get-RegSvr32Path {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("64-bit", "32-bit")]
        [string]$Bitness
    )

    $systemRoot = $env:windir
    if ($Bitness -eq "64-bit") {
        if ([IntPtr]::Size -eq 4 -and [Environment]::Is64BitOperatingSystem) {
            return Join-Path -Path $systemRoot -ChildPath "Sysnative\regsvr32.exe"
        }

        return Join-Path -Path $systemRoot -ChildPath "System32\regsvr32.exe"
    }

    if ([IntPtr]::Size -eq 4 -and [Environment]::Is64BitOperatingSystem) {
        return Join-Path -Path $systemRoot -ChildPath "System32\regsvr32.exe"
    }

    return Join-Path -Path $systemRoot -ChildPath "SysWOW64\regsvr32.exe"
}

if (-not (Test-IsAdmin)) {
    throw "Virtual camera registration requires administrator privileges."
}

if (-not [Environment]::Is64BitOperatingSystem) {
    throw "This script requires 64-bit Windows."
}

$runtimeRoot = Normalize-Path -Path $ObsRuntimeRoot
if (-not (Test-Path -LiteralPath $runtimeRoot)) {
    throw "OBS runtime root was not found: $runtimeRoot"
}

$moduleRoot = Join-Path -Path $runtimeRoot -ChildPath "data\obs-plugins\win-dshow"
$module32 = Join-Path -Path $moduleRoot -ChildPath "obs-virtualcam-module32.dll"
$module64 = Join-Path -Path $moduleRoot -ChildPath "obs-virtualcam-module64.dll"
$normalizedClsid = Normalize-Clsid -Clsid $VirtualCameraClsid
$normalizedName = $VirtualCameraName.Trim()

$regsvr64 = Get-RegSvr32Path -Bitness "64-bit"
$regsvr32 = Get-RegSvr32Path -Bitness "32-bit"

if ($Unregister) {
    Invoke-RegSvr32 -RegSvr32Path $regsvr64 -ModulePath $module64 -Label "64-bit"
    Invoke-RegSvr32 -RegSvr32Path $regsvr32 -ModulePath $module32 -Label "32-bit"
    Assert-Unregistered -Clsid $normalizedClsid -ViewLabel "64-bit" -RegistryView ([Microsoft.Win32.RegistryView]::Registry64)
    Assert-Unregistered -Clsid $normalizedClsid -ViewLabel "32-bit" -RegistryView ([Microsoft.Win32.RegistryView]::Registry32)
    Write-Host "Virtual camera unregistration completed."
    return
}

Invoke-RegSvr32 -RegSvr32Path $regsvr64 -ModulePath $module64 -Label "64-bit"
Invoke-RegSvr32 -RegSvr32Path $regsvr32 -ModulePath $module32 -Label "32-bit"
$registration = Assert-Registered -Clsid $normalizedClsid -ViewLabel "64-bit" -RegistryView ([Microsoft.Win32.RegistryView]::Registry64) -ExpectedModulePath $module64 -ExpectedFriendlyName $normalizedName
$registration32 = Assert-Registered -Clsid $normalizedClsid -ViewLabel "32-bit" -RegistryView ([Microsoft.Win32.RegistryView]::Registry32) -ExpectedModulePath $module32 -ExpectedFriendlyName $normalizedName

Write-Host "Virtual camera registration completed."
Write-Host ("  Name: " + $registration.FriendlyName)
Write-Host ("  CLSID: " + $registration.Clsid)
Write-Host ("  Module: " + $registration.ModulePath)
Write-Host ("  32-bit Module: " + $registration32.ModulePath)
