# Entrega 0.4 — editor por categoria, projetos e Link OBS

Data: 16/11/2026. Implementação no repositório existente, C++17/Qt Widgets,
OBS Studio 32.1.0 x64, Qt 6.8.3, MSVC x64. A biblioteca de produção e a instalação
principal do OBS não foram alteradas pelos testes.

## Diagnóstico do travamento

Foram examinados o caminho de atualização, os eventos do editor, o compositor,
a captura de miniaturas e os logs reais do OBS anteriores à correção:

| Log de 16/11/2026 | Falhas de criação de janela de plataforma | Ocorrências de jpSelectionWindow |
| --- | ---: | ---: |
| 15-26-45 | 5.214 | 3.476 |
| 15-28-51 | 5.309 | 3.540 |
| 15-29-47 | 4.330 | 2.887 |

Esses logs ficam em `%APPDATA%/obs-studio/logs`. A mensagem repetida era
`Failed to create platform window`. O editor anterior embutia `ObsPreview` num
QDialog e forçava uma janela nativa com `WA_NativeWindow`/`winId()`. A criação e
recriação do display dependiam de eventos de janela/pai, inclusive em janelas
inicialmente ocultas. Isso promovia janelas nativas no caminho do diálogo.

Além disso, `draftChanged` agendava uma atualização completa dos compositores
em 40 ms; movimentos atualizavam também o preview OBS. A captura síncrona de
miniaturas usava `obs_queue_task(..., true)` a partir da interface, esperando
pela thread gráfica. Portanto, interação, ciclo de vida de janelas e espera por
GPU estavam acoplados. A versão anterior não usava WebSocket para editar posições:
a causa encontrada não era excesso de mensagens de uma conexão remota.

A evidência identifica a falha de ciclo de vida nativo e os caminhos de bloqueio
presentes no código. Não havia dump com pilha para atribuir cada congelamento
relatado a uma única instrução. A correção remove esses mecanismos do editor e
mede a ausência de atualizações no compositor durante os movimentos.

A nova janela usa canvas QWidget pintado localmente, sem display OBS, sem
`WA_NativeWindow`, sem gravar a biblioteca a cada movimento e sem chamar o
compositor. A imagem é carregada ao abrir/trocar categoria. A atualização visual
é agrupada em até uma atualização por 16 ms. Capturas reais de vídeo usam tarefa
gráfica assíncrona e retorno enfileirado; a interface não espera a GPU. Apenas
Salvar/Aplicar/Take/Out podem alterar o estado persistido/publicado.

O compositor compara o estado serializado e ignora reaplicações idênticas.
O controle remoto tem no máximo uma atualização pendente por fonte, substituição
pelo estado mais recente, timeout, reconexão progressiva e confirmação pós-render.
A sinalização de tally não provoca novas transformações.

