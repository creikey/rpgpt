@echo off

START /B remedybg.exe stop-debugging
call build_desktop_debug.bat
remedybg.exe start-debugging
