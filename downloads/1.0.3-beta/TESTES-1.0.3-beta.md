# JP Fachina Graphics 1.0.3 beta — testes

- Compilação completa para Windows x64: aprovada (OBS 33.0.1 / Qt 6.11.1).
- CTest: 5 de 5 suítes aprovadas. O teste de posição verifica edição individual do scoreboard, aplicação ao item no ar, troca de modelo sem perda de geometria e preservação das outras fontes.
- Libobs com D3D11: 101 verificações aprovadas, incluindo renderização dos cinco modelos, Take/Out, atualização da pontuação e mudança real da posição/tamanho na imagem composta.
- Teste de OBS portátil: a DLL carregou em uma execução anterior. Nesta versão, o script parou na checagem de geometria do F11 (janela 1726 × 1007 num monitor informado como 1920 × 1080), antes de validar o editor com interação humana. Esse cenário ainda deve ser conferido no OBS da instalação de destino.

O scoreboard é transformado internamente na composição. Transformar a fonte JP Fachina Graphics diretamente na cena do OBS também afeta outros grafismos; para mover só o placar, use **Editar posição → Scoreboard**.
