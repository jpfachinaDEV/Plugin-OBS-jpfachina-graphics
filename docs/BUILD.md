# Compilação no Windows x64

## Correção 0.4.1 — OBS 33.0.1 / Qt 6.11.1

O pacote atual usa o kit do OBS instalado: **33.0.1 x64 / Qt 6.11.1**. A versão
0.4.0/Qt 6.8.3 continua histórica e não deve ser instalada no OBS 33. A dependência
Qt WebSockets usa API privada do Qt; não basta manter o nome Qt6WebSockets.dll.
Compile com a mesma versão do Qt do runtime. Veja [o diagnóstico](FIX-OBS33-0.4.1.md).

O SDK Qt oficial está em obs-deps `2026-10-23`; confira o digest do arquivo na
release oficial. No ambiente local, os kits são `.deps/qt-6.11.1` e
`.deps/obs-sdk-33`, preservando os anteriores. O build novo fica em `build-obs33`.
Qt WebSockets foi compilado da tag oficial `v6.11.1`; use os comandos da seção
0.4 abaixo substituindo a versão e prefira caminhos ASCII para os geradores Qt.

```powershell
. ./.deps/enter.ps1
./scripts/prepare-obs-sdk.ps1 -ObsSource .deps/obs-source-33/obs-studio-33.0.1 `
  -ObsRuntime .deps/obs-test-33/bin/64bit -OutputDirectory .deps/obs-sdk-33
cmake -S . -B build-obs33 -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  '-DCMAKE_PREFIX_PATH=C:/dev/qt-6.11.1;C:/dev/obs-sdk-33'
cmake --build build-obs33 --parallel 2
$env:PATH = 'C:/dev/qt-6.11.1/bin;' + $env:PATH
$env:QT_PLUGIN_PATH = 'C:/dev/qt-6.11.1/plugins'
ctest --test-dir build-obs33 --output-on-failure
./scripts/test-obs-dependencies.ps1 -Plugin build-obs33/jp-fachina-graphics.dll `
  -WebSockets .deps/qt-6.11.1/bin/Qt6WebSockets.dll -ObsRuntime .deps/obs-test-33/bin/64bit
./scripts/test-obs-operator.ps1 -SourceObs .deps/obs-test-33 `
  -BuildDirectory build-obs33 -QtRoot .deps/qt-6.11.1 -LegacyMigration -WebSocket
./scripts/package.ps1 -QtRoot .deps/qt-6.11.1 -BuildDirectory build-obs33 `
  -ObsRuntime .deps/obs-test-33/bin/64bit -NsisCompiler .deps/nsis/nsis-3.11/makensis.exe
```

O CMake gera `jp-build-info.json` com versões do plugin/OBS/Qt. O empacotador
inclui esse arquivo como `compatibility.json`, verifica as versões das DLLs e os
imports contra o runtime de teste antes de distribuir. O instalador verifica a
versão Qt do OBS na pasta padrão e recusa um kit diferente. Instalações em pastas
personalizadas também precisam corresponder ao kit declarado.

As seções seguintes registram o histórico e não substituem os requisitos atuais.

## Entrega 0.4

