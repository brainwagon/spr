#include "spr.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#if defined(__SSE2__)
#include <immintrin.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_MATRIX_STACK 32
#define SPR_CHUNK_SIZE 4096
#define SPR_OPACITY_THRESHOLD 0.999f

typedef struct spr_fragment_t {
    float z;
    vec3_t color;
    vec3_t opacity;
    struct spr_fragment_t* next;
} spr_fragment_t;

typedef struct spr_fragment_chunk_t {
    spr_fragment_t fragments[SPR_CHUNK_SIZE];
    struct spr_fragment_chunk_t* next;
} spr_fragment_chunk_t;

typedef void (*spr_rasterize_t)(spr_context_t* ctx, const spr_vertex_out_t* v0, const spr_vertex_out_t* v1, const spr_vertex_out_t* v2);

struct spr_context_t {
    spr_framebuffer_t fb;
    
    mat4_t projection_stack[MAX_MATRIX_STACK];
    int projection_ptr;
    
    mat4_t modelview_stack[MAX_MATRIX_STACK];
    int modelview_ptr;
    
    spr_matrix_mode_enum current_mode;
    
    spr_vertex_shader_t current_vs;
    spr_fragment_shader_t current_fs;
    void* current_uniforms;

    spr_rasterize_t rasterizer_func;

    /* A-Buffer State */
    spr_fragment_t** fragment_heads; /* Array of pointers [width * height] */
    
    spr_fragment_chunk_t* chunk_head; /* Linked list of memory blocks */
    spr_fragment_t* free_list;        /* Recycled fragments */
    size_t pool_cursor;               /* Index in current chunk */

    int cull_backface;
};

/* --- Matrix Math Helpers --- */

mat4_t spr_mat4_identity(void) {
    mat4_t m;
    memset(&m, 0, sizeof(m));
    m.m[0][0] = 1.0f; m.m[1][1] = 1.0f; m.m[2][2] = 1.0f; m.m[3][3] = 1.0f;
    return m;
}

mat4_t spr_mat4_mul(mat4_t a, mat4_t b) {
    mat4_t res;
    int r, c, k;
    memset(&res, 0, sizeof(res));
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            for (k = 0; k < 4; ++k) {
                res.m[r][c] += a.m[r][k] * b.m[k][c];
            }
        }
    }
    return res;
}

/* --- Vector Helpers --- */

static vec3_t vec3_sub(vec3_t a, vec3_t b) {
    vec3_t r; r.x = a.x - b.x; r.y = a.y - b.y; r.z = a.z - b.z; return r;
}

static vec3_t vec3_normalize(vec3_t v) {
    float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len > 0) { v.x /= len; v.y /= len; v.z /= len; }
    return v;
}

