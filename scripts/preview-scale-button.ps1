$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
$target='build/nd-four-preview'
New-Item -ItemType Directory -Force $target | Out-Null
$compiler='D:/Qt/Qt6/Tools/mingw1310_64/bin/g++.exe'
$flags=@('-std=c++17','-O2','-static','-Isrc','-DOANS_DEBUG=0','-DIBM=1','-DAPL=0','-DLIN=0','-DXPLM200','-DXPLM210','-DXPLM300','-DXPLM301','-DXPLM303','-DNOMINMAX','-DLIL=1','-IE:/cxx_project/_lib/SDK/CHeaders/XPLM','-Ivendor/lua-5.4.9/src','-Ivendor/xptools/DSF','-Ivendor/xptools/Utils','-Ivendor/xptools/lzma19/C')
& $compiler @flags '-shared' '-DXPLM=1' '-DOANS_PREVIEW_GL' 'tests/mock_xplm.cpp' '-lopengl32' "-Wl,--out-implib,$target/libXPLM.a" '-o' "$target/XPLM_64.dll"
if($LASTEXITCODE -ne 0){throw 'SDK build failed'}
$sources=@('src/core.cpp','src/runway_ahead.cpp','src/fms.cpp','src/aircraft/toliss/mcdu.cpp','src/aircraft/backend.cpp','src/aircraft/magknight/backend.cpp','src/aircraft/toliss/backend.cpp','src/aircraft/zibo/backend.cpp','src/aircraft/zibo/fms_xplm.cpp','src/aircraft/lr/backend.cpp','src/aircraft/lr/mcdu.cpp','src/aircraft/lr/mcdu_xplm.cpp','src/aircraft/toliss/mcdu_xplm.cpp','src/nd_ui.cpp','tools/scale_button_preview.cpp','src/aircraft/toliss/profile.cpp','src/a350_map.cpp','src/dsf_buildings.cpp')
$objects=@()
foreach($source in $sources){
$object=Join-Path $target (($source -replace '[/.]','_')+'.o'); $objects+=$object
$newest=(Get-ChildItem src -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime
if((Test-Path -LiteralPath $object) -and (Get-Item -LiteralPath $object).LastWriteTime -ge (Get-Item -LiteralPath $source).LastWriteTime -and (Get-Item -LiteralPath $object).LastWriteTime -ge $newest){continue}
& $compiler @flags '-c' $source '-o' $object
if($LASTEXITCODE -ne 0){throw "Compile failed: $source"}
}
& $compiler '-static' @objects 'build/lua/liblua.a' 'build/dsf/libdsf.a' "$target/libXPLM.a" '-lopengl32' '-lglu32' '-lgdi32' '-lwinhttp' '-o' "$target/nd_four_preview.exe"
if($LASTEXITCODE -ne 0){throw 'Link failed'}
& "$target/nd_four_preview.exe" $PWD.Path 'G:/SteamLibrary/steamapps/common/X-Plane 12' 'reports/runtime/scale-button-20261110'
if($LASTEXITCODE -ne 0){throw 'Render failed'}
