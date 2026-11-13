# SPDX-License-Identifier: GPL-3.0-or-later
#
# Install the Windows builds of the Ostrich instruments where hosts look for them, for the
# current user (no administrator rights needed):
#
#   VST3        %LOCALAPPDATA%\Programs\Common\VST3\<Name>.vst3
#   CLAP        %LOCALAPPDATA%\Programs\Common\CLAP\<Name>.clap
#   standalone  %LOCALAPPDATA%\Programs\Ostrich-not-Emu\<Name>.exe
#
# The builds come from the build host over SSH (plugin/ostrich/bin-win of each instrument),
# or with -From from a local checkout that has them.
#
#   tools\install-plugins.ps1 [-Instruments 32,55] [-From <checkout>] [-DryRun]
#
# A copy a running DAW has loaded cannot be overwritten on Windows, but it can be renamed:
# the old copy is moved aside, the new one put in place, and the old one removed when it is
# no longer in use (at the latest by the next run). The DAW sees the new one after a
# restart or rescan.

param(
    [string[]]$Instruments = @(),
    [string]$From = "",
    [string]$BuildHost = "kellerkind2",
    [string]$RemoteRoot = "/data/coding/Ostrich-not-Emu/ostrich-not-emu",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Vst3Dir = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
$ClapDir = Join-Path $env:LOCALAPPDATA "Programs\Common\CLAP"
$ExeDir  = Join-Path $env:LOCALAPPDATA "Programs\Ostrich-not-Emu"

if ($Instruments.Count -eq 0) {
    $Slugs = Get-ChildItem -Directory (Join-Path $Root "instruments") | ForEach-Object { $_.Name }
} else {
    $Slugs = $Instruments | ForEach-Object { "ostrich-" + ($_ -replace '^ostrich-', '') }
}

function Get-PluginName([string]$slug) {
    $mk = Join-Path $Root "instruments\$slug\plugin\ostrich\Makefile"
    if (-not (Test-Path $mk)) { throw "${slug}: no such instrument" }
    $line = Select-String -Path $mk -Pattern '^NAME\s*=\s*(\S+)' | Select-Object -First 1
    return $line.Matches[0].Groups[1].Value
}

function Remove-Aside([string]$dst) {
    # Old copies moved aside by earlier runs; those still loaded stay until next time.
    Get-ChildItem -Path (Split-Path -Parent $dst) -Filter ((Split-Path -Leaf $dst) + ".old-*") -ErrorAction SilentlyContinue |
        ForEach-Object { try { Remove-Item -Recurse -Force $_.FullName -ErrorAction Stop } catch {} }
}

function Install-Item([string]$src, [string]$dst) {
    if ($DryRun) { Write-Host "  would install $dst"; return }
    New-Item -ItemType Directory -Force (Split-Path -Parent $dst) | Out-Null
    Remove-Aside $dst
    $new = "$dst.new"
    if (Test-Path $new) { Remove-Item -Recurse -Force $new }
    Copy-Item -Recurse $src $new
    if (Test-Path $dst) {
        $aside = "$dst.old-" + (Get-Date -Format "yyyyMMddHHmmss")
        Rename-Item $dst (Split-Path -Leaf $aside)
        Move-Item $new $dst
        try { Remove-Item -Recurse -Force $aside -ErrorAction Stop }
        catch { Write-Host "  (old copy still in use, removed next time)" }
    } else {
        Move-Item $new $dst
    }
    Write-Host "  $dst"
}

$Stage = Join-Path $env:TEMP "ostrich-not-emu-install"
$failed = @()
foreach ($slug in $Slugs) {
    $name = Get-PluginName $slug
    Write-Host "$slug ($name):"
    if ($From -ne "") {
        $bin = Join-Path $From "instruments\$slug\plugin\ostrich\bin-win"
    } else {
        $bin = Join-Path $Stage $slug
        if (Test-Path $bin) { Remove-Item -Recurse -Force $bin }
        New-Item -ItemType Directory -Force $bin | Out-Null
        $remote = "${BuildHost}:$RemoteRoot/instruments/$slug/plugin/ostrich/bin-win/"
        foreach ($f in @("$name.vst3", "$name.clap", "$name.exe")) {
            & scp -q -r -B "$remote$f" "$bin\" 2>$null
        }
    }
    $n = 0
    if (Test-Path "$bin\$name.vst3" -PathType Container) { Install-Item "$bin\$name.vst3" (Join-Path $Vst3Dir "$name.vst3"); $n++ }
    if (Test-Path "$bin\$name.clap" -PathType Leaf)      { Install-Item "$bin\$name.clap" (Join-Path $ClapDir "$name.clap"); $n++ }
    if (Test-Path "$bin\$name.exe"  -PathType Leaf)      { Install-Item "$bin\$name.exe"  (Join-Path $ExeDir  "$name.exe");  $n++ }
    if ($n -eq 0) {
        Write-Host "  no Windows build found (make win on the build host)"
        $failed += $slug
    }
}
if ($failed.Count -gt 0) { exit 1 }