static vec3_t vec3_cross(vec3_t a, vec3_t b) {
    vec3_t r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

static float vec3_dot(vec3_t a, vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec4_t spr_mat4_mul_vec4(mat4_t m, vec4_t v) {
    vec4_t r;
    r.x = m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z + m.m[0][3]*v.w;
    r.y = m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z + m.m[1][3]*v.w;
    r.z = m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z + m.m[2][3]*v.w;
    r.w = m.m[3][0]*v.x + m.m[3][1]*v.y + m.m[3][2]*v.z + m.m[3][3]*v.w;
    return r;
}

uint32_t spr_make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static float spr_min3(float a, float b, float c) {
    float m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

static float spr_max3(float a, float b, float c) {
    float m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}

static float edge_function(vec2_t a, vec2_t b, vec2_t c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

/* --- Internal Helpers --- */
static spr_fragment_t* alloc_fragment(spr_context_t* ctx) {
    /* 1. Try Free List */
    if (ctx->free_list) {
        spr_fragment_t* node = ctx->free_list;
        ctx->free_list = node->next;
        return node;
    }
    
    /* 2. Try Current Chunk */
    if (ctx->chunk_head && ctx->pool_cursor < SPR_CHUNK_SIZE) {
        return &ctx->chunk_head->fragments[ctx->pool_cursor++];
    }
    
    /* 3. Allocate New Chunk */
    spr_fragment_chunk_t* new_chunk = (spr_fragment_chunk_t*)malloc(sizeof(spr_fragment_chunk_t));
    if (!new_chunk) return NULL;
    
    new_chunk->next = ctx->chunk_head;
    ctx->chunk_head = new_chunk;
    ctx->pool_cursor = 0;
    
    return &new_chunk->fragments[ctx->pool_cursor++];
}

static void free_fragment(spr_context_t* ctx, spr_fragment_t* node) {
    if (!node) return;
    node->next = ctx->free_list;
    ctx->free_list = node;
}

static void insert_fragment(spr_context_t* ctx, int idx, float z, spr_fs_output_t out) {
    spr_fragment_t* new_frag;
    spr_fragment_t* curr;
    spr_fragment_t* prev;
    
    /* Accumulator for opacity (Front-to-Back) */
    /* Accum = Accum + (1 - Accum) * Opacity */
    vec3_t total_opacity = {0.0f, 0.0f, 0.0f};
    
    /* 1. Check for Full Occlusion before insertion point */
    curr = ctx->fragment_heads[idx];
    prev = NULL;
    
    /* Sorted Insertion: Ascending Z (Near -> Far) */
    while (curr && curr->z < z) {
        /* Update opacity with current node */
        total_opacity.x += (1.0f - total_opacity.x) * curr->opacity.x;
        total_opacity.y += (1.0f - total_opacity.y) * curr->opacity.y;
        total_opacity.z += (1.0f - total_opacity.z) * curr->opacity.z;
        
        /* Early Out: Occluded? */
        if (spr_min3(total_opacity.x, total_opacity.y, total_opacity.z) > SPR_OPACITY_THRESHOLD) {
            /* We are hidden behind existing fragments. Discard. */
            return;
        }
        
        prev = curr;
        curr = curr->next;
    }
    
    /* 2. Insert New Fragment */
    new_frag = alloc_fragment(ctx);
    if (!new_frag) return;
    
    new_frag->z = z;
    new_frag->color = out.color;
    new_frag->opacity = out.opacity;
    
    if (prev) {
        prev->next = new_frag;
    } else {
        ctx->fragment_heads[idx] = new_frag;
    }
    new_frag->next = curr;
    
    /* 3. Update Opacity with New Fragment */
    total_opacity.x += (1.0f - total_opacity.x) * new_frag->opacity.x;
    total_opacity.y += (1.0f - total_opacity.y) * new_frag->opacity.y;
    total_opacity.z += (1.0f - total_opacity.z) * new_frag->opacity.z;
    
    /* 4. Cull Fragments Behind */
    if (spr_min3(total_opacity.x, total_opacity.y, total_opacity.z) > SPR_OPACITY_THRESHOLD) {
        /* Everything after new_frag is occluded */
        spr_fragment_t* to_free = new_frag->next;
        new_frag->next = NULL;
        
        while (to_free) {
            spr_fragment_t* next = to_free->next;
            free_fragment(ctx, to_free);
            to_free = next;
        }
        return;
    }
    
    /* 5. Continue Traversal (if we didn't cull everything immediately) */
    /* Note: If we inserted, we need to check if SUBSEQUENT fragments cause occlusion */
    /* Only need to continue if there were fragments after us */
    prev = new_frag;
    curr = new_frag->next;
    
    while (curr) {
        total_opacity.x += (1.0f - total_opacity.x) * curr->opacity.x;
        total_opacity.y += (1.0f - total_opacity.y) * curr->opacity.y;
        total_opacity.z += (1.0f - total_opacity.z) * curr->opacity.z;
        
        if (spr_min3(total_opacity.x, total_opacity.y, total_opacity.z) > SPR_OPACITY_THRESHOLD) {
            /* Cull remaining */
            spr_fragment_t* to_free = curr->next;
            curr->next = NULL;
             while (to_free) {
                spr_fragment_t* next = to_free->next;
                free_fragment(ctx, to_free);
                to_free = next;
            }
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void spr_draw_triangle_2d_flat(spr_context_t* ctx, vec2_t v0, vec2_t v1, vec2_t v2, uint32_t color) {
    /* Legacy Function: Just draw opaque to buffer for debug */
    /* Not using A-Buffer here */
    int min_x, min_y, max_x, max_y;
    int x, y;
    float area;
    int width = ctx->fb.width;
    int height = ctx->fb.height;

    min_x = (int)spr_min3(v0.x, v1.x, v2.x);
    min_y = (int)spr_min3(v0.y, v1.y, v2.y);
    max_x = (int)spr_max3(v0.x, v1.x, v2.x);
    max_y = (int)spr_max3(v0.y, v1.y, v2.y);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= width) max_x = width - 1;
    if (max_y >= height) max_y = height - 1;

    area = edge_function(v0, v1, v2);
    
    for (y = min_y; y <= max_y; ++y) {
        for (x = min_x; x <= max_x; ++x) {
            vec2_t p; p.x = x + 0.5f; p.y = y + 0.5f;
            float w0 = edge_function(v1, v2, p);
            float w1 = edge_function(v2, v0, p);
            float w2 = edge_function(v0, v1, p);
            
            int inside = (area > 0) ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (inside) ctx->fb.color_buffer[y * width + x] = color;
        }
    }
}

/* --- Rasterizers (A-Buffer) --- */

static void spr_rasterize_triangle_cpu(spr_context_t* ctx, const spr_vertex_out_t* v0, const spr_vertex_out_t* v1, const spr_vertex_out_t* v2) {
    int min_x, min_y, max_x, max_y;
    int x, y;
    float area;
    int width, height;
    
    if (!ctx) return;
    width = ctx->fb.width;
    height = ctx->fb.height;

    float v0x = v0->position.x; float v0y = v0->position.y;
    float v1x = v1->position.x; float v1y = v1->position.y;
    float v2x = v2->position.x; float v2y = v2->position.y;

    min_x = (int)spr_min3(v0x, v1x, v2x);
    min_y = (int)spr_min3(v0y, v1y, v2y);
    max_x = (int)spr_max3(v0x, v1x, v2x);
    max_y = (int)spr_max3(v0y, v1y, v2y);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= width) max_x = width - 1;
    if (max_y >= height) max_y = height - 1;

    vec2_t p0 = {v0x, v0y};
    vec2_t p1 = {v1x, v1y};
    vec2_t p2 = {v2x, v2y};

    area = edge_function(p0, p1, p2);
    if (ctx->cull_backface && area < 0) return;
    if (fabs(area) < 0.0001f) return;
    
    float one_over_area = 1.0f / area;

    float step_x_w0 = v2y - v1y; float step_y_w0 = v1x - v2x;
    float step_x_w1 = v0y - v2y; float step_y_w1 = v2x - v0x;
    float step_x_w2 = v1y - v0y; float step_y_w2 = v0x - v1x;

    vec2_t start_p; start_p.x = (float)min_x + 0.5f; start_p.y = (float)min_y + 0.5f;
    float row_w0 = edge_function(p1, p2, start_p);
    float row_w1 = edge_function(p2, p0, start_p);
    float row_w2 = edge_function(p0, p1, start_p);

    if (area < 0) {
        area = -area; one_over_area = -one_over_area;
        row_w0 = -row_w0; step_x_w0 = -step_x_w0; step_y_w0 = -step_y_w0;
        row_w1 = -row_w1; step_x_w1 = -step_x_w1; step_y_w1 = -step_y_w1;
        row_w2 = -row_w2; step_x_w2 = -step_x_w2; step_y_w2 = -step_y_w2;
    }

    for (y = min_y; y <= max_y; ++y) {
        float w0 = row_w0;
        float w1 = row_w1;
        float w2 = row_w2;
        
        for (x = min_x; x <= max_x; ++x) {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float alpha = w0 * one_over_area;
                float beta = w1 * one_over_area;
                float gamma = w2 * one_over_area;
                
                float inv_w0 = v0->position.w;
                float inv_w1 = v1->position.w;
                float inv_w2 = v2->position.w;
                
                float w_recip = alpha * inv_w0 + beta * inv_w1 + gamma * inv_w2;
                float w_final = 1.0f / w_recip;
                
                float z = (v0->position.z * inv_w0 * alpha + v1->position.z * inv_w1 * beta + v2->position.z * inv_w2 * gamma) * w_final;
                
                /* No Depth Test here - Just A-Buffer Insertion */
                /* Assuming Near/Far clipping happens in vertex stage (partially) */
                if (z >= 0.0f && z <= 1.0f) {
                    spr_vertex_out_t interp;
                    interp.position.x = (float)x + 0.5f;
                    interp.position.y = (float)y + 0.5f;
                    interp.position.z = z;
                    interp.position.w = w_final;
                    
                    /* Interpolate attributes */
                    float wa = alpha * inv_w0 * w_final;
                    float wb = beta * inv_w1 * w_final;
                    float wg = gamma * inv_w2 * w_final;
                    
                    interp.color.x = v0->color.x * wa + v1->color.x * wb + v2->color.x * wg;
                    interp.color.y = v0->color.y * wa + v1->color.y * wb + v2->color.y * wg;
                    interp.color.z = v0->color.z * wa + v1->color.z * wb + v2->color.z * wg;
                    interp.color.w = v0->color.w * wa + v1->color.w * wb + v2->color.w * wg;

                    interp.uv.x = v0->uv.x * wa + v1->uv.x * wb + v2->uv.x * wg;
                    interp.uv.y = v0->uv.y * wa + v1->uv.y * wb + v2->uv.y * wg;

                    interp.normal.x = v0->normal.x * wa + v1->normal.x * wb + v2->normal.x * wg;
                    interp.normal.y = v0->normal.y * wa + v1->normal.y * wb + v2->normal.y * wg;
                    interp.normal.z = v0->normal.z * wa + v1->normal.z * wb + v2->normal.z * wg;

                    spr_fs_output_t out = ctx->current_fs(ctx->current_uniforms, &interp);
                    insert_fragment(ctx, y * width + x, z, out);
                }
            }
            w0 += step_x_w0; w1 += step_x_w1; w2 += step_x_w2;
        }
        row_w0 += step_y_w0; row_w1 += step_y_w1; row_w2 += step_y_w2;
    }
}

static void spr_rasterize_triangle_simd(spr_context_t* ctx, const spr_vertex_out_t* v0, const spr_vertex_out_t* v1, const spr_vertex_out_t* v2) {
#if defined(__SSE2__)
    int min_x, min_y, max_x, max_y;
    int y;
    float area;
    int width, height;
    
    if (!ctx) return;
    width = ctx->fb.width;
    height = ctx->fb.height;

    float v0x = v0->position.x; float v0y = v0->position.y;
    float v1x = v1->position.x; float v1y = v1->position.y;
    float v2x = v2->position.x; float v2y = v2->position.y;

    min_x = (int)spr_min3(v0x, v1x, v2x);
    min_y = (int)spr_min3(v0y, v1y, v2y);
    max_x = (int)spr_max3(v0x, v1x, v2x);
    max_y = (int)spr_max3(v0y, v1y, v2y);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= width) max_x = width - 1;
    if (max_y >= height) max_y = height - 1;

    vec2_t p0 = {v0x, v0y};
    vec2_t p1 = {v1x, v1y};
    vec2_t p2 = {v2x, v2y};

    area = edge_function(p0, p1, p2);
    if (ctx->cull_backface && area < 0) return;
    if (fabs(area) < 0.0001f) return;
    
    float one_over_area = 1.0f / area;

    float step_x_w0 = v2y - v1y; float step_y_w0 = v1x - v2x;
    float step_x_w1 = v0y - v2y; float step_y_w1 = v2x - v0x;
    float step_x_w2 = v1y - v0y; float step_y_w2 = v0x - v1x;

    vec2_t start_p; start_p.x = (float)min_x + 0.5f; start_p.y = (float)min_y + 0.5f;
    float row_w0 = edge_function(p1, p2, start_p);
    float row_w1 = edge_function(p2, p0, start_p);
    float row_w2 = edge_function(p0, p1, start_p);

    if (area < 0) {
        area = -area; one_over_area = -one_over_area;
        row_w0 = -row_w0; step_x_w0 = -step_x_w0; step_y_w0 = -step_y_w0;
        row_w1 = -row_w1; step_x_w1 = -step_x_w1; step_y_w1 = -step_y_w1;
        row_w2 = -row_w2; step_x_w2 = -step_x_w2; step_y_w2 = -step_y_w2;
    }

    __m128 v_step_x_w0 = _mm_set1_ps(step_x_w0);
    __m128 v_step_x_w1 = _mm_set1_ps(step_x_w1);
    __m128 v_step_x_w2 = _mm_set1_ps(step_x_w2);
    
    __m128 v_step_x_w0_4 = _mm_mul_ps(v_step_x_w0, _mm_set1_ps(4.0f));
    __m128 v_step_x_w1_4 = _mm_mul_ps(v_step_x_w1, _mm_set1_ps(4.0f));
    __m128 v_step_x_w2_4 = _mm_mul_ps(v_step_x_w2, _mm_set1_ps(4.0f));

    __m128 v_off_w0 = _mm_mul_ps(_mm_set_ps(3,2,1,0), v_step_x_w0);
    __m128 v_off_w1 = _mm_mul_ps(_mm_set_ps(3,2,1,0), v_step_x_w1);
    __m128 v_off_w2 = _mm_mul_ps(_mm_set_ps(3,2,1,0), v_step_x_w2);
    
    __m128 zero = _mm_setzero_ps();

    for (y = min_y; y <= max_y; ++y) {
        float w0 = row_w0;
        float w1 = row_w1;
        float w2 = row_w2;
        int x = min_x;

        __m128 v_w0 = _mm_add_ps(_mm_set1_ps(w0), v_off_w0);
        __m128 v_w1 = _mm_add_ps(_mm_set1_ps(w1), v_off_w1);
        __m128 v_w2 = _mm_add_ps(_mm_set1_ps(w2), v_off_w2);
        
        int x_end_simd = min_x + ((max_x - min_x + 1) & ~3);
        
        for (; x < x_end_simd; x += 4) {
            __m128 mask = _mm_and_ps(_mm_cmpge_ps(v_w0, zero),
                          _mm_and_ps(_mm_cmpge_ps(v_w1, zero),
                                     _mm_cmpge_ps(v_w2, zero)));
            
            int m = _mm_movemask_ps(mask);
            if (m) {
                /* We have to de-vectorize to insert nodes anyway, so keep it simple */
                float w0s[4], w1s[4], w2s[4];
                _mm_storeu_ps(w0s, v_w0);
                _mm_storeu_ps(w1s, v_w1);
                _mm_storeu_ps(w2s, v_w2);
                
                for (int i=0; i<4; ++i) {
                    if (m & (1 << i)) {
                        /* Scalar Insertion Code (Copied from CPU version logic) */
                        int px = x + i;
                        float lw0 = w0s[i]; float lw1 = w1s[i]; float lw2 = w2s[i];
                        
                        float alpha = lw0 * one_over_area;
                        float beta = lw1 * one_over_area;
                        float gamma = lw2 * one_over_area;
                        
                        float inv_w0 = v0->position.w;
                        float inv_w1 = v1->position.w;
                        float inv_w2 = v2->position.w;
                        
                        float w_recip = alpha * inv_w0 + beta * inv_w1 + gamma * inv_w2;
                        float w_final = 1.0f / w_recip;
                        
                        float z = (v0->position.z * inv_w0 * alpha + v1->position.z * inv_w1 * beta + v2->position.z * inv_w2 * gamma) * w_final;
                        
                        if (z >= 0.0f && z <= 1.0f) {
                            spr_vertex_out_t interp;
                            interp.position.x = (float)px + 0.5f;
                            interp.position.y = (float)y + 0.5f;
                            interp.position.z = z;
                            interp.position.w = w_final;
                            
                            float wa = alpha * inv_w0 * w_final;
                            float wb = beta * inv_w1 * w_final;
                            float wg = gamma * inv_w2 * w_final;
                            
                            interp.color.x = v0->color.x * wa + v1->color.x * wb + v2->color.x * wg;
                            interp.color.y = v0->color.y * wa + v1->color.y * wb + v2->color.y * wg;
                            interp.color.z = v0->color.z * wa + v1->color.z * wb + v2->color.z * wg;
                            interp.color.w = v0->color.w * wa + v1->color.w * wb + v2->color.w * wg;

                            interp.uv.x = v0->uv.x * wa + v1->uv.x * wb + v2->uv.x * wg;
                            interp.uv.y = v0->uv.y * wa + v1->uv.y * wb + v2->uv.y * wg;

                            interp.normal.x = v0->normal.x * wa + v1->normal.x * wb + v2->normal.x * wg;
                            interp.normal.y = v0->normal.y * wa + v1->normal.y * wb + v2->normal.y * wg;
                            interp.normal.z = v0->normal.z * wa + v1->normal.z * wb + v2->normal.z * wg;

                            spr_fs_output_t out = ctx->current_fs(ctx->current_uniforms, &interp);
                            insert_fragment(ctx, y * width + px, z, out);
                        }
                    }
                }
            }
            v_w0 = _mm_add_ps(v_w0, v_step_x_w0_4);
            v_w1 = _mm_add_ps(v_w1, v_step_x_w1_4);
            v_w2 = _mm_add_ps(v_w2, v_step_x_w2_4);
        }
        
        if (x < max_x + 1) {
            float offset = (float)(x - min_x);
            w0 = row_w0 + offset * step_x_w0;
            w1 = row_w1 + offset * step_x_w1;
            w2 = row_w2 + offset * step_x_w2;
        }

        for (; x <= max_x; ++x) {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float alpha = w0 * one_over_area;
                float beta = w1 * one_over_area;
                float gamma = w2 * one_over_area;
                
                float inv_w0 = v0->position.w;
                float inv_w1 = v1->position.w;
                float inv_w2 = v2->position.w;
                
                float w_recip = alpha * inv_w0 + beta * inv_w1 + gamma * inv_w2;
                float w_final = 1.0f / w_recip;
                
                float z = (v0->position.z * inv_w0 * alpha + v1->position.z * inv_w1 * beta + v2->position.z * inv_w2 * gamma) * w_final;
                
                if (z >= 0.0f && z <= 1.0f) {
                    spr_vertex_out_t interp;
                    interp.position.x = (float)x + 0.5f;
                    interp.position.y = (float)y + 0.5f;
                    interp.position.z = z;
                    interp.position.w = w_final;
                    
                    float wa = alpha * inv_w0 * w_final;
                    float wb = beta * inv_w1 * w_final;
                    float wg = gamma * inv_w2 * w_final;
                    
                    interp.color.x = v0->color.x * wa + v1->color.x * wb + v2->color.x * wg;
                    interp.color.y = v0->color.y * wa + v1->color.y * wb + v2->color.y * wg;
                    interp.color.z = v0->color.z * wa + v1->color.z * wb + v2->color.z * wg;
                    interp.color.w = v0->color.w * wa + v1->color.w * wb + v2->color.w * wg;

                    interp.uv.x = v0->uv.x * wa + v1->uv.x * wb + v2->uv.x * wg;
                    interp.uv.y = v0->uv.y * wa + v1->uv.y * wb + v2->uv.y * wg;

                    interp.normal.x = v0->normal.x * wa + v1->normal.x * wb + v2->normal.x * wg;
                    interp.normal.y = v0->normal.y * wa + v1->normal.y * wb + v2->normal.y * wg;
                    interp.normal.z = v0->normal.z * wa + v1->normal.z * wb + v2->normal.z * wg;

                    spr_fs_output_t out = ctx->current_fs(ctx->current_uniforms, &interp);
                    insert_fragment(ctx, y * width + x, z, out);
                }
            }
            w0 += step_x_w0; w1 += step_x_w1; w2 += step_x_w2;
        }
        row_w0 += step_y_w0; row_w1 += step_y_w1; row_w2 += step_y_w2;
    }
#else
    spr_rasterize_triangle_cpu(ctx, v0, v1, v2);
#endif
}

/* --- Context Init Update --- */

spr_context_t* spr_init(int width, int height) {
    spr_context_t* ctx = (spr_context_t*)malloc(sizeof(spr_context_t));
    if (!ctx) return NULL;

    ctx->fb.width = width;
    ctx->fb.height = height;
    
    ctx->fb.color_buffer = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    ctx->fb.depth_buffer = NULL; 

    /* A-Buffer Init */
    ctx->fragment_heads = (spr_fragment_t**)calloc(width * height, sizeof(spr_fragment_t*));
    
    /* Dynamic Pool Init */
    ctx->chunk_head = NULL;
    ctx->free_list = NULL;
    ctx->pool_cursor = SPR_CHUNK_SIZE; /* Force new chunk on first alloc */

    if (!ctx->fb.color_buffer || !ctx->fragment_heads) {
        if (ctx->fb.color_buffer) free(ctx->fb.color_buffer);
        if (ctx->fragment_heads) free(ctx->fragment_heads);
        /* chunks are null, nothing to free */
        free(ctx);
        return NULL;
    }

    ctx->projection_ptr = 0;
    ctx->projection_stack[0] = spr_mat4_identity();
    
    ctx->modelview_ptr = 0;
    ctx->modelview_stack[0] = spr_mat4_identity();
    
    ctx->current_mode = SPR_MODELVIEW;
    
    ctx->current_vs = NULL;
    ctx->current_fs = NULL;
    ctx->current_uniforms = NULL;

    /* Default to CPU */
    ctx->rasterizer_func = spr_rasterize_triangle_cpu;
    ctx->cull_backface = 0;

    return ctx;
}

void spr_enable_cull_face(spr_context_t* ctx, int enable) {
    if (ctx) ctx->cull_backface = enable;
}

void spr_set_rasterizer_mode(spr_context_t* ctx, spr_rasterizer_mode_t mode) {
    if (!ctx) return;
    if (mode == SPR_RASTERIZER_SIMD) {
#if defined(__SSE2__)
        ctx->rasterizer_func = spr_rasterize_triangle_simd;
#else
        printf("SPR: Warning: SIMD requested but not available. Falling back to CPU.\n");
        ctx->rasterizer_func = spr_rasterize_triangle_cpu;
#endif
    } else {
        ctx->rasterizer_func = spr_rasterize_triangle_cpu;
    }
}

/* --- Matrix API --- */

static mat4_t* get_current_matrix(spr_context_t* ctx) {
    if (ctx->current_mode == SPR_PROJECTION) return &ctx->projection_stack[ctx->projection_ptr];
    return &ctx->modelview_stack[ctx->modelview_ptr];
}


void spr_matrix_mode(spr_context_t* ctx, spr_matrix_mode_enum mode) {
    if (ctx) ctx->current_mode = mode;
}

void spr_set_program(spr_context_t* ctx, spr_vertex_shader_t vs, spr_fragment_shader_t fs, void* uniform_data) {
    if (!ctx) return;
    ctx->current_vs = vs;
    ctx->current_fs = fs;
    ctx->current_uniforms = uniform_data;
}

void spr_push_matrix(spr_context_t* ctx) {
    if (!ctx) return;
    if (ctx->current_mode == SPR_PROJECTION) {
        if (ctx->projection_ptr < MAX_MATRIX_STACK - 1) {
            ctx->projection_stack[ctx->projection_ptr + 1] = ctx->projection_stack[ctx->projection_ptr];
            ctx->projection_ptr++;
        }
    } else {
        if (ctx->modelview_ptr < MAX_MATRIX_STACK - 1) {
            ctx->modelview_stack[ctx->modelview_ptr + 1] = ctx->modelview_stack[ctx->modelview_ptr];
            ctx->modelview_ptr++;
        }
    }
}

void spr_pop_matrix(spr_context_t* ctx) {
    if (!ctx) return;
    if (ctx->current_mode == SPR_PROJECTION) {
        if (ctx->projection_ptr > 0) ctx->projection_ptr--;
    } else {
        if (ctx->modelview_ptr > 0) ctx->modelview_ptr--;
    }
}

void spr_load_identity(spr_context_t* ctx) {
    if (ctx) *get_current_matrix(ctx) = spr_mat4_identity();
}

void spr_load_matrix(spr_context_t* ctx, mat4_t m) {
    if (ctx) *get_current_matrix(ctx) = m;
}

mat4_t spr_get_modelview_matrix(spr_context_t* ctx) {
    if (ctx) return ctx->modelview_stack[ctx->modelview_ptr];
    return spr_mat4_identity();
}

mat4_t spr_get_projection_matrix(spr_context_t* ctx) {
    if (ctx) return ctx->projection_stack[ctx->projection_ptr];
    return spr_mat4_identity();
}

void spr_translate(spr_context_t* ctx, float x, float y, float z) {
    mat4_t t = spr_mat4_identity();
    mat4_t* current;
    if (!ctx) return;
    t.m[0][3] = x; t.m[1][3] = y; t.m[2][3] = z;
    current = get_current_matrix(ctx);
    *current = spr_mat4_mul(*current, t);
}

void spr_scale(spr_context_t* ctx, float x, float y, float z) {
    mat4_t s = spr_mat4_identity();
    mat4_t* current;
    if (!ctx) return;
    s.m[0][0] = x; s.m[1][1] = y; s.m[2][2] = z;
    current = get_current_matrix(ctx);
    *current = spr_mat4_mul(*current, s);
}

void spr_rotate(spr_context_t* ctx, float angle_deg, float x, float y, float z) {
    mat4_t r = spr_mat4_identity();
    mat4_t* current;
    float rad = angle_deg * (float)M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    vec3_t axis = {x, y, z};
    
    if (!ctx) return; 
    
    axis = vec3_normalize(axis);
    
    r.m[0][0] = axis.x*axis.x*(1-c) + c;
    r.m[0][1] = axis.x*axis.y*(1-c) - axis.z*s;
    r.m[0][2] = axis.x*axis.z*(1-c) + axis.y*s;
    
    r.m[1][0] = axis.y*axis.x*(1-c) + axis.z*s;
    r.m[1][1] = axis.y*axis.y*(1-c) + c;
    r.m[1][2] = axis.y*axis.z*(1-c) - axis.x*s;
    
    r.m[2][0] = axis.z*axis.x*(1-c) - axis.y*s;
    r.m[2][1] = axis.z*axis.y*(1-c) + axis.x*s;
    r.m[2][2] = axis.z*axis.z*(1-c) + c;

    current = get_current_matrix(ctx);
    *current = spr_mat4_mul(*current, r);
}

void spr_lookat(spr_context_t* ctx, vec3_t eye, vec3_t center, vec3_t up) {
    vec3_t f = vec3_normalize(vec3_sub(center, eye));
    vec3_t u = vec3_normalize(up);
    vec3_t s = vec3_normalize(vec3_cross(f, u));
    vec3_t new_u = vec3_cross(s, f);
    
    mat4_t m = spr_mat4_identity();
    mat4_t* current;
    
    m.m[0][0] = s.x; m.m[0][1] = s.y; m.m[0][2] = s.z;
    m.m[1][0] = new_u.x; m.m[1][1] = new_u.y; m.m[1][2] = new_u.z;
    m.m[2][0] = -f.x; m.m[2][1] = -f.y; m.m[2][2] = -f.z;
    
    m.m[0][3] = -vec3_dot(s, eye);
    m.m[1][3] = -vec3_dot(new_u, eye);
    m.m[2][3] = vec3_dot(f, eye);

    if (!ctx) return;
    current = get_current_matrix(ctx);
    *current = spr_mat4_mul(*current, m);
}

void spr_perspective(spr_context_t* ctx, float fov_deg, float aspect, float near, float far) {
    float f = 1.0f / tanf((fov_deg * 0.5f) * (float)M_PI / 180.0f);
    mat4_t m = spr_mat4_identity();
    mat4_t* current;
    
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (far + near) / (near - far);
    m.m[2][3] = (2 * far * near) / (near - far);
    m.m[3][2] = -1.0f;
    m.m[3][3] = 0.0f;

    if (!ctx) return;
    current = get_current_matrix(ctx);
    *current = spr_mat4_mul(*current, m);
}

void spr_shutdown(spr_context_t* ctx) {
    if (ctx) {
        if (ctx->fb.color_buffer) free(ctx->fb.color_buffer);
        if (ctx->fragment_heads) free(ctx->fragment_heads);
        
        /* Free Chunks */
        spr_fragment_chunk_t* chunk = ctx->chunk_head;
        while (chunk) {
            spr_fragment_chunk_t* next = chunk->next;
            free(chunk);
            chunk = next;
        }
        
        free(ctx);
    }
}

void spr_clear(spr_context_t* ctx, uint32_t color, float depth) {
    int pixel_count;
    int i;
    (void)depth; /* Depth not used for clear in A-buffer */
    
    if (!ctx) return;

    pixel_count = ctx->fb.width * ctx->fb.height;

    /* Fill background */
    for (i = 0; i < pixel_count; ++i) {
        ctx->fb.color_buffer[i] = color;
    }
    
    /* Reset A-Buffer Head Pointers */
    /* We DO NOT free fragments here to keep them hot in the free list/pool */
    /* We just clear the heads, effectively "freeing" the linked lists into the void? */
    /* Wait, if we just clear heads, the fragments are leaked (lost reference). */
    /* We MUST return them to the free list or reset the pool cursor. */
    
    /* Strategy: Reset Pool? */
    /* If we use chunks + free list, "Reset" is complex because the list is scattered. */
    /* Option 1: Walk every head and free_fragment() every node. (Slow but correct for free list) */
    /* Option 2: Just reset pool_cursor = 0 and free_list = NULL and reuse chunks from scratch? */
    /* If we do Option 2, we must ensure we don't leak "next" pointers in the chunks? No, if we treat chunks as raw memory, it's fine. */
    /* However, Option 2 is much faster: O(1) vs O(Pixels * Fragments). */
    /* Let's go with Option 2: "Reset World". */
    /* We keep the chunks allocated, but treat them as empty. */
    
    memset(ctx->fragment_heads, 0, pixel_count * sizeof(spr_fragment_t*));
    
    /* Reset allocator */
    ctx->free_list = NULL;
    ctx->pool_cursor = 0;
    /* Note: We reuse the *first* chunk. What about subsequent chunks? */
    /* If we reset pool_cursor=0, we are only reusing the *first* chunk pointed to by chunk_head? */
    /* No, chunk_head points to the *latest* chunk (LIFO). */
    /* If we reset pool_cursor to 0, we start overwriting the *latest* chunk. */
    /* The other chunks are still linked via chunk_head->next. */
    /* This implies we only reuse the *latest* chunk and "leak" the capacity of older chunks until shutdown? */
    /* Or do we want to free all chunks except one? */
    
    /* Better approach for clear: */
    /* 1. Free all chunks except one (or keep them all but reset cursor logic?) */
    /* Keeping them all but managing cursors for *each* chunk is complex. */
    /* Simplest correct way: Free all chunks except head, reset head. */
    
    /* Let's iterate and free 'next' chunks, keeping 'chunk_head' as the only one. */
    if (ctx->chunk_head) {
        spr_fragment_chunk_t* c = ctx->chunk_head->next;
        while (c) {
            spr_fragment_chunk_t* n = c->next;
            free(c);
            c = n;
        }
        ctx->chunk_head->next = NULL;
        ctx->pool_cursor = 0;
    }
}

uint32_t* spr_get_color_buffer(spr_context_t* ctx) {
    if (!ctx) return NULL;
    return ctx->fb.color_buffer;
}

int spr_get_width(spr_context_t* ctx) {
    return ctx ? ctx->fb.width : 0;
}

int spr_get_height(spr_context_t* ctx) {
    return ctx ? ctx->fb.height : 0;
}

void spr_draw_triangle_simple(spr_context_t* ctx, vec3_t v0, vec3_t v1, vec3_t v2, uint32_t color) {
    /* Legacy fallback using old drawer if needed, but we removed it? */
    /* Let's redirect to generic drawer with a basic shader if really needed, or just stub it out. */
    /* For now, keeping it stubbed or updating it to use A-buffer manually would be tedious. */
    /* I'll leave it as is, but it calls spr_draw_triangle_2d_flat which writes DIRECTLY to buffer. */
    /* This will bypass A-buffer. That's fine for debug/test. */
    
    mat4_t mv = ctx->modelview_stack[ctx->modelview_ptr];
    mat4_t p = ctx->projection_stack[ctx->projection_ptr];
    mat4_t mvp = spr_mat4_mul(p, mv);
    
    vec4_t p0, p1, p2;
    float w, h;
    vec2_t s0, s1, s2;

    p0.x = v0.x; p0.y = v0.y; p0.z = v0.z; p0.w = 1.0f;
    p1.x = v1.x; p1.y = v1.y; p1.z = v1.z; p1.w = 1.0f;
    p2.x = v2.x; p2.y = v2.y; p2.z = v2.z; p2.w = 1.0f;
    
    p0 = spr_mat4_mul_vec4(mvp, p0);
    p1 = spr_mat4_mul_vec4(mvp, p1);
    p2 = spr_mat4_mul_vec4(mvp, p2);
    
    if (p0.w <= 0.001f || p1.w <= 0.001f || p2.w <= 0.001f) return;

    p0.x /= p0.w; p0.y /= p0.w; p0.z /= p0.w;
    p1.x /= p1.w; p1.y /= p1.w; p1.z /= p1.w;
    p2.x /= p2.w; p2.y /= p2.w; p2.z /= p2.w;

    w = (float)ctx->fb.width;
    h = (float)ctx->fb.height;
    
    s0.x = (p0.x + 1.0f) * 0.5f * w;
    s0.y = (1.0f - p0.y) * 0.5f * h;
    
    s1.x = (p1.x + 1.0f) * 0.5f * w;
    s1.y = (1.0f - p1.y) * 0.5f * h;
    
    s2.x = (p2.x + 1.0f) * 0.5f * w;
    s2.y = (1.0f - p2.y) * 0.5f * h;

    spr_draw_triangle_2d_flat(ctx, s0, s1, s2, color);
}

void spr_draw_triangles(spr_context_t* ctx, int count, const void* vertices, size_t stride) {
    if (!ctx || !ctx->current_vs || !ctx->current_fs || !ctx->rasterizer_func) return; 
    
    int i;
    const uint8_t* v_ptr = (const uint8_t*)vertices;
    
    for (i = 0; i < count; ++i) {
        spr_vertex_out_t v0, v1, v2;
        
        ctx->current_vs(ctx->current_uniforms, v_ptr, &v0); v_ptr += stride;
        ctx->current_vs(ctx->current_uniforms, v_ptr, &v1); v_ptr += stride;
        ctx->current_vs(ctx->current_uniforms, v_ptr, &v2); v_ptr += stride;
        
        if (v0.position.w <= 0.001f || v1.position.w <= 0.001f || v2.position.w <= 0.001f) continue; 
        
        float inv_w0 = 1.0f / v0.position.w;
        float inv_w1 = 1.0f / v1.position.w;
        float inv_w2 = 1.0f / v2.position.w;
        
        v0.position.x *= inv_w0; v0.position.y *= inv_w0; v0.position.z *= inv_w0;
        v1.position.x *= inv_w1; v1.position.y *= inv_w1; v1.position.z *= inv_w1;
        v2.position.x *= inv_w2; v2.position.y *= inv_w2; v2.position.z *= inv_w2;
        
        float w = (float)ctx->fb.width;
        float h = (float)ctx->fb.height;
        
        v0.position.x = (v0.position.x + 1.0f) * 0.5f * w; v0.position.y = (1.0f - v0.position.y) * 0.5f * h;
        v1.position.x = (v1.position.x + 1.0f) * 0.5f * w; v1.position.y = (1.0f - v1.position.y) * 0.5f * h;
        v2.position.x = (v2.position.x + 1.0f) * 0.5f * w; v2.position.y = (1.0f - v2.position.y) * 0.5f * h;
        
        v0.position.w = inv_w0;
        v1.position.w = inv_w1;
        v2.position.w = inv_w2;
        
        ctx->rasterizer_func(ctx, &v0, &v1, &v2);
    }
}

/* --- Resolve --- */

void spr_resolve(spr_context_t* ctx) {
    if (!ctx) return;
    int count = ctx->fb.width * ctx->fb.height;
    int i;
    
    for (i = 0; i < count; ++i) {
        spr_fragment_t* head = ctx->fragment_heads[i];
        if (!head) continue; /* Keep background */
        
        /* List is already sorted Near-to-Far (Ascending Z) by insert_fragment */
        
        /* Extract Background from buffer */
        uint32_t bg_packed = ctx->fb.color_buffer[i];
        float bg_r = (bg_packed & 0xFF) / 255.0f;
        float bg_g = ((bg_packed >> 8) & 0xFF) / 255.0f;
        float bg_b = ((bg_packed >> 16) & 0xFF) / 255.0f;
        
        /* Front-to-Back Accumulation */
        /* acc_color: Accumulated color of the layers */
        /* acc_opacity: Accumulated opacity of the layers */
        vec3_t acc_color = {0.0f, 0.0f, 0.0f};
        vec3_t acc_opacity = {0.0f, 0.0f, 0.0f};
        
        spr_fragment_t* curr = head;
        while (curr) {
            /* Front-to-Back: */
            /* C_dst = C_dst + (1 - A_dst) * C_src */
            /* A_dst = A_dst + (1 - A_dst) * A_src */
            
            float inv_op_r = 1.0f - acc_opacity.x;
            float inv_op_g = 1.0f - acc_opacity.y;
            float inv_op_b = 1.0f - acc_opacity.z;
            
            acc_color.x += inv_op_r * curr->color.x;
            acc_color.y += inv_op_g * curr->color.y;
            acc_color.z += inv_op_b * curr->color.z;
            
            acc_opacity.x += inv_op_r * curr->opacity.x;
            acc_opacity.y += inv_op_g * curr->opacity.y;
            acc_opacity.z += inv_op_b * curr->opacity.z;
            
            /* Early Exit if fully opaque */
            if (spr_min3(acc_opacity.x, acc_opacity.y, acc_opacity.z) > SPR_OPACITY_THRESHOLD) {
                break;
            }
            
            curr = curr->next;
        }
        
        /* Composite with Background */
        /* Final = AccColor + Background * (1 - AccOpacity) */
        float final_r = acc_color.x + bg_r * (1.0f - acc_opacity.x);
        float final_g = acc_color.y + bg_g * (1.0f - acc_opacity.y);
        float final_b = acc_color.z + bg_b * (1.0f - acc_opacity.z);
        
        /* Clamp */
        if (final_r > 1.0f) final_r = 1.0f;
        if (final_g > 1.0f) final_g = 1.0f;
        if (final_b > 1.0f) final_b = 1.0f;
        
        ctx->fb.color_buffer[i] = spr_make_color(
            (uint8_t)(final_r * 255.0f),
            (uint8_t)(final_g * 255.0f),
            (uint8_t)(final_b * 255.0f),
            255
        );
    }
}
