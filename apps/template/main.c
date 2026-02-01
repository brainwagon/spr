#include <SDL2/SDL.h>
#include "spr.h"
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    int width = 800;
    int height = 600;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("SPR Template", 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(renderer, 
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 
        width, height);

    spr_context_t* ctx = spr_init(width, height);
    
    /* Simple Cube Data */
    float vertices[] = {
        /* Front */
        -1,-1, 1,  1,-1, 1,  1, 1, 1,
        -1,-1, 1,  1, 1, 1, -1, 1, 1,
        /* Back */
         1,-1,-1, -1,-1,-1, -1, 1,-1,
         1,-1,-1, -1, 1,-1,  1, 1,-1,
        /* Top */
        -1, 1, 1,  1, 1, 1,  1, 1,-1,
        -1, 1, 1,  1, 1,-1, -1, 1,-1,
        /* Bottom */
        -1,-1,-1,  1,-1,-1,  1,-1, 1,
        -1,-1,-1,  1,-1, 1, -1,-1, 1,
        /* Right */
         1,-1, 1,  1,-1,-1,  1, 1,-1,
         1,-1, 1,  1, 1,-1,  1, 1, 1,
        /* Left */
        -1,-1,-1, -1,-1, 1, -1, 1, 1,
        -1,-1,-1, -1, 1, 1, -1, 1,-1
    };
    /* Colors (per vertex) - just random-ish */
    /* SPR expects spr_vertex_out_t or custom shader. 
       Let's use SHADER_CONSTANT for simplicity and provide colors via uniforms?
       Or use a simple shader that uses vertex colors?
       The default spr_vertex_t has pos, norm, uv.
       Our data is just pos.
       We need a custom vertex struct or reuse spr_vertex_t but stride it correctly.
       Let's define a simple vertex struct here and a simple shader.
    */
    
    /* Shader Logic */
    spr_matrix_mode(ctx, SPR_PROJECTION);
    spr_load_identity(ctx);
    spr_perspective(ctx, 60.0f, (float)width/(float)height, 0.1f, 100.0f);
    
    int running = 1;
    float rot_x = 0;
    float rot_y = 0;
    int last_mx = 0, last_my = 0;
    int down = 0;
    
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_MOUSEBUTTONDOWN) { down = 1; last_mx = e.button.x; last_my = e.button.y; }
            else if (e.type == SDL_MOUSEBUTTONUP) { down = 0; }
            else if (e.type == SDL_MOUSEMOTION && down) {
                rot_y += (e.motion.x - last_mx);
                rot_x += (e.motion.y - last_my);
                last_mx = e.motion.x; last_my = e.motion.y;
            }
        }
        
        spr_clear(ctx, 0xFF303030, 1.0f);
        
        spr_matrix_mode(ctx, SPR_MODELVIEW);
        spr_load_identity(ctx);
        spr_translate(ctx, 0, 0, -5);
        spr_rotate(ctx, rot_x, 1, 0, 0);
        spr_rotate(ctx, rot_y, 0, 1, 0);
        
        /* Use Fixed Function Simple Draw for template (if available) or setup a shader */
        /* spr_draw_triangle_simple is deprecated/hidden?
           Let's use spr_draw_triangles with a built-in shader (Constant).
           But Constant shader needs spr_vertex_out_t or stl_vertex_t?
           spr_shaders.h defines Constant shader assuming stl_vertex_t input (legacy).
           Actually, spr_shader_constant_vs casts input to stl_vertex_t.
           Let's mimic stl_vertex_t layout: float x,y,z, nx,ny,nz, u,v; uint16 attr;
           Or write a minimal shader here.
        */
        
        /* Minimal Shader */
        /* We can't easily define a callback here without exposing types?
           spr.h exposes spr_vertex_out_t. So yes we can.
        */
        
        /* ... drawing code ... */
        /* For simplicity, let's just draw 2D primitive for the template? 
           No, user asked for "draw a cube... with rotation". Must be 3D.
        */
        
        /* Let's construct a compatible vertex array */
        /* Using spr_vertex_t (pos, norm, uv, tan) */
        /* We will just fill pos. */
        
        spr_vertex_t cube_verts[36];
        for (int i=0; i<36; ++i) {
            cube_verts[i].position.x = vertices[i*3+0];
            cube_verts[i].position.y = vertices[i*3+1];
            cube_verts[i].position.z = vertices[i*3+2];
            
            /* Simple Face Normals */
            if (i < 6)       { cube_verts[i].normal = (vec3_t){ 0, 0, 1}; } /* Front */
            else if (i < 12) { cube_verts[i].normal = (vec3_t){ 0, 0,-1}; } /* Back */
            else if (i < 18) { cube_verts[i].normal = (vec3_t){ 0, 1, 0}; } /* Top */
            else if (i < 24) { cube_verts[i].normal = (vec3_t){ 0,-1, 0}; } /* Bottom */
            else if (i < 30) { cube_verts[i].normal = (vec3_t){ 1, 0, 0}; } /* Right */
            else             { cube_verts[i].normal = (vec3_t){-1, 0, 0}; } /* Left */
            
            cube_verts[i].uv.x = 0;
        }
        
        /* Use Matte Shader (requires uniforms) */
        /* Or Constant Shader. */
        /* Let's use Matte shader from library. */
        #include "spr_shaders.h"
        
        spr_shader_uniforms_t u = {0};
        u.mvp = spr_mat4_mul(spr_get_projection_matrix(ctx), spr_get_modelview_matrix(ctx));
        u.model = spr_get_modelview_matrix(ctx);
        u.color = (vec4_t){0.0f, 0.8f, 1.0f, 1.0f};
        u.opacity = (vec3_t){1.0f, 1.0f, 1.0f};
        u.light_dir = (vec3_t){0.0f, 0.0f, 1.0f};
        
        spr_set_program(ctx, spr_shader_matte_vs, spr_shader_matte_fs, &u);
        
        /* Draw */
        /* Matte VS expects stl_vertex_t? Checking spr_shaders.c ...
           Yes: const stl_vertex_t* v = (const stl_vertex_t*)input_vertex;
           This is a problem. The shaders are coupled to STL vertex format or SPR_VERTEX_T format depending on shader.
           spr_shader_textured_vs expects spr_vertex_t.
           spr_shader_matte_vs expects stl_vertex_t.
           
           I should update spr_shaders.c to have a "standard" VS that takes spr_vertex_t and does simple transforms.
           spr_shader_textured_vs does exactly that!
           So I can use spr_shader_textured_vs + spr_shader_matte_fs.
        */
        
        spr_set_program(ctx, spr_shader_textured_vs, spr_shader_matte_fs, &u);
        spr_draw_triangles(ctx, 36, cube_verts, sizeof(spr_vertex_t));
        
        spr_resolve(ctx);
        
        void* pixels; int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, spr_get_color_buffer(ctx), width * height * 4);
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    
    spr_shutdown(ctx);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
