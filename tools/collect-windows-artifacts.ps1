$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir = Join-Path $repoRoot 'build'
$artifactDir = Join-Path $repoRoot 'dist/windows'
$stagingDir = Join-Path $artifactDir 'bluraybackup-ex.win_x64'
$zipPath = Join-Path $artifactDir 'bluraybackup-ex.win_x64.zip'
$packagedExeName = 'bluraybackup-ex.exe'

New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
Remove-Item -Path (Join-Path $artifactDir '*') -Recurse -Force -ErrorAction SilentlyContinue

$candidateSourceDirs = @(
	(Join-Path $buildDir 'Release'),
	(Join-Path $buildDir 'Debug'),
	(Join-Path $buildDir 'RelWithDebInfo'),
	(Join-Path $buildDir 'MinSizeRel')
)

$sourceDir = $null
foreach ($candidateSourceDir in $candidateSourceDirs) {
	if (Test-Path -Path (Join-Path $candidateSourceDir $packagedExeName) -PathType Leaf) {
		$sourceDir = $candidateSourceDir
		break
	}
}

if (-not $sourceDir) {
	throw "Could not find $packagedExeName in any expected build output directory under $buildDir"
}

New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

Copy-Item -Path (Join-Path $sourceDir $packagedExeName) -Destination (Join-Path $stagingDir $packagedExeName) -Force

$runtimeDlls = Get-ChildItem -Path $sourceDir -Filter '*.dll' -File
foreach ($runtimeDll in $runtimeDlls) {
	Copy-Item -Path $runtimeDll.FullName -Destination $stagingDir -Force
}

Compress-Archive -Path (Join-Path $stagingDir '*') -DestinationPath $zipPath -CompressionLevel Optimal -Force

Write-Host "Windows artifact collected: $zipPath"