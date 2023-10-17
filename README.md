# [Dante's Cowboy - Wishlist now on Steam!](https://store.steampowered.com/app/2501370/Dantes_Cowboy)
A fantasy western RPG with an immersive and natural dynamic dialogue system powered by GPT.

![Western Frontier](https://upload.wikimedia.org/wikipedia/commons/thumb/3/32/Distribution_of_US_Rural_Population_during_1900.pdf/page1-1280px-Distribution_of_US_Rural_Population_during_1900.pdf.jpg)

# Important Building Steps and Contribution Notes
If you add new devtools functionality like a new keyboard button binding that prints stuff to help with debugging, or flycam features, or anything, MAKE SURE you add a description of that functionality and how to access it in the function make_devtools_help() so it's just clear what debugging tools there are. Document it please.

In order for exporting to work, the "exclude from view layer" tick to the left of the eyeball on each object has to be unticked, or else all animations will be frozen. @TODO fix this in the future

Every time you checkin/clone the project, make sure to call `blender_export.bat` at least once! This will auto-extract `art\art.blend` and run `art\Exporter.py`, thereby baking, validating, and exporting all the assets and the level.

When editing Exporter.py in either the blender editor, or in a text editor in the repo, you  have to continually make sure blender's internal version of the script doesn't go out of date with the actual script on disk, by either saving consistently from blender to disk if you're editing from blender, or by reloading from disk in the blend file before each commit.

Be very cautious about committing a change to any large asset files, i.e the art.blend and png files. Every time you do so, even if you change one little thing like moving the player somewhere, you copy the entire file in git lfs, ballooning the storage usage of the git project on the remote. So just try to minimize edits to those big files.

You must clone with git lfs is, and download git lfs files in this repository. If you don't know what that is, google it

Open `art.blend`, go to the scripting tab and hit the play button run the script and export all the 3d assets. Then, make sure that when you build, you also build and run the codegen so that said assets and other files are copied and imported. For debug builds on windows, that's `call build_desktop_debug.bat codegen`, the codegen argument to the build script causing it to run codegen

To enable codegen error messages, change @echo off to @echo on in run_codegen.bat

You must be on a little endian machine.

# Debugging in the web
To get this working, you're going to need to follow flooh's answer linked [here](https://groups.google.com/g/emscripten-discuss/c/DEmpyGoq6kE/m/Bx44ZmfmAAAJ), and copy pasted for redundancy here:
```
It should definitely work on Vanilla Chrome, but setting everything up can be a bit finicky:

- install the Debugging extension: https://chrome.google.com/webstore/detail/cc%20%20-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb
- in the Dev Tools settings, search for 'WebAssembly Debugging' and check that box
- compile your code with '-O0 -g' (no optimization, and debug info enabled'
- IMPORTANT: in the Chrome debugger, there's a 'Filesystem' tab on the left side which is very easy to miss. Here you need to navigate to your project directory and allow Chrome to access that area of your filesystem.

(I think/hope these are all steps)

When you load your application you should see something like this on the Dev Tools console:

[C/C++ DevTools Support (DWARF)] Loading debug symbols for http://localhost:8080/cube-sapp.wasm...
[C/C++ DevTools Support (DWARF)] Loaded debug symbols for http://localhost:8080/cube-sapp.wasm, found 91 source file(s)

...and if everything works it should look roughly like this in the debugger:

```
