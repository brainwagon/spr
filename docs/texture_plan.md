# Texture Support Implementation

## Overview
Added support for loading and sampling textures (JPG, PNG, etc.) using `stb_image.h`.

## Components

### 1. `spr_texture.[h|c]`
*   **Struct**: `spr_texture_t` holds width, height, channels, and raw pixel data.
*   **Loading**: `spr_texture_load` uses `stbi_load` to parse images.
*   **Sampling**: `spr_texture_sample` performs Point Sampling with UV Wrapping. Handles Gray, Gray+Alpha, RGB, RGBA formats.

### 2. Shader Integration
*   **Uniforms**: `spr_shader_uniforms_t` now has `void* texture_ptr`.
*   **Painted Plastic Shader**:
    *   **VS**: Generates planar UVs (`u=x*0.05, v=y*0.05`) for testing on STLs.
    *   **FS**: Samples texture, modulates with Vertex Color, and applies Plastic lighting logic.

### 3. Viewer
*   **Arguments**: Accepts optional second argument for texture file.
    *   `./viewer model.stl [texture.png]`
*   **Key '5'**: Activates `Painted Plastic` shader.

## Dependencies
*   `stb_image.h`: Single header library (downloaded).
*   **Conditional Compilation**: Guarded by `SPR_ENABLE_TEXTURES` in `Makefile`.
