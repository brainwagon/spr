# libspr Template Application

This directory contains a minimal "Hello World" application demonstrating how to use `libspr` with **SDL2**.

## Overview

This template provides the essential boilerplate to:
1.  Initialize an SDL2 window and renderer.
2.  Initialize the `libspr` context.
3.  Set up a basic 3D scene (a rotating cube).
4.  Implement a main loop that handles input, updates geometry, and blits the rendered framebuffer to the SDL window.

## Code Structure (`main.c`)

*   **Initialization**: Sets up `SDL_Init`, `SDL_CreateWindow`, and `spr_init`.
*   **Data**: Defines a simple hardcoded vertex array for a cube.
*   **Input Loop**: Handles standard SDL events (quit, mouse interaction).
*   **Rendering**:
    *   Clears the buffer (`spr_clear`).
    *   Sets up Model-View-Projection matrices (`spr_lookat`, `spr_perspective`).
    *   Configures a shader (`spr_shader_matte_fs`).
    *   Draws the mesh (`spr_draw_triangles`).
    *   Resolves the A-Buffer to the color buffer (`spr_resolve`).
*   **Display**: Locks the SDL texture, copies the `libspr` buffer, and presents it.

## Building

This application is built automatically by the root `Makefile`.

```bash
# Build only the template
make template

# Run it
./bin/template
```
