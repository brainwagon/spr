#include <SDL2/SDL.h>
#include "spr.h"
#include "spr_shaders.h"
#include "spr_texture.h" 
#include "spr_loader.h" /* Unified loader */
#include "stl.h"        /* Still needed for legacy vertex struct definition in bounds calc */
#include "font.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/* --- Window Settings --- */

/* --- Text Rendering --- */
void draw_char_overlay(uint32_t* buffer, int width, int height, int x, int y, char c, uint32_t color) {
    int idx = (unsigned char)c;
    if (idx < 32 || idx > 127) return;
    idx -= 32;
    for (int r = 0; r < 8; ++r) {
        int py = y + r;
        if (py < 0 || py >= height) continue;
        uint8_t row = font8x8_basic[idx][r];
        for (int b = 0; b < 8; ++b) {
            int px = x + b;
            if (px < 0 || px >= width) continue;
            if ((row >> (7 - b)) & 1) {
                /* Draw shadow/outline first? Nah, just simplistic overlay. */
                buffer[py * width + px] = color;
            }
        }
    }
}

void draw_string_overlay(uint32_t* buffer, int width, int height, int x, int y, const char* str, uint32_t color) {
    while (*str) {
        draw_char_overlay(buffer, width, height, x + 1, y + 1, *str, 0xFF000000); /* Drop shadow */
        draw_char_overlay(buffer, width, height, x, y, *str, color);
        x += 8;
        str++;
    }
}

/* --- Interaction State --- */
typedef struct {
    float rot_x;
    float rot_y;
    float distance;
    float pan_x;
    float pan_y;
    float light_rot_x;
    float light_rot_y;
    int mouse_down_left;
    int mouse_down_right;
    int last_mouse_x;
    int last_mouse_y;
} view_state_t;

/* Shader Enum */
typedef enum {
    SHADER_CONSTANT,
    SHADER_MATTE,
    SHADER_PLASTIC,
    SHADER_METAL,
    SHADER_PAINTED_PLASTIC
} shader_type_t;

const char* get_shader_name(shader_type_t s) {
    switch(s) {
        case SHADER_CONSTANT: return "Constant";
        case SHADER_MATTE: return "Matte";
        case SHADER_PLASTIC: return "Plastic";
        case SHADER_METAL: return "Metal";
        case SHADER_PAINTED_PLASTIC: return "Painted";
        default: return "Unknown";
    }
}

void print_help(const char* prog_name) {
    printf("Usage: %s <stl_file> [texture_file] [options]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -simd       Use SIMD rasterizer (if available)\n");
    printf("  -cpu        Use CPU rasterizer (default)\n");
    printf("  -h, --help  Show this help message\n");
    printf("\nControls:\n");
    printf("  Left Drag   Rotate Camera (Orbit)\n");
    printf("  Shift+Left  Rotate Light\n");
    printf("  Right Drag  Pan\n");
    printf("  Wheel       Zoom\n");
    printf("  's'         Toggle Stats Overlay (FPS, Frags)\n");
    printf("  'o'         Toggle Transparency (Opaque/Transparent)\n");
    printf("  'c'         Toggle Base Color (Grey/Red)\n");
    printf("  'b'         Toggle Back-face Culling\n");
    printf("  '1'-'5'     Switch Shaders (Constant, Matte, Plastic, Metal, Painted)\n");
    printf("  ESC         Exit\n");
}

