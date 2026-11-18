# JPFachina OBS Controller 1.0.3 beta

Plugin para OBS Studio **33.0.1 x64 / Qt 6.11.1**, no Windows.

## Instalação

O pacote **1.0.3 beta é para OBS 33.0.1 / Qt 6.11.1**. O pacote anterior 0.4.0 era
para OBS 32.1 / Qt 6.8.3 e não deve ser usado no OBS 33. Feche completamente o OBS
antes de atualizar; a biblioteca de conteúdos é preservada. Consulte o
[diagnóstico de compatibilidade](docs/FIX-OBS33-0.4.1.md).

Feche o OBS. Execute `downloads/1.0.3-beta/JP-Fachina-Graphics-1.0.3-beta-Windows-x64-Setup.exe`
ou extraia o ZIP em `%PROGRAMDATA%/obs-studio/plugins/`, preservando a pasta
`jp-fachina-graphics`. A DLL `Qt6WebSockets.dll` acompanha a DLL do plugin em
`bin/64bit`. Não instale duas cópias. O instalador preserva a biblioteca e não
substitui as DLLs centrais do OBS/Qt.

Para OBS portátil, `cmake --install build --prefix <OBS>` instala as duas DLLs
em `obs-plugins/64bit` e os dados em `data/obs-plugins/jp-fachina-graphics`.
Ou copie manualmente o conteúdo de `bin/64bit` do ZIP para a primeira pasta e
o conteúdo de `data` para a segunda.

Abra **Exibir → Docks → JP Fachina Graphics**. Crie ou selecione seu evento em
**Cenas**. Em **Link OBS**, configure a conexão e associe a composição a uma cena
real. A associação não troca automaticamente o programa do OBS.

## Projetos e cenas reais

**Cenas** gerencia projetos/eventos: criar, selecionar, renomear, duplicar,
excluir com confirmação, importar/exportar e abrir a pasta de dados. Cada projeto
tem ID estável, seis categorias, biblioteca, posições e preferências próprias.
Duplicar reutiliza os arquivos físicos e cria novos IDs; exige novas associações
no Link OBS. O último projeto ativo é restaurado ao iniciar.

Trocar de projeto não executa Out nem altera o programa. Um aviso identifica
outros projetos ainda confirmados no ar. Volte ao projeto anterior para operar
seus conteúdos. Projetos com conteúdo publicado precisam de Out antes da exclusão.
Excluir cadastros não apaga os arquivos originais do computador.

**Link OBS → Conexão** oferece:

- **OBS integrado:** controle nativo no processo do OBS, sem servidor WebSocket.
- **OBS WebSocket:** host, porta, senha, TLS opcional, Conectar e Desconectar.
  Ative o servidor em Ferramentas → Configurações do servidor WebSocket do OBS.
  O destino precisa deste plugin 0.4 instalado: a saída usa seu compositor.
  O executável de desenvolvimento pode usar esse modo.

O endereço pode ser `127.0.0.1` para o mesmo computador. Para outro computador,
disponibilize a biblioteca inteira por compartilhamento ou cópia e informe em
**Biblioteca no OBS de destino** a raiz que contém `media`, `thumbs` e `titles`.
Esse campo traduz caminhos; não transfere arquivos. Preserve os caminhos relativos
e sincronize novas mídias antes do Take. Sem ele, são enviados os caminhos locais.

A senha é protegida por DPAPI para o usuário Windows atual, fora do JSON da
biblioteca e das exportações. Quedas bloqueiam novos Takes e provocam reconexão
com espera progressiva. Desconectar preserva a saída e suspende comandos; o vermelho
volta somente após nova confirmação real.

**Link OBS → Cenas e fontes do OBS** lista cenas e fontes gerenciadas, permite
associar todas as categorias ou uma categoria específica, mostrar/ocultar o
vínculo e atualizar. Vínculos inexistentes são identificados. Evite composições
sobrepostas para a mesma categoria. Fontes de terceiros são preservadas. Projeto
e cena usam identificadores independentes; nomes podem mudar. Migrar uma fonte
Browser antiga pede confirmação e conserva transformação e recorte. Faça essa
migração fora de uma transmissão de produção.

