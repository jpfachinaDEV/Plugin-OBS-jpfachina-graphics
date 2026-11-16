# Publicar no GitHub

Esta pasta contém o repositório pronto. Publique **o conteúdo da pasta**, deixando
README.md na raiz do repositório. Não envie o ZIP do repositório como substituto
dos arquivos se quiser que o GitHub mostre código, imagens e instruções.

## Com Git

Crie um repositório vazio na sua conta. Como README e licença já estão incluídos,
não marque a criação automática desses arquivos. Na pasta deste projeto:

```powershell
git init
git add .
git commit -m "Publica JPFachina OBS Controller 0.4.1"
git branch -M main
git remote add origin https://github.com/SEU-USUARIO/jp-fachina-graphics.git
git push -u origin main
```

Substitua SEU-USUARIO e o nome do repositório pelos seus. A autenticação é feita
pelo próprio Git/GitHub; não coloque tokens nesses arquivos ou na URL do README.
O `.gitignore` preserva os downloads e imagens e exclui builds, dados locais e credenciais.

## Pelo navegador

No repositório, use Add file → Upload files. O envio pelo navegador aceita até
100 arquivos por vez e 25 MiB por arquivo; envie em lotes quando necessário.
Inclua os arquivos `.gitignore` e `.gitattributes`, que podem ficar ocultos no
Explorador. Veja as [instruções oficiais](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository).

## Criar a versão para download

Os links relativos do README já apontam para `downloads/0.4.1`. Para também
oferecer a versão na área Releases:

1. Abra Releases → Draft a new release.
2. Crie a tag `v0.4.1` e use o título `JPFachina OBS Controller 0.4.1`.
3. Copie as notas de `downloads/0.4.1/RELEASE-NOTES.md`.
4. Anexe o instalador EXE, o pacote ZIP e `SHA256SUMS.txt` dessa pasta.
5. Revise os arquivos e publique a release na sua conta.

O GitHub permite anexar binários às releases; consulte o
[guia oficial](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository).
As imagens em `docs/images` podem ser usadas no README, na descrição da release
e na divulgação. O código inclui apenas exemplos, sem biblioteca pessoal.

Esta preparação não cria repositório remoto nem publica arquivos na sua conta.
