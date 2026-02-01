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

static vec3_t sh_cross(vec3_t a, vec3_t b) {
    vec3_t r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

static float sh_max(float a, float b) { return (a > b) ? a : b; }

static void apply_wireframe(spr_shader_uniforms_t* u, const spr_vertex_out_t* interpolated, spr_fs_output_t* out) {
    if (u->wireframe <= 0) return;
    
    float d = 1e9;
    if (interpolated->barycentric.x < d) d = interpolated->barycentric.x;
    if (interpolated->barycentric.y < d) d = interpolated->barycentric.y;
    if (interpolated->barycentric.z < d) d = interpolated->barycentric.z;
    
    float width = u->wireframe_width;
    if (width <= 0) width = 0.02f;
    
    if (d < width) {
        float edge = 1.0f - (d / width);
        /* Use wireframe color. Premultiply by opacity. */
        out->color.x = out->color.x * (1.0f - edge) + (u->wireframe_color.x * u->opacity.x) * edge;
        out->color.y = out->color.y * (1.0f - edge) + (u->wireframe_color.y * u->opacity.y) * edge;
        out->color.z = out->color.z * (1.0f - edge) + (u->wireframe_color.z * u->opacity.z) * edge;
    } else if (u->wireframe == 2) {
        /* Wireframe only - transparent elsewhere */
        out->color.x = out->color.y = out->color.z = 0;
        out->opacity.x = out->opacity.y = out->opacity.z = 0;
    }
}

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
    
    apply_wireframe(u, interpolated, &out);
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
    
    apply_wireframe(u, interpolated, &out);
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
    
    apply_wireframe(u, interpolated, &out);
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
    vec4_t tex_col = spr_texture_sample((const spr_texture_t*)u->texture_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
    
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
        vec4_t spec_map = spr_texture_sample((const spr_texture_t*)u->specular_map_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
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
    
    apply_wireframe(u, interpolated, &out);
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
    
    /* Tangent */
    vec4_t t4 = {v->tangent.x, v->tangent.y, v->tangent.z, 0.0f};
    t4 = spr_mat4_mul_vec4(u->model, t4);
    out->tangent.x = t4.x; out->tangent.y = t4.y; out->tangent.z = t4.z;
    out->tangent.w = v->tangent.w;
    
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
    
    apply_wireframe(u, interpolated, &out);
    return out;
}

/* --- Full Wavefront MTL Shader --- */
spr_fs_output_t spr_shader_mtl_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_shader_uniforms_t* u = (spr_shader_uniforms_t*)user_data;
    spr_fs_output_t out;
    
    /* 1. Base Opacity */
    float alpha = u->opacity.y; /* Use Green channel as master opacity */
    if (u->opacity_map_ptr) {
        vec4_t map_d = spr_texture_sample((const spr_texture_t*)u->opacity_map_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
        alpha *= map_d.x; /* Use Red channel */
    }
    
    /* 2. Normal Mapping */
    vec3_t N = sh_normalize(interpolated->normal);
    
    if (u->normal_map_ptr) {
        vec3_t T = sh_normalize((vec3_t){interpolated->tangent.x, interpolated->tangent.y, interpolated->tangent.z});
        
        /* Gram-Schmidt re-orthogonalize T to N */
        float dot = sh_dot(N, T);
        T.x -= N.x * dot; T.y -= N.y * dot; T.z -= N.z * dot;
        T = sh_normalize(T);
        
        vec3_t B = sh_cross(N, T); /* Bitangent */
        /* Handedness flip if needed, assuming T.w stores it. OBJ usually doesn't store w, so assume 1.0 */
        
        /* Sample Normal Map (RGB -> [-1, 1]) */
        vec4_t nm = spr_texture_sample((const spr_texture_t*)u->normal_map_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
        vec3_t map_N;
        map_N.x = nm.x * 2.0f - 1.0f;
        map_N.y = nm.y * 2.0f - 1.0f;
        map_N.z = nm.z * 2.0f - 1.0f;
        
        /* Transform from Tangent Space to World Space */
        vec3_t final_N;
        final_N.x = T.x * map_N.x + B.x * map_N.y + N.x * map_N.z;
        final_N.y = T.y * map_N.x + B.y * map_N.y + N.y * map_N.z;
        final_N.z = T.z * map_N.x + B.z * map_N.y + N.z * map_N.z;
        N = sh_normalize(final_N);
    }

    vec3_t L = sh_normalize(u->light_dir);
    
    /* 3. Diffuse Component */
    vec3_t Kd = {u->color.x, u->color.y, u->color.z};
    if (u->texture_ptr) {
        vec4_t map_Kd = spr_texture_sample((const spr_texture_t*)u->texture_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
        Kd.x *= map_Kd.x; Kd.y *= map_Kd.y; Kd.z *= map_Kd.z;
    }
    
    float diff = sh_max(sh_dot(N, L), 0.0f);
    float amb = 0.1f; /* Small ambient */
    
    /* 4. Specular Component */
    vec3_t Ks = u->Ks;
    
    float spec = 0.0f;
    if (diff > 0.0f) {
        vec3_t V = {0.0f, 0.0f, 1.0f}; 
        vec3_t R = sh_reflect((vec3_t){-L.x, -L.y, -L.z}, N);
        float s = sh_max(sh_dot(R, V), 0.0f);
        
        float roughness = u->roughness;
        if (u->roughness_map_ptr) {
            vec4_t map_Ns = spr_texture_sample((const spr_texture_t*)u->roughness_map_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
            roughness *= map_Ns.x; /* Modulate roughness */
        }
        
        if (s > 0.0f) spec = powf(s, roughness);
    }
    
    if (u->specular_map_ptr) {
        vec4_t map_Ks = spr_texture_sample((const spr_texture_t*)u->specular_map_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
        Ks.x *= map_Ks.x; Ks.y *= map_Ks.y; Ks.z *= map_Ks.z;
    }
    
    /* 5. Emissive Component */
    vec3_t Ke = u->Ke;
    if (u->emissive_map_ptr) {
        vec4_t map_Ke = spr_texture_sample((const spr_texture_t*)u->emissive_map_ptr, interpolated->uv.x, interpolated->uv.y, u->stats);
        Ke.x *= map_Ke.x; Ke.y *= map_Ke.y; Ke.z *= map_Ke.z;
    }
    
    /* Combine */
    float r = Kd.x * (diff + amb) + Ks.x * spec + Ke.x;
    float g = Kd.y * (diff + amb) + Ks.y * spec + Ke.y;
    float b = Kd.z * (diff + amb) + Ks.z * spec + Ke.z;
    
    /* Output */
    out.color.x = r * alpha;
    out.color.y = g * alpha;
    out.color.z = b * alpha;
    out.opacity.x = alpha; out.opacity.y = alpha; out.opacity.z = alpha;
    
    apply_wireframe(u, interpolated, &out);
    return out;
}
