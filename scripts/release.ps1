<#
.SYNOPSIS
    Stamp a built installer's size and SHA-256 into the update manifest.

.DESCRIPTION
    Run this after compiling the NSIS installer and before uploading. It:

      - checks the staged dr2.exe really is the build you think it is, by
        reading its Build Number string rather than trusting the file name;
      - checks CurrentVersion in the manifest matches;
      - computes the installer's length and SHA-256;
      - writes both into the Patch entry that points at that installer.

    The hash is what stops a 1.459 client running an installer that is not the
    one you published - verification fails closed, so a stale or missing hash
    means clients refuse the update rather than take it on trust. That is also
    why this is a script: the values have to be recomputed every single time the
    installer is rebuilt, and doing it by hand is how they end up stale.

    See docs/update-system.md.

.EXAMPLE
    .\scripts\release.ps1                  # stamp the current release (145900)
    .\scripts\release.ps1 -Version 146000
    .\scripts\release.ps1 -WhatIf          # report only, write nothing
#>
[CmdletBinding()]
param(
    # Release identity, i.e. the version with its separators removed.
    [string]$Version = '145900',

    # Report what would change without writing the manifest.
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$StageDir = Join-Path $RepoRoot "resources\DR2 Online\DR2 Patch $Version"
$Manifest = Join-Path $StageDir 'updates.cfg'
$Payload = Join-Path $StageDir 'dr2.exe'
$Installer = Join-Path $StageDir "dr2_$Version.exe"

function Get-BuildNumber {
    # The identity lives in a custom VERSIONINFO string, which
    # System.Diagnostics.FileVersionInfo does not expose - read it out of the
    # resource directly. See docs/update-system.md section 1.
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $text = [System.Text.Encoding]::Unicode.GetString($bytes)
    $key = 'Build Number'
    $i = $text.IndexOf($key)
    if ($i -lt 0) { return $null }

    $rest = $text.Substring($i + $key.Length).TrimStart([char]0)
    $end = $rest.IndexOf([char]0)
    if ($end -lt 0) { return $null }
    return $rest.Substring(0, $end)
}

foreach ($p in @($StageDir, $Manifest, $Payload, $Installer)) {
    if (-not (Test-Path $p)) { throw "Not found: $p" }
}

# --- the staged payload is the build we claim it is -------------------------
$stamped = Get-BuildNumber -Path $Payload
if (-not $stamped) { throw "Could not read a Build Number from $Payload" }
if ($stamped -ne $Version) {
    throw "Staged dr2.exe reports build $stamped, not $Version. It is a stale copy - restage it before publishing."
}
Write-Host "payload    dr2.exe reports $stamped" -ForegroundColor DarkGray

# --- the manifest agrees ----------------------------------------------------
$text = [System.IO.File]::ReadAllText($Manifest)
if ($text -notmatch "CurrentVersion\(\s*$Version\s*,") {
    throw "CurrentVersion in updates.cfg is not $Version. Fix it before stamping."
}

# --- measure the installer --------------------------------------------------
$length = (Get-Item $Installer).Length
$hash = (Get-FileHash $Installer -Algorithm SHA256).Hash.ToLower()
Write-Host "installer  $length bytes" -ForegroundColor DarkGray
Write-Host "sha256     $hash" -ForegroundColor DarkGray

# --- rewrite the Patch entry that points at this installer ------------------
# Anchored on the File line rather than on position, so reordering the manifest
# - which is load-bearing for patch selection - cannot stamp the wrong entry.
$file = "downloads/dr2_$Version.exe"
$blocks = [regex]::Matches($text, '(?s)Patch\s*\([^)]*\)\s*\{.*?\}')
$target = $null
foreach ($b in $blocks) {
    if ($b.Value -match [regex]::Escape("File(`"$file`");")) { $target = $b; break }
}
if (-not $target) { throw "No Patch entry in updates.cfg references $file" }

$updated = $target.Value
$updated = [regex]::Replace($updated, 'Size\(\s*\d+\s*\);', "Size($length);")

if ($updated -match 'Hash\(\s*"[^"]*"\s*\);') {
    $updated = [regex]::Replace($updated, 'Hash\(\s*"[^"]*"\s*\);', "Hash(`"$hash`");")
} else {
    # No Hash line yet - add one immediately after Size, matching its indent.
    $updated = [regex]::Replace($updated, '(?m)^(\s*)Size\((\d+)\);', "`$1Size(`$2);`r`n`$1Hash(`"$hash`");")
}

if ($updated -eq $target.Value) {
    Write-Host "manifest   already up to date" -ForegroundColor Green
    return
}

if ($WhatIf) {
    Write-Host "`n-WhatIf: would write this entry:`n" -ForegroundColor Yellow
    Write-Host $updated
    return
}

$text = $text.Remove($target.Index, $target.Length).Insert($target.Index, $updated)

# No BOM: the manifest is parsed by the game's own config reader, which expects
# to find a function name at the very first byte.
[System.IO.File]::WriteAllText($Manifest, $text, (New-Object System.Text.UTF8Encoding $false))

Write-Host "manifest   stamped $([System.IO.Path]::GetFileName($Manifest))" -ForegroundColor Green
Write-Host "`nNext: upload dr2_$Version.exe and updates.cfg to /motd/darkreign2/downloads/,"
Write-Host "then apply Caddyfile.transition so 1.458 clients can reach them."
