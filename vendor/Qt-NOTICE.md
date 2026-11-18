Os pacotes 0.4.1 para OBS 33.0.1 usam Qt 6.11.1. qschannelbackend.dll é o
backend TLS Schannel de QtNetwork; foi obtido do pacote oficial OBS:
https://github.com/obsproject/obs-deps/releases/tag/2026-10-23

Qt6WebSockets.dll foi compilada com MSVC x64 e o mesmo SDK Qt 6.11.1, sem
alterações nos fontes oficiais do módulo Qt WebSockets v6.11.1.
Os ajustes de caminhos ASCII no build afetam somente arquivos gerados.

Qt é copyright The Qt Company Ltd. e seus contribuidores. Os componentes
redistribuídos usam LGPL-3.0, com textos LGPL/GPL adjuntos. Fontes correspondentes:
https://github.com/qt/qtbase/tree/v6.11.1
https://github.com/qt/qtwebsockets/tree/v6.11.1
https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/

Os componentes são carregados dinamicamente e podem ser substituídos por uma
compilação compatível. Não são incluídas nem substituídas as DLLs centrais
Core/Gui/Widgets/Network do OBS. O código do aplicativo e as instruções de
compilação acompanham este projeto. compatibility.json informa o kit do pacote.

Pacotes históricos 0.4.0 usavam Qt 6.8.3 para OBS 32.1.0 e conservam sua licença
e aviso correspondentes nos arquivos de distribuição anteriores.
