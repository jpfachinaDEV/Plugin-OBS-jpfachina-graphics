# Validação 0.2 — 11 de novembro de 2026

Ambiente: Windows x64, OBS 32.1.0, Qt 6.8.3, MSVC e SDKs locais do projeto.
Os testes OBS usam cópias portáteis isoladas em `.deps/obs-operator-smoke-*`;
nenhuma cena ou perfil da instalação principal foi alterado.

## Execuções

| Verificação | Resultado |
| --- | --- |
| Build CMake de DLL, desenvolvimento e testes | Passou |
| C++ existente: 8 grupos / 67 verificações | Passou |
| Novo teste do operador: 197 verificações, ou 203 com screenshots | Passou |
| Qt Windows em 100%, 125%, 150% e 200% por `QT_SCALE_FACTOR` | Passou nas quatro escalas |
| Layouts 1366×768, 1920×1080, 2560×1440, 3840×2160, 683×384 e 500×450 | Passou; dimensões de widgets e controles fixos conferidas |
| Renderização libobs/D3D11: 61 verificações | Passou |
| PNG transparente, opacidade, rotação, recorte e composição 4K | Passou com leitura de pixels da GPU |
| Preview privado, TAKE e OUT | Passou; alterar preparação preservou pixels do programa |
| WebM/VP8, MP4/H.264 e GIF animado | Passou com fixtures locais decodificadas pelo OBS |
| Atualizar outra peça preserva instância e tempo do vídeo | Passou |
| Vídeo sem repetição conserva último quadro | Passou |
| Teste da logo original e 7 testes Playwright existentes | Passou |
| Serviço Qt real + navegador + SSE + HTTPS/cache | Passou após as mudanças de biblioteca/download |
| OBS portátil: carregar DLL, abrir operador, F11, Esc e retorno ao dock | Passou |
| Seleção pela acessibilidade do Windows atualiza o controlador/editor | Passou |
| OBS portátil sem fontes antigas: nenhuma escuta HTTP na porta 28741 | Passou |
| Migração de Browser Source própria para fonte nativa, sem item duplicado | Passou no ensaio de migração |
| Preservar posição, rotação e recorte da fonte antiga durante a migração | Passou no OBS portátil |
| Reiniciar o mesmo OBS portátil e restaurar operador/dimensões | Passou |
| Encerramento pela janela principal | Passou com código 0 |
| Instalador/ZIP 0.2, DLL e backend TLS | Passou em diretório isolado; hashes da DLL conferidos |
| Desinstalar 0.2 sem apagar arquivo alheio ao pacote | Passou |

O teste de operador cobre migração com preservação dos bytes do backup,
isolamento draft/live, duplicação sem duplicar binários, sincronização parcial
e múltipla, conflitos de desfazer, nomes e conteúdo preservados, modelos,
validações, filas de importação e portabilidade de projetos. Testes de layouts
usam janelas de teste ocultas; não mudam a resolução física do Windows.
Eventos de mouse também verificam redimensionamento com âncora central e
preservação da saída publicada. O botão Salvar é exercitado após renderizar o
formulário, protegendo contra inserção acidental de propriedades nulas na leitura.

O teste render usa libobs e os módulos reais image-source, obs-filters e
obs-ffmpeg. O programa é conectado à saída libobs para renderização contínua;
as amostras de pixels são lidas de uma textura D3D11. Essa verificação não usa
o servidor HTTP nem o compositor HTML antigo.

## Ensaio curto de recursos

Executado por 60 segundos, após aquecimento, com dois vídeos, GIF e PNG:

- 2.424 atualizações de composição.
- Memória privada inicial: 135.213.056 bytes (aproximadamente 129 MiB).
- Memória privada final: 134.217.728 bytes (128 MiB).
- Pico amostrado: 135.340.032 bytes.
- Teste com 179 verificações, incluindo amostras repetidas e limite de crescimento.

