# SPDX-License-Identifier: GPL-3.0-or-later
#
# Install the Windows builds of the Ostrich instruments where hosts look for them, for all
# users of the machine:
#
#   VST3        C:\Program Files\Common Files\VST3\<Name>.vst3
#   CLAP        C:\Program Files\Common Files\CLAP\<Name>.clap
#   standalone  C:\Program Files\Ostrich-not-Emu\<Name>.exe
#
# The builds come from the build host over SSH (plugin/ostrich/bin-win of each instrument),
# or with -From from a local checkout that has them.  Writing to Program Files needs
# administrator rights: the script fetches as the user, then runs its copy step once more
# with them (one UAC prompt).  Copies an earlier version put into the user's own folders
# (%LOCALAPPDATA%\Programs\Common\VST3, ...\CLAP, ...\Programs\Ostrich-not-Emu) are
# removed afterwards, so that a host does not find the plugin twice.
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
    [switch]$DryRun,
    [string]$CopyList = ""          # internal: the copy step, run with administrator rights
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$ProgramFiles = if ($env:ProgramW6432) { $env:ProgramW6432 } else { $env:ProgramFiles }
$Vst3Dir = Join-Path $ProgramFiles "Common Files\VST3"
$ClapDir = Join-Path $ProgramFiles "Common Files\CLAP"
$ExeDir  = Join-Path $ProgramFiles "Ostrich-not-Emu"
# Where versions before 13.11.2026 installed, per user.
$UserDirs = @{
    vst3 = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
    clap = Join-Path $env:LOCALAPPDATA "Programs\Common\CLAP"
    exe  = Join-Path $env:LOCALAPPDATA "Programs\Ostrich-not-Emu"
}

function Remove-Aside([string]$dst) {
    # Old copies moved aside by earlier runs; those still loaded stay until next time.
    Get-ChildItem -Path (Split-Path -Parent $dst) -Filter ((Split-Path -Leaf $dst) + ".old-*") -ErrorAction SilentlyContinue |
        ForEach-Object { try { Remove-Item -Recurse -Force $_.FullName -ErrorAction Stop } catch {} }
}

function Install-Item([string]$src, [string]$dst) {
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
        catch { Write-Output "  (old copy still in use, removed next time)" }
    } else {
        Move-Item $new $dst
    }
    Write-Output "  $dst"
}

# ---- the copy step: "<src>`t<dst>" per line; what it did goes to <list>.log ---------------
if ($CopyList -ne "") {
    $ok = $true
    $out = foreach ($line in Get-Content $CopyList) {
        $src, $dst = $line -split "`t"
        try { Install-Item $src $dst }
        catch { Write-Output "  FAILED $dst`: $($_.Exception.Message)"; $ok = $false }
    }
    $out | Set-Content -Encoding utf8 "$CopyList.log"
    if ($ok) { exit 0 } else { exit 1 }
}

# ---- fetch, as the user -----------------------------------------------------------------
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

$Stage = Join-Path $env:TEMP "ostrich-not-emu-install"
New-Item -ItemType Directory -Force $Stage | Out-Null
$copies = @()       # "<src>`t<dst>"
$installed = @()    # @{ kind; file } of what goes in, for the old per-user copies
$failed = @()
foreach ($slug in $Slugs) {
    $name = Get-PluginName $slug
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
    foreach ($k in @(@{ kind = "vst3"; file = "$name.vst3"; dir = $Vst3Dir; container = $true },
                     @{ kind = "clap"; file = "$name.clap"; dir = $ClapDir; container = $false },
                     @{ kind = "exe";  file = "$name.exe";  dir = $ExeDir;  container = $false })) {
        $src = Join-Path $bin $k.file
        $type = if ($k.container) { "Container" } else { "Leaf" }
        if (Test-Path $src -PathType $type) {
            $copies += "$src`t$(Join-Path $k.dir $k.file)"
            $installed += @{ kind = $k.kind; file = $k.file }
            $n++
        }
    }
    if ($n -eq 0) {
        Write-Host "${slug} ($name): no Windows build found (make win on the build host)"
        $failed += $slug
    }
}

if ($DryRun) {
    foreach ($c in $copies) { Write-Host "  would install $(($c -split "`t")[1])" }
    foreach ($i in $installed) {
        $old = Join-Path $UserDirs[$i.kind] $i.file
        if (Test-Path $old) { Write-Host "  would remove the old per-user copy $old" }
    }
    if ($failed.Count -gt 0) { exit 1 } else { exit 0 }
}
if ($copies.Count -eq 0) { exit 1 }

# ---- copy, with administrator rights ----------------------------------------------------
$list = Join-Path $Stage "copy-list.txt"
$copies | Set-Content -Encoding utf8 $list
if (Test-Path "$list.log") { Remove-Item -Force "$list.log" }
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if ($admin) {
    & $PSCommandPath -CopyList $list
    $code = $LASTEXITCODE
} else {
    Write-Host "Installing to $ProgramFiles needs administrator rights (Windows asks once)."
    try {
        $p = Start-Process -FilePath "powershell.exe" -Verb RunAs -Wait -PassThru -WindowStyle Hidden `
            -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`"", "-CopyList", "`"$list`"")
        $code = $p.ExitCode
    } catch {
        Write-Host "Not installed: administrator rights were refused."
        exit 1
    }
}
if (Test-Path "$list.log") { Get-Content "$list.log" | Write-Host }
if ($code -ne 0) { Write-Host "Some copies failed."; exit 1 }

# ---- the old per-user copies ------------------------------------------------------------
foreach ($i in $installed) {
    $old = Join-Path $UserDirs[$i.kind] $i.file
    if (-not (Test-Path $old)) { continue }
    try {
        Remove-Item -Recurse -Force $old -ErrorAction Stop
        Write-Host "  removed the old per-user copy $old"
    } catch {
        Write-Host "  could not remove the old per-user copy $old (in use? close the DAW, run again)"
    }
}
if ($failed.Count -gt 0) { exit 1 }
