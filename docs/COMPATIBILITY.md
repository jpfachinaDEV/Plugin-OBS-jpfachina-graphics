# Compatibilidade e validação

O pacote 0.4.1 desta pasta é destinado a Windows x64, OBS Studio 33.0.1 e Qt 6.11.1.
As versões constam também em `compatibility.json` dentro do pacote.

Qt WebSockets usa APIs privadas do Qt. Uma DLL 6.8.3 com o mesmo nome não é
compatível com o Qt 6.11.1 desse OBS. Use o conjunto correspondente ao runtime;
não substitua as DLLs Core/Gui/Widgets/Network fornecidas pelo OBS.

O pacote inclui a DLL do plugin, Qt6WebSockets.dll, backend TLS Schannel e licenças.
O empacotador confere versões e imports contra o runtime informado. O instalador
confere o Qt do OBS na pasta padrão; instalações personalizadas também precisam
usar a versão declarada.

## O que foi verificado na versão 0.4.1

| Verificação | Resultado |
| --- | --- |
| Compilação MSVC x64 / Qt 6.11.1 | Aprovada |
| Cinco suítes CTest | 623 verificações aprovadas |
| Renderizador libobs/D3D11 | 69 verificações aprovadas |
| Integração OBS WebSocket real | 44 verificações aprovadas, incluindo gravação local |
| Símbolos OBS/Qt exigidos pelo plugin e WebSockets | Resolvidos no runtime 33.0.1 |
| Carregamento na instalação OBS 33.0.1 | Confirmado no processo e no log de inicialização |

O ensaio portátil de interface percorreu edição, Take/Out e vídeos. A automação
Win32 de F11 variou conforme o foco, portanto o smoke completo não foi classificado
como aprovado. O atalho recebeu cobertura no teste Qt.

Não há homologação de transmissão externa, quatro horas contínuas, outro computador
físico, segundo monitor, DPI misto ou WSS com certificado de produção. A gravação
local e os ensaios curtos não substituem esses cenários.

A galeria usa dados demonstrativos e uma conexão real com OBS isolado. Ela não
comprova o funcionamento de codecs e equipamentos que não fazem parte dos testes.
