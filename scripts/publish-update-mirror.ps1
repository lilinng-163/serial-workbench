# SPDX-License-Identifier: Apache-2.0

param(
    [string]$Installer,
    [string]$Server = 't_ecs',
    [string]$BaseUrl = 'https://162.14.72.194',
    [string]$WebsiteUrl = 'https://www.lilinng.cn',
    [string]$McpUrl = 'http://127.0.0.1:8765/mcp',
    [switch]$DownloadOnly
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $repoRoot '.venv/Scripts/python.exe'
if (-not (Test-Path -LiteralPath $python))
{
    $python = (Get-Command python -ErrorAction Stop).Source
}

$arguments = @(
    (Join-Path $PSScriptRoot 'sync-website-release.py'),
    '--server', $Server,
    '--base-url', $BaseUrl,
    '--website-url', $WebsiteUrl,
    '--mcp-url', $McpUrl
)
if ($Installer)
{
    $arguments += '--installer', (Resolve-Path -LiteralPath $Installer).Path
}
if ($DownloadOnly)
{
    $arguments += '--download-only'
}

& $python @arguments
if ($LASTEXITCODE -ne 0)
{
    throw 'Website release synchronization failed; inspect the output and dist/website-sync records.'
}
