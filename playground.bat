@echo off

cl /nologo /diagnostics:caret /DDEVTOOLS /Igen /Ithirdparty /Wall /FC /Zi /WX playground.c /link /noimplib /noexp || goto :error

goto :EOF

:error
echo Failed to build
exit /B %ERRORLEVEL%