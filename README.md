# rpgpt
RPG GPT. Short experience

# Important Building Steps
You must clone with git lfs is, and download git lfs files in this repository. If you don't know what that is, google it

Open `art.blend`, go to the scripting tab and hit the play button run the script and export all the 3d assets. Then, make sure that when you build, you also build and run the codegen so that said assets and other files are copied and imported. For debug builds on windows, that's `call build_desktop_debug.bat codegen`, the codegen argument to the build script causing it to run codegen
