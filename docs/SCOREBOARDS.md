# Scoreboards personalizados

No painel Qt do plugin, abra **Scoreboard**. A biblioteca mostra cinco modelos de referência e os modelos personalizados instalados. **+ Adicionar Scoreboard** oferece importar PNG, duplicar o modelo selecionado e importar um pacote ZIP. O editor de PNG permite acrescentar campos padronizados, arrastar, redimensionar pela alça, ajustar coordenadas em porcentagem, fonte, tamanho, cor, negrito, alinhamento, espaçamento, contorno, sombra, visibilidade e ordem. Há grade, zoom e desfazer/refazer. A prévia usa somente dados fictícios.

**Selecionar no perfil** cria ou atualiza o item de scoreboard na categoria Banner do projeto atual. Edite os dados da partida no lado direito da biblioteca; a pontuação muda ao alterar os controles. **Take** e **Out** usam o mesmo compositor nativo e a mesma camada dos demais conteúdos do projeto. A posição do item no vídeo pode ser ajustada em **Editar posição**. O cronômetro pode ser iniciado e pausado; seu estado é salvo no perfil.

Para adicionar escudos, escolha **Escudo 1** ou **Escudo 2**. A imagem é importada para a biblioteca de Logos do evento. Um modelo pode conter somente os campos necessários. Os identificadores disponíveis são `team1.name`, `team1.shortName`, `team1.logo`, `team1.score`, `team2.name`, `team2.shortName`, `team2.logo`, `team2.score`, `match.clock`, `match.period`, `match.extraTime`, `match.competition` e `match.custom`.

Os cinco modelos de referência são protegidos; **Editar** cria uma cópia personalizada. Um modelo personalizado pode ser renomeado no editor, duplicado, exportado e excluído quando nenhum perfil o utiliza. A biblioteca de modelos é separada dos dados dos projetos. PNGs opacos são aceitos, mas cobrem o vídeo nas regiões opacas. Texto ou números já desenhados na imagem não se tornam editáveis.

**Exportar** gera um `.zip` com `template.json` e `base.png`. **Importar pacote** aceita apenas esse formato versão 1, com dois arquivos ZIP sem compressão; verifica nomes, comprimentos e CRC antes de instalar. Pacotes com scripts, arquivos adicionais, caminhos relativos ou outros métodos ZIP são recusados. Fontes são referenciadas por família e não são incluídas no pacote; instale a mesma fonte no computador de destino se necessário. Os escudos pertencem aos dados do evento e não ao modelo.

Arquivos gerenciados ficam em `scoreboards/`, `scoreboard-thumbs/` e `scoreboard-cache/` dentro da biblioteca do plugin. O cache de quadros antigos é limpo após cinco minutos. A exportação de um projeto conserva a referência ao scoreboard; ao importar em outra instalação, importe primeiro o pacote do modelo. Modelos de referência estão sempre disponíveis.

## Limites atuais

- Os cinco visuais de referência são redesenhados em código. Os PNGs exatos mostrados no pedido não constavam nos arquivos do repositório; para usá-los pixel a pixel, importe os PNGs originais como modelos personalizados.
- A prévia do botão **Testar animação** mostra a entrada pela opacidade. Os movimentos de entrada e saída são aplicados pelo compositor no Take/Out.
- O editor de campos suporta uma instância de cada identificador. O pacote ZIP usa arquivos armazenados sem compressão para permitir validação estrita.
# Posicionamento do scoreboard

Selecione um modelo na ABA 3 e abra **Editar posição**. Escolha **Scoreboard** na lista. Arraste a prévia para mover o placar e use as oito alças para alterar o tamanho; a proporção fica preservada até desmarcar **Preservar proporção da área**. Os campos X, Y, largura, altura, escala e âncora permitem ajustes numéricos. Há comandos de centralização e alinhamento às bordas.

**Salvar** grava a posição no perfil para o próximo Take. **Salvar e aplicar no ar** também atualiza o scoreboard já visível, sem alterar outras peças. **Cancelar** descarta o ajuste ainda não salvo. A troca de modelo preserva X, Y, largura, altura e camada. **Restaurar padrão** repõe o enquadramento inicial.

O placar é um elemento interno da fonte composta JP Fachina Graphics. Transformar essa fonte diretamente na cena do OBS altera todas as peças contidas nela; use o editor do plugin para mover somente o scoreboard. O compositor aplica a transformação do placar dentro da fonte composta.

