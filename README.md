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
- [Installation](#installation)
- [Instructions](#usage-instructions)  
  - [Controls](#controls)  
  - [Camera](#camera)  
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

## Libraries used

### SDL2 - [Download](https://www.libsdl.org/)
### Vulkan - [Download](https://vulkan.lunarg.com/)
### Assimp - [Download](https://github.com/assimp/assimp)
### ImGui - [Download](https://github.com/ocornut/imgui)
### stb_image - [Download](https://github.com/nothings/stb/blob/master/stb_image.h)
### MathGeoLib - [Download](https://github.com/juj/MathGeoLib)
### Tracy - [Download](https://github.com/wolfpld/tracy)
### Parson - [Download](https://github.com/kgabis/parson)

---

## Third Party Assets used

### [Lagiacrus](https://skfb.ly/oZrqM) by [09williamsad](https://sketchfab.com/adamw1806)
### [Wolf](https://skfb.ly/KJpv) by [Juan_Puerta](https://sketchfab.com/Juan_Puerta)
### [Cypher](https://skfb.ly/6SnPX) by [vintnes6](https://sketchfab.com/vintnes6)
### [Viking room](https://skfb.ly/VAKF) by [nigelgoh](https://sketchfab.com/nigelgoh)
### [Queen Xenomorph](https://github.com/Clapcom-Studios/Alien-Extraction) by [xdavido](https://github.com/xdavido)

---

### **Installation**

> [!IMPORTANT]
> #### In order to download and execute the engine you have to [Download the Latest Release](https://github.com/francesctr4/Nous-Engine/releases) and extract the zip.
> It is recommended that you extract the zip on the parent folder of the disk, so that the path doesn't get too long. <br>
> (e.g. ```D:\Nous-Engine-v0.3```)
>
> In this case, the **executable path** will be as follows: <br>
> ```D:\Nous-Engine-v0.3\Nous-Engine.exe```

---

### **Instructions**
#### **Controls**
#### **Camera**
#### **Debug Keys**

---

### **Known Bugs**

- engine is not taking into account submeshes right now.
- crash when uploading several instances of the same textured mesh.
- vulakn synchronization issues when uploading concurrent geometry in release config
- vulkan validation errors when rendering different geometry at the same time

---

### **Future Roadmap**

While the current version of the engine demonstrates satisfactory results, this is only the beginning. There is still a great deal of potential to unlock, both by expanding the engine’s features and by diving deeper into areas where multithreading can shine even more:
From a Multithreading perspective, there are several advanced features that could be explored and implemented in the future. These include work-stealing, interruptible threads, lock-free concurrency algorithms, adding priority to jobs, job dependencies, and callback functions on job completion or failure.
On the Engine Programming side, there is a long list of modules and tools that could be added to make it more complete. These include support for scripting, physics, animation, audio, UI, shaders, particles, and AI systems. Some core engine tools could also be implemented, such as the inspector, hierarchy, scene graph, mouse picking, frustum culling, time management, and some enhancements to the resource manager and assets browser.
In terms of Graphics Programming, there is significant room to enhance the visual capabilities of the engine. Some basic improvements could include implementing forward and deferred shading models with Blinn-Phong lighting. For more advanced techniques, support for physically-based rendering (PBR) with image-based lighting (IBL) could be added, along with post-processing effects such as screen-space ambient occlusion (SSAO), environment mapping, relief mapping, multi-pass bloom, water effects and shadow mapping.
Both the engine and graphics programming features have strong potential to benefit from the multithreading capabilities of the engine. Expanding in these areas would not only enhance its functionality but also better showcase the power of the job system. The work done so far has provided a solid foundation and allowed to deeply understand the core principles of Multithreading and Vulkan within the context of a Game Engine architecture. Nous Engine will remain ongoing; a project that will continue to be developed and evolved over time as a part of the personal portfolio.

---
