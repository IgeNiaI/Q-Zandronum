; Installer script for Nullsoft's free NSIS installer
; available at: http://www.nullsoft.com/free/nsis/
;

; The name of the installer
Name "GetWAD"
OutFile "getwad-setup.exe"
CRCCheck on
AutoCloseWindow true
ShowInstDetails show

; The default installation directory
InstallDir "$PROGRAMFILES\INETDOOM\GetWAD"

; Registry key to check for directory (so if you install again, it will
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\GetWAD" "InstallDir"

; The text to prompt the user to enter a directory
DirText "Please select the directory where you want to install GetWAD."

; The stuff to install
Section "Main Section"
	SetOutPath "$INSTDIR"
	File "getwad.exe"
	File "getwad.dll"
	File "getwad.chm"

	CreateDirectory "$SMPROGRAMS\GetWAD"
	CreateShortCut  "$SMPROGRAMS\GetWAD\GetWAD Documentation.lnk" "$INSTDIR\getwad.chm"
	CreateShortCut  "$SMPROGRAMS\GetWAD\Uninstall GetWAD.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
	CreateShortCut  "$SMPROGRAMS\GetWAD\GetWAD Setup.lnk" "$INSTDIR\getwad.exe" "-setup"
	CreateShortCut  "$SMPROGRAMS\GetWAD\GetWAD.lnk" "$INSTDIR\getwad.exe"

	WriteRegStr HKLM "Software\GetWAD" "InstallDir" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GetWAD" "DisplayName" "GetWAD (remove only)"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GetWAD" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteUninstaller "uninstall.exe"

	ExecShell open '"$INSTDIR\getwad.chm"'
SectionEnd


; uninstall stuff

UninstallText "This will uninstall GetWAD. Hit next to continue."

; special uninstall section.
Section "Uninstall"

	; get rid of the registry keys
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GetWAD"
	DeleteRegKey HKLM "Software\GetWAD"

	; get rid of the installed files
	Delete "$INSTDIR\*.*"
	RMDir  "$INSTDIR"

	; Get rid of the shortcuts
	Delete "$SMPROGRAMS\GetWAD\*.*"
	RMDir  "$SMPROGRAMS\GetWAD"
SectionEnd

; eof
