Name "Patching to v1.456.0.0"

Section "Dark Reign II 1.456.0.0"
    SetOutPath $EXEDIR\..\

    Delete $EXEDIR\..\dr2*.exe

    File /r library
    File "dr2.exe"
SectionEnd

Function .onInstSuccess
    Exec "dr2.exe"
FunctionEnd
