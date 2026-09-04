# Build + package the Windows PC client. Always use this script; do not run
# cmake by hand: an unquoted -DCMAKE_PREFIX_PATH=$var in PowerShell poisons the
# CMake cache with a literal "$var". This script always supplies the real Qt
# path and reuses the canonical MSVC tree. Requires PowerShell
# FullLanguage mode; under a
# Constrained Language Mode sandbox it fails with a misleading dot-source error.
# Native build tools keep their normal inherited input/output handles.
[CmdletBinding()]
param(
    [string]$QtDir = "C:\Qt\6.11.0\msvc2022_64",
    [string]$BuildDir = "",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Architecture = "x64",
    [switch]$CleanBuild,
    [ValidateRange(1, 86400)][int]$CleanTimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

$ClientDir = $PSScriptRoot
. (Join-Path $ClientDir "build-lib.ps1")
$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $ClientDir "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    # In-tree, gitignored, and separate from the pc-build tree used by
    # make client-test, so packaging and the development loop never contend.
    $BuildDir = Join-Path $ProjectRoot "build\pc-dist"
}
$DistDir = Join-Path $ProjectRoot "release\shatranj-client"
$PackagedExe = Join-Path $DistDir "shatranj-client.exe"
$WinDeployQtExe = Join-Path $QtDir "bin\windeployqt.exe"

function Resolve-Tool {
    param([Parameter(Mandatory = $true)][string]$Name)

    return (Get-Command $Name -ErrorAction Stop).Source
}

function Test-IsWithinRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root
    )

    $pathNorm = $Path.TrimEnd('\')
    $rootNorm = $Root.TrimEnd('\')
    if ($pathNorm.Equals($rootNorm, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    return $pathNorm.StartsWith($rootNorm + '\', [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-PackagedClientNotRunning {
    param([Parameter(Mandatory = $true)][string]$Path)

    $matching = @()
    foreach ($process in Get-Process -Name "shatranj-client" -ErrorAction SilentlyContinue) {
        try {
            if ($process.Path -and (Test-IsWithinRoot -Path $process.Path -Root $Path)) {
                $matching += $process
            }
        } catch {
            # A process owned by another user is irrelevant unless its path can
            # be proven to live inside this package directory.
        }
    }
    if ($matching.Count -gt 0) {
        $ids = ($matching.Id | Sort-Object) -join ", "
        throw "Packaged Shatranj client is running from $Path (PID: $ids). Close it before rebuilding."
    }
}

function Remove-GeneratedTree {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root,
        [int]$TimeoutSeconds = $CleanTimeoutSeconds
    )

    if (-not (Test-Path -LiteralPath $Path)) { return }

    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
    if (-not (Test-IsWithinRoot -Path $resolvedPath -Root $resolvedRoot)) {
        throw "Refusing to remove outside generated tree: $resolvedPath"
    }

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastError = $null
    do {
        try {
            Remove-Item -LiteralPath $resolvedPath -Recurse -Force -ErrorAction Stop
            return
        } catch {
            $lastError = $_
            Start-Sleep -Milliseconds 300
        }
    } while ((Get-Date) -lt $deadline)

    throw "Unable to clean generated output after ${TimeoutSeconds}s: $resolvedPath. Close running Shatranj client or locked Qt DLLs and retry. Last error: $($lastError.Exception.Message)"
}

function Copy-SelectedChildren {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string[]]$Names
    )

    if (-not (Test-Path -LiteralPath $Source)) { return }
    Remove-GeneratedTree -Path $Destination -Root $DistDir
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    foreach ($name in $Names) {
        Copy-Item -LiteralPath (Join-Path $Source $name) -Destination $Destination -Force -Recurse
    }
}

function Copy-Contents {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source)) { return }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path (Join-Path $Source "*") -Destination $Destination -Force -Recurse
}

function Remove-UnlistedDeploymentFiles {
    param([Parameter(Mandatory = $true)][string]$Path)

    $keepRootFiles = @(
        "shatranj-client.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Network.dll",
        "Qt6Svg.dll", "Qt6Widgets.dll", "icuuc.dll", "concrt140.dll",
        "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
        "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll",
        "vccorlib140.dll", "vcruntime140.dll", "vcruntime140_1.dll",
        "vcruntime140_threads.dll"
    )
    $keepRootFiles += @("LICENSE", "THIRD_PARTY_NOTICES.md")
    $keepDirs = @("assets", "imageformats", "licenses", "platforms")
    foreach ($item in Get-ChildItem -LiteralPath $Path -Force) {
        if ($item.PSIsContainer) {
            if ($keepDirs -notcontains $item.Name) {
                Remove-GeneratedTree -Path $item.FullName -Root $Path
            }
        } elseif ($keepRootFiles -notcontains $item.Name) {
            Remove-GeneratedTree -Path $item.FullName -Root $Path
        }
    }

    $pluginKeep = @{ imageformats = @("qjpeg.dll"); platforms = @("qwindows.dll") }
    foreach ($dirName in $pluginKeep.Keys) {
        $dir = Join-Path $Path $dirName
        if (-not (Test-Path -LiteralPath $dir)) { continue }
        foreach ($item in Get-ChildItem -LiteralPath $dir -Force) {
            if ($item.PSIsContainer) {
                Remove-GeneratedTree -Path $item.FullName -Root $dir
            } elseif ($pluginKeep[$dirName] -notcontains $item.Name) {
                Remove-GeneratedTree -Path $item.FullName -Root $dir
            }
        }
    }
}

