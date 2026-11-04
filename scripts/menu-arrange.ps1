param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
& $Python -X utf8 (Join-Path $PSScriptRoot 'menu_arrange.py')
if ($LASTEXITCODE -ne 0) { throw 'Menu arranger failed' }