## Operação

- **ABA 1:** Logos, Banner, QR-Codes. **ABA 2:** Vinhetas, Abertura, Vídeos.
- São seis espaços 16:9 por categoria. Setas paginam bibliotecas maiores.
- Clique na mídia para selecionar: **verde**. Isso não a coloca no ar.
- **Take** usa a posição da categoria e substitui somente a mesma categoria.
- **Vermelho** exige confirmação da renderização no programa. Preview, fonte
  oculta, cena fora do programa e pedido ainda não confirmado não bastam.
- **Out** retira somente a seleção, preservando outras categorias. Aguarde a
  confirmação, inclusive durante a transição de saída.
- Trocar de aba ou selecionar outra mídia não interrompe o que está no ar.

**Ad. Conteúdo** cadastra arquivos locais e URL HTTPS, edita propriedades,
substitui e exclui. Nas propriedades ficam nome, categoria, grupo, texto,
opacidade, camada, transição, duração, loop e comportamento no término.
Execute Out antes de excluir ou substituir mídia publicada.

Para vídeos, o atalho **Reproduzir em loop** aparece na tela principal. Sem loop,
escolha manter o último quadro ou retirar do ar nas propriedades. Um novo Take
reinicia do início e aplica a preparação. Vídeos permanecem silenciosos.
Miniaturas são capturadas de forma assíncrona ao selecionar a mídia.

## Editor de posição

**Editar posição** abre uma janela dedicada. Escolha uma das seis categorias;
o enquadramento usa a resolução base do OBS conectado. A posição é compartilhada
por todos os conteúdos dessa categoria, dentro do projeto atual.

Arraste, redimensione pelos controles, digite X/Y/largura/altura, escala e rotação,
ou use as setas grandes e o teclado. Selecione **Fino: 1 px**, **Normal: 10 px**
ou **Rápido: 50 px**. A área mantém a proporção quando marcada; Shift também
preserva a proporção no canvas. O conteúdo cabe na área sem distorção. Âncora,
alinhamento, recorte e modelos reutilizáveis completam o posicionamento.

Os movimentos são locais. **Salvar** guarda a categoria atual para os próximos
Takes. **Salvar e aplicar no ar** também atualiza essa categoria já publicada,
sem modificar outras categorias. Salve cada categoria que editar. **Cancelar**
descarta mudanças não salvas; **Restaurar padrão** prepara o padrão da categoria,
que ainda precisa ser salvo. A janela não contém um display GPU nativo do OBS.

No menu **⋯**, sincronizar posições copia os campos escolhidos entre categorias
do projeto, apresenta revisão e permite desfazer/refazer. Altera a preparação.
O menu também oferece janela separada, monitor, abertura automática e retorno
ao dock. **F11** alterna tela cheia; **Esc** retorna à janela.

## Dados, recuperação e limites

Dados normais: `%APPDATA%/obs-studio/plugin_config/jp-fachina-graphics`.
Portátil: `<OBS>/config/obs-studio/plugin_config/jp-fachina-graphics`.
**Cenas → Abrir pasta da biblioteca** mostra o caminho efetivo.

Guarde `library.json`, `media`, `thumbs`, `titles`, `operator.json`, `panel.json`
e `obs-credentials.json`. O último só é descriptografável pelo usuário Windows
que o criou. A migração cria `library.before-category-positions*.backup.json`
antes de acrescentar posições compartilhadas. Preserva IDs, caminhos, preparações
antigas e saída publicada. Para reverter, feche o OBS e restaure os dados e a
versão correspondentes a partir de sua cópia completa.

PNG, JPEG, WebP, GIF, WebM e MP4 são aceitos, até 256 MB por arquivo. URLs novas
exigem HTTPS público direto, sem redirecionamentos. O cache funciona offline.
A desinstalação preserva a biblioteca. O EXE não é assinado digitalmente.

O [relatório 0.4](docs/REFACTOR-0.4.md) documenta diagnóstico e testes, incluindo
OBS real e gravação local. Segundo monitor, DPI misto, hotplug, transmissão externa,
outro computador físico e quatro horas contínuas ainda exigem homologação.
Ensaie suas cenas, transições e codecs antes da produção.
