#include "spr_shaders.h"
#include "stl.h" /* For stl_vertex_t */
#include <math.h>

/* --- Math Helpers --- */
static float sh_dot(vec3_t a, vec3_t b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

static vec3_t sh_normalize(vec3_t v) {
    float len = sqrtf(sh_dot(v, v));
    if (len > 0) { v.x/=len; v.y/=len; v.z/=len; }
    return v;
}

static vec3_t sh_reflect(vec3_t i, vec3_t n) {
    float d = sh_dot(i, n);
    vec3_t r;
    r.x = i.x - 2.0f * d * n.x;
    r.y = i.y - 2.0f * d * n.y;
    r.z = i.z - 2.0f * d * n.z;
    return r;
}

static float sh_max(float a, float b) { return (a > b) ? a : b; }
static float sh_min(float a, float b) { return (a < b) ? a : b; }

/* --- Constant Shader --- */
void spr_shader_constant_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    const stl_vertex_t* v = (const stl_vertex_t*)input_vertex;
    
    vec4_t pos = {v->x, v->y, v->z, 1.0f};
    out->position = spr_mat4_mul_vec4(u->mvp, pos);
    
    /* No lighting calc needed */
}

spr_color_t spr_shader_constant_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    (void)interpolated;
    
    spr_color_t c;
    c.r = (uint8_t)(u->color.x * 255.0f);
    c.g = (uint8_t)(u->color.y * 255.0f);
    c.b = (uint8_t)(u->color.z * 255.0f);
    c.a = (uint8_t)(u->color.w * 255.0f);
    return c;
}

/* --- Matte (Lambert) Shader --- */
void spr_shader_matte_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    const stl_vertex_t* v = (const stl_vertex_t*)input_vertex;
    
    vec4_t pos = {v->x, v->y, v->z, 1.0f};
    out->position = spr_mat4_mul_vec4(u->mvp, pos);
    
    vec4_t n4 = {v->nx, v->ny, v->nz, 0.0f};
    n4 = spr_mat4_mul_vec4(u->model, n4);
    out->normal.x = n4.x; out->normal.y = n4.y; out->normal.z = n4.z;
}

spr_color_t spr_shader_matte_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    
    vec3_t N = sh_normalize(interpolated->normal);
    vec3_t L = sh_normalize(u->light_dir);
    
    float diff = sh_max(sh_dot(N, L), 0.0f);
    float amb = 0.1f;
    
    float intensity = diff + amb;
    
    spr_color_t c;
    c.r = (uint8_t)(sh_min(u->color.x * intensity, 1.0f) * 255.0f);
    c.g = (uint8_t)(sh_min(u->color.y * intensity, 1.0f) * 255.0f);
    c.b = (uint8_t)(sh_min(u->color.z * intensity, 1.0f) * 255.0f);
    c.a = 255;
    return c;
}

/* --- Plastic (Phong/Blinn) Shader --- */
void spr_shader_plastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    /* Same vertex logic as Matte */
    spr_shader_matte_vs(user_data, input_vertex, out);
}

spr_color_t spr_shader_plastic_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    
    vec3_t N = sh_normalize(interpolated->normal);
    vec3_t L = sh_normalize(u->light_dir);
    
    float diff = sh_max(sh_dot(N, L), 0.0f);
    float amb = 0.2f;
    float spec = 0.0f;
    
    if (diff > 0.0f) {
        vec3_t V = {0.0f, 0.0f, 1.0f}; /* Simple view approximation */
        vec3_t R = sh_reflect((vec3_t){-L.x, -L.y, -L.z}, N);
        float s = sh_max(sh_dot(R, V), 0.0f);
        if (s > 0.0f) spec = powf(s, u->roughness);
    }
    
    float r = u->color.x * (diff + amb) + spec * 0.4f; /* White specular */
    float g = u->color.y * (diff + amb) + spec * 0.4f;
    float b = u->color.z * (diff + amb) + spec * 0.4f;
    
    spr_color_t c;
    c.r = (uint8_t)(sh_min(r, 1.0f) * 255.0f);
    c.g = (uint8_t)(sh_min(g, 1.0f) * 255.0f);
    c.b = (uint8_t)(sh_min(b, 1.0f) * 255.0f);
    c.a = 255;
    return c;
}

/* --- Metal Shader --- */
void spr_shader_metal_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_matte_vs(user_data, input_vertex, out);
}

spr_color_t spr_shader_metal_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    
    vec3_t N = sh_normalize(interpolated->normal);
    vec3_t L = sh_normalize(u->light_dir);
    
    /* Metal has low diffuse, high specular, and specular is tinted by base color */
    
    float diff = sh_max(sh_dot(N, L), 0.0f);
    float amb = 0.1f;
    float spec = 0.0f;
    
    if (diff > 0.0f) {
        vec3_t V = {0.0f, 0.0f, 1.0f};
        vec3_t R = sh_reflect((vec3_t){-L.x, -L.y, -L.z}, N);
        float s = sh_max(sh_dot(R, V), 0.0f);
        if (s > 0.0f) spec = powf(s, u->roughness * 1.5f); /* Sharper highlights */
    }
    
    /* Metal model: Diffuse is very dark (absorption), Specular is bright and colored */
    float diffuse_factor = 0.3f; 
    float specular_factor = 1.2f;
    
    float r = (u->color.x * diff * diffuse_factor) + (amb * u->color.x) + (spec * u->color.x * specular_factor);
    float g = (u->color.y * diff * diffuse_factor) + (amb * u->color.y) + (spec * u->color.y * specular_factor);
    float b = (u->color.z * diff * diffuse_factor) + (amb * u->color.z) + (spec * u->color.z * specular_factor);
    
    /* Add a bit of white to specular center for very bright spots? Optional. */
    
    spr_color_t c;
    c.r = (uint8_t)(sh_min(r, 1.0f) * 255.0f);
    c.g = (uint8_t)(sh_min(g, 1.0f) * 255.0f);
    c.b = (uint8_t)(sh_min(b, 1.0f) * 255.0f);
    c.a = 255;
    return c;
}
