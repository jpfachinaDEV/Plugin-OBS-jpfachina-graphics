param([string]$SourceObs = '.deps/obs-test',[string]$BuildDirectory = 'build',[string]$QtRoot = '.deps/qt',[switch]$LegacyMigration,[switch]$WebSocket)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$sourceRoot=[IO.Path]::GetFullPath((Join-Path $repoRoot $SourceObs))
$testRoot=Join-Path $repoRoot ('.deps/obs-operator-smoke-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
if(-not(Test-Path -LiteralPath (Join-Path $sourceRoot 'bin/64bit/obs64.exe'))){throw 'Prepare a cópia OBS de teste antes de executar.'}
New-Item -ItemType Directory -Path $testRoot | Out-Null
foreach($folder in 'bin','data','obs-plugins','core','config'){$source=Join-Path $sourceRoot $folder;if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination $testRoot -Recurse}}
Copy-Item -LiteralPath (Join-Path $repoRoot ($BuildDirectory+'/jp-fachina-graphics.dll')) -Destination (Join-Path $testRoot 'obs-plugins/64bit/jp-fachina-graphics.dll') -Force
[IO.File]::WriteAllText((Join-Path $testRoot 'portable_mode.txt'),'')
Copy-Item -LiteralPath (Join-Path $repoRoot ($QtRoot+'/bin/Qt6WebSockets.dll')) -Destination (Join-Path $testRoot 'obs-plugins/64bit/Qt6WebSockets.dll') -Force
$webSocketPort=0;$webSocketPassword=''
if($WebSocket){
    $listener=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0);$listener.Start();$webSocketPort=$listener.LocalEndpoint.Port;$listener.Stop();$webSocketPassword=[Guid]::NewGuid().ToString('N')
    $webSocketConfig=Join-Path $testRoot 'config/obs-studio/plugin_config/obs-websocket/config.json'
    $settings=@{server_enabled=$true;server_port=$webSocketPort;auth_required=$true;server_password=$webSocketPassword;alerts_enabled=$false;first_load=$false}
    [IO.File]::WriteAllText($webSocketConfig,($settings|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
}
$libraryRoot=Join-Path $testRoot 'config/obs-studio/plugin_config/jp-fachina-graphics'
New-Item -ItemType Directory -Path (Join-Path $libraryRoot 'media') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'assets/branding/jpfachina-sync-original.png') -Destination (Join-Path $libraryRoot 'media/logo.png')
$draft=@{x=90;y=90;width=600;height=237;z=0;transition='fade';duration=200;loop=$true}
$item=@{id='smoke-logo';name='Logo original';category='Logos';file='media/logo.png';mime='image/png';bytes=(Get-Item -LiteralPath (Join-Path $libraryRoot 'media/logo.png')).Length;origin='isolated-test';draft=$draft;live=$draft;generation=1}
Copy-Item -LiteralPath (Join-Path $repoRoot '.deps/render-fixtures/sample.mp4') -Destination (Join-Path $libraryRoot 'media/video.mp4')
$videoDraft=@{x=500;y=500;width=320;height=180;z=1;transition='none';duration=0;loop=$false;endBehavior='out'}
$videoItem=@{id='smoke-video';name='Video teste';category='Vídeos';file='media/video.mp4';mime='video/mp4';bytes=(Get-Item -LiteralPath (Join-Path $libraryRoot 'media/video.mp4')).Length;origin='isolated-test';draft=$videoDraft;live=$null;generation=0}
$project=@{id='smoke-project';key=('a'*64);name='Validação nativa isolada';width=1920;height=1080;categories=@('Logos','Tarjetas','Banners','Overlays','Outros','Vídeos');media=@($item,$videoItem)}
$library=@{schema=2;port=28741;active='smoke-project';projects=@($project)}
$utf8=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $libraryRoot 'library.json'),($library|ConvertTo-Json -Depth 20),$utf8)
[IO.File]::WriteAllText((Join-Path $libraryRoot 'operator.json'),'{"version":1,"autoOpen":true,"mode":"window","geometry":[40,40,1366,768]}',$utf8)
$sceneFile=Get-ChildItem -LiteralPath (Join-Path $testRoot 'config/obs-studio/basic/scenes') -Filter '*.json' | Select-Object -First 1
if(-not $sceneFile){throw 'Coleção de cenas de referência indisponível.'}
$collection=Get-Content -LiteralPath $sceneFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
$scene=$collection.sources | Where-Object { $_.id -eq 'scene' } | Select-Object -First 1
if(-not $scene){throw 'Cena de referência indisponível.'}
$state=@{width=1920;height=1080;items=@(@{id='smoke-logo';name='Logo original';mime='image/png';path=((Join-Path $libraryRoot 'media/logo.png').Replace('\','/'));layout=$draft;generation=1})}
$native=@{name='JP Native Smoke';uuid=[Guid]::NewGuid().ToString();id='jp_fachina_composition';versioned_id='jp_fachina_composition';settings=@{jp_fachina_project='smoke-project';state=($state|ConvertTo-Json -Depth 15 -Compress)};mixers=0;volume=0;enabled=$true;muted=$true}
if($LegacyMigration){$native.id='browser_source';$native.versioned_id='browser_source';$native.settings=@{jp_fachina_project='smoke-project';url=('http://127.0.0.1:28741/overlay/'+('a'*64));width=1920;height=1080;shutdown=$false;restart_when_active=$false}}
$scene.settings.items=@(@{name=$native.name;source_uuid=$native.uuid;id=1;visible=$true;locked=$false;rot=0;pos=@{x=0;y=0};scale=@{x=1;y=1};align=5;bounds_type=0;bounds_align=0;bounds=@{x=0;y=0};crop_left=0;crop_right=0;crop_top=0;crop_bottom=0})
if($LegacyMigration){$scene.settings.items[0].pos=@{x=37;y=48};$scene.settings.items[0].scale=@{x=0.8;y=0.8};$scene.settings.items[0].rot=5;$scene.settings.items[0].crop_left=10}
$collection.sources=@($native)+@($collection.sources | Where-Object {$_.id -eq 'scene'})
[IO.File]::WriteAllText($sceneFile.FullName,($collection|ConvertTo-Json -Depth 35),$utf8)
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class JpOperatorSmokeWindows {
 public delegate bool Callback(IntPtr window,IntPtr data);
 public static void InvokeAsync(object pattern){System.Threading.Tasks.Task.Run(()=>pattern.GetType().GetMethod("Invoke").Invoke(pattern,null));}
 [DllImport("user32.dll")] public static extern int GetGuiResources(IntPtr process,int flag);
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left,top,right,bottom; }
 [StructLayout(LayoutKind.Sequential)] public struct Point { public int x,y; }
 [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback,IntPtr data);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window,StringBuilder text,int size);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr key,IntPtr flags);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code,uint mapType);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr window,ref Point point);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window,IntPtr dc,uint flags);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static IntPtr Find(uint process,string prefix) {
  IntPtr result=IntPtr.Zero;
  EnumWindows((window,data)=>{uint owner;GetWindowThreadProcessId(window,out owner);if(owner==process){var title=new StringBuilder(512);GetWindowText(window,title,512);if(title.ToString().StartsWith(prefix)){result=window;return false;}}return true;},IntPtr.Zero);
  return result;
 }
 public static int[] Bounds(IntPtr window){Rect r;GetWindowRect(window,out r);return new int[]{r.left,r.top,r.right-r.left,r.bottom-r.top};}
 public static int[] ClientPoint(IntPtr window,int x,int y){Point p=new Point{x=x,y=y};ScreenToClient(window,ref p);return new int[]{p.x,p.y};}
}
'@
[JpOperatorSmokeWindows]::SetThreadDpiAwarenessContext([IntPtr](-4)) | Out-Null
function Send-TestKey([IntPtr]$window,[int]$key){
    $scan=[JpOperatorSmokeWindows]::MapVirtualKey([uint32]$key,0)
    $keyData=1L -bor ([long]$scan -shl 16)
    if($key -ge 0x21 -and $key -le 0x2E){$keyData=$keyData -bor 0x01000000L}
    [JpOperatorSmokeWindows]::PostMessage($window,0x100,[IntPtr]$key,[IntPtr]$keyData) | Out-Null
    [JpOperatorSmokeWindows]::PostMessage($window,0x101,[IntPtr]$key,[IntPtr]($keyData -bor 0xC0000000L)) | Out-Null
}
$obsExe=Join-Path $testRoot 'bin/64bit/obs64.exe'
$testProcess=$null
try{
    $testProcess=Start-Process -FilePath $obsExe -WorkingDirectory (Split-Path -Parent $obsExe) -ArgumentList '--portable','--multi','--disable-updater','--disable-missing-files-check' -WindowStyle Hidden -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(45);$operatorHandle=[IntPtr]::Zero
    while([DateTime]::UtcNow -lt $deadline -and $operatorHandle -eq [IntPtr]::Zero){if($testProcess.HasExited){throw "OBS encerrou prematuramente: $($testProcess.ExitCode)"};$operatorHandle=[JpOperatorSmokeWindows]::Find([uint32]$testProcess.Id,'JP Fachina Graphics');Start-Sleep -Milliseconds 200}
    if($operatorHandle -eq [IntPtr]::Zero){throw 'A janela de operação não abriu.'}
    [JpOperatorSmokeWindows]::SetForegroundWindow($operatorHandle) | Out-Null
    Start-Sleep -Milliseconds 1200
    $operatorElement=[Windows.Automation.AutomationElement]::FromHandle($operatorHandle)
    $nameCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Logo original'
    $typeCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::ControlTypeProperty),([Windows.Automation.ControlType]::Button)
    $itemCondition=New-Object Windows.Automation.AndCondition $nameCondition,$typeCondition
    $accessibilityDeadline=[DateTime]::UtcNow.AddSeconds(10)
    $logoElement=$null
    while($null -eq $logoElement -and [DateTime]::UtcNow -lt $accessibilityDeadline){
        $operatorElement=[Windows.Automation.AutomationElement]::FromHandle($operatorHandle)
        $logoElement=$operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$itemCondition)
        if($null -eq $logoElement){Start-Sleep -Milliseconds 300}
    }
    if($null -eq $logoElement){
        $nodes=$operatorElement.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)
        $names=@($nodes|ForEach-Object {@{name=$_.Current.Name;type=$_.Current.ControlType.ProgrammaticName;id=$_.Current.AutomationId}})
        [IO.File]::WriteAllText((Join-Path $repoRoot 'artifacts/accessibility-debug.json'),($names|ConvertTo-Json -Depth 4),$utf8)
        throw 'A biblioteca Qt não expôs a peça à acessibilidade do Windows. Consulte artifacts/accessibility-debug.json.'
    }
    if($LegacyMigration){
        $scenesCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Link OBS'
        $scenesButton=$operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$scenesCondition)
        [JpOperatorSmokeWindows]::InvokeAsync($scenesButton.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern))
        Start-Sleep -Milliseconds 700
        $scenesHandle=[IntPtr]::Zero;$linkDeadline=[DateTime]::UtcNow.AddSeconds(8)
        while($scenesHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $linkDeadline){$scenesHandle=[JpOperatorSmokeWindows]::Find([uint32]$testProcess.Id,'Link OBS');Start-Sleep -Milliseconds 100}
        if($scenesHandle -eq [IntPtr]::Zero){throw 'Diálogo Cenas indisponível.'}
        $scenesElement=[Windows.Automation.AutomationElement]::FromHandle($scenesHandle)
        $tabCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Cenas e fontes do OBS'
        $tab=$scenesElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$tabCondition)
        if($null -eq $tab){throw 'Aba de cenas indisponível dentro de Link OBS.'}
        $tab.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select()
        Start-Sleep -Milliseconds 300
        $associateCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Associar composição'
        $associate=$scenesElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$associateCondition)
        if($null -eq $associate){throw 'Associação indisponível no diálogo Cenas.'}
        [JpOperatorSmokeWindows]::InvokeAsync($associate.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern))
        $confirmHandle=[IntPtr]::Zero;$confirmDeadline=[DateTime]::UtcNow.AddSeconds(8)
        while($confirmHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $confirmDeadline){$confirmHandle=[JpOperatorSmokeWindows]::Find([uint32]$testProcess.Id,'Associar composição nativa');Start-Sleep -Milliseconds 150}
        if($confirmHandle -eq [IntPtr]::Zero){throw 'Associação não apresentou confirmação.'}
        $confirmElement=[Windows.Automation.AutomationElement]::FromHandle($confirmHandle)
        $yesCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Sim'
        $yes=$confirmElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$yesCondition)
        if($null -eq $yes){$yesCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Yes';$yes=$confirmElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$yesCondition)}
        if($null -eq $yes){throw 'Confirmação de migração indisponível.'}
        $yes.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 1300
        Send-TestKey $scenesHandle 0x1B
        Start-Sleep -Milliseconds 300
    }
    if($WebSocket){
        $recordDirectory=Join-Path $testRoot 'recordings';New-Item -ItemType Directory -Path $recordDirectory -Force|Out-Null
        $env:JP_TEST_RECORD_DIRECTORY=$recordDirectory;$previousPlatform=$env:QT_QPA_PLATFORM;$previousPath=$env:PATH
        try{$env:QT_QPA_PLATFORM='minimal';$env:PATH=(Join-Path $repoRoot ($QtRoot+'/bin'))+';'+$env:PATH;Push-Location $repoRoot;try{& (Join-Path $repoRoot ($BuildDirectory+'/jp-websocket-tests.exe')) --real $webSocketPort $webSocketPassword;if($LASTEXITCODE -ne 0){throw 'Teste real WebSocket falhou.'}}finally{Pop-Location}}finally{$env:QT_QPA_PLATFORM=$previousPlatform;$env:PATH=$previousPath;Remove-Item Env:JP_TEST_RECORD_DIRECTORY -ErrorAction SilentlyContinue}
    }
    $logoElement.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 300
    $editCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Editar posição'
    $editButton=$operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$editCondition)
    if($null -eq $editButton){$editCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),"Editar`nposição";$editButton=$operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$editCondition)}
    if($null -eq $editButton){throw 'Editar posição indisponível.'}
    $editorHandles=@();$libraryBeforeEdit=[IO.File]::ReadAllText((Join-Path $libraryRoot 'library.json'))
    for($editorCycle=0;$editorCycle -lt 5;$editorCycle++){
        [JpOperatorSmokeWindows]::InvokeAsync($editButton.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern))
        $editorHandle=[IntPtr]::Zero;$editorDeadline=[DateTime]::UtcNow.AddSeconds(8)
        while($editorHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $editorDeadline){$editorHandle=[JpOperatorSmokeWindows]::Find([uint32]$testProcess.Id,'Editar posição');Start-Sleep -Milliseconds 100}
        if($editorHandle -eq [IntPtr]::Zero){throw 'Editor por categoria não abriu.'}
        [JpOperatorSmokeWindows]::SetForegroundWindow($editorHandle)|Out-Null
        if($editorCycle -eq 0){$initialUserObjects=[JpOperatorSmokeWindows]::GetGuiResources($testProcess.Handle,1);$initialGdiObjects=[JpOperatorSmokeWindows]::GetGuiResources($testProcess.Handle,0)}
        for($movement=0;$movement -lt 100;$movement++){Send-TestKey $editorHandle 0x27;Send-TestKey $editorHandle 0x25}
        Start-Sleep -Milliseconds 300
        Send-TestKey $editorHandle 0x1B
        Start-Sleep -Milliseconds 200
        $editorHandles+=@{user=[JpOperatorSmokeWindows]::GetGuiResources($testProcess.Handle,1);gdi=[JpOperatorSmokeWindows]::GetGuiResources($testProcess.Handle,0)}
    }
    if([IO.File]::ReadAllText((Join-Path $libraryRoot 'library.json')) -ne $libraryBeforeEdit){throw 'Movimentar/cancelar o editor alterou a biblioteca ou o programa.'}
    $finalUserObjects=[JpOperatorSmokeWindows]::GetGuiResources($testProcess.Handle,1);$finalGdiObjects=[JpOperatorSmokeWindows]::GetGuiResources($testProcess.Handle,0)
    if($finalUserObjects -gt $initialUserObjects+20 -or $finalGdiObjects -gt $initialGdiObjects+80){throw 'Recursos de janela cresceram durante a movimentação contínua.'}
    $logoElement.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 500
    $takeCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Take'
    $takeElement=$operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$takeCondition)
    if($null -eq $takeElement -or -not $takeElement.Current.IsEnabled){throw 'TAKE indisponível após selecionar conteúdo.'}
    $takeElement.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 900
    $savedAfterTake=Get-Content -LiteralPath (Join-Path $libraryRoot 'library.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($savedAfterTake.projects[0].media[0].generation -ne 2){throw 'TAKE não gerou uma nova reprodução.'}
    # The production runtime, not a test adapter, detects media end and issues OUT.
    $tabCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'ABA 2'
    $operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$tabCondition).GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 300
    $videoCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Video teste'
    $videoElement=$operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$videoCondition)
    if($null -eq $videoElement){throw 'Miniatura do vídeo indisponível na ABA 2.'}
    $videoElement.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 300
    $takeElement.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    $videoDeadline=[DateTime]::UtcNow.AddSeconds(10);$videoEnded=$false
    while([DateTime]::UtcNow -lt $videoDeadline){Start-Sleep -Milliseconds 200;$videoState=Get-Content -LiteralPath (Join-Path $libraryRoot 'library.json') -Raw -Encoding UTF8|ConvertFrom-Json;if($videoState.projects[0].media[1].generation -eq 1 -and $null -eq $videoState.projects[0].media[1].live){$videoEnded=$true;break}}
    if(-not $videoEnded){throw 'Vídeo sem loop não executou OUT automático ao terminar.'}
    $loopCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Reproduzir em loop'
    $loopElement=$operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$loopCondition)
    if($null -eq $loopElement){throw 'Controle de loop indisponível.'}
    $loopElement.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern).Toggle()
    Start-Sleep -Milliseconds 300
    $takeElement.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 3200
    $videoState=Get-Content -LiteralPath (Join-Path $libraryRoot 'library.json') -Raw -Encoding UTF8|ConvertFrom-Json
    if($null -eq $videoState.projects[0].media[1].live -or -not $videoState.projects[0].media[1].live.loop){throw 'Loop não manteve a reprodução no compositor.'}
    if($null -eq $videoState.projects[0].media[0].live){throw 'O TAKE do vídeo retirou o logo de outra categoria.'}
    $outCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'Out'
    $operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$outCondition).GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 500
    $tabCondition=New-Object Windows.Automation.PropertyCondition ([Windows.Automation.AutomationElement]::NameProperty),'ABA 1'
    $operatorElement.FindFirst([Windows.Automation.TreeScope]::Descendants,$tabCondition).GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 300
    $normal=[JpOperatorSmokeWindows]::Bounds($operatorHandle)
    [JpOperatorSmokeWindows]::SetForegroundWindow($operatorHandle) | Out-Null
    Send-TestKey $operatorHandle 0x7A
    Start-Sleep -Milliseconds 900
    $full=[JpOperatorSmokeWindows]::Bounds($operatorHandle)
    $monitor=[Windows.Forms.Screen]::FromHandle($operatorHandle).Bounds
    $fullscreenOk=($full[0] -eq $monitor.X -and $full[1] -eq $monitor.Y -and $full[2] -eq $monitor.Width -and $full[3] -eq $monitor.Height)
    if(-not $fullscreenOk){throw "F11 não ocupou o monitor: $full vs $monitor"}
    $bitmap=New-Object Drawing.Bitmap $full[2],$full[3];$graphics=[Drawing.Graphics]::FromImage($bitmap)
    if([JpOperatorSmokeWindows]::GetForegroundWindow() -eq $operatorHandle){$graphics.CopyFromScreen($full[0],$full[1],0,0,$bitmap.Size);$graphics.Dispose()}
    else{$dc=$graphics.GetHdc();try{[JpOperatorSmokeWindows]::PrintWindow($operatorHandle,$dc,2) | Out-Null}finally{$graphics.ReleaseHdc($dc);$graphics.Dispose()}}
    $logoBounds=$logoElement.Current.BoundingRectangle
    $tallyRed=$false
    for($sampleX=[int]($logoBounds.X-$full[0]+10);$sampleX -lt [int]($logoBounds.Right-$full[0]-10);$sampleX+=10){$pixel=$bitmap.GetPixel($sampleX,[int]($logoBounds.Y-$full[1]+2));if($pixel.R -gt 180 -and $pixel.G -lt 100 -and $pixel.B -lt 110){$tallyRed=$true;break}}
    if(-not $tallyRed){$bitmap.Dispose();throw 'O programa OBS não confirmou a borda vermelha do logo.'}
    $bitmap.Save((Join-Path $repoRoot 'artifacts/obs-native-operator.png'));$bitmap.Dispose()
    Send-TestKey $operatorHandle 0x1B
    Start-Sleep -Milliseconds 700
    $restored=[JpOperatorSmokeWindows]::Bounds($operatorHandle)
    $preferences=Get-Content -LiteralPath (Join-Path $libraryRoot 'operator.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($preferences.mode -ne 'window'){throw 'Esc não restaurou o modo janela.'}
    $listeners=@(Get-NetTCPConnection -OwningProcess $testProcess.Id -State Listen -ErrorAction SilentlyContinue | Where-Object {$_.LocalPort -eq 28741})
    if($listeners.Count -ne 0){throw 'O modo nativo abriu a porta do servidor legado sem necessidade.'}
    [JpOperatorSmokeWindows]::PostMessage($operatorHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 700
    $afterClose=Get-Content -LiteralPath (Join-Path $libraryRoot 'operator.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($afterClose.mode -ne 'dock'){throw 'Fechar operador não retornou ao dock.'}
    $mainHandle=[JpOperatorSmokeWindows]::Find([uint32]$testProcess.Id,'OBS ')
    if($mainHandle -eq [IntPtr]::Zero){throw 'Janela principal OBS indisponível.'}
    [JpOperatorSmokeWindows]::PostMessage($mainHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
    if(-not $testProcess.WaitForExit(15000)){throw 'OBS não encerrou normalmente no prazo.'}
    if($testProcess.ExitCode -ne 0){throw "Código de saída OBS: $($testProcess.ExitCode)"}
    $log=Get-ChildItem -LiteralPath (Join-Path $testRoot 'config/obs-studio/logs') -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $logText=Get-Content -LiteralPath $log.FullName -Raw -Encoding UTF8
    if(-not $logText.Contains('Native Qt dock and isolated compositor loaded')){throw 'Log não confirmou carregamento da DLL.'}
    if($logText.Contains('Failed to create platform window')){throw 'OBS registrou falhas de criação de janelas durante o teste do editor.'}
    $saved=Get-Content -LiteralPath (Join-Path $libraryRoot 'library.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($saved.projects[0].media[0].generation -ne 2){throw 'Alternância de janela alterou geração de reprodução.'}
    if($LegacyMigration){$savedScene=Get-Content -LiteralPath $sceneFile.FullName -Raw -Encoding UTF8|ConvertFrom-Json;$savedItems=@(($savedScene.sources|Where-Object {$_.id -eq 'scene'}|Select-Object -First 1).settings.items);$nativeSources=@($savedScene.sources|Where-Object {$_.id -eq 'jp_fachina_composition'});if($nativeSources.Count -ne 1 -or $savedItems.Count -ne 1 -or $savedItems[0].name -ne $nativeSources[0].name){throw 'Migração duplicou ou não substituiu o item antigo.'};if([Math]::Abs($savedItems[0].pos.x-37) -gt 0.1 -or [Math]::Abs($savedItems[0].pos.y-48) -gt 0.1 -or $savedItems[0].crop_left -ne 10 -or $savedItems[0].rot -ne 5){throw 'Migração não preservou transformação/recorte.'}}
    # Reopen the same profile with the exact window preferences saved after Esc.
    [IO.File]::WriteAllText((Join-Path $libraryRoot 'operator.json'),($preferences|ConvertTo-Json -Depth 10),$utf8)
    $testProcess=Start-Process -FilePath $obsExe -WorkingDirectory (Split-Path -Parent $obsExe) -ArgumentList '--portable','--multi','--disable-updater','--disable-missing-files-check' -WindowStyle Hidden -PassThru
    $reopened=[IntPtr]::Zero;$restartDeadline=[DateTime]::UtcNow.AddSeconds(40)
    while($reopened -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $restartDeadline){if($testProcess.HasExited){throw 'OBS não reabriu o perfil de teste.'};$reopened=[JpOperatorSmokeWindows]::Find([uint32]$testProcess.Id,'JP Fachina Graphics');Start-Sleep -Milliseconds 200}
    if($reopened -eq [IntPtr]::Zero){throw 'Operador não restaurou a janela após reiniciar o OBS.'}
    Start-Sleep -Milliseconds 700
    $reopenedBounds=[JpOperatorSmokeWindows]::Bounds($reopened)
    if([Math]::Abs($reopenedBounds[2]-$restored[2]) -gt 20 -or [Math]::Abs($reopenedBounds[3]-$restored[3]) -gt 20){throw 'Dimensões da janela não foram restauradas.'}
    $closeDeadline=[DateTime]::UtcNow.AddSeconds(20)
    while(-not $testProcess.HasExited -and [DateTime]::UtcNow -lt $closeDeadline){
        $mainHandle=[JpOperatorSmokeWindows]::Find([uint32]$testProcess.Id,'OBS ')
        if($mainHandle -ne [IntPtr]::Zero){[JpOperatorSmokeWindows]::PostMessage($mainHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null}
        $null=$testProcess.WaitForExit(1000)
    }
    if(-not $testProcess.HasExited -or $testProcess.ExitCode -ne 0){throw 'Falha no encerramento após reiniciar.'}
    $result=@{passed=$true;date='2026-11-16';isolatedRoot=$testRoot;dllSha256=(Get-FileHash -LiteralPath (Join-Path $repoRoot ($BuildDirectory+'/jp-fachina-graphics.dll'))).Hash;pluginLoaded=$true;nativeSceneLoaded=$true;operatorOpened=$true;accessibleSelection=$true;f11Fullscreen=$fullscreenOk;escapeRestoresWindow=$true;closeReturnsToDock=$true;legacyHttpListener=$false;normalShutdown=$true;restartRestoresOperator=$true;normalBounds=$normal;fullscreenBounds=$full;restoredBounds=$restored;screens=@([Windows.Forms.Screen]::AllScreens | ForEach-Object {@{name=$_.DeviceName;primary=$_.Primary;width=$_.Bounds.Width;height=$_.Bounds.Height}});log=$log.FullName}
    $result.editorCycles=5;$result.editorMovements=1000;$result.editorUserObjectsBefore=$initialUserObjects;$result.editorUserObjectsAfter=$finalUserObjects;$result.editorGdiObjectsBefore=$initialGdiObjects;$result.editorGdiObjectsAfter=$finalGdiObjects;$result.editorDidNotWriteLibrary=$true;$result.webSocketIntegration=[bool]$WebSocket;$result.confirmedRedTally=$tallyRed;$result.videoOneShotAutomaticOut=$videoEnded;$result.videoLoop=$true;$result.independentCategories=$true
    if($LegacyMigration){$result.legacyMigration=$true;[IO.File]::WriteAllText((Join-Path $repoRoot 'artifacts/obs-migration-smoke-results.json'),($result|ConvertTo-Json -Depth 10),$utf8)}
    [IO.File]::WriteAllText((Join-Path $repoRoot 'artifacts/obs-native-smoke-results.json'),($result|ConvertTo-Json -Depth 10),$utf8)
    Write-Output 'PASS OBS portátil: DLL, composição nativa, operador, F11, Esc, dock, ausência de HTTP legado e encerramento.'
}finally{
    if($testProcess -and -not $testProcess.HasExited){
        $current=Get-Process -Id $testProcess.Id -ErrorAction SilentlyContinue
        if($current -and $current.Path -eq $obsExe){Stop-Process -Id $testProcess.Id -Force}
    }
}
