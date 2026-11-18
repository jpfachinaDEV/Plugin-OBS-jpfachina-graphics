# JP Fachina Graphics 0.1.0

Plugin nativo para OBS Studio **32.1.0 x64 / Qt 6.8.3**, com painel acoplável,
biblioteca local e overlay transparente 1920 × 1080. A logo SYNC original está
incorporada à DLL e mantém os bytes e a proporção do documento fornecido.

## Instalação

1. Feche o OBS.
2. Execute `JP-Fachina-Graphics-0.1.0-Windows-x64-Setup.exe`. Se a pasta exigir
   permissão, clique com o botão direito e escolha **Executar como administrador**.
3. Mantenha o destino `C:\ProgramData\obs-studio\plugins\jp-fachina-graphics`.
4. Abra o OBS e ative **Painéis → JP Fachina Graphics**.

Alternativa ZIP: extraia a pasta `jp-fachina-graphics` para
`C:\ProgramData\obs-studio\plugins`. Não copie somente a DLL: a subpasta `data`
contém o backend HTTPS necessário para downloads.

OBS portátil 32.1: copie `bin/64bit/jp-fachina-graphics.dll` do pacote para
`<OBS portátil>/obs-plugins/64bit/` e o conteúdo da pasta `data` do pacote para
`<OBS portátil>/data/obs-plugins/jp-fachina-graphics/`. O modo portátil dessa
versão usa o caminho antigo de módulos.

## Operação

Crie ou abra uma transmissão. Use **Do computador** para importar mídia ou
**Adicionar URL** para baixar um arquivo público direto. A cópia local continua
funcionando sem internet depois do download. PNG, JPEG, WebP, GIF, WebM e MP4 são
aceitos; codecs precisam ser reproduzíveis pelo navegador do OBS. MP4 comum não
tem transparência. Os vídeos são reproduzidos sem áudio para operação gráfica.

Clique em **Pré-visualizar**, ajuste a posição arrastando ou pelos campos X/Y e
redimensione pelo canto inferior direito. Segure Shift para preservar a proporção
ao redimensionar. Salve a preparação. **Colocar no ar** envia os ajustes dessa
arte; **Retirar do ar** controla sua saída. Várias artes podem ficar no ar juntas.
Editar a preparação de uma arte no ar não altera sua posição publicada até
clicar novamente em **Colocar no ar**. Vídeos de outras artes continuam tocando.

**Associar ao OBS** pede confirmação para adicionar ou reutilizar uma fonte na
cena indicada. Você também pode copiar a URL para uma fonte de navegador com
largura 1920, altura 1080 e CSS de fundo transparente. Não habilite desligamento
da fonte quando invisível se quiser manter vídeos em reprodução ao mudar cenas.
Cada transmissão tem sua URL. Use a mesma fonte como existente em outras cenas.

O fundo preto pertence apenas ao preview. A saída do compositor é transparente.
Uma imagem que já tem fundo opaco continua com esse fundo original.
Arraste a aba do painel para desacoplar e movê-lo ao segundo monitor; o OBS salva
a organização dos docks conforme seus próprios recursos.

**Limpar transmissão** exige confirmação e retira apenas as artes do projeto
ativo. As categorias podem ser renomeadas por linha. Ao remover linhas, revise a
organização das mídias. A seleção e o preview nunca colocam artes no ar sozinhos.

## Dados e recuperação

Biblioteca normal: `%APPDATA%\obs-studio\plugin_config\jp-fachina-graphics`.
No OBS portátil, fica em `config/obs-studio/plugin_config/jp-fachina-graphics`
sob a própria instalação portátil. O botão **Pasta** abre o diretório correto.

Com o OBS fechado, faça backup de toda essa pasta: `library.json`, `media` e
`thumbs`. O estado no ar e as URLs são restaurados ao reiniciar. Não apague nem
compartilhe as chaves das URLs: elas dão acesso de leitura às mídias do projeto.
Os comandos de operação usam um token separado e válido só na sessão do plugin.

A porta local padrão é 28741. Se estiver ocupada, o plugin informa o erro, sem
trocar silenciosamente as URLs. Feche a outra instância ou, com o OBS fechado,
altere `port` em `library.json` e atualize a URL da fonte com o fluxo de associação.

As remoções de mídia/projeto retiram os metadados e a exibição. Os arquivos
binários ficam preservados para recuperação, portanto ocupam disco até limpeza
manual após backup. Limite: 256 MB por arquivo, 1.000 mídias por projeto,
200 transmissões, 50 categorias e 32 MB de metadados.

## Remoção e suporte

Feche o OBS e execute `Desinstalar.exe` dentro da pasta instalada do plugin.
A biblioteca do usuário é preservada. Em instalação ZIP, remova apenas os
arquivos deste plugin. Erros aparecem no painel e no log normal do OBS,
identificados por `[JP Fachina Graphics]`.

Compilação, arquitetura e resultados de validação estão em `docs/` no projeto
fonte. A versão é inicial; consulte o relatório de validação antes de usar em
uma transmissão de produção.
