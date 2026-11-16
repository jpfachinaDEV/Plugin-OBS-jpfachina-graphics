# Instalação e operação — 0.4.1

Compatibilidade do pacote: Windows x64, OBS Studio 33.0.1 e Qt 6.11.1.
Não substitua as DLLs centrais do Qt do OBS.

## Instalar

Feche completamente o OBS. Execute o instalador EXE da pasta `downloads/0.4.1`.
O destino normal é `%PROGRAMDATA%/obs-studio/plugins/jp-fachina-graphics`.
Se o Windows recusar a gravação, execute o instalador como administrador.
Evite manter duas cópias da DLL do plugin. A biblioteca do usuário é preservada.

Na instalação manual, extraia o ZIP em `%PROGRAMDATA%/obs-studio/plugins/` e
mantenha a pasta `jp-fachina-graphics`. Dentro de `bin/64bit` precisam estar
`jp-fachina-graphics.dll` e `Qt6WebSockets.dll`, ambas do mesmo pacote.

Para OBS portátil, copie as duas DLLs de `bin/64bit` para `obs-plugins/64bit`.
Copie o conteúdo de `data` para `data/obs-plugins/jp-fachina-graphics`.
O CMake também oferece `cmake --install build --prefix <pasta-do-OBS-portátil>`.

Abra o OBS e o dock **JP Fachina Graphics**. Se necessário, habilite o plugin no
gerenciador e reabra o OBS. Em **Cenas**, crie seu projeto; em **Link OBS**,
associe a composição a uma cena real. A associação não troca o programa.

## Selecionar, Take e Out

ABA 1 contém Logos, Banner e QR-Codes. ABA 2 contém Vinhetas, Abertura e Vídeos.
Cada categoria mostra seis miniaturas 16:9; setas paginam bibliotecas maiores.

- Clique para selecionar: verde. Isso não publica a mídia.
- Take publica a seleção e substitui somente a mesma categoria.
- Vermelho exige confirmação da presença no programa. Uma fonte oculta ou uma cena fora do programa não basta.
- Out retira a seleção, preservando as outras categorias. Aguarde a confirmação, inclusive nas transições.
- Selecionar outra mídia ou trocar de aba não interrompe a saída existente.

Ad. Conteúdo importa arquivos locais ou URLs HTTPS diretas e permite editar,
substituir e excluir cadastros. Execute Out antes de substituir ou excluir uma
mídia publicada. Os arquivos originais do computador não são apagados.

Nas propriedades ficam texto, categoria, grupo, opacidade, camada, transição,
loop e comportamento no término. Para vídeos, há também um atalho de loop no
painel. Um novo Take reinicia o vídeo. A reprodução permanece silenciosa.

## Editor por categoria

Editar posição abre uma janela dedicada. A posição é compartilhada por todos
os conteúdos da categoria no projeto atual, usando a resolução base do OBS.
Arraste, redimensione, digite X/Y/tamanho/escala/rotação ou use as setas grandes
e o teclado. Escolha incrementos de 1, 10 ou 50 pixels. Há âncora, alinhamento,
recorte, modelos e opção de preservar proporção.

Os movimentos ficam em memória. Salvar guarda a categoria atual para os próximos
Takes. Salvar e aplicar no ar também atualiza essa categoria publicada. Cancelar
descarta mudanças não salvas. Restaurar padrão prepara o padrão, que ainda precisa
ser salvo. Salve cada categoria que editar.

O menu ⋯ oferece sincronização de campos de posição entre categorias com revisão,
desfazer/refazer, janela separada, monitor e retorno ao dock. F11 alterna tela cheia;
Esc retorna à janela.

## Projetos e conexão

Cenas gerencia eventos com IDs estáveis: criar, selecionar, renomear, duplicar,
excluir com confirmação, importar/exportar e abrir a pasta da biblioteca.
Duplicar reutiliza arquivos físicos, preserva preferências e gera novos IDs;
a cópia começa fora do ar e exige novas associações. O último projeto ativo é
restaurado ao iniciar.

A troca de projeto preserva o programa e avisa sobre outros projetos ainda no ar.
Volte ao projeto anterior para operar seus conteúdos. Antes de excluir um projeto
publicado, execute Out.

Link OBS concentra conexão e cenas/fontes reais. O modo integrado controla o OBS
local sem servidor WebSocket. O modo WebSocket 5 usa host, porta, senha e TLS
opcional; ative o servidor nas ferramentas do OBS. Existe reconexão progressiva.
A senha é protegida por DPAPI no Windows e não integra a exportação da biblioteca.

O OBS remoto precisa deste plugin instalado e acesso à biblioteca. O campo de
pasta no destino traduz os caminhos relativos; não transfere os arquivos. Use
uma cópia ou compartilhamento da biblioteca e sincronize novas mídias antes de
operar. A conexão foi testada em OBS real no mesmo computador.

Em Cenas e fontes do OBS, associe todas as categorias ou uma categoria específica,
mostre/oculte os vínculos e atualize. Fontes ausentes são indicadas. As ações
preservam fontes de terceiros. Projeto e cena usam identificadores independentes.

## Dados e recuperação

Dados normais: `%APPDATA%/obs-studio/plugin_config/jp-fachina-graphics`.
Portátil: `<OBS>/config/obs-studio/plugin_config/jp-fachina-graphics`.
Use Cenas → Pasta da biblioteca para abrir o local efetivo.

Guarde uma cópia da pasta inteira, incluindo `library.json`, `media`, `thumbs`,
`titles`, `operator.json`, `panel.json` e `obs-credentials.json`. O arquivo de
credenciais só pode ser descriptografado pelo usuário Windows que o criou.
A migração de posições cria backup antes de acrescentar a configuração por
categoria e preserva os dados existentes. A desinstalação preserva a biblioteca.

São aceitos PNG, JPEG, WebP, GIF, WebM e MP4, até 256 MB por arquivo. URLs novas
exigem HTTPS público direto, sem redirecionamentos. O cache funciona offline.

Ensaie suas cenas e codecs antes da produção. Quatro horas contínuas, transmissão
externa, outro computador físico, segundo monitor e DPI misto ainda exigem
homologação. O instalador não possui assinatura digital.