int main(int argc, char* argv[]) {
    int win_width = 800;
    int win_height = 600;

    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    const char* filename = NULL;
    const char* tex_filename = NULL;
    spr_rasterizer_mode_t mode = SPR_RASTERIZER_CPU; /* Default to CPU */
    shader_type_t current_shader = SHADER_PLASTIC;

    /* Parse Args */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-simd") == 0) {
            mode = SPR_RASTERIZER_SIMD;
        } else if (strcmp(argv[i], "-cpu") == 0) {
            mode = SPR_RASTERIZER_CPU;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            if (!filename) filename = argv[i];
            else if (!tex_filename) tex_filename = argv[i];
        }
    }

    if (!filename) {
        printf("Error: No STL file specified.\n");
        print_help(argv[0]);
        return 1;
    }

    if (mode == SPR_RASTERIZER_SIMD) {
        printf("Mode: SIMD (if available)\n");
    } else {
        printf("Mode: CPU\n");
    }

    /* Load Mesh */
    printf("Loading %s...\n", filename);
    spr_mesh_t* mesh = spr_load_mesh(filename);
    if (!mesh) {
        printf("Failed to load mesh.\n");
        return 1;
    }
    
    /* Load Texture Override */
    spr_texture_t* spr_tex = NULL;
    
    if (tex_filename) {
        printf("Loading texture %s...\n", tex_filename);
        spr_tex = spr_texture_load(tex_filename);
        if (!spr_tex) printf("Failed to load texture.\n");
        else printf("Texture loaded: %dx%d (%d channels)\n", spr_tex->width, spr_tex->height, spr_tex->channels);
    } else if (mesh->texture) {
        spr_tex = mesh->texture;
        current_shader = SHADER_PAINTED_PLASTIC; /* Auto-switch */
    }
    
    /* Calculate Bounds for auto-centering */
    float minx=1e9, miny=1e9, minz=1e9;
    float maxx=-1e9, maxy=-1e9, maxz=-1e9;
    for (int i=0; i<mesh->vertex_count; ++i) {
        float vx, vy, vz;
        if (mesh->type == SPR_MESH_STL) {
            stl_vertex_t* v = &((stl_vertex_t*)mesh->vertices)[i];
            vx = v->x; vy = v->y; vz = v->z;
        } else {
            spr_vertex_t* v = &((spr_vertex_t*)mesh->vertices)[i];
            vx = v->position.x; vy = v->position.y; vz = v->position.z;
        }
        if (vx < minx) minx = vx;
        if (vx > maxx) maxx = vx;
        if (vy < miny) miny = vy;
        if (vy > maxy) maxy = vy;
        if (vz < minz) minz = vz;
        if (vz > maxz) maxz = vz;
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
    
    SDL_Window* window = SDL_CreateWindow("SPR Object Viewer", 
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
        win_width, win_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(renderer, 
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 
        win_width, win_height);

    /* Init SPR */
    spr_context_t* ctx = spr_init(win_width, win_height);
    spr_stats_t* stats_ptr = spr_get_stats_ptr(ctx);
    spr_set_rasterizer_mode(ctx, mode);
    
    /* View State */
    view_state_t view = {0};
    view.distance = size * 2.0f;
    view.rot_x = 0.0f;
    view.rot_y = 0.0f;
    view.pan_x = 0.0f;
    view.pan_y = 0.0f;
    view.light_rot_x = 45.0f;
    view.light_rot_y = 30.0f;
    
    /* Stats State */
    int show_stats = 0;
    int frame_count = 0;
    int current_fps = 0;
    int color_mode = 0; /* 0: Grey, 1: Red */
    int opacity_mode = 0; /* 0: Opaque, 1: Transparent (0.5) */
    int cull_mode = 0; /* 0: None, 1: Backface */
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
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                win_width = e.window.data1;
                win_height = e.window.data2;
                
                spr_shutdown(ctx);
                ctx = spr_init(win_width, win_height);
                spr_set_rasterizer_mode(ctx, mode);
                
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, win_width, win_height);
            }
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = 0; break;
                    case SDLK_s: show_stats = !show_stats; break;
                    case SDLK_c: color_mode = !color_mode; break;
                    case SDLK_o: opacity_mode = !opacity_mode; break;
                    case SDLK_b: cull_mode = !cull_mode; break;
                    case SDLK_1: current_shader = SHADER_CONSTANT; break;
                    case SDLK_2: current_shader = SHADER_MATTE; break;
                    case SDLK_3: current_shader = SHADER_PLASTIC; break;
                    case SDLK_4: current_shader = SHADER_METAL; break;
                    case SDLK_5: current_shader = SHADER_PAINTED_PLASTIC; break;
                }
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
                    if (SDL_GetModState() & KMOD_SHIFT) {
                        view.light_rot_y += dx * 0.5f;
                        view.light_rot_x += dy * 0.5f;
                    } else {
                        view.rot_y += dx * 0.5f;
                        view.rot_x += dy * 0.5f;
                    }
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
        
        /* Reset Texture Stats */
        if (tex_filename && spr_tex) spr_tex->sample_count = 0;
        if (mesh->materials) {
            for (int i=0; i<mesh->material_count; ++i) {
                if (mesh->materials[i].map_Kd) mesh->materials[i].map_Kd->sample_count = 0;
                if (mesh->materials[i].map_Ks) mesh->materials[i].map_Ks->sample_count = 0;
            }
        }
        
        spr_matrix_mode(ctx, SPR_PROJECTION);
        spr_load_identity(ctx);
        spr_perspective(ctx, 45.0f, (float)win_width/(float)win_height, size*0.01f, size*10.0f);
        
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
        
        float lx = sinf(view.light_rot_y * (float)M_PI / 180.0f) * cosf(view.light_rot_x * (float)M_PI / 180.0f);
        float ly = sinf(view.light_rot_x * (float)M_PI / 180.0f);
        float lz = cosf(view.light_rot_y * (float)M_PI / 180.0f) * cosf(view.light_rot_x * (float)M_PI / 180.0f);
        spr_uniforms_set_light_dir(&u, lx, ly, lz);
        
        u.eye_pos = eye; 
        u.texture_ptr = spr_tex;
        u.stats = stats_ptr;
        
        if (color_mode == 0) {
            spr_uniforms_set_color(&u, 0.7f, 0.7f, 0.7f, 1.0f); /* Grey */
        } else {
            spr_uniforms_set_color(&u, 0.8f, 0.2f, 0.2f, 1.0f); /* Red */
        }
        
        if (opacity_mode == 0) {
            spr_uniforms_set_opacity(&u, 1.0f, 1.0f, 1.0f); /* Opaque */
        } else {
            spr_uniforms_set_opacity(&u, 0.5f, 0.5f, 0.5f); /* 50% Translucent */
        }
        
        u.roughness = 32.0f;

        /* Culling */
        spr_enable_cull_face(ctx, cull_mode);

        size_t stride = (mesh->type == SPR_MESH_STL) ? sizeof(stl_vertex_t) : sizeof(spr_vertex_t);
        
        /* Render Groups */
        for (int g = 0; g < mesh->group_count; ++g) {
            spr_mesh_group_t* group = &mesh->groups[g];
            
            /* Apply Material or Global Defaults */
            if (tex_filename && spr_tex) {
                /* Global override */
                u.texture_ptr = spr_tex;
                u.specular_map_ptr = NULL;
            } else if (group->material) {
                spr_uniforms_set_color(&u, group->material->Kd.x, group->material->Kd.y, group->material->Kd.z, group->material->d);
                spr_uniforms_set_opacity(&u, group->material->d, group->material->d, group->material->d);
                u.roughness = group->material->Ns;
                u.texture_ptr = group->material->map_Kd;
                u.specular_map_ptr = group->material->map_Ks;
            } else {
                /* Reset to global defaults if no material */
                u.texture_ptr = NULL;
                u.specular_map_ptr = NULL;
                /* Note: u.color was set by color_mode block earlier */
            }
            
            /* Shader Selection */
            spr_vertex_shader_t vs = NULL;
            spr_fragment_shader_t fs = NULL;
            
            shader_type_t shader = current_shader;
            /* Auto-switch to Painted if texture available and using default Plastic */
            if (u.texture_ptr && shader == SHADER_PLASTIC) {
                shader = SHADER_PAINTED_PLASTIC;
            }

            switch (shader) {
                case SHADER_CONSTANT:
                    fs = spr_shader_constant_fs; vs = spr_shader_constant_vs;
                    break;
                case SHADER_MATTE:
                    fs = spr_shader_matte_fs; vs = spr_shader_matte_vs;
                    break;
                case SHADER_PLASTIC:
                    fs = spr_shader_plastic_fs; vs = spr_shader_plastic_vs;
                    break;
                case SHADER_METAL:
                    if (color_mode == 0 && !group->material) { 
                        spr_uniforms_set_color(&u, 0.95f, 0.85f, 0.5f, 1.0f); /* Gold override */
                    }
                    u.roughness = 64.0f;
                    fs = spr_shader_metal_fs; vs = spr_shader_metal_vs;
                    break;
                case SHADER_PAINTED_PLASTIC:
                    fs = spr_shader_paintedplastic_fs; vs = spr_shader_paintedplastic_vs;
                    break;
            }
            
            /* If OBJ, override VS to standard textured VS */
            if (mesh->type == SPR_MESH_OBJ) {
                vs = spr_shader_textured_vs;
            }
            
            spr_set_program(ctx, vs, fs, &u);
            
            /* Draw Group */
            void* start_ptr = (uint8_t*)mesh->vertices + (group->start_vertex * stride);
            spr_draw_triangles(ctx, group->vertex_count / 3, start_ptr, stride);
        }
        
        /* Resolve A-Buffer */
        spr_resolve(ctx);
        
        /* Stats Overlay */
        if (show_stats) {
            char stats_buf[64];
            spr_stats_t stats = spr_get_stats(ctx);
            uint32_t col = 0xFFFFFFFF;
            int y = 10;
            
            snprintf(stats_buf, sizeof(stats_buf), "FPS: %d", current_fps);
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;

            snprintf(stats_buf, sizeof(stats_buf), "Time: %.2f ms", current_render_ms);
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;
            
            snprintf(stats_buf, sizeof(stats_buf), "Frags: %d", stats.peak_fragments);
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;

            snprintf(stats_buf, sizeof(stats_buf), "Chunks: %d", stats.total_chunks);
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;

            snprintf(stats_buf, sizeof(stats_buf), "Tex Samples: %llu", (unsigned long long)stats.texture_samples);
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;

            snprintf(stats_buf, sizeof(stats_buf), "Triangles: %llu", (unsigned long long)stats.total_triangles);
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;

            snprintf(stats_buf, sizeof(stats_buf), "Shader: %s", get_shader_name(current_shader));
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;
            
            snprintf(stats_buf, sizeof(stats_buf), "Cull: %s", cull_mode ? "ON" : "OFF");
            draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;
            
            /* Per-Texture Stats */
            if (tex_filename && spr_tex) {
                snprintf(stats_buf, sizeof(stats_buf), "Override: %llu", (unsigned long long)spr_tex->sample_count);
                draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;
            }
            if (mesh->materials) {
                for (int i=0; i<mesh->material_count; ++i) {
                    if (mesh->materials[i].map_Kd && mesh->materials[i].map_Kd->sample_count > 0) {
                        snprintf(stats_buf, sizeof(stats_buf), "[%d] %.20s(D): %llu", i, mesh->materials[i].name, (unsigned long long)mesh->materials[i].map_Kd->sample_count);
                        draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;
                    }
                    if (mesh->materials[i].map_Ks && mesh->materials[i].map_Ks->sample_count > 0) {
                        snprintf(stats_buf, sizeof(stats_buf), "[%d] %.20s(S): %llu", i, mesh->materials[i].name, (unsigned long long)mesh->materials[i].map_Ks->sample_count);
                        draw_string_overlay(spr_get_color_buffer(ctx), win_width, win_height, 10, y, stats_buf, col); y += 12;
                    }
                }
            }
        }

        
        uint64_t end_time = SDL_GetPerformanceCounter();
        accumulated_render_ms += (double)((end_time - start_time) * 1000) / perf_freq;

        /* Update SDL */
        void* pixels;
        int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, spr_get_color_buffer(ctx), win_width * win_height * 4);
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
        }
    }
    
    spr_shutdown(ctx);
    if (tex_filename && spr_tex) {
        spr_texture_free(spr_tex); /* Free manual override */
    }
    spr_free_mesh(mesh); /* Frees mesh and its internal texture */
    
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}