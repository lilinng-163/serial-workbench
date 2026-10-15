# SPDX-License-Identifier: Apache-2.0

param(
    [string]$PackageDir = 'dist/SerialWorkbenchNative',
    [string]$OutputDir = 'dist/releases',
    [Parameter(Mandatory = $true)][string]$QtSourceRoot,
    [string]$MakeNSIS = 'makensis',
    [switch]$InstallerOnly
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$packagePath = (Resolve-Path -LiteralPath (Join-Path $repoRoot $PackageDir)).Path
$outputPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDir))
$project = Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw
$version = [regex]::Match($project, 'VERSION\s+(\d+\.\d+\.\d+)').Groups[1].Value
if (-not $version) { throw 'Cannot read the CMake project version.' }

$compiler = (Get-Command $MakeNSIS -ErrorAction Stop).Source
$qtSources = (Resolve-Path -LiteralPath $QtSourceRoot).Path
$stageName = 'release-' + $version + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$stageRoot = Join-Path $repoRoot ('build/' + $stageName)
$payload = Join-Path $stageRoot 'SerialWorkbench'
New-Item -ItemType Directory -Path $payload, $outputPath -Force | Out-Null

foreach ($name in 'SerialWorkbench.exe', 'SerialWorkbenchMCP.exe', 'SerialWorkbenchSSHAskpass.exe')
{
    Copy-Item -LiteralPath (Join-Path $packagePath $name) -Destination $payload
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination $payload
Get-ChildItem -LiteralPath $packagePath -File -Filter '*.dll' | Copy-Item -Destination $payload

foreach ($name in 'generic', 'iconengines', 'imageformats', 'licenses', 'networkinformation', 'platforms', 'styles', 'tls')
{
    $source = Join-Path $packagePath $name
    if (Test-Path -LiteralPath $source)
    {
        Copy-Item -LiteralPath $source -Destination $payload -Recurse
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $payload 'platforms/qwindows.dll')))
{
    throw 'The Qt Windows platform plugin is missing. Run scripts/build.ps1 first.'
}

# Retain license texts and original attribution notices for the bundled Qt modules.
$qtNotices = Join-Path $payload 'licenses/Qt'
New-Item -ItemType Directory -Path $qtNotices -Force | Out-Null
foreach ($module in 'qtbase', 'qt5compat', 'qtsvg', 'qttranslations')
{
    $moduleRoot = Join-Path $qtSources $module
    if (-not (Test-Path -LiteralPath (Join-Path $moduleRoot 'LICENSES')))
    {
        throw "Qt source license directory is missing: $moduleRoot/LICENSES"
    }
    $notices = Get-ChildItem -LiteralPath $moduleRoot -File -Recurse | Where-Object {
        $_.FullName.StartsWith((Join-Path $moduleRoot 'LICENSES') + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase) -or
        $_.Name -match '^(LICENSE|COPYING|NOTICE)' -or $_.Name -eq 'qt_attributions.json'
    }
    foreach ($notice in $notices)
    {
        $relative = $notice.FullName.Substring($qtSources.Length + 1)
        $destination = Join-Path $qtNotices $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $notice.FullName -Destination $destination
    }
}

$instructions = @"
Serial Workbench $version (Windows x64)

安装包 setup.exe：可选择安装目录，并注册 Windows 卸载入口。
安装版个人设置保存在 %LOCALAPPDATA%\SerialWorkbench\settings.json。

单文件 EXE：双击后自动展开运行库并启动。运行库缓存在
%LOCALAPPDATA%\SerialWorkbench\runtime\，配置保存在
%LOCALAPPDATA%\SerialWorkbench\settings.json。后续启动复用缓存。

便携 ZIP：解压整个目录后双击 SerialWorkbench.exe。
保留 DLL、插件和许可证目录；配置保存在该目录的 settings.json。
SerialWorkbenchMCP.exe 是供 AI 客户端调用的 STDIO 桥。

桌面程序不需要安装 Python 或 Qt。SSH 功能使用 Windows OpenSSH 客户端。
首次启动默认端口 NULL，不会自动连接硬件。

