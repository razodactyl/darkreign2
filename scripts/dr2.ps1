<#
.SYNOPSIS
    Build and run Dark Reign 2 without the Visual Studio IDE.

.EXAMPLE
    .\scripts\dr2.ps1 build            # build Debug
    .\scripts\dr2.ps1 build -Config Release
    .\scripts\dr2.ps1 build -Target util
    .\scripts\dr2.ps1 rebuild
    .\scripts\dr2.ps1 clean
    .\scripts\dr2.ps1 run              # build, then launch the game
    .\scripts\dr2.ps1 launch           # launch without building
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'rebuild', 'clean', 'run', 'launch')]
    [string]$Action = 'build',

    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',

    # MSBuild target: a project name (e.g. util, graphics) or blank for the whole solution.
    [string]$Target = '',

    # Command line passed to the game when launching.
    [string]$GameArgs = '--borderless -s',

    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Solution = Join-Path $RepoRoot 'dr2.sln'

function Find-MSBuild {
    # The projects require PlatformToolset v145, which only ships with VS 18
    # (2026) - hence -prerelease, and hence 2022 Build Tools will not do.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere - is Visual Studio installed?"
    }

    $found = & $vswhere -prerelease -latest `
        -requires Microsoft.Component.MSBuild `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1

    if (-not $found) { throw 'No Visual Studio install with MSBuild and the C++ toolset was found.' }
    return $found
}

function Invoke-Build {
    param([string]$MSBuildTarget)

    $msbuild = Find-MSBuild
    if (-not $Quiet) { Write-Host "MSBuild: $msbuild" -ForegroundColor DarkGray }

    $msbuildArgs = @(
        $Solution
        "-p:Configuration=$Config"
        '-p:Platform=Win32'
        '-m'
        '-nologo'
        ('-v:' + $(if ($Quiet) { 'quiet' } else { 'minimal' }))
    )
    if ($MSBuildTarget) { $msbuildArgs += "-t:$MSBuildTarget" }

    & $msbuild @msbuildArgs
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }
}

function Get-GamePath {
    # appdr2 links straight into the installed game folder; read OutDir/TargetName
    # from the project so this keeps working if those are ever changed.
    $proj = Join-Path $RepoRoot 'appdr2\appdr2.vcxproj'
    [xml]$xml = Get-Content $proj
    $ns = @{ m = 'http://schemas.microsoft.com/developer/msbuild/2003' }
    $cond = "'`$(Configuration)|`$(Platform)'=='$Config|Win32'"

    $group = Select-Xml -Xml $xml -Namespace $ns -XPath "//m:PropertyGroup[@Condition=`"$cond`"][m:OutDir]" |
        Select-Object -First 1
    if (-not $group) { throw "Could not read OutDir for $Config from appdr2.vcxproj." }

    $outDir = $group.Node.OutDir
    $name = $group.Node.TargetName -replace '\$\(Configuration\)', $Config
    if (-not $name) { $name = 'appdr2' }

    return [pscustomobject]@{
        Dir = $outDir.TrimEnd('\')
        Exe = Join-Path $outDir "$name.exe"
    }
}

function Invoke-Launch {
    $game = Get-GamePath
    if (-not (Test-Path $game.Exe)) { throw "Game executable not found: $($game.Exe)" }

    Write-Host "Launching $($game.Exe) $GameArgs" -ForegroundColor Cyan
    # Working directory must be the game folder - it resolves data packs relatively.
    $p = Start-Process -FilePath $game.Exe -WorkingDirectory $game.Dir `
        -ArgumentList $GameArgs -PassThru -Wait
    Write-Host "Game exited with code $($p.ExitCode)."
}

switch ($Action) {
    'build'   { Invoke-Build $Target }
    'rebuild' { Invoke-Build $(if ($Target) { "$Target`:Rebuild" } else { 'Rebuild' }) }
    'clean'   { Invoke-Build $(if ($Target) { "$Target`:Clean" } else { 'Clean' }) }
    'launch'  { Invoke-Launch }
    'run'     { Invoke-Build $Target; Invoke-Launch }
}
