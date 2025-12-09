# Nous Engine
### **A multithreaded, Vulkan-based game engine developed in C++**

![GitHub repo size](https://img.shields.io/github/repo-size/francesctr4/Nous-Engine)

![C++23](https://img.shields.io/badge/C%2B%2B-23%2B-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![SDL3](https://img.shields.io/badge/SDL3-8B4513?style=flat&logo=sdl&logoColor=white)
![Vulkan](https://img.shields.io/badge/Vulkan-AC162C?style=flat&logo=vulkan&logoColor=white)

![CLion](https://img.shields.io/badge/CLion-000000?style=flat&logo=clion&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-064F8C?style=flat&logo=cmake&logoColor=white)
![vcpkg](https://img.shields.io/badge/vcpkg-8f4b8d?style=flat&logo=github&logoColor=white)

---

![Showcase](https://github.com/user-attachments/assets/384d2461-457c-42ee-ba22-7a63221d16c2)

---

### **Enhancing Software Performance through Multithreading and Parallel Programming Techniques**

This repository contains source code developed as part of my **Bachelor's Thesis (TFG)**:

- **Bachelor's Thesis URL:** [https://hdl.handle.net/2117/439429](https://hdl.handle.net/2117/439429)

- **Video Showcase:** [https://youtu.be/A4NfI6ALey8](https://youtu.be/A4NfI6ALey8)

- **Nous Engine Repository:** [https://github.com/francesctr4/Nous-Engine](https://github.com/francesctr4/Nous-Engine)

- **Multithreading Library:** [https://github.com/francesctr4/NOUS_Multithreading.h](https://github.com/francesctr4/NOUS_Multithreading.h)

**Author:** Francesc Teruel Rodriguez ([francesctr4](https://github.com/francesctr4) on GitHub)  
**Created:** 11/07/2025  
**Published Version:** v0.3

**License:** CC-BY-NC-SA License <br>
Attribution-NonCommercial-ShareAlike 4.0 International

Copyright (©) 2025 Francesc Teruel Rodríguez

---

### **Contributing**
Pull requests are welcome! If you'd like to suggest improvements, add features, or report issues, feel free to open a GitHub issue or PR.

---

## 📚 **Table of Contents**

- [Home](#nous-engine)
- [Changelog](#changelog)
- [Dependencies](#dependencies)
- [Context](#context)
- [Features](#features)
- [Third Party Libraries](#third-party-libraries)
- [Third Party Assets](#third-party-assets)
- [Installation](#installation)
- [Controls](#controls)  
  - [Camera](#camera)
  - [Multithreading](#multithreading)
  - [Debug Keys](#debug-keys)
- [Known Bugs](#known-bugs)
- [Future Roadmap](#future-roadmap)

---

### **Changelog**

#### Version 0.5 - Scripting System and Major Improvements (Current)
The v0.5 release introduces major architectural improvements, new subsystems, and powerful tooling upgrades across the entire engine:

#### 🔥 Scripting System
- Dynamic scripting using DLLs with an exposed EngineAPI
- Hot Reload support for instant iteration
- Jobified script updating for performance
- Automatic C++ ↔ Script bindings

#### 📁 Project Structure & Build System
- Complete project file reorganization
- Modularized into multiple CMakeLists.txt using a clean recursive structure
- Engine split into DLL + LIB layers (EditorApp.exe / GameApp.exe)
- Clear separation between Editor, Game, and Engine Core
- Updated internal structure documented with Lucidchart diagrams

#### 🛠️ Editor & Tools
- Improved Console Window with new log channels
- Expanded Logger system with channel-based filtering
- Enhanced Hierarchy and Inspector windows
- Full Scene Serialization, Save/Load pipeline (now jobified)

#### 🧠 Memory System Improvements
- Upgraded Memory Manager and DynamicAllocator → now fully supports polymorphic types
- Memory Window to monitor allocations in real time
- Custom STL allocator integrated with NousVector
- MemoryTag refactor using magic_enum for reflective memory tagging

#### ⚡ Core Systems
- Major upgrade to the Event System
- ECS improvements and integration with inspector/hierarchy
- Scene loading/saving threaded for performance

#### 🧪 Testing & Documentation
- Added unit tests with GoogleTest
- Integrated ctest for automated test execution
- Doxygen documentation generation for the entire engine

- Scripting System (dll, engineAPI, hot reload, jobified, bindings)
- Project Files reestructuration
- Divided project into multiple CMakeLists.txt recursive structure
- DLL + LIBs internal engine reestructuration (EditorApp.exe / GameApp.exe), static and dynamic libraries (esquema lucidchart)
- Editor and Game separated from engine core
- Console Window + Logger improvements (log channels)
- Memory Window + Memory Manager improvements. Stl allocator for nous vector. see tagged allocations in real time
- Rework to MemoryManager and DynamicAllocator -> now works with polymorphic types!
- MemoryTag refactor using magic_enum library, now reflective!
- Event System major upgrade
- ECS
- Scene Serialization
- Scene Save/Loading (Jobified)
- hierarchy and inspector windows
- unit testing with gtest + run ctests
- Nous Engine documentation generation with Doxygen!

<img width="484" height="726" alt="image" src="https://github.com/user-attachments/assets/56cd6a5c-f617-4d05-a2a1-52a66d011e79" />
<img width="1447" height="438" alt="image" src="https://github.com/user-attachments/assets/79714723-08fa-4677-8ec5-e45d97e7e52c" />
<img width="484" height="338" alt="image" src="https://github.com/user-attachments/assets/23f8dd0c-fc80-48ea-a3ca-702bf379c540" />
<img width="867" height="569" alt="image" src="https://github.com/user-attachments/assets/6c4f352f-8cdf-4cdd-a29f-45f99708741d" />

#### Version 0.4 - After Bachelor's Thesis Delivery
- **Development Environment**: Migrated project to **CLion** for improved cross-platform IDE support.
- **Build System**: Rebuilt the project using **CMake** with support for **vcpkg** (manifest mode) and **CMake presets**.
- **Dependency Management**: All third-party libraries are now handled through **vcpkg**, simplifying setup and updates.
- **Cross-Platform Foundations**: Refactored the codebase to begin supporting **Linux** and **macOS**, in addition to Windows.
- **Library Updates**:
  - Upgraded from **SDL2 → SDL3**
  - Replaced **MathGeoLib → glmath** for math utilities
- **CMake Minimum Version**: Now requires **CMake 3.21 or higher**

#### Version 0.3
- **Bachelor's Thesis**: Final Delivery.

#### Version 0.2
- **Bachelor's Thesis**: Delivery 2.

#### Version 0.1
- **Bachelor's Thesis**: Delivery 1.

---

### **Dependencies**  

> [!IMPORTANT]
> #### For Windows Development and Execution
> 1. **Visual Studio Community 2022**: Download and install [Visual Studio Community 2022](https://visualstudio.microsoft.com/es/free-developer-offers/) with the following workloads:
>    - Desktop development with C++
>    - Make sure to include the latest C++ toolset and Windows SDK.
> 
> 2. **Vulkan SDK 1.3.296.0**: Download and install the [Vulkan SDK 1.3.296.0](https://sdk.lunarg.com/sdk/download/1.3.296.0/windows/VulkanSDK-1.3.296.0-Installer.exe). This provides the Vulkan runtime, headers, and tools (like `glslc` for shader compilation).
> 
> 3. **CMake (≥ 3.21)**: Build system.
> 
> 4. **Ninja**: Build generator (specified in CMakePresets).
> 
> 5. **C++23 Compiler**: e.g., MSVC, GCC ≥ 11, Clang ≥ 14...
>
> 6. **vcpkg**: [Clone the repository](https://github.com/microsoft/vcpkg) on the same parent directory as this project (e.g. Nous-Engine/../vcpkg).
>
> #### Additional Dependencies (Managed by vcpkg)
> The project uses **vcpkg** to manage the following dependencies. They will be automatically installed and built when configuring the project with CMake.
>
> - **SDL3** (with Vulkan support enabled)
> - **Vulkan** (Headers and libraries)
> - **Assimp** (Asset import library)
> - **Dear ImGui** (with features: docking-experimental, SDL3-binding, SDL3-renderer-binding, Vulkan-binding)
> - **STB** (Single-file image loading library)
> - **Parson** (Lightweight JSON library)
> - **Tracy** (Profiling tool)
> - **GLM** (Mathematics library for graphics)
>
> To install all required dependencies in classic mode:
> ```
> vcpkg install vulkan sdl3[vulkan] assimp imgui[docking-experimental,sdl3-binding,sdl3-renderer-binding,vulkan-binding] stb parson tracy glm
> ```

---

## Nous Engine v0.3

### **Context**

Multithreading is a powerful yet complex area of software development, especially when applied to game engines. While professional ones include advanced multithreading systems, their internal implementations are rarely shared publicly. Moreover, existing tutorials often lack depth, leaving many developers unprepared for production level usage.

This project aims to bridge that gap by providing a beginner-friendly and practical introduction to multithreading, answering essential questions around performance, architecture, challenges, and limitations. The goal is to create a custom solution built from scratch, offering a solid starting point for developers interested in concurrent programming and game engine architecture on how to use multithreading effectively.

The practical part of the bachelor's thesis consists on developing a multithreaded game engine written in C++, featuring a Vulkan-based renderer to take advantage of its multithreading affinity. The chosen approach for the multithreading implementation is a thread-based job system built on top of a thread pool.

---

### **Features**

Nous Engine is a modular C++ game engine with a focus on multithreaded performance and low-level rendering control. Below is an overview of its key features:

#### Core Engine Architecture
- Modular engine structure (Renderer, Resource Manager, Input, etc.)
- Thread-based Job System built on top of a Thread Pool
- Memory management system with custom allocators and tagging
- Event system for module communication
- Logging and assertion utilities

#### Rendering & Graphics
- Vulkan renderer backend
- Camera controls
- Offscreen rendering support (Scene and Game viewports)
- 3D Geometry, Material and Texture loading

#### File & Resource Management
- Resource manager with support for meshes, textures, and materials
- Custom file system and serialization for accessing engine assets

#### Editor & Debugging Tools
- ImGui editor UI
- Asset browser for visual resource management
- Debugging tools for multithreading and resources

#### Build & Development
- Custom script to build the engine

---

### Third Party Libraries

#### SDL2 - [Download](https://www.libsdl.org/)
#### Vulkan - [Download](https://vulkan.lunarg.com/)
#### Assimp - [Download](https://github.com/assimp/assimp)
#### ImGui - [Download](https://github.com/ocornut/imgui)
#### stb_image - [Download](https://github.com/nothings/stb/blob/master/stb_image.h)
#### MathGeoLib - [Download](https://github.com/juj/MathGeoLib)
#### Tracy - [Download](https://github.com/wolfpld/tracy)
#### Parson - [Download](https://github.com/kgabis/parson)

---

### Third Party Assets

#### [Lagiacrus](https://skfb.ly/oZrqM) by [09williamsad](https://sketchfab.com/adamw1806)
#### [Wolf](https://skfb.ly/KJpv) by [Juan_Puerta](https://sketchfab.com/Juan_Puerta)
#### [Cypher](https://skfb.ly/6SnPX) by [vintnes6](https://sketchfab.com/vintnes6)
#### [Viking room](https://skfb.ly/VAKF) by [nigelgoh](https://sketchfab.com/nigelgoh)
#### [Queen Xenomorph](https://github.com/Clapcom-Studios/Alien-Extraction) by [xdavido](https://github.com/xdavido)

---

### **Installation**

> [!IMPORTANT]
> #### In order to download and execute the engine you have to [Download the Latest Release](https://github.com/francesctr4/Nous-Engine/releases) and extract the zip.
> It is recommended that you extract the zip on the parent folder of the disk, so that the path doesn't get too long. <br>
> (e.g. ```D:\Nous-Engine-v0.3```)
>
> In this case, the **executable path** will be as follows: <br>
> ```D:\Nous-Engine-v0.3\Nous-Engine\Nous-Engine.exe```

---

### **Controls**

#### **Camera**

The 3D editor camera supports smooth navigation using a combination of mouse and keyboard inputs. Controls are only active when the **scene viewport is hovered**.

| Input                                              | Action                                |
|----------------------------------------------------|---------------------------------------|
| `Right Mouse Button (RMB)` + `W`                   | Move Forward                          |
| `Right Mouse Button (RMB)` + `S`                   | Move Backward                         |
| `Right Mouse Button (RMB)` + `A`                   | Move Left                             |
| `Right Mouse Button (RMB)` + `D`                   | Move Right                            |
| `Right Mouse Button (RMB)` + `E`                   | Move Up                               |
| `Right Mouse Button (RMB)` + `Q`                   | Move Down                             |
| `Shift` (Hold while moving)                        | Speed Boost                           |
| `Right Mouse Button (RMB)` + Mouse Drag            | Rotate Camera                         |
| `Alt` + `Right Mouse Button (RMB)` + Mouse Drag    | Orbit around target (origin)          |
| `Middle Mouse Button (MMB)` + Mouse Drag           | Pan camera (move on X/Y axes)         |
| `Mouse Wheel Scroll`                               | Zoom In / Out                         |

#### **Multithreading**

![Multithreading](https://github.com/user-attachments/assets/08b39436-886a-477b-af06-01833380d4af)

This tool provides **real-time visibility** into the **Job System** and thread pool usage in the engine. It is divided into two main sections:

- **Job System Overview**
- **Job Queue**

---

##### Job System Overview

This section displays the current state of the thread pool and job processing.

| Label | Description |
|-------|-------------|
| **Max Hardware Threads** | Number of hardware threads detected on the system (`std::thread::hardware_concurrency() - 1`). |
| **Total Worker Threads** | Number of active worker threads in the pool (excluding the main thread). |
| **Total Jobs** | Total number of jobs currently being handled or queued. |
| **Active Threads** | Visual indicator showing how many threads are actively running jobs (green bar). |
| **Thread Count Spinner** | Lets you configure the number of worker threads. Setting it to `0` disables multithreading. |
| **Resize Pool Button** | Applies the new thread count, resizing the thread pool at runtime. |
| **Multithreaded Mode** | Indicates if the job system is currently in multithreaded mode. |

---

##### Thread Table

Displays the current state of each thread (including the main thread).

| Column | Description |
|--------|-------------|
| **ID** | Unique identifier for each thread. |
| **Name** | Friendly label for the thread (e.g., Main Thread, Worker Thread 1, etc.). |
| **State** | Thread status — either `RUNNING` (executing a job) or `READY` (idle). |
| **Current Job** | The name of the job currently being executed. `None` if the thread is idle. |
| **Time (s)** | Duration (in seconds) the current job has been running / has taken to complete. |

---

##### Job Queue

This section lists all **pending jobs** that are waiting to be assigned to threads.

| Column | Description |
|--------|-------------|
| **Job Name** | The label of the pending job, useful for tracking or debugging. |
| **(count)** | Displays the number of currently queued jobs. |

---

##### Dynamic Thread Scaling

You can dynamically resize the thread pool using the spinner and **Resize Pool** button.

- Setting the worker count to `0` switches the system to **single-threaded mode**.
- The thread count can be adjusted in the range: `[0, (std::thread::hardware_concurrency() - 1) * 2]`

#### **Debug Keys**

These shortcuts trigger test jobs and resource loading using the job system for debugging and performance testing.

| Key    | Action Description                                                                   |
|--------|--------------------------------------------------------------------------------------|
| `F1`   | Submits a job to **load Lagiacrus Head mesh and material**.                          |
| `F2`   | Submits a job to **load Cypher mesh and material**.                                  |
| `F3`   | Submits a job to **load Queen Xenomorph mesh and material**.                         |
| `F4`   | Submits a job to **load Wolf mesh and material**.                                    |
| `F5`   | Submits **4 staggered jobs**, each loading a mesh and material with delays.          |
| `F6`   | Clears all loaded resources on the scene.                                            |
| `F7`   | Submits a test job that **sleeps for 5 seconds**.                                    |
| `F8`   | Submits **100 CPU-bound jobs** doing heavy dummy work for stress testing.            |

---

### **Known Bugs**

The following issues are primarily related to the **Resource Manager** and the **Vulkan Renderer Backend**.
While these fall outside the core focus of the thesis (which is centered on multithreading and the job system architecture), they are acknowledged and will be addressed in the near future:

- Submesh support is currently unimplemented; only a single mesh per entity is rendered.
- Crashes may occur when uploading multiple instances of the same textured mesh.
- Vulkan synchronization issues may happen when uploading geometry concurrently (RNG).
- Vulkan validation layer errors may appear when rendering multiple geometries on the scene.

These issues do not affect the multithreading implementation, which is functioning as intended.

---

### **Future Roadmap**

While the current version of *Nous Engine* delivers promising results, it remains a foundation for much broader possibilities. Future development will focus on expanding the engine's capabilities:

#### Multithreading

Planned improvements to the Job System include:

- Work-stealing mechanisms for better load balancing
- Interruptible and cooperative threads
- Lock-free concurrency algorithms
- Job prioritization
- Job dependencies and graph-based scheduling
- Callback support on job completion or failure

#### Engine Programming

To evolve into a more complete engine, future modules and tools may include:

- Scripting system
- Physics engine
- Animation system
- Audio engine
- User Interface (UI) system
- Particle system
- Artificial Intelligence (AI) module
- Shader system

Additionally, planned core tools include:

- Scene graph and hierarchy system
- Inspector and property editor
- Mouse picking and object selection
- Frustum culling
- Advanced time management
- Resource manager and asset browser enhancements

#### Graphics Programming

The render engine will continue to evolve with both foundational and advanced techniques:

- Forward and Deferred rendering pipelines
- Phong and Blinn-Phong lighting models
- Physically-Based Rendering (PBR) with Image-Based Lighting (IBL)
- Shadow Mapping
- Screen Space Ambient Occlusion (SSAO)
- Environment and Relief mapping
- Multi-pass bloom and Water effects
- More Multithreaded Rendering features

---

As development continues, both the engine’s architecture and its multithreading capabilities will be refined and extended. *Nous Engine* remains an evolving project.

---
