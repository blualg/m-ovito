param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\\build-modern3"),
    [string]$ArchiveRoot = "C:\\Users\\tsaka\\Documents\\CODEX\\Ovito-build-archives",
    [string]$Label,
    [string]$Notes = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = (Resolve-Path $BuildDir).Path

if (-not $Label) {
    $Label = Get-Date -Format "yyyy-MM-dd-HHmmss"
}

$archiveDir = Join-Path $ArchiveRoot $Label
if (Test-Path $archiveDir) {
    throw "Archive directory already exists: $archiveDir"
}

$runtimeDirectories = @(
    ".qt",
    "doc",
    "generic",
    "iconengines",
    "imageformats",
    "networkinformation",
    "platforms",
    "styles",
    "tls",
    "translations"
)

$runtimePatterns = @("*.exe", "*.dll", "*.conf")

New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null

foreach ($directory in $runtimeDirectories) {
    $source = Join-Path $resolvedBuildDir $directory
    if (Test-Path $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $archiveDir $directory) -Recurse
    }
}

foreach ($pattern in $runtimePatterns) {
    Get-ChildItem -LiteralPath $resolvedBuildDir -File -Filter $pattern | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $archiveDir $_.Name)
    }
}

$branch = (git -C $repoRoot branch --show-current).Trim()
$commit = (git -C $repoRoot rev-parse HEAD).Trim()
$tags = @(git -C $repoRoot tag --points-at HEAD | Where-Object { $_ -and $_.Trim().Length -gt 0 } | ForEach-Object { $_.Trim() })

$manifest = [ordered]@{
    label = $Label
    created_at = (Get-Date).ToString("o")
    repo_root = $repoRoot
    build_dir = $resolvedBuildDir
    archive_dir = $archiveDir
    branch = $branch
    commit = $commit
    tags = $tags
    entrypoint = "ovito.exe"
    harness = "ovito_transport_harness.exe"
    tested_command = ".\\ovito.exe --nogui"
    notes = if ($Notes) { @($Notes) } else { @() }
}

$manifestPath = Join-Path $archiveDir "manifest.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "Created local build archive:" $archiveDir
