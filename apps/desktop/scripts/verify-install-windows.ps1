<#
.SYNOPSIS
  Install-verify for the NSIS installer on a CLEAN runner.
.DESCRIPTION
  Silent-installs to a temp directory, runs `aedst-desktop.exe --version`, checks the
  expected version, silent-uninstalls, and verifies the app files and Start Menu
  entries are gone while user data is kept (the uninstaller's delete-user-data prompt
  defaults to NO in silent mode).
  Requires main.cpp to handle `--version` (print AEDST_VERSION to stdout, exit 0, no
  window). The exe is a GUI-subsystem binary, so stdout is captured through a redirect.
.EXAMPLE
  ./scripts/verify-install-windows.ps1 -Installer build\msvc-release\packages\AEDST-0.1.0-win64.exe -Version 0.1.0
#>
param(
  [Parameter(Mandatory = $true)][string]$Installer,
  [Parameter(Mandatory = $true)][string]$Version,
  [string]$InstallDir = 'C:\aedst-verify'
)
$ErrorActionPreference = 'Stop'

function Fail($msg) { Write-Host "::error::$msg"; exit 1 }

$Installer = (Resolve-Path $Installer).Path
if (Test-Path $InstallDir) { Remove-Item -Recurse -Force $InstallDir }

# NSIS: /S silent, /D=<dir> must be the LAST argument and unquoted.
$p = Start-Process -FilePath $Installer -ArgumentList '/S', "/D=$InstallDir" -Wait -PassThru
if ($p.ExitCode -ne 0) { Fail "installer exited $($p.ExitCode)" }

$exe = Join-Path $InstallDir 'bin\aedst-desktop.exe'
if (-not (Test-Path $exe)) { Fail "$exe missing after install" }
$uninstaller = Join-Path $InstallDir 'Uninstall.exe'
if (-not (Test-Path $uninstaller)) { Fail "$uninstaller missing after install" }

# --version, output redirected (GUI-subsystem exe).
$outFile = Join-Path $env:TEMP 'aedst-version.txt'
$run = Start-Process -FilePath $exe -ArgumentList '--version' -PassThru -RedirectStandardOutput $outFile -WindowStyle Hidden
$null = $run.Handle
if (-not $run.WaitForExit(30000)) { $run.Kill(); Fail '--version did not exit within 30 s (does main.cpp handle --version before creating the GUI?)' }
if ($run.ExitCode -ne 0) { Fail "--version exited $($run.ExitCode)" }
$out = (Get-Content $outFile -Raw)
Write-Host "--version output: $out"
if ($out -notmatch [regex]::Escape($Version)) { Fail "--version output does not contain $Version" }

# User data must survive an uninstall by default (silent mode answers the prompt NO).
$userData = Join-Path $env:APPDATA 'AEDST'
New-Item -ItemType Directory -Force -Path $userData | Out-Null
Set-Content -Path (Join-Path $userData 'verify-marker.txt') -Value 'keep'

# `_?=<dir>` makes the uninstaller run in place and synchronously (otherwise it copies
# itself to %TEMP% and returns immediately); it then cannot delete itself or the dir.
$u = Start-Process -FilePath $uninstaller -ArgumentList '/S', "_?=$InstallDir" -Wait -PassThru
if ($u.ExitCode -ne 0) { Fail "uninstaller exited $($u.ExitCode)" }

if (Test-Path $exe) { Fail 'aedst-desktop.exe still present after uninstall' }
if (Test-Path (Join-Path $InstallDir 'bin')) { Fail 'bin\ still present after uninstall' }
foreach ($sm in @("$env:ProgramData\Microsoft\Windows\Start Menu\Programs\AEDST", "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\AEDST")) {
  if (Test-Path $sm) { Fail "Start Menu folder still present: $sm" }
}
if (-not (Test-Path (Join-Path $userData 'verify-marker.txt'))) { Fail 'user data was deleted although the option defaults to off' }

Remove-Item -Recurse -Force $userData -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $InstallDir -ErrorAction SilentlyContinue
Write-Host 'OK: install / --version / uninstall verified'