O alvo permanece OBS 32.1.0 / Qt 6.8.3 / MSVC x64. Agora também é necessário
**Qt WebSockets 6.8.3**, incluindo headers, configuração CMake e DLL de release.
O SDK OBS Qt usado abaixo não inclui esse módulo. Instale-o com o mesmo kit Qt
ou compile a [tag oficial v6.8.3](https://github.com/qt/qtwebsockets/tree/v6.8.3)
contra esse SDK; não misture Qt 6.9/6.10 nem MinGW com o OBS.

Exemplo a partir dos fontes extraídos, no Developer PowerShell x64:

```powershell
cmake -S C:/dev/qtwebsockets-6.8.3 -B C:/dev/qtwebsockets-build -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=C:/dev/qt-6.8.3 `
  -DCMAKE_INSTALL_PREFIX=C:/dev/qt-6.8.3 `
  -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF
cmake --build C:/dev/qtwebsockets-build --parallel 2
cmake --install C:/dev/qtwebsockets-build
```

Use diretórios ASCII para esse build: o gerador syncqt do SDK pode falhar em
caminhos com acentos. O ambiente local contém o módulo em `.deps/qt`.
A instalação CMake, o ZIP e o EXE incluem `Qt6WebSockets.dll` junto à DLL do plugin.

`ctest --test-dir build --output-on-failure` executa cinco suítes: core, operator,
panel, position e websocket. A última usa servidor local simulado; a validação
abaixo conecta adicionalmente ao OBS real, com autenticação e gravação temporária:

```powershell
. ./.deps/enter.ps1
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-obs-operator.ps1 -LegacyMigration -WebSocket
```

Esse helper cria uma cópia portátil isolada, senha de teste e biblioteca própria.
Não altera a instalação principal nem transmite para plataformas externas.
Exercita editor real, Take/Out, migração em **Link OBS**, loop, reconexão, troca
de projeto preservando o programa, gravação e teste intensivo de movimentos.
Para capturas com fontes reais: `jp-position-tests --visual` com
`QT_QPA_PLATFORM=windows`. Veja [o relatório 0.4](REFACTOR-0.4.md).

O executável de desenvolvimento usa a interface nativa por padrão. Sem o frontend
OBS, conecte por WebSocket em Link OBS. `--legacy-ui` e `--no-browser` conservam
o serviço antigo para integrações e testes de compatibilidade.

As seções 0.3/0.2 abaixo registram o histórico; as instruções e semântica 0.4
acima e no LEIA-ME prevalecem. Os pacotes atuais usam a versão 0.4.0.

## Entrega 0.3

O alvo permanece OBS 32.1.0 x64 / Qt 6.8.3. A interface nativa passa a abrir
por padrão no executável de desenvolvimento. `--native-ui` continua aceito;
`--legacy-ui` abre explicitamente o painel HTML de compatibilidade e
`--no-browser` mantém o serviço usado pelo teste end-to-end anterior.
Sem o frontend OBS, Take/Out ficam indisponíveis na interface de desenvolvimento.

O build também gera `jp-panel-tests`. `ctest --test-dir build --output-on-failure`
executa core, operator e panel. Os testes de renderização reais continuam separados:

```powershell
. ./.deps/enter.ps1 # se estiver usando as dependências locais já preparadas
$env:PATH = (Join-Path (Get-Location) '.deps/obs-test/bin/64bit') + ';' + $env:PATH
$env:QT_PLUGIN_PATH = Join-Path (Get-Location) '.deps/qt/plugins'
./build/jp-render-tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-obs-operator.ps1
```

O helper de OBS usa uma cópia portátil isolada. O teste atual também verifica
TAKE via acessibilidade, borda vermelha confirmada, vídeo sem loop com OUT
automático e vídeo em loop preservando outra categoria. `-LegacyMigration`
exercita a migração de Browser Source pelo diálogo Cenas.

Para screenshots com fontes reais do Windows, execute `jp-panel-tests --visual`
com `QT_QPA_PLATFORM=windows`; o teste mantém a janela fora da tela. A plataforma
Qt minimal usada pelo CTest pode renderizar glifos como quadrados em capturas.

Os pacotes agora usam a versão 0.3.0. Consulte [a entrega](REFACTOR-0.3.md)
para mudanças de operação e limites atuais; as seções 0.1/0.2 abaixo registram
o preparo original do ambiente.

## Atualização 0.2

A DLL agora usa dock Qt Widgets e compositor nativo. Não depende de Browser
Panel API 3 para a interface. Os módulos de imagem, mídia FFmpeg e filtros do OBS
devem acompanhar o runtime. A biblioteca, downloader e o backend de compatibilidade
continuam usando Qt Core/Gui/Widgets/Network 6.8.3.

Além dos testes anteriores, o build gera `jp-operator-tests` e, com plugin
habilitado, `jp-render-tests`. O segundo precisa das DLLs OBS no PATH. Consulte
[validação 0.2](VALIDATION-0.2.md) para comandos e distinção entre testes reais e
limites físicos. Para abrir a interface Qt de desenvolvimento:

```powershell
./build/jp-graphics-dev.exe --native-ui --data C:/dev/jp-graphics-library
```

Os comandos de build abaixo continuam aplicáveis. O modo de desenvolvimento
antigo sem `--native-ui` é usado pela regressão de compatibilidade HTML.

## Dependências

- Visual Studio 2022 Build Tools, compilador MSVC v143 x64 e Windows SDK.
- CMake 3.28 ou posterior e Ninja.
- OBS Studio 32.1.0 x64 completo, com obs-browser, e os fontes oficiais da tag
  `32.1.0`. Não use headers de uma versão posterior com DLLs anteriores.
- Qt **6.8.3 MSVC x64**: Core, Gui, Widgets, Network e as ferramentas de build.
  O pacote oficial usado é
  [windows-deps-qt6-2025-08-23-x64-RelWithDebInfo.zip](https://github.com/obsproject/obs-deps/releases/download/2025-08-23/windows-deps-qt6-2025-08-23-x64-RelWithDebInfo.zip).
  Inclua o backend `plugins/tls/qschannelbackend.dll` no pacote final.
- Node.js e Google Chrome para os testes de navegador. São ferramentas de teste,
  não dependências de execução do plugin instalado.
- NSIS 3.11 para gerar o instalador EXE; o ZIP também é gerado sem NSIS.

O [template oficial de plugin OBS](https://github.com/obsproject/obs-plugintemplate)
usa `OBS::libobs`, `OBS::obs-frontend-api` e Qt6. Este projeto segue esses mesmos
targets e a [API de docks](https://docs.obsproject.com/reference-frontend-api).
O header Browser Panel vem do commit
`ea04212e4bbadd077f9e6038758c4e4779c24fa3`, submódulo da tag OBS 32.1.0, e foi
copiado sem alterações.

## Preparar o SDK de ligação

Use um SDK libobs/obs-frontend-api instalado a partir do OBS 32.1.0, ou prepare
import libraries a partir dos binários oficiais e dos respectivos headers:

```powershell
# Developer PowerShell for VS 2022, ambiente x64
./scripts/prepare-obs-sdk.ps1 `
  -ObsSource 'C:/dev/obs-studio-32.1.0' `
  -ObsRuntime 'C:/Program Files/obs-studio/bin/64bit' `
  -OutputDirectory 'C:/dev/obs-sdk-32.1.0'
```

O script verifica as versões, usa `dumpbin` e `lib` oficiais para gerar import
libraries, configura `obsconfig.h` a partir do template upstream e cria os
targets CMake de importação. Os headers públicos continuam sendo os arquivos
oficiais intactos. Ele não altera nem recompila o OBS instalado.

## Compilar e testar

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_TRY_COMPILE_CONFIGURATION=RelWithDebInfo `
  '-DCMAKE_PREFIX_PATH=C:/dev/qt-6.8.3;C:/dev/obs-sdk-32.1.0'
cmake --build build --parallel 2

$env:PATH = 'C:/dev/qt-6.8.3/bin;' + $env:PATH
$env:QT_PLUGIN_PATH = 'C:/dev/qt-6.8.3/plugins'
ctest --test-dir build --output-on-failure
npm ci
npm test
node tests/native-e2e.mjs
```

`tests/native-e2e.mjs` usa o executável nativo de desenvolvimento e dados em
`.deps/native-e2e-data`, na porta 28746, com um navegador headless. O teste faz
um download real do ícone público do OBS pelo GitHub para verificar HTTPS e
cache. Os demais testes são locais. A logo do usuário não é enviada à internet.
As portas de teste 28746, 28748 e 28749 precisam estar livres.

Para verificar apenas o serviço Qt e os testes nativos sem SDK OBS:

```powershell
cmake -S . -B build -G Ninja -DJP_BUILD_PLUGIN=OFF `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo '-DCMAKE_PREFIX_PATH=C:/dev/qt-6.8.3'
cmake --build build
ctest --test-dir build --output-on-failure
```

## Desenvolver sem abrir o OBS

```powershell
./build/jp-graphics-dev.exe --data C:/dev/jp-graphics-library
```

O mesmo Store, servidor HTTP e downloader de produção atendem o painel no
navegador padrão. O fluxo de associação a uma cena informa o uso manual da URL
nesse modo, pois não existe frontend OBS no executável de desenvolvimento.
`--no-browser` evita abrir o navegador. Feche o processo de desenvolvimento antes
de iniciar o plugin na mesma porta ou biblioteca.

## Empacotar

Depois de compilar e testar:

```powershell
./scripts/package.ps1 -QtRoot C:/dev/qt-6.8.3 `
  -NsisCompiler C:/dev/nsis-3.11/makensis.exe
```

Gera a estrutura moderna `jp-fachina-graphics/bin/64bit` + `data`, ZIP, EXE e
checksums em `dist`. O instalador não embute nem substitui as DLLs centrais do
OBS/Qt. Leva apenas a DLL do plugin e o backend TLS compatível, com as licenças.
O executável não é assinado digitalmente.

`cmake --install build --prefix C:/dev/obs-portatil` oferece o layout antigo
adequado ao OBS portátil 32.1. Para instalação normal, use o pacote em ProgramData
descrito no LEIA-ME. Não instale duas cópias simultâneas do plugin.

## Marca

```powershell
node scripts/extract-branding.mjs C:/caminho/JP_Fachina_Graphics_PROMPT_COMPLETO.md
npm run test:branding
```

A extração trata Base64 exclusivamente como dados. O teste compara SHA-256,
assinatura PNG e dimensões. O recurso incorporado no painel, na tela inicial e
nas informações do programa usa esse mesmo arquivo, sem recorte ou recoloração.
