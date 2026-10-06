; VYNX QR installer.
;
; Written by hand rather than generated, because the two things that matter here
; are both things CPack's NSIS generator gets wrong for this project: the Start
; Menu shortcut has to point at "VYNX QR.exe" and not at the CMake target name,
; and the install must be per-user so a small utility never asks for an
; administrator prompt.
;
; Verified on Windows 11: install, run from the installed location, Start Menu
; entry present, uninstall leaves nothing behind.

Unicode true
!include "MUI2.nsh"

!ifndef VYNX_VERSION
  !define VYNX_VERSION "1.0.1"
!endif
!ifndef VYNX_PAYLOAD_DIR
  !define VYNX_PAYLOAD_DIR "..\\bin"
!endif
!ifndef VYNX_ICON
  !define VYNX_ICON "..\\..\\app\\resources\\icons\\icon.ico"
!endif
!ifndef VYNX_LICENSE
  !define VYNX_LICENSE "..\\..\\LICENSE"
!endif

!define PRODUCT "VYNX QR"
!define PUBLISHER "VYNX"
!define AUTHOR "Maks0101aps"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\VYNX QR"

Name "${PRODUCT}"
OutFile "VYNX-QR-Setup-x64-${VYNX_VERSION}.exe"
InstallDir "$LOCALAPPDATA\Programs\VYNX QR"
InstallDirRegKey HKCU "Software\VYNX QR" "InstallDir"
RequestExecutionLevel user
ShowInstDetails show
ShowUninstDetails show
SetCompressor /SOLID lzma

VIProductVersion "${VYNX_VERSION}.0"
VIAddVersionKey "ProductName"     "${PRODUCT}"
VIAddVersionKey "CompanyName"     "${PUBLISHER}"
VIAddVersionKey "FileDescription" "Fast, private QR codes for Windows."
VIAddVersionKey "FileVersion"     "${VYNX_VERSION}"
VIAddVersionKey "ProductVersion"  "${VYNX_VERSION}"
VIAddVersionKey "LegalCopyright"  "Copyright (c) 2026 Maks0101aps"
VIAddVersionKey "LegalURL"        "https://github.com/Maks0101aps/VYNX-QR"

!define MUI_ICON "${VYNX_ICON}"
!define MUI_UNICON "${VYNX_ICON}"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\VYNX QR.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open VYNX QR"

!insertmacro MUI_PAGE_LICENSE "${VYNX_LICENSE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "VYNX QR" InstallSection
  SetOutPath "$INSTDIR"

  File "${VYNX_PAYLOAD_DIR}\LICENSE"
  File "${VYNX_PAYLOAD_DIR}\THIRD_PARTY_LICENSES.md"
  File /r "${VYNX_PAYLOAD_DIR}\licenses"

  ; The executable and the Qt runtime beside it. No service, no scheduled task,
  ; no autostart, nothing running after the window closes.
  File "${VYNX_PAYLOAD_DIR}\VYNX QR.exe"
  File /nonfatal /r "${VYNX_PAYLOAD_DIR}\Qt6*.dll"
  SetOutPath "$INSTDIR\platforms"
  File /nonfatal /r "${VYNX_PAYLOAD_DIR}\platforms\*.dll"
  SetOutPath "$INSTDIR\styles"
  File /nonfatal /r "${VYNX_PAYLOAD_DIR}\styles\*.dll"
  SetOutPath "$INSTDIR"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Start Menu, which is what makes the application findable from the Start search.
  CreateDirectory "$SMPROGRAMS\${PRODUCT}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT}\${PRODUCT}.lnk" "$INSTDIR\VYNX QR.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  ; Apps and Features, so the product can be removed the normal way.
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "DisplayName"     "${PRODUCT}"
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "DisplayVersion"  "${VYNX_VERSION}"
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "Publisher"       "${PUBLISHER}"
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "DisplayIcon"     "$INSTDIR\VYNX QR.exe"
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "URLInfoAbout"    "https://github.com/Maks0101aps/VYNX-QR"
  WriteRegStr   HKCU "${UNINSTALL_KEY}" "HelpLink"        "https://github.com/Maks0101aps/VYNX-QR"
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1

  WriteRegStr HKCU "Software\VYNX QR" "InstallDir" "$INSTDIR"
SectionEnd

Section "Uninstall"
  ; Only the application. Preferences under %APPDATA%\VYNX\QR are deliberately
  ; left alone: someone reinstalling expects their settings to survive.
  Delete "$INSTDIR\VYNX QR.exe"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  ; `Delete` takes a single file and no /r, so the runtime DLLs are named one by
  ; one. Anything windeployqt adds later is caught by removing the directory.
  Delete "$INSTDIR\Qt6Core.dll"
  Delete "$INSTDIR\Qt6Gui.dll"
  Delete "$INSTDIR\Qt6Widgets.dll"
  RMDir /r "$INSTDIR"

  Delete "$SMPROGRAMS\${PRODUCT}\${PRODUCT}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT}"

  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  DeleteRegKey HKCU "Software\VYNX QR"
SectionEnd
