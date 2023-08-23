@echo off

pushd %~dp0%\art

if not exist "art.blend" (
    powershell Expand-Archive -Path art.zip -DestinationPath . || goto :error
)

set "blender="
if exist "%ProgramFiles%\Blender Foundation\Blender 3.5\blender.exe" (
  echo Using Blender 3.5 detected
  set "blender=%ProgramFiles%\Blender Foundation\Blender 3.5\blender.exe"
)
if exist "%ProgramFiles%\Blender Foundation\Blender 3.6\blender.exe" (
  echo Using Blender 3.6 detected
  set "blender=%ProgramFiles%\Blender Foundation\Blender 3.6\blender.exe"
)

if "%blender%" neq "" (
    call "%blender%" --background art.blend --python Exporter.py || goto :error
) else (
    goto :error
)

goto :success

:error
echo Blender export failed

:success
set "returncode=%ERRORLEVEL%"
popd
exit /B %returncode%
