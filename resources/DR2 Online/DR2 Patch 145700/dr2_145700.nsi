Name "Patching to v1.457.0.0"

Section "Dark Reign II 1.457.0.0"
    SetOutPath $EXEDIR\..\

    Delete $EXEDIR\..\dr2*.exe
    Delete $EXEDIR\..\mss*.dll
    Delete $EXEDIR\..\audiere.dll
    RMDir /r "$EXEDIR\..\Sounds"

    File /r library
    File /r music
    File "dr2.exe"
    File "mss32.dll"
SectionEnd

Function .onInstSuccess
    Exec "dr2.exe"
FunctionEnd
