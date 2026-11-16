param(
    [Parameter(Mandatory=$true)][string]$SourceObs,
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$BuildDirectory='build',
    [string]$PluginDll='',
    [string]$OutputDirectory='docs/images'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$runtime=(Resolve-Path -LiteralPath $SourceObs).Path
$qt=(Resolve-Path -LiteralPath $QtRoot).Path
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
if(-not $PluginDll){$PluginDll=Join-Path $build 'jp-fachina-graphics.dll'}
$plugin=(Resolve-Path -LiteralPath $PluginDll).Path
$gallery=Join-Path $build 'jp-gallery.exe'
if(-not(Test-Path -LiteralPath $gallery)){throw 'Compile o alvo jp-gallery com JP_BUILD_GALLERY=ON.'}
$output=if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $repo $OutputDirectory}
$isolated=Join-Path ([IO.Path]::GetTempPath()) ('jp-gallery-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $isolated|Out-Null
foreach($folder in @('bin','data','core')){if(Test-Path -LiteralPath (Join-Path $runtime $folder)){Copy-Item -LiteralPath (Join-Path $runtime $folder) -Destination $isolated -Recurse}}
if(-not(Test-Path -LiteralPath (Join-Path $runtime 'core'))){Copy-Item -LiteralPath (Join-Path $runtime 'obs-plugins') -Destination $isolated -Recurse}
$bins=Join-Path $isolated 'obs-plugins/64bit'
New-Item -ItemType Directory -Force -Path $bins|Out-Null
Copy-Item -LiteralPath $plugin -Destination (Join-Path $bins 'jp-fachina-graphics.dll')
Copy-Item -LiteralPath (Join-Path $qt 'bin/Qt6WebSockets.dll') -Destination $bins
$config=Join-Path $isolated 'config/obs-studio'
foreach($folder in @('basic/profiles/Demo','basic/scenes','plugin_config/obs-websocket','plugin_config/jp-fachina-graphics')){New-Item -ItemType Directory -Path (Join-Path $config $folder) -Force|Out-Null}
$encoding=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $isolated 'portable_mode.txt'),'')
[IO.File]::WriteAllText((Join-Path $config 'global.ini'),"[General]`nLastVersion=553648384`nEnableAutoUpdates=false`n[Basic]`nProfile=Demo`nProfileDir=Demo`nSceneCollection=Demo`nSceneCollectionFile=Demo`n",$encoding)
[IO.File]::WriteAllText((Join-Path $config 'user.ini'),"[General]`nFirstRun=true`nConfirmOnExit=false`n[BasicWindow]`nPreviewEnabled=true`nPreviewProgramMode=false`n",$encoding)
[IO.File]::WriteAllText((Join-Path $config 'basic/profiles/Demo/basic.ini'),"[General]`nName=Demo`n[Video]`nBaseCX=1920`nBaseCY=1080`nOutputCX=1920`nOutputCY=1080`nFPSType=0`nFPSCommon=30`n[Output]`nMode=Simple`n[SimpleOutput]`nStreamEncoder=x264`nRecEncoder=x264`n",$encoding)
$sceneName='Cena demonstrativa'
$scene=@{name=$sceneName;uuid=[Guid]::NewGuid().ToString();id='scene';versioned_id='scene';settings=@{items=@();id_counter=0};mixers=0;enabled=$true;muted=$false;volume=1.0}
$collection=@{name='Demo';current_scene=$sceneName;current_program_scene=$sceneName;sources=@($scene);scene_order=@(@{name=$sceneName;uuid=$scene.uuid});transitions=@();quick_transitions=@();current_transition='Fade';transition_duration=300}
[IO.File]::WriteAllText((Join-Path $config 'basic/scenes/Demo.json'),($collection|ConvertTo-Json -Depth 16),$encoding)
[IO.File]::WriteAllText((Join-Path $config 'plugin_config/jp-fachina-graphics/operator.json'),'{"version":1,"autoOpen":false,"mode":"dock"}',$encoding)
$listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start();$port=$listener.LocalEndpoint.Port;$listener.Stop()
$password=[Guid]::NewGuid().ToString('N')
$websocket=@{server_enabled=$true;server_port=$port;auth_required=$true;server_password=$password;alerts_enabled=$false;first_load=$false}
[IO.File]::WriteAllText((Join-Path $config 'plugin_config/obs-websocket/config.json'),($websocket|ConvertTo-Json),$encoding)
$process=$null;$previousPath=$env:PATH;$previousPlugin=$env:QT_PLUGIN_PATH;$previousPlatform=$env:QT_QPA_PLATFORM
try{
    $exe=Join-Path $isolated 'bin/64bit/obs64.exe'
    $process=Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe) -ArgumentList '--portable','--multi','--disable-updater','--disable-missing-files-check' -WindowStyle Hidden -PassThru
    $env:PATH=(Join-Path $qt 'bin')+';'+$env:PATH
    $env:QT_PLUGIN_PATH=Join-Path $qt 'plugins'
    $env:QT_QPA_PLATFORM='windows'
    & $gallery --repo $repo --output $output --port $port --password $password
    if($LASTEXITCODE -ne 0){throw 'Não foi possível gerar a galeria.'}
    Write-Output 'Galeria real Qt/OBS criada com dados exclusivamente demonstrativos.'
}finally{
    $env:PATH=$previousPath;$env:QT_PLUGIN_PATH=$previousPlugin;$env:QT_QPA_PLATFORM=$previousPlatform
    if($process -and -not $process.HasExited){$null=$process.CloseMainWindow();if(-not $process.WaitForExit(10000)){Stop-Process -Id $process.Id -Force}}
}
