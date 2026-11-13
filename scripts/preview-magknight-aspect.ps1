$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
$previewRoot='build/787-aspect-preview'
New-Item -ItemType Directory -Force -Path $previewRoot | Out-Null
$compiler='D:/Qt/Qt6/Tools/mingw1310_64/bin/g++.exe'
$env:PATH=(Split-Path $compiler)+';'+$env:PATH
& cmake --build build/pruned --parallel 4 --target ToLissOANS
if($LASTEXITCODE -ne 0){throw 'Release build failed'}
$flags=@('-std=c++17','-O2','-static','-Isrc','-DOANS_DEBUG=0','-DIBM=1','-DAPL=0','-DLIN=0','-DXPLM200','-DXPLM210','-DXPLM300','-DXPLM301','-DXPLM303','-DNOMINMAX','-IE:/cxx_project/_lib/SDK/CHeaders/XPLM')
& $compiler @flags -shared -DXPLM=1 -DOANS_PREVIEW_GL tests/mock_xplm.cpp -lopengl32 "-Wl,--out-implib,$previewRoot/libXPLM.a" -o "$previewRoot/XPLM_64.dll"
if($LASTEXITCODE -ne 0){throw 'Preview SDK build failed'}
$objects=@(Get-ChildItem build/pruned/CMakeFiles/ToLissOANS.dir/src -Recurse -Filter '*.obj' | Where-Object Name -ne 'plugin.cpp.obj' | ForEach-Object FullName)
& $compiler @flags tools/magknight_aspect_preview.cpp @objects build/pruned/liba350_map.a build/pruned/liboans_toliss_fms.a build/pruned/liboans_dsf.a build/pruned/liboans_core.a build/pruned/liboans_dsf_reader.a build/pruned/liboans_md5.a build/pruned/liboans_lua.a "$previewRoot/libXPLM.a" -lopengl32 -lglu32 -lgdi32 -lwinhttp -o "$previewRoot/preview.exe"
if($LASTEXITCODE -ne 0){throw 'Preview build failed'}
& "$previewRoot/preview.exe" $PWD.Path (Join-Path $PWD 'reports/runtime/787-aspect')
if($LASTEXITCODE -ne 0){throw 'Preview failed'}
