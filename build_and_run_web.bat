@echo off

call build_web_debug.bat %* || goto :EOF
START "" "http://localhost:8000"
pushd %~dp0%\build_web
python -m http.server
popd