Referências técnicas: [janelas nativas Qt](https://doc.qt.io/qt-6/qwidget.html#native-widgets-vs-alien-widgets)
e [protocolo oficial OBS WebSocket](https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md).

## Novo editor e preservação da operação

Cada projeto guarda `categoryLayouts` e `categoryFrames` para Logos, Banner,
QR-Codes, Vinhetas, Abertura e Vídeos. O editor usa a resolução base do OBS
conectado e converte os valores quando necessário. O compositor mantém a proporção
da imagem dentro da área, com âncora/alinhamento, escala, rotação e recorte.

A janela dedicada permite arrastar, redimensionar, editar valores, restaurar o
padrão, usar modelos e cancelar. As setas têm 84 × 72 pixels, repetição ao segurar,
contraste azul, nomes acessíveis, atalhos de teclado e passos de 1/10/50 pixels.
Os campos e o corpo possuem rolagem para janelas menores. Não há prévia ao vivo
opcional enviando comandos durante o arrasto: toda edição permanece local.

Salvar afeta a categoria atual e prepara os próximos Takes. Salvar e aplicar
no ar atualiza também os conteúdos publicados dessa categoria; preserva geração,
reprodução e as demais categorias. Cancelar descarta os rascunhos não salvos.
A troca de projeto fecha qualquer sessão de edição pertencente ao anterior.

Propriedades individuais continuam em Ad. Conteúdo: texto, grupo, categoria,
loop, término, opacidade, camada e transição. A sincronização agora copia somente
os campos de posição escolhidos entre categorias, com revisão e desfazer/refazer.
Take aplica a posição compartilhada, reinicia o vídeo e substitui somente a mesma
categoria. Out preserva as outras. As duas abas, miniaturas 16:9, marca original,
36 espaços iniciais e cores verde/vermelho foram preservados.

## Projetos e cenas do OBS

Cenas agora gerencia eventos: criar, renomear, selecionar, duplicar, excluir com
confirmação, importar/exportar, pasta e diagnóstico. Os projetos usam UUIDs
estáveis, não nomes. A duplicação copia organização, posições e preferências,
reutiliza os arquivos em cache e gera novos IDs; começa fora do ar, sem vínculos
que possam controlar as fontes do projeto original. Importar também limpa esses
vínculos. Não existe o antigo limite visual de duas lives; o Store conserva seu
limite técnico de proteção de 200 projetos.

Selecionar outro projeto não muda a cena de programa nem executa Out. O controlador
acompanha o estado confirmado de todos os projetos por provedor, avisa sobre o
anterior ainda no ar e restaura o tally ao voltar. A exclusão de um projeto
publicado é recusada até Out; originais e arquivos do cache não são apagados.

Link OBS concentra conexão e cenas reais. Oferece modo integrado nativo ou cliente
WebSocket 5 assíncrono com host, porta, senha, TLS, conectar/desconectar e reconexão.
A senha usa DPAPI, em arquivo separado da biblioteca/exportação; não aparece nos
logs. Configurações de destino são independentes por projeto. Os vínculos guardam
UUID de cena e fonte, categoria e nomes apenas para apresentação. Há listagem,
associação de todas/uma categoria, mostrar/ocultar e indicação de fonte ausente.

As ações verificam a propriedade das fontes antes de alterá-las. O código evita
sobreposição de composições globais e por categoria e reutiliza fontes existentes.
Fontes externas ao gerenciamento não recebem transformações ou configuração.
A troca de destino OBS é bloqueada enquanto o projeto possui conteúdo publicado.

Para controle remoto, o OBS de destino precisa do plugin 0.4 e acesso aos arquivos
por caminho local/compartilhamento. O campo de raiz remota traduz caminhos da
biblioteca; não implementa upload. A conexão foi validada com OBS real no mesmo
computador. TLS depende de servidor/proxy WSS adequado e certificado válido;
certificados inválidos não são ignorados.

O vermelho usa o snapshot do compositor depois de renderizar, presença na árvore
ativa do programa e geração do Take. WebSocket exige avanço da sequência de
observação; estado parado perde validade. Pedido enviado não é confirmação.
A queda de conexão limpa confirmação remota e bloqueia novos Takes, mantendo a
saída já existente. O fim de vídeo sem loop também é tratado no compositor.

## Migração e limpeza

A abertura preserva as migrações anteriores. Para `positionVersion < 1`, cria
backup exato `library.before-category-positions*.backup.json`, acrescenta as
posições compartilhadas, grava atomicamente e marca a versão. Usa a primeira
preparação existente de cada categoria como posição inicial, ou um padrão quando
vazia. Não altera IDs, caminhos, mídias, preferências nem o snapshot já publicado.
As preparações individuais antigas permanecem nos dados para recuperação.
Reabrir não repete a migração nem cria outro backup. Não houve conversão da
biblioteca real do usuário durante o desenvolvimento.

Removidos/substituídos:

- Classe ObsPreview e seu ciclo de criação/destruição de display nativo no diálogo.
- Editor por mídia e controles de propriedades embutidos no painel principal.
- Atualização do OBS disparada por cada draftChanged do editor de posição.
- Captura de miniatura síncrona no caminho de interação da interface.
- Diálogo antigo que usava Cenas para associar cenas reais.
- Gerenciador Projeto/biblioteca duplicado no menu e sincronização antiga por mídia na UI.
- Callbacks de preview/fábrica sem uso e listeners da interface substituída.

Os arquivos web/servidor anteriores continuam porque fontes Browser existentes,
exportações e testes ainda os referenciam. O serviço legado só é iniciado no
plugin quando há fonte antiga correspondente. A migração para o compositor nativo
conserva transformações e recortes da cena e pede confirmação.

## Arquivos alterados

| Arquivos | Responsabilidade |
| --- | --- |
| `src/project-settings.cpp` (novo), `store.hpp`, `store.cpp`, `store-operations.cpp` | Posições por categoria, migração, duplicação, vínculos, persistência e sincronização |
| `src/position-editor.hpp/.cpp` (novos), `position-canvas.cpp` | Sessão local, janela, canvas, setas, modelos e recorte |
| `src/project-dialogs.cpp`, `category-sync-dialog.cpp` (novos) | Projetos, Link OBS, propriedades e sincronização |
| `src/operator-panel.hpp/.cpp`, `operator-dialogs.cpp` | Painel preservado, abertura dos novos fluxos e remoção dos antigos |
| `src/controller.hpp/.cpp` | Tally por projeto/provedor, troca segura, ações e undo/redo |
| `src/obs-websocket.hpp/.cpp` (novos) | Protocolo OBS 5, credenciais, conexões, cenas, fontes e confirmação |
| `src/composition-state.cpp` (novo), `obs-native.hpp/.cpp` | Estado compartilhado, compositor, cache e captura assíncrona |
| `src/plugin-main.cpp`, `dev-main.cpp` | Integração dos novos componentes, OBS nativo e executável remoto |
| `tests/position-project-test.cpp`, `websocket-test.cpp` (novos) | Estado, interação, migração, projetos, protocolo e OBS real |
| `tests/operator-test.cpp`, `panel-test.cpp`, `render-test.cpp` | Regressão adaptada ao posicionamento compartilhado |
| `scripts/test-obs-operator.ps1` | OBS portátil, UI, recursos de janela, WebSocket e gravação |
| `CMakeLists.txt`, `package.json`, `package-lock.json` | Build 0.4, cinco suítes e Qt WebSockets |
| `scripts/package.ps1`, `installer.nsi`, `vendor/Qt-NOTICE.md` | Distribuição da dependência Qt, licenças e instalador |
| `README.md`, `LEIA-ME.md`, `docs/BUILD.md`, este relatório | Operação, build, diagnóstico e entrega |
| `artifacts/` | Resultados JSON, log de testes e capturas verificáveis |

Não há Git instalado neste ambiente; a tabela registra os arquivos trabalhados
nesta entrega, sem alegar um diff contra commit remoto.

## Testes e evidências

As cinco suítes CTest passaram. A saída completa fica em
`artifacts/ctest-0.4.log`. As contagens e os hashes finais estão em
`artifacts/validation-0.4.json`.

Resultados: core **67**, operator **204**, panel **209**, position **112** e
websocket simulado **31** verificações (623 no CTest). A integração WebSocket
real realizou **44** verificações, incluindo gravação. Renderização D3D11 sem
estresse adicional: **68** verificações. Branding e os **7** testes Playwright
passaram, assim como a integração do serviço Qt/HTTP.

O ensaio adicional D3D11 de **60 segundos** passou com **193 verificações** e
**2.449 atualizações**. Memória privada inicial/final: **130.392.064 /
132.390.912 bytes**; pico: **132.493.312 bytes**, dentro do limite de 64 MiB
adicionais verificado pelo teste. Isso mede o renderizador isolado e não equivale
a uma transmissão prolongada. O instalador e o ZIP também foram verificados em
destino isolado, incluindo hashes das duas DLLs e documentos.

Na execução real final, foram **246.445 movimentos locais** durante gravação,
com **zero** estados adicionais aplicados e **zero** fontes criadas no compositor.
O teste de janela realizou **5 ciclos / 1.000 movimentos**: objetos USER de
**130 para 125**, GDI de **72 para 66**, biblioteca intacta e nenhuma nova falha
de criação de janela. O arquivo de gravação resultante tem **3.209.445 bytes**.
Esses movimentos de modelo são distintos dos eventos no editor real.

SHA-256 da DLL validada no OBS e distribuída:
`3874F82070BFDAF4A5FFF4DC582ABABE2868560B90AE8AA077108F969CFDFFCE`.

| Requisito solicitado | Cobertura |
| --- | --- |
| 1–6: abrir, setas, mouse, X/Y, tamanho, salvar | Teste QWidget com eventos reais, três incrementos, persistência e cancelamento |
| 7–8: mesma categoria e independência | Take entre QR-Codes; Apply preserva Logos; máscara de sync com undo/redo |
| 9–13: CRUD, confirmação, duplicação, troca | Store e gerenciador Qt offline; cancelar/confirmar exclusão, preservar arquivos/programa |
| 14: persistência e migração | Reabrir Store, posições independentes, backup exato e migração idempotente |
| 15: associação em Link OBS | UI Automation no OBS portátil e cliente WebSocket com cenas/fontes reais |
| 16–17: Take/Out e cores | Painel Qt, programa real, geração e confirmação pós-render |
| 18: vídeo com/sem loop | OBS real: one-shot com Out automático e loop preservando outra categoria |
| 19–20: offline e reconexão | CRUD offline, Take recusado, reconexão voluntária real e queda simulada |
| 21: movimento contínuo | Sessão intensiva sem escritas/comandos; cinco ciclos e mil movimentos no editor real |
| 22: estabilidade durante saída | Gravação local real durante o teste intensivo e renderização D3D11 separada |

O teste de comunicação simulado usa a mesma implementação assíncrona de produção,
mas distingue explicitamente pedido de conteúdo efetivamente observado. O teste
real usa servidor OBS autenticado, plugin carregado, Take/Out de duas categorias,
leitura de contadores do renderizador, Save/Apply, troca de projeto, desconexão,
reconexão e remoção de fonte. Durante a movimentação, os contadores de estados
aplicados e fontes criadas ficam inalterados. Os números medidos constam em
`websocket-real-0.4.json` e `position-stress-0.4.json`.

`obs-native-smoke-results.json` registra cinco aberturas/fechamentos, mil comandos
de movimento, biblioteca inalterada, contagem USER/GDI, F11/Esc, retorno ao dock,
reinício e migração. O log dessa execução é verificado contra novas falhas de
criação de janela. A instalação principal e a biblioteca de produção permanecem
fora do teste. Os binários/arquivos portáteis gerados ficam em `.deps`.

Também foram executados renderização libobs/D3D11, branding original por SHA-256,
sete testes Playwright do painel legado e integração real do serviço Qt/HTTP,
SSE, HTTPS e cache offline. Esses testes de navegador cobrem compatibilidade;
a validação da interface atual está nas suítes Qt e no OBS real.

## Limites de homologação

O ensaio usa um monitor físico 1920×1080. Layouts maiores/menores são testados por
redimensionamento, mas isso não comprova DPI misto ou hotplug físico. Não foi feita
transmissão a uma plataforma externa, ensaio de quatro horas, conexão entre dois
computadores físicos ou teste WSS com certificado de produção. A gravação local
não substitui esses ensaios. O renderizador mantém os vídeos silenciosos como antes.

Antes de operar uma produção: faça backup da biblioteca, instale com OBS fechado,
ensaie suas cenas/codecs, teste Take/Out simultâneos e vídeos, verifique vínculos
e tally no programa, reconexão e monitor de operação. Para outra máquina, valide
primeiro o acesso à biblioteca compartilhada. Os pacotes 0.4 e suas somas SHA-256
ficam em `dist/`; o teste de instalação é feito em destino isolado.
