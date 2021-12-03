# Luz Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Windows](https://github.com/hadryansalles/Luz/actions/workflows/Windows.yml/badge.svg)](https://github.com/hadryansalles/Luz/actions/workflows/Windows.yml)
[![Ubuntu](https://github.com/hadryansalles/Luz/actions/workflows/Ubuntu.yml/badge.svg)](https://github.com/hadryansalles/Luz/actions/workflows/Ubuntu.yml)

A Vulkan engine that I'm developing to study and implement modern rendering techniques used by AAA games.
- [Videos on Youtube](https://www.youtube.com/user/HadryanSalles/videos)
- [Features](#features)
- [How to build and run](#build)
- [References and credits](#references)

<a name="features"/>

## Features
- PBR Shading with metallic, roughness, normal, ambient occlusion and emission
- Vulkan bindless resources
- Scene hierarchy
- Asynchronous glTF and OBJ model loading
- Viewport camera with Perspective and Orthographic projections and Fly and Orbit controls
- Widgets for adjusting Vulkan settings at runtime

## Gallery

- Ray traced shadows and ambient occlusion
![raytraced](https://user-images.githubusercontent.com/37905502/144621461-52f1ab97-ff6b-4f6f-a83a-cc6f67f5ead6.gif)

- PBR Shading and glTF models
![pbr](https://user-images.githubusercontent.com/37905502/144612584-1d752a16-c978-4f43-93d6-2e2362b2804b.gif)

- Textures drag and drop
![dragndrop](https://user-images.githubusercontent.com/37905502/144619247-737d37c1-ba67-4f9a-abf4-63e4d2f965d6.gif)

- Asynchronous model loading
![async](https://user-images.githubusercontent.com/37905502/144617077-167d6a0b-4455-4aa3-9a60-2c2b5c49dd14.gif)

<a name="build"/>

## Requirements
- C++17 compiler. Tested with ``Visual Studio 2019``, ``Clang`` and ``GCC``
- [CMake 3.7](https://cmake.org/download/) or higher
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)

### For Linux

This projects uses GLFW library, to compile it under Linux with X11 (like the default Ubuntu 20.04) you will need:

```sh
sudo apt-get install xorg-dev
```

If you are using another window manager (like Wayland) you can check the dependencies [here](https://www.glfw.org/docs/latest/compile.html#compile_deps).

## Build and Run
```sh
git clone --recursive https://github.com/hadryansalles/Luz
cd Luz
mkdir build
cd build
cmake ..
```

- GCC or Clang: 
```sh
make run -j
```

- Visual Studio: open ``build/Luz.sln`` and compile/run the project ``Luz``.

<a name="references"/>

## References and Credits

- [GLFW](https://github.com/glfw/glfw) used to open the application window
- [glm](https://github.com/g-truc/glm) used as the math library
- [ImGui](https://github.com/ocornut/imgui) used to make the user interface
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) used to create 3D gizmos
- [spdlog](https://github.com/gabime/spdlog) used as the logging library
- [stb_image](https://github.com/nothings/stb) used to load image files
- [tiny_obj_loader](https://github.com/tinyobjloader/tinyobjloader) used to load wavefront .obj files
- [optick](https://github.com/bombomby/optick) used to profile the engine
