# Technology Stack: Simple Polygon Renderer (SPR)

## Core Library (libspr)
- **Language:** ANSI C (C89/C90) for maximum portability.
- **Dependencies:** 
    - Standard C Library (libc).
    - `stb_image.h` (Vendored) for texture loading.
    - `cgltf.h` (Vendored) for glTF 2.0 parsing.
    - Ubiquitous system libraries: `zlib`, `libpng` (Optional/Pragmatic).
- **Output:** Raw RGB or RGBA pixel buffers.
- **Internal APIs:** 
    - Custom 3D Math (Vectors/Matrices).
    - Software Rasterizer (SIMD optimized where applicable).
    - Unified Mesh Loader (STL, OBJ/MTL, glTF 2.0/GLB).

## Application Layer (Example Apps & Viewers)
- **Language:** C (C99/C11).
- **Windowing & Input:** SDL2 (used in `viewer` and `template` apps).
- **UI & Statistics:** Custom bitmap font rendering (`spr_font`) and on-screen overlays.

## Infrastructure
- **Build System:** GNU Makefile.
- **Compilers:** GCC, Clang.
- **Platforms:** Linux, Windows, macOS, WebAssembly (via Emscripten).
