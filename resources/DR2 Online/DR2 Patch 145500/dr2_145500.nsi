Name "Patching to v1.455.0.0"

Section "Dark Reign II 1.455.0.0"
    SetOutPath $EXEDIR\..\

    Delete $EXEDIR\..\winmm*.dll
    Delete $EXEDIR\..\dr2*.exe

    File /r library
    File "dr2.exe"
SectionEnd

Function .onInstSuccess
    Exec "dr2.exe"
FunctionEnd