项目源码与许可证：https://github.com/lilinng-163/serial-workbench
Qt 来源：https://code.qt.io/ （qtbase、qt5compat、qtsvg、qttranslations）
项目采用 Apache-2.0；第三方许可证保留在 licenses 目录。
"@
[IO.File]::WriteAllText((Join-Path $payload 'README.txt'), $instructions, [Text.UTF8Encoding]::new($false))

$privateFiles = Get-ChildItem -LiteralPath $payload -Recurse -File -Filter 'settings*.json'
if ($privateFiles) { throw 'Personal settings must not be included in a release.' }

# Exact uninstall list protects user-created files in a custom directory.
$removeManifest = Join-Path $stageRoot 'remove-files.nsh'
$relativeFiles = Get-ChildItem -LiteralPath $payload -Recurse -File | ForEach-Object {
    $_.FullName.Substring($payload.Length + 1)
}
$relativeDirectories = Get-ChildItem -LiteralPath $payload -Recurse -Directory | ForEach-Object {
    $_.FullName.Substring($payload.Length + 1)
} | Sort-Object Length -Descending
function ConvertTo-NSISLiteral([string]$text)
{
    return $text.Replace('$', '$$').Replace('"', '$\"')
}
$removeLines = @('; Generated exact uninstall file list.')
$removeLines += $relativeFiles | ForEach-Object { 'Delete "$INSTDIR\' + (ConvertTo-NSISLiteral $_) + '"' }
$removeLines += 'Delete "$INSTDIR\SHA256SUMS.txt"'
$removeLines += $relativeDirectories | ForEach-Object { 'RMDir "$INSTDIR\' + (ConvertTo-NSISLiteral $_) + '"' }
[IO.File]::WriteAllLines($removeManifest, $removeLines, [Text.UTF8Encoding]::new($false))

$manifest = Get-ChildItem -LiteralPath $payload -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($payload.Length + 1).Replace('\', '/')
    (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $relative
}
$manifestPath = Join-Path $payload 'SHA256SUMS.txt'
[IO.File]::WriteAllLines($manifestPath, $manifest, [Text.UTF8Encoding]::new($false))
$payloadId = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.Substring(0, 12).ToLowerInvariant()
$baseName = "SerialWorkbench-$version-windows-x64"
$executable = Join-Path $outputPath ($baseName + '.exe')
$archive = Join-Path $outputPath ($baseName + '-portable.zip')

$installer = Join-Path $outputPath ($baseName + '-setup.exe')
$brandDirectory = Join-Path $repoRoot 'resources/brand'
$sizeKB = [int][Math]::Ceiling(((Get-ChildItem -LiteralPath $payload -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1024))
& $compiler /V2 /INPUTCHARSET UTF8 "/DVERSION=$version" "/DPAYLOAD_DIR=$payload" "/DOUTPUT_EXE=$installer" `
    "/DBRAND_DIR=$brandDirectory" "/DREMOVE_MANIFEST=$removeManifest" "/DSIZE_KB=$sizeKB" `
    (Join-Path $PSScriptRoot 'windows-installer.nsi')
if ($LASTEXITCODE) { throw 'Windows installer compilation failed.' }
$deliverables = @($installer)
if (-not $InstallerOnly)
{
    & $compiler /V2 /INPUTCHARSET UTF8 "/DVERSION=$version" "/DPAYLOAD_ID=$payloadId" "/DPAYLOAD_DIR=$payload" `
        "/DOUTPUT_EXE=$executable" (Join-Path $PSScriptRoot 'release-launcher.nsi')
    if ($LASTEXITCODE) { throw 'Single-file launcher compilation failed.' }
    Compress-Archive -LiteralPath $payload -DestinationPath $archive -CompressionLevel Optimal -Force
    $deliverables += $executable, $archive
}

$checksums = foreach ($file in $deliverables)
{
    (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetFileName($file)
}
[IO.File]::WriteAllLines((Join-Path $outputPath 'SHA256SUMS.txt'), $checksums, [Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    Version = $version
    StageDirectory = $stageRoot
    PayloadDirectory = $payload
    Installer = $installer
    UninstallManifest = $removeManifest
    Executable = if ($InstallerOnly) { $null } else { $executable }
    PortableZip = if ($InstallerOnly) { $null } else { $archive }
    Checksums = Join-Path $outputPath 'SHA256SUMS.txt'
}
