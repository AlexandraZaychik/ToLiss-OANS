param([string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',[switch]$DebugSymbols)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$source=Join-Path $projectRoot 'vendor/xptools'
$output=Join-Path $projectRoot 'build/dsf'
New-Item -ItemType Directory -Force $output | Out-Null
$objects=@()
$symbolArgs=@(if($DebugSymbols){'-g'})
$lzma=@('7zAlloc','7zArcIn','7zBuf','7zBuf2','7zCrc','7zCrcOpt','7zDec','7zFile','7zStream','Bcj2','Bra','Bra86','BraIA64','CpuArch','Delta','Lzma2Dec','LzmaDec')
$sources=@('DSF/DSFLib.cpp','Utils/XChunkyFileUtils.cpp','Utils/md5.c')
$sources+=@($lzma | ForEach-Object { 'lzma19/C/'+$_+'.c' })
foreach($relative in $sources) {
    $file=Join-Path $source $relative
    $object=Join-Path $output ([IO.Path]::GetFileNameWithoutExtension($file)+'.o')
    $compiler=if($file.EndsWith('.cpp')){'g++.exe'}else{'gcc.exe'}
    # Compile the small reader each build so vendored header patches are included.
    & (Join-Path $CompilerDirectory $compiler) '-O2' @symbolArgs '-w' '-DLIL=1' '-DBIG=0' '-DUSE_7Z=0' '-DNOMINMAX' "-I$source/DSF" "-I$source/Utils" "-I$source/lzma19/C" '-c' $file '-o' $object
    if($LASTEXITCODE -ne 0) {throw "DSF reader compile failed: $relative"}
    $objects+=$object
}
& (Join-Path $CompilerDirectory 'ar.exe') 'rcs' (Join-Path $output 'libdsf.a') @objects
if($LASTEXITCODE -ne 0) {throw 'DSF reader archive failed'}
Write-Output 'Built private DSFLib + LZMA runtime reader'
