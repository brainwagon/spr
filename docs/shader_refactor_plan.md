# Shader Refactoring and Expansion Plan

## Goal
Move the existing shader code out of `viewer.c` into a reusable library (`spr_shaders`) and implement additional common shading models.

## 1. New Library Files

### `spr_shaders.h`
*   **Unified Uniforms**: Define a `spr_shader_uniforms_t` structure to serve as a standard interface for these shaders. This simplifies the host application code.
    *   `mat4_t mvp` (Model-View-Projection Matrix)
    *   `mat4_t model` (Model Matrix for normal transformation)
    *   `vec3_t light_dir` (Direction to light source)
    *   `vec3_t eye_pos` (Camera position for specular calculations)
    *   `vec4_t color` (Base color / Albedo)
    *   `float roughness` (Controls specular highlight sharpness)
*   **Function Prototypes**:
    *   `spr_shader_constant_vs`, `spr_shader_constant_fs`
    *   `spr_shader_matte_vs`, `spr_shader_matte_fs`
    *   `spr_shader_plastic_vs`, `spr_shader_plastic_fs`
    *   `spr_shader_metal_vs`, `spr_shader_metal_fs`

### `spr_shaders.c`
*   **Math Helpers**: internal implementations of `dot`, `normalize`, `reflect`, `powf` (or fast approximation).
*   **Constant Shader**: Flat color, useful for wireframes or unlit rendering.
*   **Matte Shader**: Lambertian diffuse lighting only. No specular. Good for chalk/clay.
*   **Plastic Shader**: The existing implementation (Lambert + Blinn-Phong Specular).
*   **Metal Shader**: Anisotropic-ish or just high-contrast specular with low diffuse contribution.

## 2. Updates to `viewer.c`
*   **Includes**: Remove local math/shader code, include `spr_shaders.h`.
*   **Input**: Add number keys (1-4) to switch between active shaders at runtime.
*   **Render Loop**: Populate the `spr_shader_uniforms_t` struct and pass the selected function pointers to `spr_set_program`.

## 3. Build System
*   **Makefile**: Add `spr_shaders.o` to the build targets and link it into `viewer`.

## Execution Steps
1.  Create `spr_shaders.h` with the struct and prototypes.
2.  Create `spr_shaders.c` with the implementations.
3.  Update `Makefile`.
4.  Modify `viewer.c` to use the new system.
