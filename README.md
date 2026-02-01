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
*   **Unified Loader**: Integrated support for **STL** and **Wavefront OBJ** (including `.mtl` material libraries with full map support).
*   **Texturing**: Point-sampled texture mapping with UV wrapping. Supports JPG, PNG, and other formats via `stb_image.h`. Note: `map_Bump` in MTL files is treated as an alias for `norm` (Normal Mapping).
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

The project uses a unified `Makefile` that builds the library and all applications.

### Build Everything
```bash
make
```

### Build Specific Targets
```bash
make lib       # Build libspr.a
make viewer    # Build the viewer
make template  # Build the template app
make test      # Build the headless test
```

## Usage

### Running the Viewer
```bash
./bin/viewer <model_file> [texture_file] [options]
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
*   **'w' Key**: Cycle Wireframe Mode (Off / Overlay / Only)
*   **1-6 Keys**: Switch Shaders (Constant, Matte, Plastic, Metal, Painted, MTL)
*   **ESC**: Exit

## Project Structure

*   `src/`: Core `libspr` library source code.
    *   `spr.[h|c]`: Core renderer.
    *   `spr_shaders.[h|c]`: Shader library.
    *   `spr_loader.[h|c]`: Mesh loader.
    *   `spr_texture.[h|c]`: Texture management.
    *   `spr_font.[h|c]`: Bitmap font utilities.
*   `apps/`: Applications.
    *   `viewer/`: The full interactive object viewer.
    *   `template/`: A minimal "Hello World" example.
    *   `test_headless/`: Automated testing.
*   `lib/`: Compiled static library (`libspr.a`).
*   `bin/`: Compiled executables.
*   `Makefile`: Generalized build system.
