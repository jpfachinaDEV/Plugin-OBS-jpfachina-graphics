# Refatoração 0.3 — JPFachina OBS Controller

Entrega de 16/11/2026. Interface e operações alteradas no projeto existente.

## Arquitetura encontrada e preservada

O produto é um plugin C++17/Qt Widgets 6.8.3 para OBS Studio 32.1.0 x64.
Não era um cliente WebSocket: a DLL acessa libobs e obs-frontend-api diretamente.
O dock e a janela de operação compartilham Controller, Store e Downloader.
Store grava JSON atomicamente com QSaveFile, mantém bloqueio de instância e
copia arquivos para um cache local. O compositor nativo usa cenas privadas,
fontes de imagem e fontes FFmpeg do próprio OBS. O preview é uma composição
separada, que não publica alterações na saída.

O servidor HTTP e os arquivos em web/ ainda têm referências efetivas:
alimentam Browser Sources de instalações anteriores. Não foram apagados;
o runtime só inicia o servidor quando encontra essas fontes. A migração de
associação preserva transformação, recorte, visibilidade e ordem das fontes
anteriores. Bibliotecas antigas recebem backup antes da atualização.

Não foram adicionadas dependências de execução ou uma segunda aplicação.
O executável de desenvolvimento existente abre o painel nativo por padrão;
`--legacy-ui` ou `--no-browser` mantém o caminho de teste de compatibilidade.

## Implementação

- Duas abas principais, seis categorias e 36 cartões iniciais. Cada categoria
  mantém seis espaços; bibliotecas maiores têm paginação em grupos de seis.
- Fundo radial escuro, botões arredondados azuis/vermelhos, categorias brancas,
  abas com borda branca e a logomarca original incorporada sem editar seus bytes.
- Cartões 16:9 com ajuste proporcional da mídia, nome e estados acessíveis.
  Resoluções estreitas reorganizam colunas e permitem rolagem.
- Seleção verde independente da saída. A borda vermelha depende da presença
  da composição na árvore ativa da saída de programa e do retorno do compositor
  após criação das fontes e atualização no thread gráfico. Uma solicitação TAKE
  gravada na biblioteca não é tratada como confirmação de programa.
- TAKE troca atomicamente o conteúdo da categoria correspondente, conserva
  outras categorias e incrementa a geração de reprodução. Não troca cenas.
  Cada novo TAKE de vídeo reinicia o mesmo decoder, sem duplicá-lo.
- OUT atua somente no conteúdo selecionado. O vermelho persiste durante a
  transição de saída e desaparece após confirmação. Avisos distinguem comando
  enviado de confirmação recebida, incluindo a geração esperada no TAKE.
- Loop e comportamento final por mídia, persistidos. FFmpeg usa o valor real
  de `looping`. O runtime consulta o estado `OBS_MEDIA_STATE_ENDED` e retira
  conteúdos configurados para OUT automático; a alternativa mantém o quadro.
- Miniaturas de vídeo são capturadas pela composição privada ao selecionar
  a mídia e ficam em cache. Os 36 cartões não criam 36 players. O preview de
  vídeo é liberado quando a miniatura está pronta e o editor está fechado.
- Cadastro, renomeação, categorias, substituição de arquivo em segundo plano,
  importação local/HTTPS e exclusão de cadastro. Exclusão/substituição pelo
  painel exigem retirada do ar; os arquivos originais não são apagados.
- Cenas consulta o frontend real do OBS, lista composições e seus projetos,
  associa o projeto à cena escolhida, permite mostrar/ocultar sua composição
  e oferece troca explícita de cena com confirmação. As categorias são camadas
  da composição nativa, preservando a arquitetura já existente.
- Editor auxiliar preserva canvas, coordenadas, escala, rotação, recorte,
  opacidade, âncoras, alinhamento, presets e sincronização com máscara de campos.
  Campos de tamanho permitem preservar a proporção; no canvas, Shift mantém
  a proporção durante o redimensionamento.
