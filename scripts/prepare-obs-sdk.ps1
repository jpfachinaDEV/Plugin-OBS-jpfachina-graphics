param(
    [Parameter(Mandatory=$true)][string]$ObsSource,
    [Parameter(Mandatory=$true)][string]$ObsRuntime,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath $ObsSource).Path
$runtimeRoot = (Resolve-Path -LiteralPath $ObsRuntime).Path
$sdkOutput = [IO.Path]::GetFullPath($OutputDirectory)
foreach($tool in 'dumpbin.exe','lib.exe') { if(-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw 'Execute no Developer PowerShell do Visual Studio x64.' } }
$header = [IO.File]::ReadAllText((Join-Path $sourceRoot 'libobs/obs-config.h'))
$major = [regex]::Match($header,'LIBOBS_API_MAJOR_VER\s+(\d+)').Groups[1].Value
$minor = [regex]::Match($header,'LIBOBS_API_MINOR_VER\s+(\d+)').Groups[1].Value
$patch = [regex]::Match($header,'LIBOBS_API_PATCH_VER\s+(\d+)').Groups[1].Value
$version = "$major.$minor.$patch"
if($version -notin @('32.1.0','33.0.1')) { throw 'Esta preparação suporta as tags OBS 32.1.0 e 33.0.1. Use fontes correspondentes ao runtime.' }
$runtimeVersion = (Get-Item -LiteralPath (Join-Path $runtimeRoot 'obs64.exe')).VersionInfo.ProductVersion
if($runtimeVersion -ne $version) { throw "Versões diferentes: fontes $version, OBS $runtimeVersion." }
New-Item -ItemType Directory -Force -Path (Join-Path $sdkOutput 'lib'),(Join-Path $sdkOutput 'include'),(Join-Path $sdkOutput 'lib/cmake/libobs'),(Join-Path $sdkOutput 'lib/cmake/obs-frontend-api') | Out-Null
# Generate only OBS's configuration header. Public headers remain the unmodified official source files.
$config = [IO.File]::ReadAllText((Join-Path $sourceRoot 'libobs/obsconfig.h.in'))
$config = $config.Replace('#cmakedefine OBS_DATA_PATH "@OBS_DATA_PATH@"','#define OBS_DATA_PATH "data"')
$config = $config.Replace('#cmakedefine OBS_PLUGIN_PATH "@OBS_PLUGIN_PATH@"','#define OBS_PLUGIN_PATH "obs-plugins"')
$config = $config.Replace('#cmakedefine OBS_PLUGIN_DESTINATION "@OBS_PLUGIN_DESTINATION@"','#define OBS_PLUGIN_DESTINATION "obs-plugins/64bit"')
$config = [regex]::Replace($config,'(?m)^#cmakedefine (\w+).*$', '/* #undef $1 */').Replace('@OBS_RELEASE_CANDIDATE@','0').Replace('@OBS_BETA@','0')
[IO.File]::WriteAllText((Join-Path $sdkOutput 'include/obsconfig.h'),$config,[Text.UTF8Encoding]::new($false))
foreach($library in 'obs','obs-frontend-api') {
    $dll = Join-Path $runtimeRoot ($library+'.dll')
    $exports = & dumpbin.exe /nologo /exports $dll
    if($LASTEXITCODE -ne 0) { throw "Falha ao ler exports de $library." }
    $symbols = @($exports | ForEach-Object { if($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') { $Matches[1] } })
    if($symbols.Count -lt 10) { throw "Exports inválidos em $library." }
    $def = Join-Path $sdkOutput ('lib/'+$library+'.def')
    [IO.File]::WriteAllLines($def,(@("LIBRARY $library.dll",'EXPORTS')+$symbols),[Text.UTF8Encoding]::new($false))
    & lib.exe /nologo /machine:x64 "/def:$def" "/out:$(Join-Path $sdkOutput ('lib/'+$library+'.lib'))"
    if($LASTEXITCODE -ne 0) { throw "Falha ao criar import library de $library." }
}
$cmakeSource = $sourceRoot.Replace('\','/')
$cmakeRuntime = $runtimeRoot.Replace('\','/')
$cmakeSdk = $sdkOutput.Replace('\','/')
$libConfig = @"
if(NOT TARGET OBS::libobs)
  add_library(OBS::libobs SHARED IMPORTED)
  set_target_properties(OBS::libobs PROPERTIES IMPORTED_IMPLIB "$cmakeSdk/lib/obs.lib" IMPORTED_LOCATION "$cmakeRuntime/obs.dll" INTERFACE_INCLUDE_DIRECTORIES "$cmakeSource/libobs;$cmakeSdk/include")
endif()
"@
$frontConfig = @"
if(NOT TARGET OBS::obs-frontend-api)
  add_library(OBS::obs-frontend-api SHARED IMPORTED)
  set_target_properties(OBS::obs-frontend-api PROPERTIES IMPORTED_IMPLIB "$cmakeSdk/lib/obs-frontend-api.lib" IMPORTED_LOCATION "$cmakeRuntime/obs-frontend-api.dll" INTERFACE_INCLUDE_DIRECTORIES "$cmakeSource/frontend/api")
endif()
"@
$versionConfig = @"
set(PACKAGE_VERSION "$version")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()
if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
  set(PACKAGE_VERSION_EXACT TRUE)
endif()
"@
[IO.File]::WriteAllText((Join-Path $sdkOutput 'lib/cmake/libobs/libobsConfig.cmake'),$libConfig)
[IO.File]::WriteAllText((Join-Path $sdkOutput 'lib/cmake/libobs/libobsConfigVersion.cmake'),$versionConfig)
[IO.File]::WriteAllText((Join-Path $sdkOutput 'lib/cmake/obs-frontend-api/obs-frontend-apiConfig.cmake'),$frontConfig)
Write-Output "Import libraries prontas para OBS $version em $sdkOutput. Nenhum arquivo do OBS foi modificado."
