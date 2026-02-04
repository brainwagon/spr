# Initial Concept
A minimalist C library for software-based 3D rendering, accompanied by a fully functional interactive object viewer supporting STL and OBJ formats.

# Product Definition: Simple Polygon Renderer (SPR)

## Product Vision
SPR is a simple yet robust framework for CPU-based 3D rendering. It aims to provide a reliable, dependency-free environment for loading 3D models and developing simple games or applications where a lightweight software renderer is preferred over complex graphics APIs.

## Target Use Cases
- **Embedded Viewer:** A robust solution for embedding 3D previews directly into other applications without requiring heavy graphics APIs.
- **Lightweight Game Development:** A foundation for building simple games that can run reliably on a wide range of hardware without GPU dependencies.
- **Asset Pipeline Tooling:** Providing a lightweight mechanism for asset previews and headless rendering in build pipelines.
- **Cross-Platform Graphics:** Delivering consistent rendering results across a wide variety of platforms, from desktop to low-power embedded hardware and the web.

## Target Audience
- **Tool & Game Developers:** Those building asset pipelines, 3D previewers, or simple games requiring a software-fallback or primary CPU renderer.
- **Embedded Systems Developers:** Engineers working on low-resource hardware where hardware-accelerated graphics are unavailable or unnecessary.
- **Graphics Enthusiasts:** Developers looking for a clean, understandable codebase to explore software rasterization.

## Core Features & Goals
- **Robust CPU Rendering:** Focus on providing a stable and efficient framework for all rendering tasks to be performed on the CPU.
- **Practical Feature Set:** Support essential features for model loading, texturing, and shading sufficient for interactive applications, without the complexity of full global illumination.
- **Minimal Dependencies:** Maintenance of a lean core by relying primarily on the standard C library and ubiquitous, stable libraries like `zlib` or `libpng`.
- **Advanced Format Support:** Expansion beyond STL and OBJ to include modern formats like glTF and PLY.
- **Optimized Performance:** Continuous improvement of the software rasterizer through multithreading and expanded SIMD support (SSE2 and beyond).
- **Universal Portability:** Support for multiple windowing/input backends (Win32, X11, SDL2) and first-class WebAssembly support via Emscripten.
