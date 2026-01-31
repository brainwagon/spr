# Plan: Per-Triangle/Vertex Color Support

## Goal
Modify the shader pipeline to support reading color values from the geometry (vertices/triangles) and using them as the base color for shading. Currently, shaders rely solely on a global `uniform` color.

## Analysis
*   **Current State**:
    *   `spr_vertex_out_t` has a `vec4_t color` field.
    *   **Rasterizer**: Interpolates this color field correctly.
    *   **Vertex Shaders** (`spr_shaders.c`): Currently **do not write** to `out->color`. This leads to uninitialized values being passed to the rasterizer.
    *   **Fragment Shaders** (`spr_shaders.c`): Currently **ignore** `interpolated->color`, using only `u->color`.

## Proposed Changes

### 1. Update Fragment Shaders (`spr_shaders.c`)
Modify `spr_shader_matte_fs`, `spr_shader_plastic_fs`, and `spr_shader_metal_fs` to:
*   Read `interpolated->color`.
*   Multiply the uniform base color (`u->color`) by the interpolated vertex color.
    *   Formula: `FinalAlbedo = UniformColor * VertexColor`
    *   This allows global tinting (Uniform) while preserving mesh detail (Vertex).

### 2. Update Vertex Shaders (`spr_shaders.c`)
Since the current input format (`stl_vertex_t`) does not have color info, we must ensure safe defaults:
*   Modify `spr_shader_*_vs` to set `out->color = (1.0, 1.0, 1.0, 1.0)` (White).
*   This ensures that if no color is provided, the shaders behave as they do now (purely uniform-driven).

### 3. STL Attribute Mapping (Per-Triangle Color)
Binary STL files contain a 2-byte "attribute byte count" per triangle. We will update the `stl_loader` and shaders to support the "VisCAM/Magics" standard for storing color in these bytes:
*   **Format**: 1 bit (ignored/used), 5 bits Red, 5 bits Green, 5 bits Blue.
*   **Vertex Shader**: Will decode this 16-bit value into `out->color`.

### 4. Explicit Base Color API
To make setting the base color easier for the user without manual struct manipulation, we will add:
*   **Helper**: `void spr_uniforms_set_color(spr_shader_uniforms_t* u, float r, float g, float b, float a);`
*   This ensures the "UniformColor" part of the `Uniform * Vertex` equation is easily accessible.

## Execution Plan
1.  Modify `stl.h/c` to store the 16-bit attribute per triangle.
2.  Modify `spr_shaders.c` to initialize `out->color` in all Vertex Shaders (defaulting to White if attribute is 0, or decoding the STL attribute).
3.  Modify `spr_shaders.c` to modulate result by `interpolated->color` in all Fragment Shaders.
4.  Add the `spr_uniforms_set_color` helper to `spr_shaders.h`.
5.  Update `viewer.c` to allow the user to override the mesh color via keys (e.g. 'c' to cycle random colors).
