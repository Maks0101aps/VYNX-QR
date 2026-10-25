; Extract a private Qt runtime, open the shared graphical wizard, then clean up.
; /S still directly invokes the same per-user NSIS backend for automation.
Unicode true
!include "FileFunc.nsh"
Name "VYNX QR Setup"
OutFile "${VYNX_OUTPUT}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\VYNX QR"
InstallDirRegKey HKCU "Software\VYNX QR" "InstallDir"
SilentInstall silent
SetCompressor /SOLID lzma
Icon "${VYNX_ICON}"
VIProductVersion "${VYNX_VERSION}.0"
VIAddVersionKey "ProductName" "VYNX QR Setup"
VIAddVersionKey "FileDescription" "VYNX QR Setup"
VIAddVersionKey "FileVersion" "${VYNX_VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright (c) 2026 Maks0101aps"
Var Parameters
Var Result
Section
  InitPluginsDir
  SetOutPath "$PLUGINSDIR"
  File /oname=install-backend.exe "${VYNX_BACKEND}"
  ${GetParameters} $Parameters
  ClearErrors
  ${GetOptions} $Parameters "/S" $0
  IfErrors graphical
    ; NSIS consumes /D itself; it is not included in GetParameters.
    ExecWait '"$PLUGINSDIR\install-backend.exe" /S /D=$INSTDIR' $Result
    Goto done
  graphical:
    File "${VYNX_PAYLOAD_DIR}\VYNX QR.exe"
    File "${VYNX_PAYLOAD_DIR}\Qt6Core.dll"
    File "${VYNX_PAYLOAD_DIR}\Qt6Gui.dll"
    File "${VYNX_PAYLOAD_DIR}\Qt6Widgets.dll"
    SetOutPath "$PLUGINSDIR\platforms"
    File "${VYNX_PAYLOAD_DIR}\platforms\qwindows.dll"
    SetOutPath "$PLUGINSDIR\styles"
    File /nonfatal "${VYNX_PAYLOAD_DIR}\styles\*.dll"
    SetOutPath "$PLUGINSDIR"
    ExecWait '"$PLUGINSDIR\VYNX QR.exe" --setup --backend "$PLUGINSDIR\install-backend.exe"' $Result
    IfErrors 0 done
      MessageBox MB_OK|MB_ICONSTOP "VYNX QR Setup could not start. Try downloading the installer again."
      StrCpy $Result 1
  done:
    SetErrorLevel $Result
SectionEnd
