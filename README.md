# Simple Polygon Renderer (SPR) & Object Viewer

![Benchy](benchy.png)

A minimalist C library for software-based 3D rendering, accompanied by a fully functional interactive object viewer supporting STL and OBJ formats.

## Features

### SPR Library (`spr.h` / `spr.c`)
*   **Zero Dependencies**: Relies only on the standard C library and `stb_image.h` for textures.
*   **Advanced A-Buffer Transparency**: 
    *   Order-Independent Transparency (OIT) using a per-pixel fragment list.
    *   **Dynamic Memory**: Fragment allocation using chunks and free-list recycling to minimize overhead.
    *   **Occlusion Culling**: Early rejection of fragments and culling of occluded layers based on accumulated opacity (Threshold: 0.999).
*   **Unified Loader**: Integrated support for **STL** and **Wavefront OBJ** (including `.mtl` material libraries).
*   **Texturing**: Point-sampled texture mapping with UV wrapping. Supports JPG, PNG, and other formats via `stb_image.h`.
*   **Programmable Pipeline**: Support for custom **Vertex** and **Fragment** shaders.
*   **SIMD Optimized**: Includes SSE2 optimized paths for high-performance rasterization.
*   **Core Math**: 3D Matrices and Vectors via a transform stack (Push/Pop, ModelView/Projection).
*   **Output**: Renders to a raw 32-bit RGBA buffer.

### Object Viewer (`viewer.c`)
*   **Interactive**: Real-time orbit, pan, zoom, and light rotation using **SDL2**.
*   **Resizable**: Supports dynamic window resizing with automatic aspect ratio correction.
*   **Shaders**: Includes a library of common shaders (`spr_shaders.h`): **Constant**, **Matte**, **Plastic**, **Metal**, and **Painted Plastic** (Textured).
*   **On-Screen Statistics**: Toggleable overlay showing FPS, Render Time, peak fragment counts, memory chunk usage, total triangles, and per-texture sampling counts.

## Prerequisites

*   **GCC** (or any standard C compiler)
*   **SDL2 Development Libraries** (only for the viewer)
    *   *Debian/Ubuntu*: `sudo apt install libsdl2-dev`
    *   *Fedora*: `sudo dnf install SDL2-devel`

## Building

The project uses a simple `Makefile`. Building with textures is enabled by default.

### Build the Viewer
```bash
make
```

### Build the Standalone Test (No SDL required)
```bash
make test_spr
```

## Usage

### Running the Viewer
```bash
./viewer <model_file> [texture_file] [options]
```

**Options**:
*   `-simd` : Use SIMD rasterizer (if available)
*   `-cpu`  : Use CPU rasterizer (default)
*   `-h`    : Show help message

**Available Test Models**:
*   `stl/3DBenchy.stl` - The famous printer benchmark.
*   `stl/dome.stl` - A geodesic dome (good for transparency tests).
*   `obj/cat/12221_Cat_v1_l3.obj` - A textured cat model.

**Controls**:
*   **Left Mouse Drag**: Rotate camera (Orbit)
*   **Shift + Left Mouse Drag**: Rotate light direction
*   **Right Mouse Drag**: Pan
*   **Mouse Wheel**: Zoom
*   **'s' Key**: Toggle On-Screen Statistics Overlay
*   **'o' Key**: Toggle Transparency (Opaque / Translucent)
*   **'c' Key**: Toggle Base Color (Grey / Red)
*   **'b' Key**: Toggle Back-face Culling
*   **1-5 Keys**: Switch Shaders (Constant, Matte, Plastic, Metal, Painted)
*   **ESC**: Exit

## Project Structure

*   `spr.[h|c]`: Core renderer library (A-Buffer, SIMD, Math).
*   `spr_shaders.[h|c]`: Library of common shading models.
*   `spr_loader.[h|c]`: Unified STL/OBJ mesh loader with MTL support.
*   `spr_texture.[h|c]`: Texture loading and sampling (wraps `stb_image.h`).
*   `stl.[h|c]`: Legacy STL file loader.
*   `viewer.c`: Interactive viewer application.
*   `font.h`: Minimal 8x8 bitmap font for overlay rendering.
*   `Makefile`: Build system.
*   `docs/`: Implementation details and architecture notes.
