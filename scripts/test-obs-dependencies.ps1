param(
    [Parameter(Mandatory=$true)][string]$Plugin,
    [Parameter(Mandatory=$true)][string]$WebSockets,
    [Parameter(Mandatory=$true)][string]$ObsRuntime
)
$ErrorActionPreference='Stop'
if(-not(Get-Command dumpbin.exe -ErrorAction SilentlyContinue)){throw 'Use o Developer PowerShell MSVC x64 para verificar os imports.'}
$runtime=(Resolve-Path -LiteralPath $ObsRuntime).Path
$qtVersion=(Get-Item -LiteralPath (Join-Path $runtime 'Qt6Core.dll')).VersionInfo.FileVersion
$webSocketsVersion=(Get-Item -LiteralPath $WebSockets).VersionInfo.FileVersion
if($webSocketsVersion -ne $qtVersion){throw "Qt WebSockets $webSocketsVersion incompatível com Qt $qtVersion do OBS. Recompile usando o mesmo kit."}
foreach($binary in @($Plugin,$WebSockets)){
    $module=''
    $imports=@{}
    foreach($line in (& dumpbin.exe /nologo /imports $binary)){
        if($line -match '^    (\S+\.dll)\s*$'){
            $module=$Matches[1]
            $imports[$module]=@()
        }elseif($line -match '^\s+[0-9A-F]+ ([?A-Za-z_][^\s]*)\s*$' -and $module){
            $imports[$module]+=$Matches[1]
        }
    }
    if($LASTEXITCODE -ne 0 -or $imports.Count -eq 0){throw "Não foi possível examinar os imports de $binary."}
    foreach($module in @('obs.dll','obs-frontend-api.dll','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','Qt6WebSockets.dll')){
        if(-not $imports.ContainsKey($module)){continue}
        $dependency=if($module -eq 'Qt6WebSockets.dll'){$WebSockets}else{Join-Path $runtime $module}
        $exports=@{}
        foreach($line in (& dumpbin.exe /nologo /exports $dependency)){
            if($line -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)'){$exports[$Matches[1]]=$true}
        }
        if($LASTEXITCODE -ne 0 -or $exports.Count -eq 0){throw "Não foi possível examinar os exports de $dependency."}
        $missing=@($imports[$module]|Where-Object {-not $exports.ContainsKey($_)})
        if($missing.Count){throw "$binary depende de símbolos ausentes em ${module}: $($missing -join ', ')"}
    }
}
Write-Output "PASS dependências OBS/Qt: Qt $qtVersion, imports do plugin e de Qt WebSockets resolvidos."
