# SPDX-License-Identifier: GPL-3.0-or-later
#
# From Windows: build the Ostrich instruments on the build host and install the plugins on
# both sides, in one go.
#
#   tools\build.ps1 [-Instruments 32,55] [-LinuxOnly] [-NoInstall]
#
# 1. checks that the build host has the commit checked out here (push first otherwise:
#    git push kk2 main)
# 2. runs tools/build.sh there: Linux and Windows builds, Linux plugins installed there
# 3. runs tools\install-plugins.ps1 here: Windows plugins into C:\Program Files\Common Files\VST3
#    and ...\CLAP (one UAC prompt)

param(
    [string[]]$Instruments = @(),
    [switch]$LinuxOnly,
    [switch]$NoInstall,
    [string]$BuildHost = "kellerkind2",
    [string]$RemoteRoot = "/data/coding/Ostrich-not-Emu/ostrich-not-emu"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

$here  = (& git -C $Root rev-parse HEAD).Trim()
$there = (& ssh $BuildHost "git -C $RemoteRoot rev-parse HEAD").Trim()
if ($here -ne $there) {
    Write-Host "The build host is at $($there.Substring(0,7)), this checkout at $($here.Substring(0,7))."
    Write-Host "Push first (git push kk2 main), then run this again."
    exit 1
}

$buildArgs = @()
if ($LinuxOnly) { $buildArgs += "--linux-only" }
if ($NoInstall) { $buildArgs += "--no-install" }
$buildArgs += $Instruments
& ssh $BuildHost "$RemoteRoot/tools/build.sh $($buildArgs -join ' ')"
$buildOk = ($LASTEXITCODE -eq 0)

if (-not $NoInstall -and -not $LinuxOnly) {
    Write-Host "=== installing the Windows plugins"
    & (Join-Path $PSScriptRoot "install-plugins.ps1") -Instruments $Instruments -BuildHost $BuildHost -RemoteRoot $RemoteRoot
}
if (-not $buildOk) { Write-Host "Some builds failed; see above."; exit 1 }
