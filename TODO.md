# Future Tasks

## Rendering
*   [ ] **SIMD Fragment Shaders**: Currently, the rasterizer processes pixels in groups of 4 (SIMD), but calls the fragment shader function pointer *serially* for each active pixel. Changing the shader interface to accept `spr_vertex_out_t_4x` would allow fully vectorized shading.
*   [ ] **Texture Mapping**: The pipeline supports UV coordinates, but there is no texture sampler or image loading support.
*   [ ] **Back-face Culling**: Implement an option to cull back-facing triangles before rasterization to save processing time on closed meshes.

## File Formats
*   [ ] **OBJ Support**: The `cat` directory contains an `.obj` file. Implementing a simple OBJ loader would allow viewing this and other common models.

## Viewer
*   [ ] **Lighting Controls**: Allow rotating the light source interactively.
*   [ ] **Wireframe Mode**: A generic way to render wireframes (perhaps via barycentric coordinates in the shader).
