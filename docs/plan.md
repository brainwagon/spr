# Simple Polygon Renderer (SPR) Development Plan

## Goal
Develop a minimalist, single-file ANSI C library for software-based 3D polygon rendering. The renderer will output to a raw RGBA buffer, suitable for integration into windowing systems or saving to image files.

## Architecture

### 1. Core Structure (`spr.h` / `spr.c`)
- [x] **Language**: ANSI C (C89/C90 compliant where possible).
- [x] **Output**: 32-bit RGBA Framebuffer.
- [x] **No Dependencies**: Standard C library only (`math.h`, `stdlib.h`, `string.h`).

### 2. Data Structures
- [x] **`vec4_t`, `mat4_t`**: Basic linear algebra types.
- [x] **`vertex_t`**: Generic vertex structure (position, optional uv, normal).
- [x] **`framebuffer_t`**: Holds pointer to pixel data, dimensions, and Z-buffer.
- [x] **`context_t`**: Global or passed state containing the transform stack, current shader, and viewport settings.

### 3. Pipeline Stages
- [x] **Vertex Processing**: Transform vertices from Model space -> World -> View -> Clip space.
- [x] **Clipping**: Simple clipping against the view frustum (Near plane rejection).
- [x] **Perspective Division & Viewport Transform**: Project to screen coordinates.
- [x] **Rasterization**: Convert triangles to fragments using barycentric coordinates.
- [x] **Fragment Processing**: Execute "Fragment Shader" for each pixel to determine color.
- [x] **Depth Test**: Z-buffer check.
- [x] **Blending**: (Implicit opacity for now, overwrite).

## API Design

```c
// Setup
spr_context_t* spr_init(int width, int height);
void spr_shutdown(spr_context_t* ctx);

// Framebuffer
void spr_clear(spr_context_t* ctx, uint32_t color, float depth);
uint32_t* spr_get_buffer(spr_context_t* ctx);

// Linear Algebra & Transforms
void spr_matrix_mode(spr_context_t* ctx, spr_matrix_mode_enum mode);
void spr_push_matrix(spr_context_t* ctx);
void spr_pop_matrix(spr_context_t* ctx);
void spr_load_identity(spr_context_t* ctx);
void spr_translate(spr_context_t* ctx, float x, float y, float z);
void spr_rotate(spr_context_t* ctx, float angle, float x, float y, float z);
void spr_scale(spr_context_t* ctx, float x, float y, float z);
void spr_lookat(spr_context_t* ctx, vec3_t eye, vec3_t center, vec3_t up);
void spr_perspective(spr_context_t* ctx, float fov, float aspect, float near, float far);

// Shaders
void spr_set_program(spr_context_t* ctx, spr_vertex_shader_t vs, spr_fragment_shader_t fs, void* user_data);

// Drawing
void spr_draw_triangles(spr_context_t* ctx, int count, const void* vertices, size_t stride);
```

## Implementation Phases

### Phase 1: Skeleton & Framebuffer
- [x] Create `spr.c` and `spr.h`.
- [x] Implement `spr_init` and `spr_clear`.
- [x] Implement a utility to write the buffer to a PPM image for testing.

### Phase 2: Math & Rasterization (2D)
- [x] Implement `vec4` and `mat4` operations.
- [x] Implement a basic Barycentric rasterizer for a 2D triangle.
- [x] Output a static colored triangle to PPM.

### Phase 3: Transforms & 3D
- [x] Implement the Matrix Stack (push/pop).
- [x] Implement View and Projection matrix helpers (`lookat`, `perspective`).
- [x] Add basic vertex transformation logic.

### Phase 4 & 5: Interpolation, Depth & Shading
- [x] Implement barycentric interpolation for colors/attributes across the triangle.
- [x] Implement Z-buffering (Depth Buffer).
- [x] Define the shader function pointer interface.
- [x] Implement a sample shader (Vertex & Fragment).

## Testing Strategy
- [x] **Unit Tests**: Compile `test.c` which uses the library to render frames.
- [x] **Verification**: Check generated PPM files against expected output.