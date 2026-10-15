# SPDX-License-Identifier: Apache-2.0

param(
    [string]$QtRoot = $env:QTDIR,
    [string]$CompilerBin = $env:MINGW_BIN,
    [string]$CMake = 'cmake',
    [string]$Ninja = 'ninja',
    [string]$BuildDir = 'build/native-release',
    [string]$PackageDir = 'dist/SerialWorkbenchNative',
    [ValidateRange(1, 64)][int]$Jobs = 4,
    [switch]$SkipTests,
    [switch]$SkipPackage
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $QtRoot -or -not $CompilerBin)
{
    throw 'Set QTDIR and MINGW_BIN, or pass -QtRoot and -CompilerBin.'
}

$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$CompilerBin = (Resolve-Path -LiteralPath $CompilerBin).Path
$buildPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$destination = [IO.Path]::GetFullPath((Join-Path $repoRoot $PackageDir))
$originalPath = $env:PATH
$env:PATH = "$CompilerBin;$QtRoot/bin;$env:PATH"

Push-Location $repoRoot
try
{
    $cmakePath = (Get-Command $CMake -ErrorAction Stop).Source
    $ninjaPath = (Get-Command $Ninja -ErrorAction Stop).Source
    $ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
    $testing = if ($SkipTests) { 'OFF' } else { 'ON' }

    & $cmakePath -S $repoRoot -B $buildPath -G Ninja `
        -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_BINDIR=." `
        "-DBUILD_TESTING=$testing" "-DCMAKE_PREFIX_PATH=$QtRoot" `
        "-DCMAKE_CXX_COMPILER=$CompilerBin/g++.exe" "-DCMAKE_MAKE_PROGRAM=$ninjaPath"
    if ($LASTEXITCODE) { throw 'CMake configuration failed' }

    & $cmakePath --build $buildPath --parallel $Jobs
    if ($LASTEXITCODE) { throw 'Native build failed' }

    if (-not $SkipTests)
    {
        & $ctestPath --test-dir $buildPath --output-on-failure
        if ($LASTEXITCODE)
        {
            Get-Content -LiteralPath (Join-Path $buildPath 'native-tests.txt') -ErrorAction SilentlyContinue
            Get-Content -LiteralPath (Join-Path $buildPath 'script-tests.txt') -ErrorAction SilentlyContinue
            throw 'Native tests failed'
        }
    }

    if (-not $SkipPackage)
    {
        $running = Get-Process SerialWorkbench,SerialWorkbenchMCP,SerialWorkbenchSSHAskpass -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -and ([IO.Path]::GetDirectoryName($_.Path) -eq $destination) }
        if ($running) { throw 'Close the native workbench before packaging over its running executable.' }

        & $cmakePath --install $buildPath --prefix $destination
        if ($LASTEXITCODE) { throw 'Package installation failed' }

        foreach ($executable in 'SerialWorkbench.exe', 'SerialWorkbenchMCP.exe', 'SerialWorkbenchSSHAskpass.exe')
        {
            & "$QtRoot/bin/windeployqt.exe" --release --no-translations `
                --no-system-d3d-compiler --no-opengl-sw (Join-Path $destination $executable)
            if ($LASTEXITCODE) { throw "Qt deployment failed: $executable" }
        }
        Write-Host "Native package ready: $destination"
    }
}
finally
{
    Pop-Location
    $env:PATH = $originalPath
}
