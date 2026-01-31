# Simple Polygon Renderer (SPR) & STL Viewer

![Benchy](benchy.png)

A minimalist C library for software-based 3D rendering, accompanied by a fully functional interactive STL viewer.

## Features

### SPR Library (`spr.h` / `spr.c`)
*   **Zero Dependencies**: Relies only on the standard C library (`stdlib.h`, `math.h`, `string.h`).
*   **A-Buffer Transparency**: Implements Order-Independent Transparency (OIT) using a per-pixel fragment list and back-to-front compositing.
*   **Programmable Pipeline**: Support for custom **Vertex** and **Fragment** shaders.
*   **SIMD Optimized**: Includes SSE2 optimized paths for high-performance rasterization.
*   **Core Math**: 3D Matrices and Vectors via a transform stack (Push/Pop, ModelView/Projection).
*   **Output**: Renders to a raw 32-bit RGBA buffer.

### STL Viewer (`viewer.c`)
*   **Interactive**: Real-time orbit, pan, zoom, and light rotation using **SDL2**.
*   **Shaders**: Includes a library of common shaders (`spr_shaders.h`): **Constant**, **Matte**, **Plastic**, and **Metal**.
*   **Format Support**: Loads both Binary and ASCII `.stl` files.
    *   **Color Support**: Reads per-triangle 15-bit RGB color attributes (VisCAM/SolidView standard).

## Prerequisites

*   **GCC** (or any standard C compiler)
*   **SDL2 Development Libraries** (only for the viewer)
    *   *Debian/Ubuntu*: `sudo apt install libsdl2-dev`
    *   *Fedora*: `sudo dnf install SDL2-devel`

## Building

The project uses a simple `Makefile`.

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
./viewer stl/bracket.stl [-simd | -cpu]
```

**Available Models**:
*   `stl/bracket.stl` - A simple mechanical bracket.
*   `stl/3DBenchy.stl` - The famous printer benchmark.
*   `stl/dome.stl` - A geodesic dome (good for transparency tests).
*   `stl/tire.stl` - A complex tire tread pattern.
*   `stl/wheel.stl` - A wheel rim.

**Controls**:
*   **Left Mouse Drag**: Rotate camera (Orbit)
*   **Shift + Left Mouse Drag**: Rotate light direction
*   **Right Mouse Drag**: Pan
*   **Mouse Wheel**: Zoom
*   **'s' Key**: Toggle Statistics Overlay (FPS, Frags, etc.)
*   **'o' Key**: Toggle Transparency (Opaque / Translucent)
*   **'c' Key**: Toggle Base Color (Grey / Red)
*   **'b' Key**: Toggle Back-face Culling
*   **1-4 Keys**: Switch Shaders (Constant, Matte, Plastic, Metal)

## Project Structure

*   `spr.h` / `spr.c`: The core renderer library (A-Buffer, SIMD).
*   `spr_shaders.h` / `spr_shaders.c`: Library of common shading models.
*   `stl.h` / `stl.c`: STL file loader with color attribute support.
*   `viewer.c`: Interactive viewer application.
*   `Makefile`: Build system.
*   `docs/`: Development plans and architecture notes.