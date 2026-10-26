[CmdletBinding(SupportsShouldProcess)]
param([string]$AircraftPath = 'G:\SteamLibrary\steamapps\common\X-Plane 12\Aircraft\ToLissA320_V1p3p2')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot 'dist\ToLissOANS'
$aircraftFull = [IO.Path]::GetFullPath($AircraftPath).TrimEnd('\','/')
if (!(Test-Path -LiteralPath (Join-Path $aircraftFull 'a320.acf'))) { throw 'Expected ToLiss A320 aircraft directory with a320.acf' }
if (!(Test-Path -LiteralPath (Join-Path $source '64\win.xpl'))) { throw 'Build the plugin first: scripts\build.ps1' }
if (Get-Process -Name 'X-Plane' -ErrorAction SilentlyContinue) { throw 'Close X-Plane before installing. This script does not start or stop it.' }
$pluginRoot = [IO.Path]::GetFullPath((Join-Path $aircraftFull 'plugins'))
$target = [IO.Path]::GetFullPath((Join-Path $pluginRoot '00_ToLissOANS'))
$candidateNames = @('00_ToLissOANS','zz_ToLissOANS','ToLissOANS')
$candidateNames += @(Get-ChildItem -LiteralPath $pluginRoot -Directory | Where-Object { $_.Name -match '^ToLissOANS-\d+\.\d+\.\d+$' } | ForEach-Object { $_.Name })
$candidatePaths = $candidateNames | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $pluginRoot $_)) }
foreach ($candidatePath in $candidatePaths) {
    if ([IO.Path]::GetDirectoryName($candidatePath) -ne $pluginRoot) { throw 'Unexpected installation target' }
}
$existingPaths = @($candidatePaths | Where-Object { Test-Path -LiteralPath $_ })
if ($existingPaths.Count -gt 1) { throw 'Multiple OANS plugin directories exist; inspect duplicate plugins before installing' }
$existingTarget = if ($existingPaths.Count -eq 1) { $existingPaths[0] } else { $null }
if ($existingTarget) {
    $backupRoot = Join-Path $projectRoot ('backups\ToLissOANS-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    if ($PSCmdlet.ShouldProcess($backupRoot,'Back up existing ToLissOANS plugin')) {
        New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
        Copy-Item -LiteralPath $existingTarget -Destination $backupRoot -Recurse -Force
    }
}
if ($existingTarget -and $existingTarget -ne $target -and $PSCmdlet.ShouldProcess($existingTarget,'Rename our plugin to 00_ToLissOANS; required by verified ND rendering order')) {
    Move-Item -LiteralPath $existingTarget -Destination $target
}
if ($PSCmdlet.ShouldProcess($target,'Install independent ToLissOANS plugin files')) {
    New-Item -ItemType Directory -Path (Join-Path $target '64') -Force | Out-Null
    foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File) {
        $relative = [IO.Path]::GetRelativePath($source,$file.FullName)
        $destination = Join-Path $target $relative
        # Existing user configuration and runtime logs are preserved across upgrades.
        if (($relative -eq 'config.ini' -or $relative -match '^config[\\/].+\.json$') -and (Test-Path -LiteralPath $destination)) { continue }
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
    }
    $builtHash = (Get-FileHash -LiteralPath (Join-Path $source '64\win.xpl') -Algorithm SHA256).Hash
    $installedHash = (Get-FileHash -LiteralPath (Join-Path $target '64\win.xpl') -Algorithm SHA256).Hash
    if ($builtHash -ne $installedHash) { throw 'Installed binary hash mismatch' }
    Write-Output "Installed: $target"
    Write-Output "SHA256: $installedHash"
    Write-Output 'X-Plane was not started.'
}
