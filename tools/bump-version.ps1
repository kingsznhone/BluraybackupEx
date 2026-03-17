param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$DebianRevision = "1"
)

$ErrorActionPreference = "Stop"

if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') {
    throw "Version must match semantic version format, for example 2.1.0"
}

if ($DebianRevision -notmatch '^[0-9]+$') {
    throw "DebianRevision must be numeric"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$versionFile = Join-Path $repoRoot "VERSION"
$vcpkgFile = Join-Path $repoRoot "vcpkg.json"
$changelogFile = Join-Path $repoRoot "debian/changelog"

Set-Content -Path $versionFile -Value "$Version`r`n" -NoNewline:$false

$vcpkg = Get-Content -Raw $vcpkgFile | ConvertFrom-Json
$vcpkg.version = $Version
$vcpkg | ConvertTo-Json -Depth 10 | Set-Content -Path $vcpkgFile

$name = git config user.name
$email = git config user.email
if ([string]::IsNullOrWhiteSpace($name) -or [string]::IsNullOrWhiteSpace($email)) {
    throw "git config user.name and user.email must be set before updating debian/changelog"
}

$timestamp = [System.DateTimeOffset]::Now.ToString(
    'ddd, dd MMM yyyy HH:mm:ss zzz',
    [System.Globalization.CultureInfo]::InvariantCulture
)

$entry = @"
bluraybackup-ex ($Version-$DebianRevision) unstable; urgency=medium

  * Bump upstream version to $Version.

 -- $name <$email>  $timestamp

"@

$existing = Get-Content -Raw $changelogFile
Set-Content -Path $changelogFile -Value ($entry + $existing)

Write-Host "Updated VERSION, vcpkg.json, and debian/changelog to bluraybackup-ex $Version-$DebianRevision"