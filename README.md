# rpgpt
RPG GPT. Short experience

![Western Frontier](https://upload.wikimedia.org/wikipedia/commons/thumb/3/32/Distribution_of_US_Rural_Population_during_1900.pdf/page1-1280px-Distribution_of_US_Rural_Population_during_1900.pdf.jpg)

# Important Building Steps and Contribution Notes
Every time you checkin/clone the project, you have to unzip art.blend... If this is annoying to you, make a git hook

Be very cautious about committing a change to any large asset files, i.e the art.blend and png files. Every time you do so, even if you change one little thing like moving the player somewhere, you copy the entire file in git lfs, ballooning the storage usage of the git project on the remote. So just try to minimize edits to those big files.

You must clone with git lfs is, and download git lfs files in this repository. If you don't know what that is, google it

Open `art.blend`, go to the scripting tab and hit the play button run the script and export all the 3d assets. Then, make sure that when you build, you also build and run the codegen so that said assets and other files are copied and imported. For debug builds on windows, that's `call build_desktop_debug.bat codegen`, the codegen argument to the build script causing it to run codegen

To enable codegen error messages, change @echo off to @echo on in run_codegen.bat
