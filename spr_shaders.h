#ifndef SPR_SHADERS_H
#define SPR_SHADERS_H

#include "spr.h"

/* Standard Uniforms for common shaders */
typedef struct {
    mat4_t mvp;         /* Model-View-Projection */
    mat4_t model;       /* Model (World) Matrix for normals */
    vec3_t light_dir;   /* Direction TO light */
    vec3_t eye_pos;     /* Camera position in World space */
    vec4_t color;       /* Base Color (RGBA) - Alpha often ignored if Opacity used */
    vec3_t opacity;     /* Per-channel Opacity (Transmission) */
    float roughness;    /* Specular power (shininess) */
    void* texture_ptr;  /* Optional pointer to spr_texture_t (Diffuse) */
    void* specular_map_ptr; /* Optional pointer to spr_texture_t (Specular) */
} spr_shader_uniforms_t;

/* --- Constant (Unlit) --- */
void spr_shader_constant_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_fs_output_t spr_shader_constant_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Matte (Diffuse / Lambert) --- */
void spr_shader_matte_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_fs_output_t spr_shader_matte_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Plastic (Diffuse + Specular) --- */
void spr_shader_plastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_fs_output_t spr_shader_plastic_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Painted Plastic (Textured) --- */
void spr_shader_paintedplastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_fs_output_t spr_shader_paintedplastic_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Textured Mesh Vertex Shader (Expects spr_vertex_t) --- */
void spr_shader_textured_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);

/* --- Metal (High Specular, Low Diffuse) --- */
void spr_shader_metal_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_fs_output_t spr_shader_metal_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Helpers --- */
void spr_uniforms_set_color(spr_shader_uniforms_t* u, float r, float g, float b, float a);
void spr_uniforms_set_opacity(spr_shader_uniforms_t* u, float r, float g, float b);
void spr_uniforms_set_light_dir(spr_shader_uniforms_t* u, float x, float y, float z);

#endif /* SPR_SHADERS_H */