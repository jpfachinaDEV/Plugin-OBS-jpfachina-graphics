param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$ObsRuntime,
    [string]$BuildDirectory='build',
    [string]$NsisCompiler='',
    [string]$OutputDirectory='downloads'
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$build=if([IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory}else{Join-Path $repo $BuildDirectory}
$info=Get-Content -LiteralPath (Join-Path $build 'jp-build-info.json') -Raw -Encoding UTF8|ConvertFrom-Json
$version=$info.pluginVersion
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Versão de build inválida.'}
$qt=(Resolve-Path -LiteralPath $QtRoot).Path
$binary=Join-Path $build 'jp-fachina-graphics.dll'
$webSockets=Join-Path $qt 'bin/Qt6WebSockets.dll'
$tls=Join-Path $qt 'plugins/tls/qschannelbackend.dll'
foreach($file in @($webSockets,$tls)){if((Get-Item -LiteralPath $file).VersionInfo.FileVersion -ne ($info.qtVersion+'.0')){throw 'Dependência Qt incompatível com o kit do build.'}}
& (Join-Path $PSScriptRoot 'test-obs-dependencies.ps1') -Plugin $binary -WebSockets $webSockets -ObsRuntime $ObsRuntime
$output=if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $repo $OutputDirectory}
$output=Join-Path $output $version
$stage=Join-Path ([IO.Path]::GetTempPath()) ('jp-fachina-release-'+[Guid]::NewGuid().ToString('N'))
$package=Join-Path $stage 'jp-fachina-graphics'
foreach($folder in @('bin/64bit','data/qt-plugins/tls','data/licenses','docs')){New-Item -ItemType Directory -Force -Path (Join-Path $package $folder)|Out-Null}
New-Item -ItemType Directory -Force -Path $output|Out-Null
Copy-Item -LiteralPath $binary,$webSockets -Destination (Join-Path $package 'bin/64bit')
Copy-Item -LiteralPath $tls -Destination (Join-Path $package 'data/qt-plugins/tls')
Copy-Item -LiteralPath (Join-Path $build 'jp-build-info.json') -Destination (Join-Path $package 'compatibility.json')
Copy-Item -LiteralPath (Join-Path $build 'jp-build-info.json') -Destination (Join-Path $output 'compatibility.json')
foreach($name in @('LICENSE','LEIA-ME.md')){Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $package}
foreach($name in @('BUILD.md','COMPATIBILITY.md')){Copy-Item -LiteralPath (Join-Path $repo ('docs/'+$name)) -Destination (Join-Path $package 'docs')}
foreach($name in @('Qt-LGPL-3.0.txt','Qt-GPL-3.0.txt','Qt-NOTICE.md')){Copy-Item -LiteralPath (Join-Path $repo ('vendor/'+$name)) -Destination (Join-Path $package 'data/licenses')}
Copy-Item -LiteralPath (Join-Path $repo 'vendor/obs-browser/COPYING') -Destination (Join-Path $package 'data/licenses/obs-browser-COPYING')
Add-Type -AssemblyName System.IO.Compression
$zip=Join-Path $output "JP-Fachina-Graphics-$version-Windows-x64.zip"
$stream=[IO.File]::Open($zip,[IO.FileMode]::Create)
$archive=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Create)
try{
    foreach($file in (Get-ChildItem -LiteralPath $package -Recurse -File)){
        $relative=$file.FullName.Substring($package.Length+1).Replace('\','/')
        $entry=$archive.CreateEntry(('jp-fachina-graphics/'+$relative),[IO.Compression.CompressionLevel]::Optimal)
        $input=[IO.File]::OpenRead($file.FullName);$entryStream=$entry.Open()
        try{$input.CopyTo($entryStream)}finally{$entryStream.Dispose();$input.Dispose()}
    }
}finally{$archive.Dispose();$stream.Dispose()}
if($NsisCompiler){
    $compiler=(Resolve-Path -LiteralPath $NsisCompiler).Path
    $setup=Join-Path $output "JP-Fachina-Graphics-$version-Windows-x64-Setup.exe"
    & $compiler '/V2' "/DVERSION=$version" "/DOBS_VERSION=$($info.obsVersion)" "/DQT_VERSION=$($info.qtVersion)" "/DPACKAGE_DIR=$package" "/DOUTPUT_FILE=$setup" (Join-Path $PSScriptRoot 'installer.nsi')
    if($LASTEXITCODE -ne 0){throw 'Falha ao compilar o instalador.'}
}
$hashes=Get-ChildItem -LiteralPath $output -File|Where-Object {$_.Extension -in '.zip','.exe'}|Sort-Object Name|ForEach-Object {((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower()+'  '+$_.Name)}
[IO.File]::WriteAllLines((Join-Path $output 'SHA256SUMS.txt'),$hashes,[Text.UTF8Encoding]::new($false))
Write-Output "Downloads prontos: $output"
