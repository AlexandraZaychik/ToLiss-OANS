param(
    [string]$BaselineRoot='build/bulk-vertex-before',
    [string]$XPlaneRoot='G:\SteamLibrary\steamapps\common\X-Plane 12',
    [string]$Icao='ZUCK',
    [int]$Frames=180,
    [int]$Repeats=3,
    [double]$RangeNm=.5,
    [long]$ProcessorMask=1,
    [string]$OutputDirectory='build/bulk-vertex-benchmark'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$baseline=[IO.Path]::GetFullPath((Join-Path $projectRoot $BaselineRoot))
$buildings=Join-Path $projectRoot ('cache/dsf_buildings_'+$Icao+'.json')
if(!(Test-Path -LiteralPath $buildings)){$buildings=Join-Path $projectRoot ('cache/osm2_'+$Icao+'.json')}
$output=Join-Path $projectRoot $OutputDirectory
New-Item -ItemType Directory -Force $output | Out-Null
# Compile the current and baseline map_performance_probe before invoking this
# script. Each run uses one ND and the same core; no simulator process is used.
for($round=1;$round -le $Repeats;$round++) {
    $order=if($round%2){@('before','after')}else{@('after','before')}
    foreach($phase in $order) {
        $root=if($phase -eq 'before'){$baseline}else{Join-Path $projectRoot 'build'}
        $assets=if($phase -eq 'before'){Join-Path $baseline 'assets'}else{Join-Path $projectRoot 'assets'}
        $log=Join-Path $output ($phase+'-'+$round+'.txt')
        $err=Join-Path $output ($phase+'-'+$round+'.err')
        $arguments=@(('"'+$assets+'"'),('"'+$XPlaneRoot+'"'),$Icao,('"'+$buildings+'"'),$Frames,$RangeNm,1)
        $process=Start-Process -FilePath (Join-Path $root 'map_performance_probe.exe') -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError $err
        $process.ProcessorAffinity=[IntPtr]$ProcessorMask
        $process.WaitForExit()
        if($process.ExitCode -ne 0){throw "Benchmark failed: $err"}
        Write-Output "round=$round phase=$phase CPU-mask=$ProcessorMask"
        Get-Content -LiteralPath $log
    }
}
