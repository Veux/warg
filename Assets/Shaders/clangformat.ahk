#NoEnv  ; Recommended for performance and compatibility with future AutoHotkey releases.
; #Warn  ; Enable warnings to assist with detecting common errors.
SendMode Input  ; Recommended for new scripts due to its superior speed and reliability.
SetWorkingDir %A_ScriptDir%  ; Ensures a consistent starting directory.

#SingleInstance Force
#UseHook


~^r::
    ih := InputHook()
    ih.KeyOpt("{All}", "E")  ; End and Suppress
    ; Exclude the modifiers
    ih.KeyOpt("{LCtrl}{RCtrl}{LAlt}{RAlt}{LShift}{RShift}{LWin}{RWin}", "-E")
    ih.Start()
    ErrorLevel := ih.Wait()  ; Store EndReason in ErrorLevel
	result := ih.EndMods . ih.EndKey
    RegisteredKeyComb := { ">^f": "clangformat chord"}
	if ( RegisteredKeyComb.HasKey( result ) )
        {
        sleep 150
        Send ^+s
        sleep 150
		Run, clang-format -i -style=file *.vert *.frag, ,Min 
	}
    ;else
		;MsgBox %  "Other key Comb than registered was pressed.`n`tResult = [" . result . "]`n`tErrorlevel = [" . ErrorLevel . "]"
return