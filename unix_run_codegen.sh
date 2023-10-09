set -e

if [ -d gen ]; then
  echo "Codegen dir already exists"
else
    mkdir gen
fi

echo Shader Gen


unameOut="$(uname -s)"
# https://stackoverflow.com/questions/3466166/how-to-check-if-running-in-cygwin-mac-or-linux

case "${unameOut}" in
    Darwin*)   
    if [[ $(uname -m) == 'arm64' ]]; then
        SHADER_EXECUTABLE="thirdparty/sokol-shdc-mac-arm64"
    else
        echo "Haven't downloaded the x64 macos shader binary yet, sorry"
    fi;;

    Linux*)     echo "Linux not supported yet.";;
    *)          echo "No idea what this machine is dude, sorry";;
esac

$SHADER_EXECUTABLE --input threedee.glsl --output gen/threedee.glsl.h --slang glsl300es:hlsl5:glsl330