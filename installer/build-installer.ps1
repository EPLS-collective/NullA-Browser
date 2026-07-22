param(
    [string]$Version = "",
    [string]$IsccPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = "Stop"

$repo = "EPLS-collective/NullA-Browser"
$distDir = Join-Path $PSScriptRoot "dist"
$extractDir = Join-Path $distDir "NullA"
$zipPath = Join-Path $distDir "NullA-Windows.zip"

if ($Version -eq "") {
    Write-Host "No version specified, using the latest GitHub release..."
    $releaseUrl = "https://api.github.com/repos/$repo/releases/latest"
} else {
    Write-Host "Using specified version: $Version"
    $releaseUrl = "https://api.github.com/repos/$repo/releases/tags/v$Version"
}

$release = Invoke-RestMethod -Uri $releaseUrl -Headers @{ "User-Agent" = "NullA-Installer-Builder" }
$tag = $release.tag_name.TrimStart("v")
Write-Host "Release found: v$tag"

$asset = $release.assets | Where-Object { $_.name -like "*Windows*.zip" } | Select-Object -First 1
if (-not $asset) {
    throw "No NullA-Windows.zip asset found in this release."
}

if (Test-Path $extractDir) { Remove-Item -Recurse -Force $extractDir }
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

Write-Host "Downloading: $($asset.browser_download_url)"
Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $zipPath

Write-Host "Extracting..."
Expand-Archive -Path $zipPath -DestinationPath $distDir -Force

Write-Host "Compiling Setup.exe (v$tag)..."
& $IsccPath "/DMyAppVersion=$tag" (Join-Path $PSScriptRoot "NullA.iss")

Write-Host "Done! Output: dist\NullA Setup.exe"
