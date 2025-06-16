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
- [Description](#description)
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

### **Description**

---

### **Features**

#### Resource Manager
#### Vulkan Implementation
#### Multithreading Implementation
#### Assets Browser
#### Offscreen Rendering (scene and game viewports)
#### 3d geometry and texture loading
#### build system

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
#### **Multithreading**
#### **Debug Keys**

---

### **Known Bugs**

- Submesh support is currently unimplemented; only a single mesh per entity is rendered.
- Crashes may occur when uploading multiple instances of the same textured mesh simultaneously.
- Vulkan synchronization issues are present when uploading geometry concurrently in **Release** configurations.
- Vulkan validation layer errors may appear when rendering different geometries concurrently.

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
