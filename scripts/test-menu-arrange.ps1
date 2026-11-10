param([string]$Python='python', [string]$SdkRoot='', [string]$CompilerDirectory='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & $Python -X utf8 'tests/menu_arrange_tests.py'
    if($LASTEXITCODE -ne 0){throw 'Menu persistence/conflict regression failed'}
} finally { Pop-Location }
