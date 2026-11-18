param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$BuildDirectory = 'build',
    [string]$NsisCompiler = '',
    [string]$OutputDirectory = 'dist',
    [string]$Version = '1.0.3-beta',
    [Parameter(Mandatory=$true)][string]$ObsRuntime
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$packageRoot = Join-Path $outputRoot 'jp-fachina-graphics'
$binary = Join-Path $repoRoot ($BuildDirectory+'/jp-fachina-graphics.dll')
$infoPath=Join-Path $repoRoot ($BuildDirectory+'/jp-build-info.json')
if(-not(Test-Path -LiteralPath $infoPath)){throw 'Reconfigure o CMake para gerar jp-build-info.json.'}
$buildInfo=Get-Content -LiteralPath $infoPath -Raw -Encoding UTF8 | ConvertFrom-Json
if($buildInfo.pluginVersion -ne $Version){throw 'A versão do pacote difere da versão compilada.'}
$tls = Join-Path ([IO.Path]::GetFullPath($QtRoot)) 'plugins/tls/qschannelbackend.dll'
$webSockets = Join-Path ([IO.Path]::GetFullPath($QtRoot)) 'bin/Qt6WebSockets.dll'
if(-not (Test-Path -LiteralPath $binary)) { throw 'Compile e valide a DLL antes de empacotar.' }
if(-not (Test-Path -LiteralPath $tls)) { throw 'O SDK Qt precisa incluir plugins/tls/qschannelbackend.dll.' }
if(-not (Test-Path -LiteralPath $webSockets)) { throw 'O SDK Qt precisa incluir bin/Qt6WebSockets.dll.' }
if((Get-Item -LiteralPath $webSockets).VersionInfo.FileVersion -ne ($buildInfo.qtVersion+'.0')) { throw 'Qt WebSockets difere do kit utilizado no build.' }
$qtVersion=(Get-Item -LiteralPath $tls).VersionInfo.FileVersion
if($qtVersion -ne ($buildInfo.qtVersion+'.0')) { throw 'Backend TLS difere do kit utilizado no build.' }
& (Join-Path $PSScriptRoot 'test-obs-dependencies.ps1') -Plugin $binary -WebSockets $webSockets -ObsRuntime $ObsRuntime
foreach($dir in 'bin/64bit','data/qt-plugins/tls','data/licenses','docs') { New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot $dir) | Out-Null }
Copy-Item -LiteralPath $infoPath -Destination (Join-Path $packageRoot 'compatibility.json')
Copy-Item -LiteralPath $binary -Destination (Join-Path $packageRoot 'bin/64bit/jp-fachina-graphics.dll')
Copy-Item -LiteralPath $webSockets -Destination (Join-Path $packageRoot 'bin/64bit/Qt6WebSockets.dll')
Copy-Item -LiteralPath $tls -Destination (Join-Path $packageRoot 'data/qt-plugins/tls/qschannelbackend.dll')
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE'),(Join-Path $repoRoot 'LEIA-ME.md') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/BUILD.md'),(Join-Path $repoRoot 'docs/REFACTOR-0.3.md'),(Join-Path $repoRoot 'docs/REFACTOR-0.4.md'),(Join-Path $repoRoot 'docs/FIX-OBS33-0.4.1.md'),(Join-Path $repoRoot 'docs/SCOREBOARDS.md') -Destination (Join-Path $packageRoot 'docs')
Copy-Item -LiteralPath (Join-Path $repoRoot 'vendor/obs-browser/COPYING') -Destination (Join-Path $packageRoot 'data/licenses/obs-browser-COPYING')
Copy-Item -LiteralPath (Join-Path $repoRoot 'vendor/Qt-LGPL-3.0.txt'),(Join-Path $repoRoot 'vendor/Qt-GPL-3.0.txt'),(Join-Path $repoRoot 'vendor/Qt-NOTICE.md') -Destination (Join-Path $packageRoot 'data/licenses')
$zip=Join-Path $outputRoot "JP-Fachina-Graphics-$Version-Windows-x64.zip"
Compress-Archive -LiteralPath $packageRoot -DestinationPath $zip -Force
$sourceItems=@('src','web','scripts','docs','tests','assets','vendor','artifacts','CMakeLists.txt','resources.qrc','package.json','package-lock.json','playwright.config.mjs','README.md','LEIA-ME.md','LICENSE','.gitignore') | ForEach-Object { Join-Path $repoRoot $_ }
Compress-Archive -LiteralPath $sourceItems -DestinationPath (Join-Path $outputRoot "JP-Fachina-Graphics-$Version-Fontes.zip") -Force
if($NsisCompiler) {
    $compiler=(Resolve-Path -LiteralPath $NsisCompiler).Path
    $installer=Join-Path $outputRoot "JP-Fachina-Graphics-$Version-Windows-x64-Setup.exe"
    & $compiler '/V2' "/DVERSION=$Version" "/DNUMERIC_VERSION=$($buildInfo.binaryVersion)" "/DOBS_VERSION=$($buildInfo.obsVersion)" "/DQT_VERSION=$($buildInfo.qtVersion)" "/DPACKAGE_DIR=$packageRoot" "/DOUTPUT_FILE=$installer" (Join-Path $PSScriptRoot 'installer.nsi')
    if($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o instalador NSIS.' }
}
Get-ChildItem -LiteralPath $outputRoot -File | Where-Object { $_.Extension -in '.exe','.zip' } | ForEach-Object { $file=$_; $hash=Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256; [pscustomobject]@{File=$file.Name;SHA256=$hash.Hash} } | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $outputRoot 'SHA256SUMS.json')
Write-Output "Pacotes preparados em $outputRoot."
