#include <SDL2/SDL.h>
#include "spr.h"
#include "spr_shaders.h"
#include "stl.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/* --- Window Settings --- */
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

/* --- Interaction State --- */
typedef struct {
    float rot_x;
    float rot_y;
    float distance;
    float pan_x;
    float pan_y;
    int mouse_down_left;
    int mouse_down_right;
    int last_mouse_x;
    int last_mouse_y;
} view_state_t;

/* --- Math Helpers --- */
static float sh_dot(vec3_t a, vec3_t b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static vec3_t sh_normalize(vec3_t v) {
    float len = sqrtf(sh_dot(v, v));
    if (len > 0) { v.x/=len; v.y/=len; v.z/=len; }
    return v;
}

/* Shader Enum */
typedef enum {
    SHADER_CONSTANT,
    SHADER_MATTE,
    SHADER_PLASTIC,
    SHADER_METAL
} shader_type_t;

const char* get_shader_name(shader_type_t s) {
    switch(s) {
        case SHADER_CONSTANT: return "Constant";
        case SHADER_MATTE: return "Matte";
        case SHADER_PLASTIC: return "Plastic";
        case SHADER_METAL: return "Metal";
        default: return "Unknown";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <stl_file> [-simd | -cpu]\n", argv[0]);
        return 1;
    }

    const char* filename = NULL;
    spr_rasterizer_mode_t mode = SPR_RASTERIZER_CPU; /* Default to CPU */

    /* Parse Args */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-simd") == 0) {
            mode = SPR_RASTERIZER_SIMD;
        } else if (strcmp(argv[i], "-cpu") == 0) {
            mode = SPR_RASTERIZER_CPU;
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        }
    }

    if (!filename) {
        printf("Error: No STL file specified.\n");
        return 1;
    }

    if (mode == SPR_RASTERIZER_SIMD) {
        printf("Mode: SIMD (if available)\n");
    } else {
        printf("Mode: CPU\n");
    }

    /* Load STL */
    printf("Loading %s...\n", filename);
    stl_object_t* mesh = stl_load(filename);
    if (!mesh) {
        printf("Failed to load mesh.\n");
        return 1;
    }
    
    /* Calculate Bounds for auto-centering */
    float minx=1e9, miny=1e9, minz=1e9;
    float maxx=-1e9, maxy=-1e9, maxz=-1e9;
    for (int i=0; i<mesh->vertex_count; ++i) {
        if (mesh->vertices[i].x < minx) minx = mesh->vertices[i].x;
        if (mesh->vertices[i].x > maxx) maxx = mesh->vertices[i].x;
        if (mesh->vertices[i].y < miny) miny = mesh->vertices[i].y;
        if (mesh->vertices[i].y > maxy) maxy = mesh->vertices[i].y;
        if (mesh->vertices[i].z < minz) minz = mesh->vertices[i].z;
        if (mesh->vertices[i].z > maxz) maxz = mesh->vertices[i].z;
    }
    float cx = (minx+maxx)*0.5f;
    float cy = (miny+maxy)*0.5f;
    float cz = (minz+maxz)*0.5f;
    float size = (maxx-minx);
    if ((maxy-miny) > size) size = (maxy-miny);
    if ((maxz-minz) > size) size = (maxz-minz);
    if (size <= 0.0001f) size = 1.0f;
    
    printf("Bounds: [%.2f, %.2f] [%.2f, %.2f] [%.2f, %.2f]\n", minx, maxx, miny, maxy, minz, maxz);
    printf("Center: %.2f %.2f %.2f, Size: %.2f\n", cx, cy, cz, size);

    /* Init SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow("SPR STL Viewer", 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
        
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(renderer, 
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 
        WINDOW_WIDTH, WINDOW_HEIGHT);

    /* Init SPR */
    spr_context_t* ctx = spr_init(WINDOW_WIDTH, WINDOW_HEIGHT);
    spr_set_rasterizer_mode(ctx, mode);
    
    /* View State */
    view_state_t view = {0};
    view.distance = size * 2.0f;
    view.rot_x = 0.0f;
    view.rot_y = 0.0f;
    view.pan_x = 0.0f;
    view.pan_y = 0.0f;
    
    shader_type_t current_shader = SHADER_PLASTIC;
    
    /* FPS State */
    int show_fps = 1;
    int frame_count = 0;
    int current_fps = 0;
    int color_mode = 0; /* 0: Grey, 1: Red */
    double current_render_ms = 0.0;
    double accumulated_render_ms = 0.0;
    uint32_t last_time = SDL_GetTicks();
    uint64_t perf_freq = SDL_GetPerformanceFrequency();

    /* Main Loop */
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = 0; break;
                    case SDLK_f: show_fps = !show_fps; break;
                    case SDLK_c: color_mode = !color_mode; break;
                    case SDLK_1: current_shader = SHADER_CONSTANT; break;
                    case SDLK_2: current_shader = SHADER_MATTE; break;
                    case SDLK_3: current_shader = SHADER_PLASTIC; break;
                    case SDLK_4: current_shader = SHADER_METAL; break;
                }
                
                /* Immediate title update if paused logic existed, but we redraw anyway */
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) view.mouse_down_left = 1;
                if (e.button.button == SDL_BUTTON_RIGHT) view.mouse_down_right = 1;
                view.last_mouse_x = e.button.x;
                view.last_mouse_y = e.button.y;
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) view.mouse_down_left = 0;
                if (e.button.button == SDL_BUTTON_RIGHT) view.mouse_down_right = 0;
            }
            else if (e.type == SDL_MOUSEMOTION) {
                int dx = e.motion.x - view.last_mouse_x;
                int dy = e.motion.y - view.last_mouse_y;
                view.last_mouse_x = e.motion.x;
                view.last_mouse_y = e.motion.y;
                
                if (view.mouse_down_left) {
                    view.rot_y += dx * 0.5f;
                    view.rot_x += dy * 0.5f;
                }
                if (view.mouse_down_right) {
                    view.pan_x += dx * (size * 0.002f);
                    view.pan_y -= dy * (size * 0.002f);
                }
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                view.distance -= e.wheel.y * (size * 0.1f);
                if (view.distance < size * 0.1f) view.distance = size * 0.1f;
            }
        }
        
        /* Render SPR */
        uint64_t start_time = SDL_GetPerformanceCounter();

        uint32_t clear_col = spr_make_color(30, 30, 30, 255);
        spr_clear(ctx, clear_col, 1.0f);
        
        spr_matrix_mode(ctx, SPR_PROJECTION);
        spr_load_identity(ctx);
        spr_perspective(ctx, 45.0f, (float)WINDOW_WIDTH/(float)WINDOW_HEIGHT, size*0.01f, size*10.0f);
        
        spr_matrix_mode(ctx, SPR_MODELVIEW);
        spr_load_identity(ctx);
        
        /* Camera Setup */
        vec3_t eye = {0, 0, view.distance};
        vec3_t center = {0, 0, 0};
        vec3_t up = {0, 1, 0};
        spr_lookat(ctx, eye, center, up);
        
        /* Model Transforms */
        spr_translate(ctx, view.pan_x, view.pan_y, 0);
        spr_rotate(ctx, view.rot_x, 1, 0, 0);
        spr_rotate(ctx, view.rot_y, 0, 1, 0);
        spr_translate(ctx, -cx, -cy, -cz); /* Center model */

        /* Uniforms */
        spr_shader_uniforms_t u;
        u.mvp = spr_mat4_mul(spr_get_projection_matrix(ctx), spr_get_modelview_matrix(ctx));
        u.model = spr_get_modelview_matrix(ctx);
                u.light_dir = (vec3_t){0.5f, 1.0f, 1.0f}; 
                u.light_dir = sh_normalize(u.light_dir);
                u.eye_pos = eye; 
                
                if (color_mode == 0) {
                    spr_uniforms_set_color(&u, 0.7f, 0.7f, 0.7f, 1.0f); /* Grey */
                } else {
                    spr_uniforms_set_color(&u, 0.8f, 0.2f, 0.2f, 1.0f); /* Red */
                }
                
                u.roughness = 32.0f;
        
                switch (current_shader) {
                    case SHADER_CONSTANT:
                        spr_set_program(ctx, spr_shader_constant_vs, spr_shader_constant_fs, &u);
                        break;
                    case SHADER_MATTE:
                        spr_set_program(ctx, spr_shader_matte_vs, spr_shader_matte_fs, &u);
                        break;
                    case SHADER_PLASTIC:
                        spr_set_program(ctx, spr_shader_plastic_vs, spr_shader_plastic_fs, &u);
                        break;
                    case SHADER_METAL:
                        if (color_mode == 0) {
                            spr_uniforms_set_color(&u, 0.95f, 0.85f, 0.5f, 1.0f); /* Gold override */
                        }
                        u.roughness = 64.0f;
                        spr_set_program(ctx, spr_shader_metal_vs, spr_shader_metal_fs, &u);
                        break;
                }        
        /* Draw */
        spr_draw_triangles(ctx, mesh->vertex_count / 3, mesh->vertices, sizeof(stl_vertex_t));
        
        uint64_t end_time = SDL_GetPerformanceCounter();
        accumulated_render_ms += (double)((end_time - start_time) * 1000) / perf_freq;

        /* Update SDL */
        void* pixels;
        int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, spr_get_color_buffer(ctx), WINDOW_WIDTH * WINDOW_HEIGHT * 4);
        SDL_UnlockTexture(texture);
        
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        /* FPS Update */
        frame_count++;
        if (SDL_GetTicks() - last_time >= 1000) {
            current_fps = frame_count;
            current_render_ms = accumulated_render_ms / frame_count;
            frame_count = 0;
            accumulated_render_ms = 0.0;
            last_time = SDL_GetTicks();
            if (show_fps) {
                char title[128];
                snprintf(title, sizeof(title), "SPR STL Viewer [%s] [FPS: %d] [Render: %.2f ms]", 
                    get_shader_name(current_shader), current_fps, current_render_ms);
                SDL_SetWindowTitle(window, title);
            } else {
                SDL_SetWindowTitle(window, "SPR STL Viewer");
            }
        }
    }
    
    spr_shutdown(ctx);
    stl_free(mesh);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
