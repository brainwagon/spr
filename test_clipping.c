#include "spr.h"
#include "spr_shaders.h"
#include <stdio.h>
#include <stdlib.h>

/* Simple vertex for testing */
typedef struct {
    float x, y, z;
    float nx, ny, nz;
    uint16_t attr;
} test_vertex_t;

int main() {
    int width = 100;
    int height = 100;
    spr_context_t* ctx = spr_init(width, height);
    
    /* Set up shaders */
    spr_shader_uniforms_t u;
    spr_matrix_mode(ctx, SPR_PROJECTION);
    spr_load_identity(ctx);
    /* Near plane at 1.0 */
    spr_perspective(ctx, 90.0f, 1.0f, 1.0f, 100.0f);
    
    spr_matrix_mode(ctx, SPR_MODELVIEW);
    spr_load_identity(ctx);
    
    u.mvp = spr_mat4_mul(spr_get_projection_matrix(ctx), spr_get_modelview_matrix(ctx));
    spr_set_program(ctx, spr_shader_constant_vs, spr_shader_constant_fs, &u);
    spr_uniforms_set_color(&u, 1.0f, 1.0f, 1.0f, 1.0f);
    spr_uniforms_set_opacity(&u, 1.0f, 1.0f, 1.0f);

    /* Test 1: Triangle entirely in front of near plane (z = -2) */
    test_vertex_t v_front[] = {
        { 0.0f,  0.5f, -2.0f, 0,0,0, 0 },
        {-0.5f, -0.5f, -2.0f, 0,0,0, 0 },
        { 0.5f, -0.5f, -2.0f, 0,0,0, 0 }
    };
    
    spr_clear(ctx, 0, 1.0f);
    spr_draw_triangles(ctx, 1, v_front, sizeof(test_vertex_t));
    spr_resolve(ctx);
    
    int hits = 0;
    uint32_t* buf = spr_get_color_buffer(ctx);
    for (int i=0; i<width*height; ++i) if (buf[i] != 0) hits++;
    printf("Triangle at z=-2: %d pixels\n", hits);

    /* Test 2: Triangle crossing near plane (z ranges from -2 to 0) */
    /* Vertex 2 is at z=0, which is BEHIND the near plane (near=1.0, so z must be <= -1.0) */
    test_vertex_t v_cross[] = {
        { 0.0f,  0.5f, -2.0f, 0,0,0, 0 },
        {-0.5f, -0.5f, -2.0f, 0,0,0, 0 },
        { 0.5f, -0.5f,  0.0f, 0,0,0, 0 }
    };
    
    spr_clear(ctx, 0, 1.0f);
    spr_draw_triangles(ctx, 1, v_cross, sizeof(test_vertex_t));
    spr_resolve(ctx);
    
    hits = 0;
    buf = spr_get_color_buffer(ctx);
    for (int i=0; i<width*height; ++i) if (buf[i] != 0) hits++;
    printf("Triangle crossing z=-1: %d pixels\n", hits);

    spr_shutdown(ctx);
    return 0;
}
