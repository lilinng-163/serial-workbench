param(
    [string]$QtRoot = 'D:/lib/Qt/6.11.1/mingw_64',
    [string]$CompilerBin = 'D:/lib/Qt/Tools/mingw1310_64/bin',
    [string]$CMake = 'C:/CMakev4.3/bin/cmake.exe',
    [string]$Ninja = 'E:/MinGW/w64devkit/bin/ninja.exe'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$env:PATH = "$CompilerBin;$QtRoot/bin;$env:PATH"
& $CMake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$CompilerBin/g++.exe" "-DCMAKE_MAKE_PROGRAM=$Ninja"
if ($LASTEXITCODE) { throw 'CMake configuration failed' }
& $CMake --build build/native -j 4
if ($LASTEXITCODE) { throw 'Native build failed' }
& "$([IO.Path]::GetDirectoryName($CMake))/ctest.exe" --test-dir build/native --output-on-failure
if ($LASTEXITCODE)
{
    Get-Content -LiteralPath 'build/native/native-tests.txt'
    throw 'Native tests failed'
}
$destination = Join-Path $PSScriptRoot 'dist/SerialWorkbenchNative'
$running = Get-Process SerialWorkbench,SerialWorkbenchMCP -ErrorAction SilentlyContinue | Where-Object { $_.Path -and ([IO.Path]::GetDirectoryName($_.Path) -eq $destination) }
if ($running) { throw 'Close the native workbench before packaging over its running executable.' }
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -LiteralPath 'build/native/bin/SerialWorkbench.exe','build/native/bin/SerialWorkbenchMCP.exe' -Destination $destination -Force
& "$QtRoot/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$destination/SerialWorkbench.exe"
if ($LASTEXITCODE) { throw 'Qt deployment failed' }
& "$QtRoot/bin/windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$destination/SerialWorkbenchMCP.exe"
if ($LASTEXITCODE) { throw 'Bridge deployment failed' }
Write-Host "Native package ready: $destination"