if (-not (Test-Path -LiteralPath $QtDir)) { throw "Qt directory not found: $QtDir" }
if (-not (Test-Path -LiteralPath $WinDeployQtExe)) { throw "Qt deploy tool not found: $WinDeployQtExe" }
$cmake = Resolve-Tool "cmake"

if ($CleanBuild) {
    Remove-GeneratedTree -Path $BuildDir -Root (Split-Path -Parent $BuildDir)
}
Assert-PackagedClientNotRunning -Path $DistDir
Remove-GeneratedTree -Path $DistDir -Root $ProjectRoot
Remove-GeneratedTree -Path (Join-Path $ProjectRoot "release\netchesszx-client") -Root $ProjectRoot
Remove-GeneratedTree -Path (Join-Path $ProjectRoot "release\shatranj") -Root $ProjectRoot
Remove-GeneratedTree -Path (Join-Path $ClientDir "dist\shatranj") -Root $ClientDir
Remove-GeneratedTree -Path (Join-Path $ClientDir "dist\netchesszx-client") -Root $ClientDir
Remove-GeneratedTree -Path (Join-Path $ClientDir "dist\zxchess-client") -Root $ClientDir

$configureArgs = @(
    "-S", $ClientDir,
    "-B", $BuildDir,
    "-G", $Generator,
    "-DCMAKE_PREFIX_PATH=$QtDir"
)
if (-not [string]::IsNullOrWhiteSpace($Architecture)) {
    $configureArgs += @("-A", $Architecture)
}

Invoke-ProcessChecked -FilePath $cmake -Arguments $configureArgs -Name "cmake configure"
Invoke-ProcessChecked -FilePath $cmake -Arguments @("--build", $BuildDir, "--config", $Config, "--parallel") -Name "cmake build"

$exe = @(
    (Join-Path $BuildDir "$Config\shatranj-client.exe"),
    (Join-Path $BuildDir "shatranj-client.exe")
) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $exe) { throw "Built executable not found under $BuildDir" }

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
Copy-Item -LiteralPath $exe -Destination $PackagedExe -Force
$deployMode = if ($Config -eq "Debug") { "--debug" } else { "--release" }
Invoke-ProcessChecked `
    -FilePath $WinDeployQtExe `
    -Arguments @($deployMode, "--compiler-runtime", "--no-translations", "--no-system-d3d-compiler", "--no-opengl-sw", $PackagedExe) `
    -Name "windeployqt"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) { throw "vswhere not found: $vswhere" }
$vsInstall = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$crtDir = Get-ChildItem -LiteralPath (Join-Path $vsInstall "VC\Redist\MSVC") -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName "x64\Microsoft.VC143.CRT" } |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if (-not $crtDir) { throw "MSVC x64 runtime directory not found" }
Copy-Item -LiteralPath (Get-ChildItem -LiteralPath $crtDir -Filter "*.dll").FullName `
    -Destination $DistDir -Force

Copy-SelectedChildren `
    -Source (Join-Path $ProjectRoot "assets\pc-client\piece_sets") `
    -Destination (Join-Path $DistDir "assets\pc-client\piece_sets") `
    -Names @("california", "gioco", "kiwen-suwi", "merida", "mpchess")
Copy-SelectedChildren `
    -Source (Join-Path $ProjectRoot "assets\pc-client\boards") `
    -Destination (Join-Path $DistDir "assets\pc-client\boards") `
    -Names @("blue.png", "blue3.jpg", "green.png", "purple-diag.png", "wood4.jpg")
Copy-Contents `
    -Source (Join-Path $ProjectRoot "assets\pc-client\about") `
    -Destination (Join-Path $DistDir "assets\pc-client\about")
Copy-Contents `
    -Source (Join-Path $ProjectRoot "licenses") `
    -Destination (Join-Path $DistDir "licenses")
Copy-Item -LiteralPath (Join-Path $ProjectRoot "LICENSE") -Destination $DistDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "THIRD_PARTY_NOTICES.md") -Destination $DistDir -Force
Copy-Item `
    -LiteralPath (Join-Path $ProjectRoot "assets\lichess\LICENSE.lichess-AGPL-3.0.txt") `
    -Destination (Join-Path $DistDir "licenses") -Force

Remove-UnlistedDeploymentFiles -Path $DistDir

Write-Host "Built: $exe"
Write-Host "Packaged: $PackagedExe"
