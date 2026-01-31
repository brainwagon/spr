#include "spr_texture.h" /* For texture sampling */
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

void spr_uniforms_set_opacity(spr_shader_uniforms_t* u, float r, float g, float b) {
    if (!u) return;
    u->opacity.x = r;
    u->opacity.y = g;
    u->opacity.z = b;
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
    
    /* Assume 15-bit RGB (0BBBBBGGGGGRRRRR) */
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

/* --- Constant Shader --- */
void spr_shader_constant_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    const stl_vertex_t* v = (const stl_vertex_t*)input_vertex;
    
    vec4_t pos = {v->x, v->y, v->z, 1.0f};
    out->position = spr_mat4_mul_vec4(u->mvp, pos);
    decode_stl_color(v->attr, &out->color);
}

spr_fs_output_t spr_shader_constant_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    spr_fs_output_t out;
    
    /* Result = (Uniform * Vertex) */
    float r = u->color.x * interpolated->color.x;
    float g = u->color.y * interpolated->color.y;
    float b = u->color.z * interpolated->color.z;
    
    /* Premultiply by opacity */
    out.color.x = r * u->opacity.x;
    out.color.y = g * u->opacity.y;
    out.color.z = b * u->opacity.z;
    out.opacity = u->opacity;
    
    return out;
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

spr_fs_output_t spr_shader_matte_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    spr_fs_output_t out;
    
    vec3_t N = sh_normalize(interpolated->normal);
    vec3_t L = sh_normalize(u->light_dir);
    
    float diff = sh_max(sh_dot(N, L), 0.0f);
    float amb = 0.1f;
    float intensity = diff + amb;
    
    float br = u->color.x * interpolated->color.x;
    float bg = u->color.y * interpolated->color.y;
    float bb = u->color.z * interpolated->color.z;
    
    /* Premultiply */
    out.color.x = (br * intensity) * u->opacity.x;
    out.color.y = (bg * intensity) * u->opacity.y;
    out.color.z = (bb * intensity) * u->opacity.z;
    out.opacity = u->opacity;
    
    return out;
}

/* --- Plastic (Phong/Blinn) Shader --- */
void spr_shader_plastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_matte_vs(user_data, input_vertex, out);
}

spr_fs_output_t spr_shader_plastic_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    spr_fs_output_t out;
    
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
    
    /* Specular is additive and usually NOT multiplied by opacity in "glass" models (highlights remain bright),
       but for standard "transparent plastic", it usually fades. 
       Let's fade everything for consistency with "Over" operator. */
    
    float r = br * (diff + amb) + spec * 0.4f;
    float g = bg * (diff + amb) + spec * 0.4f;
    float b = bb * (diff + amb) + spec * 0.4f;
    
    out.color.x = r * u->opacity.x;
    out.color.y = g * u->opacity.y;
    out.color.z = b * u->opacity.z;
    out.opacity = u->opacity;
    
    return out;
}

/* --- Painted Plastic Shader --- */
void spr_shader_paintedplastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_matte_vs(user_data, input_vertex, out);
    
    /* Generate UVs (Planar Z Projection for testing) */
    /* Scale factor 0.05 to make texture repeat reasonably on typical STL units (mm) */
    const stl_vertex_t* v = (const stl_vertex_t*)input_vertex;
    out->uv.x = v->x * 0.05f;
    out->uv.y = v->y * 0.05f;
}

spr_fs_output_t spr_shader_paintedplastic_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    spr_fs_output_t out;
    
    /* Sample Texture */
    vec4_t tex_col = spr_texture_sample((const spr_texture_t*)u->texture_ptr, interpolated->uv.x, interpolated->uv.y);
    
    /* Reuse Plastic Lighting Logic */
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
    
    /* Specular Map Modulation */
    if (u->specular_map_ptr) {
        vec4_t spec_map = spr_texture_sample((const spr_texture_t*)u->specular_map_ptr, interpolated->uv.x, interpolated->uv.y);
        spec *= spec_map.x; /* Use Red channel for intensity */
    }
    
    /* Mix Texture with Vertex Color (Multiply) */
    float base_r = tex_col.x * u->color.x * interpolated->color.x;
    float base_g = tex_col.y * u->color.y * interpolated->color.y;
    float base_b = tex_col.z * u->color.z * interpolated->color.z;
    
    float r = base_r * (diff + amb) + spec * 0.4f;
    float g = base_g * (diff + amb) + spec * 0.4f;
    float b = base_b * (diff + amb) + spec * 0.4f;
    
    out.color.x = r * u->opacity.x;
    out.color.y = g * u->opacity.y;
    out.color.z = b * u->opacity.z;
    out.opacity = u->opacity;
    
    return out;
}

/* --- Textured Mesh Vertex Shader --- */
void spr_shader_textured_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    const spr_vertex_t* v = (const spr_vertex_t*)input_vertex;
    
    /* Position */
    vec4_t pos = {v->position.x, v->position.y, v->position.z, 1.0f};
    out->position = spr_mat4_mul_vec4(u->mvp, pos);
    
    /* Normal */
    vec4_t n4 = {v->normal.x, v->normal.y, v->normal.z, 0.0f};
    n4 = spr_mat4_mul_vec4(u->model, n4);
    out->normal.x = n4.x; out->normal.y = n4.y; out->normal.z = n4.z;
    
    /* UV */
    out->uv = v->uv;
    
    /* Color (Default White) */
    out->color.x = 1.0f; out->color.y = 1.0f; out->color.z = 1.0f; out->color.w = 1.0f;
}

/* --- Metal Shader --- */
void spr_shader_metal_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    spr_shader_matte_vs(user_data, input_vertex, out);
}

spr_fs_output_t spr_shader_metal_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    spr_fs_output_t out;
    
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
    
    out.color.x = r * u->opacity.x;
    out.color.y = g * u->opacity.y;
    out.color.z = b * u->opacity.z;
    out.opacity = u->opacity;
    
    return out;
}
