# Compilar, testar e empacotar

## Dependências

- Windows x64, MSVC v143 e Windows SDK, no Developer PowerShell x64.
- CMake 3.28 ou posterior e Ninja.
- OBS Studio 33.0.1 x64 e os fontes oficiais da mesma tag.
- Qt 6.11.1 MSVC x64: Core, Gui, Widgets, Network, WebSockets e ferramentas.
- Node.js e Google Chrome para testes de compatibilidade do navegador.
- NSIS para gerar o instalador EXE.

Fontes oficiais: [OBS 33.0.1](https://github.com/obsproject/obs-studio/tree/33.0.1),
[SDK Qt OBS](https://github.com/obsproject/obs-deps/releases/tag/2026-10-23),
[Qt WebSockets 6.11.1](https://github.com/qt/qtwebsockets/tree/v6.11.1).
Confira o digest do SDK na release oficial. Use diretórios ASCII para preparar
os módulos Qt; alguns geradores falham com caminhos acentuados.

O SDK OBS Qt pode não incluir WebSockets. Nesse caso, compile o módulo contra o
mesmo kit Qt, sem misturar versões:

```powershell
cmake -S C:/dev/qtwebsockets-6.11.1 -B C:/dev/qtwebsockets-build -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=C:/dev/qt-6.11.1 `
  -DCMAKE_INSTALL_PREFIX=C:/dev/qt-6.11.1 -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF
cmake --build C:/dev/qtwebsockets-build --parallel 2
cmake --install C:/dev/qtwebsockets-build
```

## Preparar o SDK OBS e compilar

Os caminhos abaixo são exemplos. O script cria import libraries a partir das DLLs
OBS e dos headers da mesma versão; não modifica o OBS instalado.

```powershell
./scripts/prepare-obs-sdk.ps1 -ObsSource C:/dev/obs-studio-33.0.1 `
  -ObsRuntime C:/dev/obs-portatil/bin/64bit -OutputDirectory C:/dev/obs-sdk-33
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  '-DCMAKE_PREFIX_PATH=C:/dev/qt-6.11.1;C:/dev/obs-sdk-33'
cmake --build build --parallel 2
```

Para compilar somente o aplicativo e as suítes Qt, acrescente `-DJP_BUILD_PLUGIN=OFF`.
O aplicativo `build/jp-graphics-dev.exe` abre a interface nativa e pode conectar
a um OBS com o plugin instalado por WebSocket. `--data <pasta>` escolhe uma
biblioteca separada; `--legacy-ui` mantém a interface HTML de compatibilidade.

## Testar

```powershell
$env:PATH = 'C:/dev/qt-6.11.1/bin;C:/dev/obs-portatil/bin/64bit;' + $env:PATH
$env:QT_PLUGIN_PATH = 'C:/dev/qt-6.11.1/plugins'
ctest --test-dir build --output-on-failure
$env:QT_QPA_PLATFORM = 'minimal'
./build/jp-render-tests.exe --obs-root C:/dev/obs-portatil
npm ci
npm test
```

CTest executa core, operator, panel, position e websocket; a última usa servidor
simulado. Os pequenos arquivos de mídia necessários estão em `tests/fixtures`.
O script de OBS real precisa de uma cópia portátil iniciada ao menos uma vez,
com um perfil e uma coleção de cenas básicos:

```powershell
./scripts/test-obs-operator.ps1 -SourceObs C:/dev/obs-portatil `
  -QtRoot C:/dev/qt-6.11.1 -BuildDirectory build -LegacyMigration -WebSocket
```

Ele trabalha numa cópia temporária. Os artefatos de testes e bibliotecas gerados
são locais e estão excluídos pelo `.gitignore`. Os limites do ensaio constam em
[COMPATIBILITY.md](COMPATIBILITY.md).

## Gerar imagens reais

Configure com `-DJP_BUILD_GALLERY=ON` e compile o alvo `jp-gallery` junto do plugin.
Veja [SCREENSHOTS.md](SCREENSHOTS.md). O utilitário usa os widgets de produção e
um OBS separado com perfil, senha temporária e mídias demonstrativas próprios.

## Gerar os downloads

```powershell
./scripts/package.ps1 -QtRoot C:/dev/qt-6.11.1 -BuildDirectory build `
  -ObsRuntime C:/dev/obs-portatil/bin/64bit -NsisCompiler C:/dev/nsis/makensis.exe
```

O CMake gera `jp-build-info.json`. O empacotador confere as dependências, monta o
ZIP e o EXE em `downloads/0.4.1`, e atualiza `SHA256SUMS.txt`. O estágio temporário
fica na pasta temporária do Windows, fora dos arquivos publicados.

A DLL Qt6WebSockets.dll deve acompanhar a DLL do plugin. O backend TLS fica em
`data/qt-plugins/tls`. Os avisos e as licenças precisam acompanhar a distribuição.
Para verificar apenas os imports:

```powershell
./scripts/test-obs-dependencies.ps1 -Plugin build/jp-fachina-graphics.dll `
  -WebSockets C:/dev/qt-6.11.1/bin/Qt6WebSockets.dll `
  -ObsRuntime C:/dev/obs-portatil/bin/64bit
```
