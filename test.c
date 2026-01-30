#include "spr.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void write_ppm(const char* filename, int width, int height, const uint32_t* buffer) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to open file for writing");
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; ++i) {
        uint32_t pixel = buffer[i];
        uint8_t r = (pixel) & 0xFF;
        uint8_t g = (pixel >> 8) & 0xFF;
        uint8_t b = (pixel >> 16) & 0xFF;
        fputc(r, f);
        fputc(g, f);
        fputc(b, f);
    }
    fclose(f);
    printf("Wrote %s\n", filename);
}

/* User Data */
typedef struct {
    float x, y, z;
    float r, g, b;
} my_vertex_t;

typedef struct {
    mat4_t mvp;
    mat4_t model_view; /* For lighting if we wanted */
} my_uniforms_t;

/* Vertex Shader */
void my_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    my_uniforms_t* uniforms = (my_uniforms_t*)user_data;
    const my_vertex_t* v = (const my_vertex_t*)input_vertex;
    
    vec4_t pos;
    pos.x = v->x; pos.y = v->y; pos.z = v->z; pos.w = 1.0f;
    
    /* Transform to Clip Space */
    out->position = spr_mat4_mul_vec4(uniforms->mvp, pos);
    
    /* Pass Color */
    out->color.x = v->r;
    out->color.y = v->g;
    out->color.z = v->b;
    out->color.w = 1.0f;
    
    /* Pass Attributes (Normal/UV) if we had them */
    out->uv.x = 0.0f; out->uv.y = 0.0f;
    out->normal.x = 0.0f; out->normal.y = 0.0f; out->normal.z = 0.0f;
}

/* Fragment Shader */
spr_color_t my_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    spr_color_t c;
    /* Basic pass-through of interpolated color */
    /* Clamp to 0-255 */
    c.r = (uint8_t)(interpolated->color.x * 255.0f);
    c.g = (uint8_t)(interpolated->color.y * 255.0f);
    c.b = (uint8_t)(interpolated->color.z * 255.0f);
    c.a = 255;
    return c;
}

int main() {
    int width = 800;
    int height = 600;

    spr_context_t* ctx = spr_init(width, height);
    if (!ctx) return 1;

    /* Clear */
    uint32_t clear_color = spr_make_color(50, 50, 50, 255); /* Dark Grey */
    spr_clear(ctx, clear_color, 1.0f);

    /* Setup Transforms */
    spr_matrix_mode(ctx, SPR_PROJECTION);
    spr_load_identity(ctx);
    spr_perspective(ctx, 45.0f, (float)width / (float)height, 0.1f, 100.0f);

    spr_matrix_mode(ctx, SPR_MODELVIEW);
    spr_load_identity(ctx);
    
    vec3_t eye = {0.0f, 2.0f, 4.0f};
    vec3_t center = {0.0f, 0.0f, 0.0f};
    vec3_t up = {0.0f, 1.0f, 0.0f};
    spr_lookat(ctx, eye, center, up);

    /* Rotate object */
    spr_rotate(ctx, 30.0f, 0.0f, 1.0f, 0.0f);

    /* Prepare Uniforms */
    my_uniforms_t uniforms;
    mat4_t p = spr_get_projection_matrix(ctx);
    mat4_t mv = spr_get_modelview_matrix(ctx);
    uniforms.mvp = spr_mat4_mul(p, mv);
    
    /* Setup Program */
    spr_set_program(ctx, my_vs, my_fs, &uniforms);

    /* Geometry: A simple colored pyramid */
    my_vertex_t vertices[] = {
        /* Base (Red/Green quad split into 2 triangles) */
        /* Tri 1 */
        { -1.0f, 0.0f, -1.0f,   1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, -1.0f,    0.0f, 1.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f,     0.0f, 0.0f, 1.0f },
        
        /* Tri 2 */
        { -1.0f, 0.0f, -1.0f,   1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f,     0.0f, 0.0f, 1.0f },
        { -1.0f, 0.0f, 1.0f,    1.0f, 1.0f, 0.0f },

        /* Sides (meeting at top 0, 1.5, 0) */
        /* Front */
        { -1.0f, 0.0f, 1.0f,    1.0f, 1.0f, 0.0f },
        { 1.0f, 0.0f, 1.0f,     0.0f, 0.0f, 1.0f },
        { 0.0f, 1.5f, 0.0f,     1.0f, 1.0f, 1.0f }, /* White Top */
        
        /* Right */
        { 1.0f, 0.0f, 1.0f,     0.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, -1.0f,    0.0f, 1.0f, 0.0f },
        { 0.0f, 1.5f, 0.0f,     1.0f, 1.0f, 1.0f },
        
        /* Back */
        { 1.0f, 0.0f, -1.0f,    0.0f, 1.0f, 0.0f },
        { -1.0f, 0.0f, -1.0f,   1.0f, 0.0f, 0.0f },
        { 0.0f, 1.5f, 0.0f,     1.0f, 1.0f, 1.0f },
        
        /* Left */
        { -1.0f, 0.0f, -1.0f,   1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 1.0f,    1.0f, 1.0f, 0.0f },
        { 0.0f, 1.5f, 0.0f,     1.0f, 1.0f, 1.0f },
    };

    /* Draw */
    spr_draw_triangles(ctx, 18 / 3, vertices, sizeof(my_vertex_t));

    write_ppm("output_final.ppm", width, height, spr_get_color_buffer(ctx));
    spr_shutdown(ctx);
    return 0;
}