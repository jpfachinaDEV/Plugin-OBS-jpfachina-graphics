# Validação — 9 de novembro de 2026

## Ambiente

Windows 11 x64, OBS Studio 32.1.0, Qt 6.8.3, MSVC 19.44.35229, Windows SDK
10.0.26100 e CMake 3.31.8. Os SDKs e as ferramentas foram preparados em `.deps/`,
sem instalação global do compilador e sem substituir arquivos do OBS do usuário.
Build `RelWithDebInfo`, C++17, opções `/W4 /permissive- /utf-8`.

## Resultados automatizados

| Verificação | Resultado |
| --- | --- |
| Compilar DLL nativa do plugin e executável de desenvolvimento | Passou |
| Suíte C++: 8 grupos, 67 verificações | Passou |
| SHA-256, assinatura e dimensões da logo original | Passou |
| Suíte Playwright em Chrome: 7 testes | Passou |
| Integração navegador + serviço Qt real + SSE | Passou |
| Download HTTPS real com IP público fixado e cache local | Passou |
| Gerar instalador Windows NSIS e ZIP | Passou |
| Instalar EXE em diretório isolado e conferir SHA-256 da DLL | Código 0; bytes iguais |
| Desinstalar nesse diretório isolado | Código 0; DLL removida |
| Carregar a DLL final e servir a logo no OBS 32.1.0 portátil | Passou; hash original |
| Encerrar pela janela principal com a DLL final ativa | Passou; sem processo/servidor residual |

Os testes C++ exercitam persistência após reinício, isolamento, draft/live,
geração de reprodução, rollback em validação, trava de instância, corrupção
preservada, renomeação de categorias, miniaturas, bloqueio de traversal,
assinaturas de arquivo, endereços privados/IPv6, HTTP autenticado, rejeição de
Origin externa, Range parcial/inválido, leitura entre projetos e liberação de
porta ao parar o serviço.

Os testes de navegador exercitam logo e layout nos tamanhos de monitor e dock,
edição e preparação sem publicar, colocação/retirada independentes, arraste e
redimensionamento, limpeza com confirmação, troca de projeto, escape de nomes,
transparência e preservação do nó de vídeo ao atualizar outra arte. A sequência
rápida retirar/recolocar cancela a saída pendente.

O teste integrado usa o executável **nativo**, sem backend substituto. Confere
401 sem token, bloqueio de Origin externa, confirmação obrigatória, estado
separado por projeto, preview privado, atualização SSE real, cache HTTP de mídia
e persistência dos ajustes. O arquivo usado no teste de download é um ícone
público do OBS; não se enviou a logo ou a biblioteca do usuário à internet.

## Evidências

- `artifacts/native-panel.png`: interface falando com o serviço Qt compilado.
- `artifacts/native-overlay.png`: overlay desse teste capturado com canal alpha.
- `artifacts/native-e2e-results.json`: resultado e verificações do teste integrado.
- `artifacts/obs-smoke-results.json`: resultado do ensaio da DLL final no OBS.
- `artifacts/panel-monitor.png` e `artifacts/panel-dock.png`: layouts de navegador.
- `test-results/browser-results.json`: relatório Playwright da última execução.
- `dist/SHA256SUMS.json`: checksums dos pacotes de entrega.

O servidor estático em `tests/browser-server.mjs` existe apenas para os testes
de UI com fixtures. Não faz parte do produto. Os testes nativos e o integrado
verificam separadamente o backend C++ real.

## Ensaio no OBS real

Uma cópia separada da instalação OBS foi criada em `.deps/obs-test` e executada
em modo portátil. O carregamento da DLL, registro do dock QCef e início do
servidor foram confirmados no log com:

```text
[JP Fachina Graphics] Loaded. Local service bound to 127.0.0.1; library ready.
```

Os perfis, cenas e arquivos da instalação principal do usuário não foram
alterados. Os ensaios ocorreram somente nessa cópia portátil e nos diretórios
de teste. Não foi iniciada transmissão ou gravação.

O ensaio final também confirmou HTTP 200 para `/branding.png`, com o SHA-256
original `49e0bd162507b25133d594a42729b2d711fcbc8fe34d475d163f17e6b8c309ff`,
e encerramento normal da janela principal, sem servidor, trava de biblioteca ou
processo residual do teste. Um procedimento inicial que enviava fechamento para
todas as janelas internas provocava crash também no OBS sem o plugin; esse
procedimento foi corrigido e os ensaios de referência e com plugin passaram.

## Limites da validação

- Não houve ensaio prolongado de CPU/RAM, transmissão real, reconexão de segundo
  monitor ou bateria ampla de arquivos/codec. O compositor preserva o DOM de
  vídeos, mas continuidade de playback e desempenho no CEF devem ser conferidos
  com as mídias concretas da operação.
- Não foi feita homologação em outras versões do OBS, Qt ou Windows, nem Windows
  ARM64. A versão exige Browser Panel API 3 e é direcionada ao OBS 32.1.0 x64.
- Os diálogos nativos de seleção de arquivos e confirmação de associação à cena
  foram implementados e compilados; sua interação manual completa precisa de
  conferência pelo operador antes da primeira live.
- Downloads só aceitam URL pública final e direta. Redirects, redes privadas,
  proxies e portas personalizadas são recusados. Isso limita alguns CDNs.
- O instalador foi executado com destino isolado. A permissão da pasta ProgramData
  pode exigir execução como administrador na instalação definitiva.
- A versão não é assinada digitalmente. O relatório não representa certificação
  de produção nem promessa de compatibilidade com qualquer arquivo MP4/WebM.

Nenhuma imagem de referência do primeiro print estava disponível no repositório
inicial, que estava vazio. O painel escuro foi criado com base na descrição; não
se alegou reprodução de um print que não foi fornecido.
