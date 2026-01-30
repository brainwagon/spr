#include "spr_shaders.h"
#include "stl.h" /* For stl_vertex_t */
#include <math.h>

/* --- Helpers --- */

void spr_uniforms_set_color(spr_shader_uniforms_t* u, float r, float g, float b, float a) {
    if (!u) return;
    u->color.x = r;
    u->color.y = g;
    u->color.z = b;
    u->color.w = a;
}

void spr_uniforms_set_light_dir(spr_shader_uniforms_t* u, float x, float y, float z) {
    if (!u) return;
    /* We normalize it here for safety, though shaders also normalize */
    float len = sqrtf(x*x + y*y + z*z);
    if (len > 0.0f) {
        u->light_dir.x = x / len;
        u->light_dir.y = y / len;
        u->light_dir.z = z / len;
    } else {
        u->light_dir.x = 0.0f; u->light_dir.y = 0.0f; u->light_dir.z = 1.0f;
    }
}

static void decode_stl_color(uint16_t attr, vec4_t* out_color) {
    /* If attribute is 0, default to White */
    if (attr == 0) {
        out_color->x = 1.0f;
        out_color->y = 1.0f;
        out_color->z = 1.0f;
        out_color->w = 1.0f;
        return;
    }
    
    /* Assume 15-bit RGB (0BBBBBGGGGGRRRRR) or similar */
    /* Some formats use bit 15 as 'valid' flag. We'll ignore it and mask 0x7FFF */
    
    /* Standard VisCAM/SolidView:
       Bit 0-4: Red (0-31)
       Bit 5-9: Green (0-31)
       Bit 10-14: Blue (0-31)
    */
    
    float r = (float)(attr & 0x1F) / 31.0f;
    float g = (float)((attr >> 5) & 0x1F) / 31.0f;
    float b = (float)((attr >> 10) & 0x1F) / 31.0f;
    
    out_color->x = r;
    out_color->y = g;
    out_color->z = b;
    out_color->w = 1.0f;
}

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
    
    /* Decode vertex color */
    decode_stl_color(v->attr, &out->color);
}

spr_color_t spr_shader_constant_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    
    /* Combine Uniform Color * Vertex Color */
    float r = u->color.x * interpolated->color.x;
    float g = u->color.y * interpolated->color.y;
    float b = u->color.z * interpolated->color.z;
    float a = u->color.w * interpolated->color.w;
    
    spr_color_t c;
    c.r = (uint8_t)(sh_min(r, 1.0f) * 255.0f);
    c.g = (uint8_t)(sh_min(g, 1.0f) * 255.0f);
    c.b = (uint8_t)(sh_min(b, 1.0f) * 255.0f);
    c.a = (uint8_t)(sh_min(a, 1.0f) * 255.0f);
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
    
    decode_stl_color(v->attr, &out->color);
}

spr_color_t spr_shader_matte_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    
    vec3_t N = sh_normalize(interpolated->normal);
    vec3_t L = sh_normalize(u->light_dir);
    
    float diff = sh_max(sh_dot(N, L), 0.0f);
    float amb = 0.1f;
    float intensity = diff + amb;
    
    /* Base Color = Uniform * Vertex */
    float br = u->color.x * interpolated->color.x;
    float bg = u->color.y * interpolated->color.y;
    float bb = u->color.z * interpolated->color.z;
    
    spr_color_t c;
    c.r = (uint8_t)(sh_min(br * intensity, 1.0f) * 255.0f);
    c.g = (uint8_t)(sh_min(bg * intensity, 1.0f) * 255.0f);
    c.b = (uint8_t)(sh_min(bb * intensity, 1.0f) * 255.0f);
    c.a = 255;
    return c;
}

/* --- Plastic (Phong/Blinn) Shader --- */
void spr_shader_plastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
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
        vec3_t V = {0.0f, 0.0f, 1.0f}; 
        vec3_t R = sh_reflect((vec3_t){-L.x, -L.y, -L.z}, N);
        float s = sh_max(sh_dot(R, V), 0.0f);
        if (s > 0.0f) spec = powf(s, u->roughness);
    }
    
    float br = u->color.x * interpolated->color.x;
    float bg = u->color.y * interpolated->color.y;
    float bb = u->color.z * interpolated->color.z;
    
    float r = br * (diff + amb) + spec * 0.4f;
    float g = bg * (diff + amb) + spec * 0.4f;
    float b = bb * (diff + amb) + spec * 0.4f;
    
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
    
    float diff = sh_max(sh_dot(N, L), 0.0f);
    float amb = 0.1f;
    float spec = 0.0f;
    
    if (diff > 0.0f) {
        vec3_t V = {0.0f, 0.0f, 1.0f};
        vec3_t R = sh_reflect((vec3_t){-L.x, -L.y, -L.z}, N);
        float s = sh_max(sh_dot(R, V), 0.0f);
        if (s > 0.0f) spec = powf(s, u->roughness * 1.5f);
    }
    
    float br = u->color.x * interpolated->color.x;
    float bg = u->color.y * interpolated->color.y;
    float bb = u->color.z * interpolated->color.z;
    
    float diffuse_factor = 0.3f; 
    float specular_factor = 1.2f;
    
    float r = (br * diff * diffuse_factor) + (amb * br) + (spec * br * specular_factor);
    float g = (bg * diff * diffuse_factor) + (amb * bg) + (spec * bg * specular_factor);
    float b = (bb * diff * diffuse_factor) + (amb * bb) + (spec * bb * specular_factor);
    
    spr_color_t c;
    c.r = (uint8_t)(sh_min(r, 1.0f) * 255.0f);
    c.g = (uint8_t)(sh_min(g, 1.0f) * 255.0f);
    c.b = (uint8_t)(sh_min(b, 1.0f) * 255.0f);
    c.a = 255;
    return c;
}