Resultado registrado em `artifacts/renderer-stress.json`. É um ensaio curto de
um processo de testes libobs; não é medição comparativa de CPU/RAM de uma live
real nem substitui a execução contínua de quatro horas prevista no plano.

## Evidências

- `artifacts/obs-native-operator.png`: janela real de operação no OBS portátil.
- `artifacts/obs-native-smoke-results.json`: carga, atalhos, dock, geometria,
  ausência do HTTP legado e hash da DLL ensaiada.
- `artifacts/obs-migration-smoke-results.json`: ensaio de migração da fonte antiga.
- `artifacts/qt-layout-*.png`: layouts de teste Qt; o último conjunto utiliza a
  última escala executada. Dimensões dos PNGs incluem o devicePixelRatio.
- `artifacts/native-compositor.png`, `native-private-preview.png` e
  `native-compositor-4k.png`: capturas do renderer nativo com alpha.
- `artifacts/native-e2e-results.json`: integração do caminho legado preservado.
- `artifacts/package-0.2-results.json`: instalação/desinstalação isoladas e hashes
  do instalador e da DLL verificados.
- `build/Testing/Temporary/LastTest.log`: execução mais recente do CTest.
- `test-results/browser-results.json`: regressão Playwright.

## Matriz obrigatória e limites

| Item | Cobertura desta execução |
| --- | --- |
| 1–4. DLL, dock, janela e tela cheia | OBS real e testes Qt |
| 5. Seleção do segundo monitor | Não executada: ambiente expôs somente um monitor físico |
| 6. Mudanças de resolução | Layouts virtuais testados; mudança de modo físico do Windows não executada |
| 7. DPI | Escalas fixas testadas; passagem entre monitores com DPI diferentes não executada |
| 8. Desconexão de monitor | Algoritmo de recuperação de geometria testado; hotplug físico não executado |
| 9. Persistência visual | JSON, modos e dimensões conferidos; reinício do mesmo perfil OBS passou |
| 10. Operação simultânea | Modelo único e fonte OBS ativa; uso prolongado com operador humano não executado |
| 11–14. Cadastro, editor, sincronização e publicação | C++, Qt e renderer real; arraste manual completo permanece recomendável |
| 15. Mudança de cenas | Fonte em coleção real; bateria extensa de trocas e Studio Mode não executada |
| 16–18. Mídia, biblioteca e reinício | Persistência, migração, IO assíncrono, codecs e reinício real do mesmo OBS portátil passaram |
| 19. Erros | Validação, arquivo/caminho inválido, conflito de undo, corrupção e falta de integração; falha física de disco não simulada |
| 20. Recursos | Ensaio curto de 60 s; quatro horas e comparação de frames/CPU da live não executadas |

Também permanecem pendentes homologação de migração em grupos/cenas aninhadas
compartilhadas, ensaio de todos os diálogos por um operador humano, codecs com
alpha além das fixtures, HDR e outras versões do OBS/Qt/Windows. Os testes não
representam certificação para qualquer transmissão ou arquivo de mídia.

## Repetir

```powershell
. ./.deps/enter.ps1
$env:QT_PLUGIN_PATH = "$PWD/.deps/qt/plugins"
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
npm.cmd test
node tests/native-e2e.mjs
node tests/make-media-fixtures.mjs
$env:PATH = "$PWD/.deps/obs-test/bin/64bit;" + $env:PATH
$env:QT_QPA_PLATFORM = 'minimal'
./build/jp-render-tests.exe
# Ensaio opcional, até 14400 segundos:
./build/jp-render-tests.exe --stress-seconds 60
# Em outro processo PowerShell, sem QT_QPA_PLATFORM=minimal:
./scripts/test-obs-operator.ps1
./scripts/test-obs-operator.ps1 -LegacyMigration
```

O script OBS copia o runtime de teste para uma pasta nova, usa somente esse
executável e confere seu PID/caminho antes da limpeza de processos. Logs de
ensaios anteriores não são considerados prova de execução do build atual.
