#ifndef SPR_SHADERS_H
#define SPR_SHADERS_H

#include "spr.h"

/* Standard Uniforms for common shaders */
typedef struct {
    mat4_t mvp;         /* Model-View-Projection */
    mat4_t model;       /* Model (World) Matrix for normals */
    vec3_t light_dir;   /* Direction TO light */
    vec3_t eye_pos;     /* Camera position in World space */
    vec4_t color;       /* Base Color (RGBA) */
    float roughness;    /* Specular power (shininess) */
} spr_shader_uniforms_t;

/* --- Constant (Unlit) --- */
void spr_shader_constant_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_color_t spr_shader_constant_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Matte (Diffuse / Lambert) --- */
void spr_shader_matte_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_color_t spr_shader_matte_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Plastic (Diffuse + Specular) --- */
void spr_shader_plastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_color_t spr_shader_plastic_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Metal (High Specular, Low Diffuse) --- */
void spr_shader_metal_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out);
spr_color_t spr_shader_metal_fs(void* user_data, const spr_vertex_out_t* interpolated);

/* --- Helpers --- */
void spr_uniforms_set_color(spr_shader_uniforms_t* u, float r, float g, float b, float a);
void spr_uniforms_set_light_dir(spr_shader_uniforms_t* u, float x, float y, float z);

#endif /* SPR_SHADERS_H */
