# Correção 0.4.1 — carregamento no OBS 33.0.1

A captura do gerenciador corresponde à instalação OBS 33.0.1 x64 / Qt 6.11.1.
O log real `2026-11-16 17-07-15.txt` registra o módulo como não carregado.
O pacote 0.4.0 tinha sido compilado/testado para OBS 32.1.0 / Qt 6.8.3.

## Causa comprovada

A inspeção dos imports da `Qt6WebSockets.dll` 6.8.3 mostrou dependência do símbolo
`??0QObjectPrivate@@QEAA@H@Z` em Qt6Core.dll. Esse símbolo não existe na Qt6Core.dll
6.11.1 do OBS instalado. O carregador do Windows falha antes de executar
`obs_module_load`; no código do OBS 33, `os_dlopen` omite a mensagem detalhada
quando recebe `ERROR_PROC_NOT_FOUND`, e o log mostra apenas o módulo não carregado.

Portanto, a DLL não estava ausente nem desativada. A biblioteca WebSockets usava
API privada de outro kit Qt. A compatibilidade de APIs públicas de Qt não garante
compatibilidade dessas chamadas privadas. Copiar a DLL central Qt6Core antiga
para o OBS 33 seria incorreto; ela não é incluída nem substituída pela correção.

Referências oficiais: [carregador Windows OBS 33](https://github.com/obsproject/obs-studio/blob/33.0.1/libobs/util/platform-windows.c)
e [dependências OBS](https://github.com/obsproject/obs-deps/releases/tag/2026-10-23).

## Alterações

- Plugin 0.4.1 recompilado contra fontes e import libraries OBS 33.0.1, com o kit
  Qt 6.11.1 MSVC x64 correspondente ao runtime instalado.
- Qt WebSockets 6.11.1 recompilado da tag oficial `v6.11.1`, sem alterações nos
  fontes do módulo. Backend TLS também corresponde a Qt 6.11.1.
- O CMake gera `jp-build-info.json` com versão do plugin, OBS, Qt e arquitetura;
  o pacote distribui esses dados como `compatibility.json`.
- O empacotador verifica a versão de cada dependência e seus imports/exports
  contra o runtime OBS informado, recusando símbolos ausentes e kits diferentes.
- O instalador informa as versões corretas e verifica o Qt da instalação OBS na
  pasta padrão, recusando uma versão diferente. Para instalações personalizadas,
  o kit também deve corresponder ao declarado no pacote.
- O helper de teste aceita build/Qt/runtime separados, copia também `core` do
  OBS 33 e usa ações acessíveis assíncronas para os diálogos Qt 6.11.
- O teste do renderizador aceita `--obs-root` e o layout de módulos `core` do OBS 33.
- F11 também é tratado quando o foco está na janela de operação, preservando os
  atalhos do painel. A automação de teclado envia scan codes válidos no Qt 6.11.

Arquivos trabalhados: `CMakeLists.txt`, `package.json`, `package-lock.json`,
`scripts/prepare-obs-sdk.ps1`, `scripts/test-obs-dependencies.ps1` (novo),
`scripts/test-obs-operator.ps1`, `scripts/package.ps1`, `scripts/installer.nsi`,
`tests/render-test.cpp`, `src/operator-window.cpp`, `tests/operator-test.cpp`,
`README.md`, `LEIA-ME.md`, `docs/BUILD.md`, este relatório,
`vendor/Qt-NOTICE.md` e evidências em `artifacts`.

As bibliotecas e fontes de desenvolvimento antigas foram mantidas em pastas
separadas. `build-obs33` e `.deps/qt-6.11.1` são os novos caminhos. Pacotes antigos
0.4.0 continuam disponíveis para o ambiente 32.1 originalmente testado, mas não
são indicados para OBS 33. A versão 0.4.1 preserva a lógica e os dados da 0.4.
Não é necessário recriar projetos, cadastrar mídias ou refazer posições.

## Verificação

O build MSVC x64 passou. As cinco suítes CTest passaram com Qt 6.11.1: core,
operator, panel, position e websocket, totalizando 623 verificações.
A verificação de dependências resolveu todos os imports OBS/Qt do plugin e da
biblioteca Qt WebSockets no runtime 33.0.1. O teste negativo recusa a dependência
6.8.3 usada no pacote anterior.

O teste portátil usa uma cópia dos binários do próprio OBS 33 instalado, mas
configuração e biblioteca temporárias. Ele não usa a biblioteca de produção.
Carregamento do módulo, abertura do operador, cenas reais, associação, Take/Out,
vídeos, editor e comunicação são verificados nesse runtime. A integração WebSocket
inclui autenticação, tally pós-render, gravação local, reconexão e preservação do
programa ao trocar de projeto. Os resultados finais ficam em
`artifacts/validation-0.4.1-obs33.json` e no log CTest correspondente.

Resultados confirmados: 623 verificações CTest, 69 do renderizador D3D11 e
44 da integração WebSocket real. O ensaio portátil também percorreu edição,
Take/Out e vídeos antes de chegar à verificação de tela cheia. A automação Win32
de F11 variou conforme o foco entre janelas; a última execução não concluiu esse
teste. Isso é registrado como limitação, sem classificar o smoke inteiro como
aprovado. O atalho recebeu cobertura no teste Qt. O carregamento que motivou o
reparo foi confirmado adicionalmente na instalação principal.

## Instalar

1. Feche completamente o OBS, inclusive a janela do gerenciador de plugins.
2. Execute `JP-Fachina-Graphics-0.4.1-Windows-x64-Setup.exe` e mantenha a pasta
   do plugin em `%PROGRAMDATA%/obs-studio/plugins/jp-fachina-graphics`.
3. Abra o OBS e mantenha JP Fachina Graphics ativado no gerenciador de plugins.
4. Abra o dock JP Fachina Graphics. Os projetos e arquivos anteriores permanecem
   na pasta de dados existente.

O instalador atualiza a DLL do plugin, Qt6WebSockets.dll, backend TLS e documentos;
não substitui Qt6Core/Gui/Widgets/Network do OBS, cenas ou biblioteca do usuário.
Se a pasta exigir permissão, execute o instalador como administrador. Para OBS
portátil, o ZIP contém as duas DLLs e os dados; siga o LEIA-ME.

O OBS principal não foi encerrado à força. Quando estava fechado, foi feito
backup da pasta do plugin, dos JSONs da biblioteca e das cenas em
`.deps/repair-backup-obs33-6b7ee39e`, e instalado o pacote corrigido. Depois foi
aberto novamente e verificado o módulo carregado no processo e o log atual
`2026-11-16 17-36-51.txt`, que confirma `operator ready`. A DLL carregada corresponde
ao build validado. Não foram executados Take/Out de teste na biblioteca principal.
