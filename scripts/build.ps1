param(
    [string]$SdkRoot = 'E:\cxx_project\_lib\SDK',
    [string]$CompilerDirectory = 'D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [switch]$SkipTests,
    [ValidateSet("Both","Release","Debug")][string]$BuildFlavor = "Both"
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $CompilerDirectory 'g++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw "Compiler not found: $compiler" }
if (!(Test-Path -LiteralPath (Join-Path $SdkRoot 'CHeaders\XPLM\XPLMPlugin.h'))) { throw "X-Plane SDK not found: $SdkRoot" }
$buildDir = Join-Path $projectRoot 'build'
$packageDir = Join-Path $projectRoot 'dist\ToLissOANS'
New-Item -ItemType Directory -Force -Path $buildDir,(Join-Path $packageDir '64') | Out-Null
$common = @('-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-static','-Isrc')
Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'build-lua.ps1') -CompilerDirectory $CompilerDirectory -DebugSymbols
    & (Join-Path $PSScriptRoot 'build-dsf.ps1') -CompilerDirectory $CompilerDirectory -DebugSymbols
    $dsfArgs = @('-DNOMINMAX','-DLIL=1','-Ivendor/xptools/DSF','-Ivendor/xptools/Utils','-Ivendor/xptools/lzma19/C','src/dsf_buildings.cpp',(Join-Path $buildDir 'dsf/libdsf.a'))
    $luaArgs = @('-Ivendor/lua-5.4.9/src','src/a350_map.cpp',(Join-Path $buildDir 'lua/liblua.a'))
    if (!$SkipTests) {
        & (Join-Path $PSScriptRoot 'test-suite.ps1') -UnitOnly -SdkRoot $SdkRoot -CompilerDirectory $CompilerDirectory
        if ($LASTEXITCODE -ne 0) { throw 'Pruned test suite failed' }
    }
    & $compiler @common 'src/core.cpp' 'tools/dsf_buildings_probe.cpp' @dsfArgs '-o' (Join-Path $buildDir 'dsf_buildings_probe.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Runtime DSF probe compilation failed' }
    & $compiler @common 'src/fms.cpp' 'src/aircraft/lr/mcdu.cpp' 'tools/lr_fms_probe.cpp' '-o' (Join-Path $buildDir 'lr_fms_probe.exe')
    if ($LASTEXITCODE -ne 0) { throw 'LR FMS probe compilation failed' }
    & $compiler @common 'src/fms.cpp' 'src/aircraft/toliss/mcdu.cpp' 'tools/fms_probe.cpp' '-o' (Join-Path $buildDir 'fms_probe.exe')
    if ($LASTEXITCODE -ne 0) { throw 'FMS live probe compilation failed' }
    & $compiler @common 'src/core.cpp' 'tools/apt_inspect.cpp' '-o' (Join-Path $buildDir 'apt_inspect.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Airport inspector compilation failed' }
    $flavors = if($BuildFlavor -eq 'Both'){@('Release','Debug')}else{@($BuildFlavor)}
    foreach($flavor in $flavors) {
        $packageName = if($flavor -eq 'Debug'){'ToLissOANS-debug'}else{'ToLissOANS'}
        $packageDir = Join-Path $projectRoot ('dist/'+$packageName)
        New-Item -ItemType Directory -Force (Join-Path $packageDir '64') | Out-Null
        $flavorArgs = if($flavor -eq 'Debug'){@('-DOANS_DEBUG=1','-Og','-g','-fno-omit-frame-pointer')}else{@('-DOANS_DEBUG=0','-O2','-s')}
        & $compiler @common @flavorArgs '-shared' '-DIBM=1' '-DAPL=0' '-DLIN=0' '-DXPLM200' '-DXPLM210' '-DXPLM300' '-DXPLM301' '-DXPLM303' '-DNOMINMAX' "-I$SdkRoot\CHeaders\XPLM" 'src/core.cpp' 'src/runway_ahead.cpp' 'src/fms.cpp' 'src/aircraft/toliss/mcdu.cpp' 'src/aircraft/backend.cpp' 'src/aircraft/magknight/backend.cpp' 'src/aircraft/toliss/backend.cpp' 'src/aircraft/zibo/backend.cpp' 'src/aircraft/zibo/fms_xplm.cpp' 'src/aircraft/lr/backend.cpp' 'src/aircraft/lr/mcdu.cpp' 'src/aircraft/lr/mcdu_xplm.cpp' 'src/aircraft/toliss/mcdu_xplm.cpp' 'src/nd_ui.cpp' 'src/plugin.cpp' 'src/aircraft/toliss/profile.cpp' @luaArgs @dsfArgs (Join-Path $SdkRoot 'Libraries\Win\XPLM_64.lib') '-lopengl32' '-lglu32' '-lgdi32' '-lwinhttp' '-o' (Join-Path $packageDir '64\win.xpl')
        if ($LASTEXITCODE -ne 0) { throw 'Native plugin compilation failed' }
        Copy-Item -LiteralPath (Join-Path $projectRoot 'config.ini') -Destination $packageDir -Force
        Copy-Item -LiteralPath (Join-Path $projectRoot 'config') -Destination $packageDir -Recurse -Force
        # Generated packages now store aircraft profiles only by family.
        Get-ChildItem -LiteralPath (Join-Path $packageDir 'config') -File -Filter '*.json' | ForEach-Object {
            Remove-Item -LiteralPath $_.FullName
        }
        Copy-Item -LiteralPath (Join-Path $projectRoot 'assets') -Destination $packageDir -Recurse -Force
        if(Test-Path -LiteralPath (Join-Path $projectRoot 'cache')) {
            Copy-Item -LiteralPath (Join-Path $projectRoot 'cache') -Destination $packageDir -Recurse -Force
        }
        # Exported developer sources deliberately omit Markdown/reports. Those
        # optional documents must not prevent a clean build from that source tree.
        foreach ($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','THIRD_PARTY_NOTICES.txt','接手说明.md')) {
            $document=Join-Path $projectRoot $file
            if(Test-Path -LiteralPath $document) { Copy-Item -LiteralPath $document -Destination $packageDir -Force }
        }
        $reportSource=Join-Path $projectRoot 'reports'
        if(Test-Path -LiteralPath $reportSource) {
            $reportDir=Join-Path $packageDir 'reports'
            New-Item -ItemType Directory -Force $reportDir | Out-Null
            Get-ChildItem -LiteralPath $reportSource -File -Filter '*.md' | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $reportDir -Force
            }
        }
        $flavor.ToLowerInvariant() | Set-Content -LiteralPath (Join-Path $packageDir 'build-flavor.txt') -Encoding ascii
        if($flavor -eq 'Debug') {
            & (Join-Path $PSScriptRoot 'build-log-collector.ps1') -CompilerDirectory $CompilerDirectory -Destination $packageDir
        }
        Write-Output "Built $flavor native plugin: $(Join-Path $packageDir '64\win.xpl')"
    }
} finally { Pop-Location }
