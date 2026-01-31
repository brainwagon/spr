#ifndef SPR_H
#define SPR_H

#include <stdint.h>
#include <stddef.h>

/* --- Types --- */

typedef struct {
    uint8_t r, g, b, a;
} spr_color_t;

typedef struct { float x, y; } vec2_t;
typedef struct { float x, y, z; } vec3_t;
typedef struct { float x, y, z, w; } vec4_t;
typedef struct { float m[4][4]; } mat4_t;

mat4_t spr_mat4_identity(void);
mat4_t spr_mat4_mul(mat4_t a, mat4_t b);
vec4_t spr_mat4_mul_vec4(mat4_t m, vec4_t v);

typedef enum {
    SPR_PROJECTION,
    SPR_MODELVIEW
} spr_matrix_mode_enum;

/* Shader Types */
typedef struct {
    vec4_t position; /* Clip space position (x, y, z, w) */
    vec4_t color;
    vec2_t uv;
    vec3_t normal;
    float depth; 
} spr_vertex_out_t;

/* Standard Textured Vertex Input (for OBJ/etc) */
typedef struct {
    vec3_t position;
    vec3_t normal;
    vec2_t uv;
} spr_vertex_t;

typedef struct {
    vec3_t color;   /* Premultiplied: Base * Opacity */
    vec3_t opacity; /* Transmission (0=transparent, 1=opaque) */
} spr_fs_output_t;

/* User Callbacks */
typedef void (*spr_vertex_shader_t)(void* uniform_data, const void* vertex_in, spr_vertex_out_t* out);
typedef spr_fs_output_t (*spr_fragment_shader_t)(void* uniform_data, const spr_vertex_out_t* interpolated);

typedef struct {
    int width;
    int height;
    uint32_t* color_buffer; /* RGBA packed: 0xAABBGGRR (little endian) or R, G, B, A bytes */
    float* depth_buffer;
} spr_framebuffer_t;

typedef struct spr_context_t spr_context_t; /* Opaque context handle */

/* --- API --- */

/* Initialization */
spr_context_t* spr_init(int width, int height);
void spr_shutdown(spr_context_t* ctx);

/* Framebuffer Management */
/* Color is 0xAABBGGRR */
void spr_clear(spr_context_t* ctx, uint32_t color, float depth);
uint32_t* spr_get_color_buffer(spr_context_t* ctx);
int spr_get_width(spr_context_t* ctx);
int spr_get_height(spr_context_t* ctx);

/* Utils */
uint32_t spr_make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* Matrix & Transform API */
void spr_matrix_mode(spr_context_t* ctx, spr_matrix_mode_enum mode);
void spr_push_matrix(spr_context_t* ctx);
void spr_pop_matrix(spr_context_t* ctx);
void spr_load_identity(spr_context_t* ctx);
void spr_load_matrix(spr_context_t* ctx, mat4_t m);
void spr_translate(spr_context_t* ctx, float x, float y, float z);
void spr_rotate(spr_context_t* ctx, float angle_deg, float x, float y, float z);
void spr_scale(spr_context_t* ctx, float x, float y, float z);
void spr_lookat(spr_context_t* ctx, vec3_t eye, vec3_t center, vec3_t up);
void spr_perspective(spr_context_t* ctx, float fov_deg, float aspect, float near, float far);

mat4_t spr_get_modelview_matrix(spr_context_t* ctx);
mat4_t spr_get_projection_matrix(spr_context_t* ctx);

/* Shaders */
void spr_set_program(spr_context_t* ctx, spr_vertex_shader_t vs, spr_fragment_shader_t fs, void* uniform_data);

typedef enum {
    SPR_RASTERIZER_CPU,
    SPR_RASTERIZER_SIMD
} spr_rasterizer_mode_t;

void spr_set_rasterizer_mode(spr_context_t* ctx, spr_rasterizer_mode_t mode);

/* Drawing */
void spr_draw_triangles(spr_context_t* ctx, int count, const void* vertices, size_t stride);

/* Framebuffer Resolve (A-Buffer) */
void spr_resolve(spr_context_t* ctx);

/* Culling */
void spr_enable_cull_face(spr_context_t* ctx, int enable);

/* Statistics */
typedef struct {
    int active_fragments; /* Currently allocated (not freed) */
    int peak_fragments;   /* High water mark since last clear */
    int total_chunks;     /* Number of memory chunks currently allocated */
} spr_stats_t;

spr_stats_t spr_get_stats(spr_context_t* ctx);

/* Primitive Drawing (Phase 2 testing) - Keeping for debug/internal */
void spr_draw_triangle_2d_flat(spr_context_t* ctx, vec2_t v0, vec2_t v1, vec2_t v2, uint32_t color);

/* 3D Drawing (Fixed Function - Deprecated or wrapping shader?) */
/* Let's keep the simple one for "immediate mode" style testing, but maybe implement it using the shader system internally? */
/* Or just remove it to enforce shader usage. Let's keep it but rename/clarify. */
void spr_draw_triangle_simple(spr_context_t* ctx, vec3_t v0, vec3_t v1, vec3_t v2, uint32_t color);

#endif /* SPR_H */
