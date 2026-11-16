Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!ifndef VERSION
  !define VERSION "0.4.1"
!endif
!ifndef OBS_VERSION
  !define OBS_VERSION "33.0.1"
!endif
!ifndef QT_VERSION
  !define QT_VERSION "6.11.1"
!endif
!ifndef PACKAGE_DIR
  !error "Defina PACKAGE_DIR apontando para o pacote preparado."
!endif
!ifndef OUTPUT_FILE
  !define OUTPUT_FILE "JP-Fachina-Graphics-${VERSION}-Windows-x64-Setup.exe"
!endif
Name "JP Fachina Graphics"
OutFile "${OUTPUT_FILE}"
InstallDir ""
RequestExecutionLevel user
SetCompressor /SOLID lzma
VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1046 "ProductName" "JP Fachina Graphics"
VIAddVersionKey /LANG=1046 "FileDescription" "Instalador do plugin JP Fachina Graphics para OBS Studio"
VIAddVersionKey /LANG=1046 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1046 "LegalCopyright" "JP Fachina Graphics"
!define MUI_WELCOMEPAGE_TITLE "JP Fachina Graphics"
!define MUI_WELCOMEPAGE_TEXT "Plugin de gráficos para OBS Studio ${OBS_VERSION} x64 com Qt ${QT_VERSION}.$\r$\n$\r$\nFeche o OBS antes de instalar. O instalador adiciona apenas os arquivos do plugin e preserva a biblioteca do usuário.$\r$\n$\r$\nSe a pasta de destino exigir permissão, execute este instalador como administrador."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PACKAGE_DIR}\LICENSE"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Pasta deste plugin dentro de ProgramData\obs-studio\plugins. Para OBS portátil, use o pacote ZIP e as instruções de instalação portátil."
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "PortugueseBR"
Function .onInit
  SetShellVarContext all
  ${If} ${FileExists} "$PROGRAMFILES64\obs-studio\bin\64bit\Qt6Core.dll"
    ${GetFileVersion} "$PROGRAMFILES64\obs-studio\bin\64bit\Qt6Core.dll" $0
    ${If} $0 != "${QT_VERSION}.0"
      MessageBox MB_OK|MB_ICONSTOP "Este pacote foi compilado para OBS ${OBS_VERSION} / Qt ${QT_VERSION}. Seu OBS instalado usa Qt $0. Use um pacote correspondente para evitar erro de carregamento." /SD IDOK
      SetErrorLevel 2
      Abort
    ${EndIf}
  ${EndIf}
  ${If} $INSTDIR == ""
    StrCpy $INSTDIR "$APPDATA\obs-studio\plugins\jp-fachina-graphics"
  ${EndIf}
FunctionEnd
Section "JP Fachina Graphics"
  SetShellVarContext all
  SetOutPath "$INSTDIR\bin\64bit"
  ClearErrors
  File "${PACKAGE_DIR}\bin\64bit\jp-fachina-graphics.dll"
  File "${PACKAGE_DIR}\bin\64bit\Qt6WebSockets.dll"
  ${If} ${Errors}
    MessageBox MB_OK|MB_ICONSTOP "Não foi possível instalar. Feche o OBS e verifique a permissão da pasta. Se necessário, execute como administrador."
    Abort
  ${EndIf}
  SetOutPath "$INSTDIR\data\qt-plugins\tls"
  File "${PACKAGE_DIR}\data\qt-plugins\tls\qschannelbackend.dll"
  SetOutPath "$INSTDIR\data\licenses"
  File "${PACKAGE_DIR}\data\licenses\*"
  SetOutPath "$INSTDIR"
  File "${PACKAGE_DIR}\LICENSE"
  File "${PACKAGE_DIR}\LEIA-ME.md"
  File "${PACKAGE_DIR}\compatibility.json"
  SetOutPath "$INSTDIR\docs"
  File "${PACKAGE_DIR}\docs\BUILD.md"

  File "${PACKAGE_DIR}\docs\COMPATIBILITY.md"

  WriteUninstaller "$INSTDIR\Desinstalar.exe"
SectionEnd
Section "Uninstall"
  Delete "$INSTDIR\bin\64bit\jp-fachina-graphics.dll"
  Delete "$INSTDIR\bin\64bit\Qt6WebSockets.dll"
  Delete "$INSTDIR\data\qt-plugins\tls\qschannelbackend.dll"
  Delete "$INSTDIR\data\licenses\Qt-LGPL-3.0.txt"
  Delete "$INSTDIR\data\licenses\Qt-GPL-3.0.txt"
  Delete "$INSTDIR\data\licenses\Qt-NOTICE.md"
  Delete "$INSTDIR\data\licenses\obs-browser-COPYING"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\LEIA-ME.md"
  Delete "$INSTDIR\compatibility.json"
  Delete "$INSTDIR\docs\BUILD.md"

  Delete "$INSTDIR\docs\COMPATIBILITY.md"

  Delete "$INSTDIR\Desinstalar.exe"
  RMDir "$INSTDIR\data\qt-plugins\tls"
  RMDir "$INSTDIR\data\qt-plugins"
  RMDir "$INSTDIR\data\licenses"
  RMDir "$INSTDIR\data"
  RMDir "$INSTDIR\bin\64bit"
  RMDir "$INSTDIR\bin"
  RMDir "$INSTDIR\docs"
  RMDir "$INSTDIR"
SectionEnd
