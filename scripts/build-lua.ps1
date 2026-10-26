param([string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',[switch]$DebugSymbols)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$source=Join-Path $projectRoot 'vendor/lua-5.4.9/src'
$output=Join-Path $projectRoot 'build/lua'
New-Item -ItemType Directory -Force $output | Out-Null
$objects=@()
$symbolArgs=@(if($DebugSymbols){'-g'})
foreach($file in Get-ChildItem -LiteralPath $source -Filter '*.c') {
    if($file.Name -in @('lua.c','luac.c')) {continue}
    $object=Join-Path $output ($file.BaseName+'.o')
    if($DebugSymbols -or !(Test-Path -LiteralPath $object) -or $file.LastWriteTimeUtc -gt (Get-Item -LiteralPath $object).LastWriteTimeUtc) {
        & (Join-Path $CompilerDirectory 'gcc.exe') '-O2' @symbolArgs '-c' $file.FullName '-o' $object
        if($LASTEXITCODE -ne 0) {throw "Lua compile failed: $($file.Name)"}
    }
    $objects+=$object
}
& (Join-Path $CompilerDirectory 'ar.exe') 'rcs' (Join-Path $output 'liblua.a') @objects
if($LASTEXITCODE -ne 0) {throw 'Lua archive failed'}
Write-Output 'Built private static Lua 5.4.9 runtime (official source)'