- Projetos, importação/exportação, histórico, desfazer/refazer sincronização,
  modelos, seleção de monitor, F11/Esc e restauração de janela foram preservados.
- Link OBS conecta/desconecta o controle local e consulta novamente os estados
  ao reconectar. Desconectar não desmonta a composição que já está no OBS.

## Conexão e compatibilidade

Endereço, porta e senha de um servidor WebSocket **não foram implementados**.
A arquitetura preservada opera dentro do próprio OBS, sem conexão de rede ou
senha. Link OBS informa essa característica e controla a disponibilidade dos
comandos nativos. O executável de desenvolvimento não controla um OBS remoto.
Para operar de outra máquina seria necessária uma extensão explícita da
arquitetura; nenhum campo sem efeito foi incluído na interface.

As categorias antigas são conservadas na biblioteca e no gerenciador.
O painel apresenta Banners/Tarjetas/Rodapés em Banner, Overlays/Animações em
Vinhetas e outras categorias antigas em Banner ou Vídeos, conforme o tipo.
Isso mantém os arquivos e nomes recuperáveis e permite reorganização pelo
gerenciador, sem criar abas principais adicionais. A substituição por TAKE
considera a categoria apresentada no painel.

Eventos do frontend acionam atualização de cena/coleção. Uma consulta local
a cada 250 ms lê o snapshot compacto do compositor e a árvore do programa,
reconhecendo visibilidade, transições, remoções e fim de mídia. Não há consultas
de rede nem regravação da biblioteca a cada consulta; a interface só recebe
alteração quando o conjunto confirmado muda.

## Arquivos alterados

| Grupo | Arquivos |
| --- | --- |
| Interface | src/operator-panel.cpp, src/operator-panel.hpp, src/operator-dialogs.cpp |
| Cartões novos | src/media-card.cpp, src/media-card.hpp |
| Operação e estados | src/controller.cpp, src/controller.hpp |
| Integração real | src/plugin-main.cpp, src/obs-native.cpp, src/obs-native.hpp |
| Persistência | src/store.cpp, src/store.hpp, src/store-operations.cpp |
| Inicialização/build | src/dev-main.cpp, CMakeLists.txt, package.json, package-lock.json |
| Testes | tests/panel-test.cpp (novo), tests/core-test.cpp, tests/operator-test.cpp, tests/render-test.cpp |
| Validação/pacote | scripts/test-obs-operator.ps1, scripts/package.ps1, scripts/installer.nsi |
| Documentação | README.md, LEIA-ME.md, docs/BUILD.md, este relatório |

Nenhum arquivo de produção foi removido. As duas versões de layout antigas
(splitter e abas compactas) e seus estilos/listas foram substituídas dentro de
operator-panel.cpp. A dependência de include do antigo header obs-browser no
target do plugin foi retirada; licenças e recursos de compatibilidade foram
mantidos por terem consumidores ou valor de procedência.

## Testes e evidências

Os resultados abaixo devem ser lidos junto com os artefatos da execução atual,
não com relatórios 0.1/0.2 existentes no repositório.

| Verificação | Evidência |
| --- | --- |
| Build C++/Qt/DLL | cmake --build build --parallel 2 |
| Store/HTTP/segurança de importação | CTest core: 67 verificações |
| Sincronização, canvas, projetos, migração, janela | CTest operator: 203 verificações |
| Duas abas, 36 espaços, 16:9, estados confirmados, categorias independentes, loop persistido | CTest panel: 210 verificações |
| Renderização real D3D11/FFmpeg | jp-render-tests: 68 verificações; inclui vídeo com/sem loop e reinício do mesmo decoder |
| Marca original | teste SHA-256, assinatura e dimensões PNG |
| Compatibilidade HTML | sete testes Playwright |
| Serviço Qt real e cache HTTPS | tests/native-e2e.mjs aprovado, com autenticação, SSE, isolamento e download real |
| OBS portátil real | scripts/test-obs-operator.ps1 e artifacts/obs-native-smoke-results.json |
| Aparência | artifacts/panel-aba1-1920.png, panel-aba2-1920.png, obs-native-operator.png |

