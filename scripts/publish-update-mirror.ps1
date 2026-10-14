# SPDX-License-Identifier: Apache-2.0

param(
    [Parameter(Mandatory = $true)][string]$Installer,
    [string]$Server = 't_ecs',
    [string]$BaseUrl = 'https://162.14.72.194'
)

$ErrorActionPreference = 'Stop'
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$fileName = [IO.Path]::GetFileName($installerPath)
$match = [regex]::Match($fileName, '^SerialWorkbench-(\d+\.\d+\.\d+)-windows-x64-setup\.exe$')
if (-not $match.Success) { throw 'Unexpected installer filename.' }

$version = $match.Groups[1].Value
$sha256 = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant()
$size = (Get-Item -LiteralPath $installerPath).Length
if ($size -le 0 -or $size -gt 300MB) { throw 'Installer size is outside the updater limit.' }

$manifest = [ordered]@{
    version = $version
    filename = $fileName
    url = "$BaseUrl/updates/$fileName"
    sha256 = $sha256
    size = $size
} | ConvertTo-Json

$staging = Join-Path $env:TEMP ('serial-workbench-mirror-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging | Out-Null
$manifestPath = Join-Path $staging 'latest.json'
[IO.File]::WriteAllText($manifestPath, $manifest + "`n", [Text.UTF8Encoding]::new($false))

try
{
    $remoteInstaller = "/tmp/$fileName"
    $remoteManifest = "/tmp/serial-workbench-$version-latest.json"
    & scp $installerPath "${Server}:$remoteInstaller"
    if ($LASTEXITCODE) { throw 'Installer upload failed.' }
    & scp $manifestPath "${Server}:$remoteManifest"
    if ($LASTEXITCODE) { throw 'Manifest upload failed.' }

    $remoteDir = '/srv/lilinng-web/www/shared/updates'
    $command = "sudo -n install -m 644 '$remoteInstaller' '$remoteDir/$fileName'" +
        " && echo '$sha256  $remoteDir/$fileName' | sha256sum -c -" +
        " && sudo -n install -m 644 '$remoteManifest' '$remoteDir/latest.json.next'" +
        " && sudo -n mv '$remoteDir/latest.json.next' '$remoteDir/latest.json'"
    & ssh -o BatchMode=yes $Server $command
    if ($LASTEXITCODE) { throw 'Mirror publication or server-side verification failed.' }

    Write-Host "Published $version at $BaseUrl/updates/latest.json"
}
finally
{
    Remove-Item -LiteralPath $manifestPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $staging -Force -ErrorAction SilentlyContinue
}
