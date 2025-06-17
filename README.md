# Nous Engine
### **A multithreaded, Vulkan-based game engine developed in C++**

![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20%2B-blue)

---

![Showcase](https://github.com/user-attachments/assets/384d2461-457c-42ee-ba22-7a63221d16c2)

---

### **Enhancing Software Performance through Multithreading and Parallel Programming Techniques**

This repository contains source code developed as part of my **Bachelor's Thesis (TFG)**:

- **Nous Engine Repository:** [https://github.com/francesctr4/Nous-Engine](https://github.com/francesctr4/Nous-Engine)

- **Multithreading Library:** [https://github.com/francesctr4/NOUS_Multithreading.h](https://github.com/francesctr4/NOUS_Multithreading.h)

**Author:** Francesc Teruel Rodriguez ([francesctr4](https://github.com/francesctr4) on GitHub)  
**Updated:** 30/06/2025  
**Version:** 0.3  

**License:** MIT License  
© 2025 Francesc Teruel Rodriguez

---

### **Contributing**
Pull requests are welcome! If you'd like to suggest improvements, add features, or report issues, feel free to open a GitHub issue or PR.

---

## 📚 **Table of Contents**

- [Home](#nous-engine)
- [Context](#context)
- [Features](#features)
- [Dependencies](#dependencies)
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

### **Context**

Multithreading is a powerful yet complex area of software development, especially when applied to game engines. While professional ones include advanced multithreading systems, their internal implementations are rarely shared publicly. Moreover, existing tutorials often lack depth, leaving many developers unprepared for production level usage.

This project aims to bridge that gap by providing a beginner-friendly and practical introduction to multithreading, answering essential questions around performance, architecture, challenges, and limitations. The goal is to create a custom solution built from scratch, offering a solid starting point for developers interested in concurrent programming and game engine architecture on how to use multithreading effectively.

The practical part of the bachelor's thesis consists on developing a multithreaded game engine written in C++, featuring a Vulkan-based renderer to take advantage of its multithreading affinity. The chosen approach for the multithreading implementation is a thread-based job system built on top of a thread pool.

---

### **Features**

Nous Engine is a modular C++ game engine with a focus on multithreaded performance and low-level rendering control. Below is an overview of its key features:

#### 🧠 Core Engine Architecture
- Modular engine structure (Renderer, Resource Manager, Input, etc.)
- Multithreading Job System
- Memory management system with custom allocators and tagging
- Event system for modular communication
- Logging and assertion utilities

---

#### 🖼️ Rendering & Graphics
- Vulkan renderer backend with validation and GPU info logging
- Offscreen rendering support (Game and Scene viewports)
- Material system with support for `.nmat` custom materials
- Scene and game cameras
- 3D geometry and texture loading (supports FBX, OBJ, etc.)

---

#### 🛠️ Editor & Tooling
- ImGui-based editor UI
- Asset browser for visual resource management
- Debugging tools: multithreaded job viewer, job queue monitor, memory tools
- Serialization system for saving/loading resources

---

#### 🗃️ File & Resource Management
- Resource manager with support for meshes, textures, and materials
- Custom file system for accessing engine assets
- File-based serialization for components and scene data

---

#### 🏗️ Build & Development
- Cross-platform CMake build system
- Written in modern C++ (C++20 standard)
- Built-in unit testing framework
- Flexible engine module structure for extensibility

---

### **Dependencies**  

> [!IMPORTANT]
> #### It is _required_ to download [Visual Studio Community 2022](https://visualstudio.microsoft.com/es/free-developer-offers/) with the following extension to execute the engine.
> <table>
>   <tr>
>     <td align="center">
>       <img src="https://github.com/Clapcom-Studios/Alien-Extraction/assets/99948892/ded6aef0-c9ff-4666-95cb-3123b605b5cf" alt="Image 2"/>
>     </td>
>   </tr>
> </table>
>
> #### It is also _required_ to download the [Vulkan SDK 1.3.296.0](https://sdk.lunarg.com/sdk/download/1.3.296.0/windows/VulkanSDK-1.3.296.0-Installer.exe) to execute the engine.
> #### Nous Engine uses ISO C++20 Standard (compile with `/std:c++20` or `-std=c++20`) or newer.

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
| **Multithreaded Mode Button** | Indicates if the job system is currently in multithreaded mode. |

---

##### Thread Table

Displays the current state of each thread (including the main thread).

| Column | Description |
|--------|-------------|
| **ID** | Unique identifier for each thread. |
| **Name** | Friendly label for the thread (e.g., Main Thread, Worker Thread 1, etc.). |
| **State** | Thread status — either `RUNNING` (executing a job) or `READY` (idle). |
| **Current Job** | The name of the job currently being executed. `None` if the thread is idle. |
| **Time (s)** | Duration (in seconds) the current job has been running. Useful for spotting long-running tasks. |

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
| `F7`   | Submits a job that **sleeps for 5 seconds**.                                         |
| `F8`   | Submits **100 CPU-bound jobs** doing heavy dummy work for stress testing.            |

---

### **Known Bugs**

The following issues are primarily related to the **Resource Manager** and the **Vulkan backend implementation**. 
While these fall outside the core focus of the thesis — which centered on multithreading and job system architecture — they are acknowledged and will be addressed in the near future.

- Submesh support is currently unimplemented; only a single mesh per entity is rendered.
- Crashes may occur when uploading multiple instances of the same textured mesh.
- Vulkan synchronization issues may happen when uploading geometry concurrently (RNG).
- Vulkan validation layer errors may appear when rendering multiple geometries on the scene.

These issues do not affect the core multithreaded job system, which is functioning as intended.

---

### **Future Roadmap**

While the current version of *Nous Engine* delivers promising results, it remains a foundation for much broader possibilities. Future development will focus on both expanding engine capabilities and deepening multithreading optimizations.

#### Multithreading Enhancements

Planned improvements to the Job System include:

- Work-stealing mechanisms for better load balancing
- Interruptible and cooperative threads
- Lock-free concurrency algorithms
- Job prioritization
- Job dependencies and graph-based scheduling
- Callback support on job completion or failure

#### Engine Features

To evolve into a more complete engine, future modules and tools may include:

- Scripting system (e.g., Lua or C#)
- Physics engine integration
- Animation system
- Audio subsystem
- User Interface (UI) framework
- Particle system
- Artificial Intelligence (AI) modules
- Shader management and editor tools

Additionally, planned core tools include:

- Scene graph and hierarchy system
- Inspector and property editor
- Mouse picking and object selection
- Frustum culling
- Advanced time management
- Resource manager and asset browser enhancements

#### Graphics Features

Graphics rendering will continue to evolve with both foundational and advanced techniques:

- Forward and deferred rendering pipelines
- Blinn-Phong and Phong lighting models
- Physically-Based Rendering (PBR) with Image-Based Lighting (IBL)
- Post-processing effects:
  - Screen Space Ambient Occlusion (SSAO)
  - Environment and relief mapping
  - Shadow mapping
  - Multi-pass bloom and water effects

---

As development continues, both the engine’s architecture and its multithreading capabilities will be refined and extended. *Nous Engine* remains an evolving project.

---
