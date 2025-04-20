# GraphicsTransforms

This project is a demo of basic rendering with OpenGL 3.3.

On startup, you'll see a white quad. To quit, press **Esc**. Enable view and
prjection matrices of your camera using buttons **c** and **b**.

From now on, you are able to navigate in a 3D space by using WASD keys and the
mouse. Mouse wheel is used for zoom.

There are 2 cameras on the scene, press **q** to switch between them. The
second camera is useful to see the first camera's frustum. While using the
second camera, press **o** and **p** to enable rendering camera's pyramid and
frustum. Press **o** and **p** again to disable them (when coming back to the
first camera). **m** and **,** do the same for the second camera. Note that
a camera pyramid may look unexpected if the respective projection is disabled.

Note that even if the second camera is active, zoom via mouse wheel still
applies to the first camera. That's done to make it easier to see frustum
change during zoom updates. Press **e** if you want to switch camera that
receives zoom updates.

You can change projection from perspective to orthographic and back by pressing
**z** for first camera and **x** for second camera.

Resetting cameras to their initial position is **1** and **2** (note that the
projection type is not reset).

Buttons **r**, **t**, **y**, **u** enables/disables rendering, scaling,
rotation, and translation of the quad you see. Play with them to see how the
final model matrix of the quad is constructed from the
scaling/rotation/translation parts.

Buttons **f**, **g**, **h**, **j** works for the copy of the quad, but it
applies another translation.

Enable 2 quads and their scaling/rotation/translation and reset the first
camera (**1**) and switch between perspective and orthographic projection
(**z**). See how perspective and orthographic projections transform parallel
lines.

Button **i** enables rendering of a simple animation of 2 quads, one of each is
rotating around the origin, and another is rotating on the first's quad edge.

Button **k** enables rendering of 3 quads that represent one of the corners of
the Rubik's cube. Press **l** to simulate rotation of the Rubik's cube top
side.

## Getting the project

1. *Via browser download.* On the project's GitHub page, press
   *Code*->*Download ZIP*. Extract files from the ZIP archive wherever you
   prefer.
2. *Via git*. Git is required.
    1. On MS Windows, install Git Bash from https://git-scm.com/downloads. On
       other platforms, install git with your package manager.
    2. Open Git Bash.
    3. Type `cd <path>` and press *Enter*, where `<path>` is a directory where
       you want to clone the project.
        * For example, `cd ~/repos` if there is the `repos` directory in your
          home directory.
    4. Type
       ```
       git clone https://github.com/KostiantynTokar/GraphicsTransforms.git
       ```
       and press *Enter*.

## Build

### Prerequisites

Python 3 is required for glad to build the project.
* On MS Windows, install python from https://www.python.org/downloads. Ensure
  that it is added to PATH system variable.
* On other platforms, use your package manager.

The first build requires an internet connection to download all dependencies
and allow glad to fetch OpenGL API from Khronos database and generate C++
headers.

### Visual Studio (MS Windows)

1. Make sure that the *Desktop development with C++* workload is installed.
    1. Open *Visual Studio Installer* (you can find it from the Start menu
       (windows logo) by typing "Visual Studio Installer").
    2. Select *Modify* for your selected version of Visual Studio.
    3. In the *Workloads* tab select *Desktop development with C++* workload
       (mark it with a tick).
    4. Click *Modify*.
2. Update Visual Studio if necessary.
    1. Open *Visual Studio Installer* (you can find it from the Start menu
       (windows logo) by typing "Visual Studio Installer").
    2. If active, select *Update* for your selected version of Visual Studio.
3. Run Visual Studio.
4. Select *Open a Local Folder*. Choose the folder where the project is
   located.
5. Wait until Cmake generation finished.
6. In the *Solution Explorer* window (open it with *Ctrl+Alt+L* or
   *View*->*Solution Explorer*) click on *CMakeLists.txt* with the right mouse
   button and select *Set as Startup Item*.
7. Build and run (press *F5* or *Debug*->*Start Debugging* or press the button
   with the triangle labeled *CMakeLists.txt*).

### Command Line (MS Windows, Linux, MacOS)

1. A C++ compiler and CMake is required.
    * On MS Windows, install Visual Studio with the *Desktop development with C++*
      workload (see [Visual Studio](#visual-studio-ms-windows)).
    * On other platforms, use your package manager to get a compiler and follow
      instructions to install CMake or build it from source.
2. Open your shell.
    * On MS Windows, open *Developer PowerShell for VS <year>*. (you can find
      it from the Start menu (windows logo) by typing "Developer PowerShell for
      VS")
    * On other platforms, open your favorite shell.
3. Type `cd <path>` and press *Enter*, where `<path>` is the path to the project.
    * For example, `cd ~/repos/GraphicsTransforms`.
4. **Configuration.** Type
   ```
   cmake -B build
   ```
   and press *Enter*. This command generates build files in the `build`
   directory.
5. **Build.** Type
   ```
   cmake --build build
   ```
   and press *Enter*. This command uses build files in the `build` directory to
   create the executable. You can find the executable in some of the
   subdirectories of `build` (it depends on the build system).
6. Run the executable by typing its path in the shell, or by opening it in the
   file browser.

Note that the configuration (step 4) is required only once. After that, to
rebuild the project it's enough repeat the build step (`cmake --build build`).