O teste de unidade do painel injeta confirmações deliberadamente para verificar
que comandos sozinhos nunca tornam um cartão vermelho. Isso não substitui o
teste real do compositor ou do plugin. O teste portátil carrega a DLL compilada
em perfil isolado, usa acessibilidade do Windows para operar o painel e verifica
reprodução, encerramento e persistência. A instalação principal não é alterada.

## Executar e validar manualmente

1. Com OBS fechado, instale o pacote 0.3 apropriado para OBS 32.1.0/Qt 6.8.3.
   Não mantenha duas DLLs do plugin instaladas simultaneamente.
2. Abra o dock JP Fachina Graphics. Em Cenas, escolha a cena e associe a
   composição do projeto. Use a cena no programa para testar a indicação vermelha.
3. Cadastre dois logos, um banner e um QR-Code. Clique no primeiro logo: verde,
   sem publicar. TAKE: vermelho após confirmação. Selecione outro logo: verde,
   mantendo o primeiro vermelho até novo TAKE. Confira a substituição.
4. Publique banner e QR-Code. Retire apenas o selecionado com OUT e confira
   que logo/banner permanecem. Troque ABA 1/ABA 2 sem interromper a saída.
5. Cadastre um vídeo em cada categoria da ABA 2. Verifique loop ligado,
   desligado, manutenção do quadro e retirada automática. Confira início em zero
   em novo TAKE e retorno da preferência após selecionar outra mídia e reiniciar.
6. No OBS, oculte/mostre a composição, troque a cena de programa e use o modo
   estúdio para deixar a composição apenas no preview. Vermelho deve acompanhar
   somente o programa. Confira também transições entre cenas com a composição.
7. Edite posição/tamanho/alinhamento e salve: programa não deve mudar antes
   de TAKE. Teste presets, máscara de sincronização e desfazer/refazer.
8. Teste cadastro, substituição e exclusão fora do ar; tente fazê-los enquanto
   a mídia estiver no ar e confira o bloqueio. Confira os arquivos originais.
9. Desconecte pelo Link OBS: comandos ficam bloqueados e a saída se mantém.
   Reconecte: a borda deve voltar a refletir a saída real.
10. Abra o menu ⋯, selecione o monitor secundário e use F11/Esc. Reinicie o OBS
    e confira aba, biblioteca, posições, loop e preferência da janela.

## Limites declarados

- O ambiente dispõe de um monitor físico. Segundo monitor real, DPI misto,
  hotplug e operação contínua de quatro horas exigem homologação local.
- Ainda é necessário ensaio manual de alterações externas combinadas com
  transições complexas, fontes compartilhadas e coleções de produção.
- Os vídeos continuam silenciosos, como na versão anterior. Codecs e manutenção
  do último quadro dependem do módulo FFmpeg do OBS e do formato do arquivo.
- A miniatura de vídeo é gerada ao selecioná-lo; antes disso aparece um indicador
  de prévia pendente. Não há decodificação simultânea da biblioteca inteira.
- Downloads preservam a política existente: HTTPS público direto, sem redirects,
  até 256 MB; mídia fica local após importação. Substituição por URL pode ser feita
  importando o novo arquivo na categoria e retirando/excluindo o cadastro anterior.
- Bibliotecas muito grandes e uso remoto via WebSocket não foram homologados.
- O instalador não é assinado digitalmente. O alvo continua OBS 32.1.0 x64;
  não se afirma compatibilidade de ABI com outras versões do OBS/Qt.

Referência de integração: [API de fontes do OBS](https://docs.obsproject.com/reference-sources)
e [frontend do OBS](https://docs.obsproject.com/reference-frontend-api). Os headers
locais usados no build são os da versão 32.1.0; a documentação online pode
descrever uma versão mais recente.
