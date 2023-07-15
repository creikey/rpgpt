#NoEnv
#SingleInstance, Force
SendMode, Input
SetBatchLines, -1
SetWorkingDir, %A_ScriptDir%

; Fuck windows for having this hardcoded
^Esc::return 

^5::
IfWinNotExist, rpgptbuild
{
  run, wt.exe cmd /K "title rpgptbuild && cd C:\Users\Cameron\Documents\rpgpt && vcvars"
}
WinActivate, rpgptbuild
If WinActive("rpgptbuild")
{
    Send, {Enter}
    Send, run_remedy build_desktop_debug.bat codegen
    Send, {Enter}
}
Send, {Blind} ; So it doesn't hold down ctrl after running! WTF
return