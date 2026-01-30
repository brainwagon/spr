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

### 3. Future Input Expansion
To actually *feed* varying colors to the triangle:
*   **API**: The user can define a custom vertex structure with color.
*   **Custom Shader**: The user would write a custom VS to copy `input->r/g/b` to `out->color`.
*   **STL 16-bit Attribute**: We could optionally map the STL "attribute byte count" (often unused) to a color table or value in a specialized shader, effectively getting "color from the triangle".

## Execution Plan
1.  Modify `spr_shaders.c` to initialize `out->color` in all Vertex Shaders.
2.  Modify `spr_shaders.c` to modulate result by `interpolated->color` in all Fragment Shaders.
3.  (Optional) Verify by hardcoding a test color (e.g. Red) in the VS to see if it renders Red.
