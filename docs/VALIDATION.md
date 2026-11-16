# Verificação desta pasta

A cópia preparada para publicação foi compilada separadamente, a partir dos seus
próprios fontes. As cinco suítes CTest passaram, totalizando 623 verificações.
O teste de integridade da marca e os sete testes Playwright também passaram.

As seis imagens foram geradas usando a interface Qt de produção e uma conexão
autenticada com OBS isolado. O instalador público foi executado em uma pasta de
teste; as DLLs e os hashes SHA-256 foram conferidos. Os links locais dos documentos
foram verificados, e os dados pessoais/de desenvolvimento ficaram fora da pasta.

`VALIDATION.json` resume os resultados desta preparação. Os limites de homologação
da versão permanecem em [COMPATIBILITY.md](COMPATIBILITY.md).
