<#
.SYNOPSIS
  Authenticode-signs files with signtool (SHA-256 digest + RFC 3161 timestamp).
.DESCRIPTION
  Certificate source, first match wins:
    1. -PfxPath / $env:WINDOWS_CERT_PFX_PATH
    2. $env:WINDOWS_CERT_PFX_BASE64 (decoded to a temp file, removed afterwards)
    3. -Thumbprint / $env:WINDOWS_CERT_THUMBPRINT (certificate already in the store,
       e.g. an EV token / cloud HSM exposed as a CSP)
  Password: $env:WINDOWS_CERT_PASSWORD (never a command-line argument in logs).
  Works for .exe/.dll/.msi and for .msix/.appx (the MSIX manifest Publisher must equal
  the certificate subject exactly).
.EXAMPLE
  ./packaging/windows/sign.ps1 -Files build\msvc-release\App\aedst-desktop.exe
.NOTES
  UNVERIFIED: never run against a real certificate in the authoring environment.
#>
param(
  [Parameter(Mandatory = $true)][string[]]$Files,
  [string]$PfxPath = $env:WINDOWS_CERT_PFX_PATH,
  [string]$Thumbprint = $env:WINDOWS_CERT_THUMBPRINT,
  [string]$TimestampUrl = $(if ($env:WINDOWS_TIMESTAMP_URL) { $env:WINDOWS_TIMESTAMP_URL } else { 'http://timestamp.digicert.com' }),
  [string]$Description = 'AEDST'
)
$ErrorActionPreference = 'Stop'

function Find-SignTool {
  $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
  $found = Get-ChildItem -Path $kits -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\' } | Sort-Object FullName -Descending | Select-Object -First 1
  if ($found) { return $found.FullName }
  throw 'signtool.exe not found (install the Windows SDK or run from a VS Developer prompt).'
}

$signtool = Find-SignTool
$tempPfx = $null
try {
  $certArgs = @()
  if (-not $PfxPath -and $env:WINDOWS_CERT_PFX_BASE64) {
    $tempDir = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
    $tempPfx = Join-Path $tempDir "aedst-sign-$([guid]::NewGuid().ToString('N')).pfx"
    [IO.File]::WriteAllBytes($tempPfx, [Convert]::FromBase64String($env:WINDOWS_CERT_PFX_BASE64))
    $PfxPath = $tempPfx
  }
  if ($PfxPath) {
    $certArgs += @('/f', $PfxPath)
    if ($env:WINDOWS_CERT_PASSWORD) { $certArgs += @('/p', $env:WINDOWS_CERT_PASSWORD) }
  } elseif ($Thumbprint) {
    $certArgs += @('/sha1', $Thumbprint)
  } else {
    throw 'No certificate: set WINDOWS_CERT_PFX_BASE64, WINDOWS_CERT_PFX_PATH or WINDOWS_CERT_THUMBPRINT.'
  }

  foreach ($f in $Files) {
    if (-not (Test-Path $f)) { throw "File not found: $f" }
    Write-Host "Signing $f"
    & $signtool sign /fd sha256 /tr $TimestampUrl /td sha256 /d $Description @certArgs $f
    if ($LASTEXITCODE -ne 0) { throw "signtool sign failed for $f ($LASTEXITCODE)" }
    & $signtool verify /pa $f
    if ($LASTEXITCODE -ne 0) { throw "signtool verify failed for $f ($LASTEXITCODE)" }
  }
}
finally {
  if ($tempPfx -and (Test-Path $tempPfx)) { Remove-Item -Force $tempPfx }
}
