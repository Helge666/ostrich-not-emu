; SPDX-License-Identifier: GPL-3.0-or-later
;
; Ostrich 55 -- the Windows installer (NSIS 3, built on Linux by installer/build.sh).
;
; Installs, each one a component the user may leave out:
;   VST3        C:\Program Files\Common Files\VST3\Ostrich55.vst3
;   CLAP        C:\Program Files\Common Files\CLAP\Ostrich55.clap
;   Standalone  C:\Program Files\Ostrich 55\Ostrich55.exe, with a Start menu entry
; and makes C:\ProgramData\Ostrich 55\roms, writable for all users: the plugin copies the
; ROM images the user picks there (README.md, "Installing").  No ROM image is part of
; this installer; the uninstaller leaves that folder alone, it holds the user's files.
;
;   makensis -DVERSION=0.1.0 -DSRC=<folder with Ostrich55.vst3, .clap, .exe> -DOUT=<setup.exe> ostrich55.nsi

Unicode true
ManifestDPIAware true
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef SRC
  !define SRC "../plugin/ostrich/bin-win"
!endif
!ifndef OUT
  !define OUT "Ostrich55-Setup-${VERSION}.exe"
!endif

!define PRODUCT "Ostrich 55"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Ostrich55"

Name "${PRODUCT} ${VERSION}"
OutFile "${OUT}"
InstallDir "$PROGRAMFILES64\${PRODUCT}"
BrandingText "${PRODUCT} ${VERSION}"

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_WELCOMEPAGE_TITLE "${PRODUCT} ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "An emulation of the Roland SC-55 Sound Canvas as a VST3 and CLAP plugin and a standalone program, running the unit's original firmware on Nuked-SC55.$\r$\n$\r$\nYou need your own ROM images of an SC-55; this installer contains none. The first time Ostrich 55 runs, it asks for them.$\r$\n$\r$\nA non-commercial fan project, not affiliated with Roland."
!define MUI_FINISHPAGE_TEXT "Ostrich 55 is installed.$\r$\n$\r$\nLoad it in your DAW (or start it from the Start menu). The first time, click CHOOSE FILE in its panel and pick one of your SC-55 ROM images: the others in the same folder are found by their contents and copied to C:\ProgramData\Ostrich 55\roms."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SRC}/LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Ostrich 55 needs 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "VST3 plugin" SecVst3
  SetOutPath "$COMMONFILES64\VST3"
  File /r "${SRC}/Ostrich55.vst3"
SectionEnd

Section "CLAP plugin" SecClap
  SetOutPath "$COMMONFILES64\CLAP"
  File "${SRC}/Ostrich55.clap"
SectionEnd

Section "Standalone program" SecStandalone
  SetOutPath "$INSTDIR"
  File "${SRC}/Ostrich55.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT}.lnk" "$INSTDIR\Ostrich55.exe"
SectionEnd

Section "-common"
  SetOutPath "$INSTDIR"
  File "${SRC}/LICENSE.txt"
  File "${SRC}/README.txt"
  File "${SRC}/THIRD-PARTY.txt"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; The ROM folder, writable for the users group (S-1-5-32-545, whatever its name in the
  ; system's language): the plugin runs as the user and copies the images there.
  CreateDirectory "$COMMONAPPDATA\${PRODUCT}\roms"
  nsExec::ExecToLog 'icacls "$COMMONAPPDATA\${PRODUCT}" /grant *S-1-5-32-545:(OI)(CI)M /T'
  Pop $0

  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "Ostrich 55 project"
  WriteRegStr HKLM "${UNINST_KEY}" "URLInfoAbout" "https://github.com/Helge666/ostrich55"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
SectionEnd

LangString DESC_Vst3 ${LANG_ENGLISH} "The VST3 plugin, for Cubase and most other hosts."
LangString DESC_Vst3 ${LANG_GERMAN} "Das VST3-Plugin, für Cubase und die meisten anderen Hosts."
LangString DESC_Clap ${LANG_ENGLISH} "The CLAP plugin, for Bitwig, REAPER and other CLAP hosts."
LangString DESC_Clap ${LANG_GERMAN} "Das CLAP-Plugin, für Bitwig, REAPER und andere CLAP-Hosts."
LangString DESC_Standalone ${LANG_ENGLISH} "Ostrich 55 as a program of its own, with a Start menu entry."
LangString DESC_Standalone ${LANG_GERMAN} "Ostrich 55 als eigenes Programm, mit Eintrag im Startmenü."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVst3} $(DESC_Vst3)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecClap} $(DESC_Clap)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStandalone} $(DESC_Standalone)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\Ostrich55.vst3"
  Delete "$COMMONFILES64\CLAP\Ostrich55.clap"
  Delete "$SMPROGRAMS\${PRODUCT}.lnk"
  Delete "$INSTDIR\Ostrich55.exe"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\THIRD-PARTY.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINST_KEY}"
  ; $COMMONAPPDATA\Ostrich 55\roms stays: the user's ROM images.
SectionEnd
