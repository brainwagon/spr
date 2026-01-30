#include <SDL2/SDL.h>
#include "spr.h"
#include "stl.h"
#include <stdio.h>
#include <math.h>

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

/* --- Shader Uniforms --- */
typedef struct {
    mat4_t mvp;
    mat4_t model; /* To transform normals to world space */
    vec3_t light_dir;
    vec3_t eye_pos;
} plastic_uniforms_t;

/* --- Math Helpers for Shader --- */
static float dot(vec3_t a, vec3_t b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static vec3_t normalize(vec3_t v) {
    float len = sqrtf(dot(v, v));
    if (len > 0) { v.x/=len; v.y/=len; v.z/=len; }
    return v;
}
static vec3_t reflect(vec3_t i, vec3_t n) {
    float d = dot(i, n);
    vec3_t r;
    r.x = i.x - 2.0f * d * n.x;
    r.y = i.y - 2.0f * d * n.y;
    r.z = i.z - 2.0f * d * n.z;
    return r;
}

/* --- Vertex Shader --- */
void plastic_vs(void* user_data, const void* input_vertex, spr_vertex_out_t* out) {
    plastic_uniforms_t* u = (plastic_uniforms_t*)user_data;
    const stl_vertex_t* v = (const stl_vertex_t*)input_vertex;
    
    vec4_t pos = {v->x, v->y, v->z, 1.0f};
    out->position = spr_mat4_mul_vec4(u->mvp, pos);
    
    /* Pass Normal (Simple: assuming no non-uniform scaling, model matrix is rotation mostly) */
    /* Technically needs Inverse Transpose of Model, but if Model is rigid, it's fine. */
    vec4_t n4 = {v->nx, v->ny, v->nz, 0.0f};
    n4 = spr_mat4_mul_vec4(u->model, n4);
    
    out->normal.x = n4.x; out->normal.y = n4.y; out->normal.z = n4.z;
    
    /* Position in World Space (for specular) - approximate using ModelView or just Model */
    /* Let's pass World Position if needed, but for simple viewer, view-space calculation is common. */
    /* Here we compute lighting in World Space. */
}

/* --- Fragment Shader --- */
spr_color_t plastic_fs(void* user_data, const spr_vertex_out_t* interpolated) {
    plastic_uniforms_t* u = (plastic_uniforms_t*)user_data;
    
    vec3_t N = normalize(interpolated->normal);
    vec3_t L = normalize(u->light_dir);
    
    /* Diffuse (Lambert) */
    float diff = dot(N, L);
    if (diff < 0) diff = 0;
    
    /* Ambient */
    float amb = 0.2f;
    
    /* Specular (Phong) */
    /* View Vector (assuming eye is at 0,0,dist in View space, but we are in World space here?) */
    /* Actually, we didn't pass world pos. Let's assume directional light + simplified view. */
    /* Or just standard Blinn-Phong. */
    float spec = 0.0f;
    if (diff > 0) {
        /* Simple view vector approximation: Eye is "in front" */
        /* Better: pass eye_pos in uniforms. */
        /* But we need pixel position to compute V properly. */
        /* For "Simple Plastic", let's just do Diffuse + Constant Specular Highlight based on Normal? */
        /* Or just Gouraud shading (Vertex Shader) if we want speed? */
        /* Let's try proper per-pixel. */
        /* Wait, I didn't interpolate WorldPos. */
        /* Let's use Half-Vector H = (L + V) / |L + V| */
        /* Assume View Vector V is roughly -LightDir for a "headlight" or fixed camera? */
        /* Let's assume V is (0, 0, 1) in view space. */
        /* Too complex for 3 lines. Let's stick to simple Diffuse + Ambient + hardcoded Specular direction. */
        
        vec3_t V = {0.0f, 0.0f, 1.0f}; /* Approximate view direction */
        vec3_t R = reflect((vec3_t){-L.x, -L.y, -L.z}, N);
        float s = dot(R, V);
        if (s > 0) {
            spec = powf(s, 32.0f);
        }
    }
    
    /* Color: Grey Plastic */
    float r = 0.7f * (diff + amb) + spec * 0.4f;
    float g = 0.7f * (diff + amb) + spec * 0.4f;
    float b = 0.7f * (diff + amb) + spec * 0.4f;
    
    if (r > 1.0f) r = 1.0f;
    if (g > 1.0f) g = 1.0f;
    if (b > 1.0f) b = 1.0f;
    
    spr_color_t c;
    c.r = (uint8_t)(r * 255);
    c.g = (uint8_t)(g * 255);
    c.b = (uint8_t)(b * 255);
    c.a = 255;
    
    return c;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <stl_file>\n", argv[0]);
        return 1;
    }

    /* Load STL */
    printf("Loading %s...\n", argv[1]);
    stl_object_t* mesh = stl_load(argv[1]);
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
    
    /* View State */
    view_state_t view = {0};
    view.distance = size * 2.0f;
    view.rot_x = 0.0f;
    view.rot_y = 0.0f;
    view.pan_x = 0.0f;
    view.pan_y = 0.0f;
    
    /* Main Loop */
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
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
        plastic_uniforms_t u;
        u.mvp = spr_mat4_mul(spr_get_projection_matrix(ctx), spr_get_modelview_matrix(ctx));
        u.model = spr_get_modelview_matrix(ctx); /* Approximation for normals */
        u.light_dir = (vec3_t){0.5f, 1.0f, 1.0f}; /* Light from top-right-front */
        u.light_dir = normalize(u.light_dir);
        
        spr_set_program(ctx, plastic_vs, plastic_fs, &u);
        
        /* Draw */
        spr_draw_triangles(ctx, mesh->vertex_count / 3, mesh->vertices, sizeof(stl_vertex_t));
        
        /* Update SDL */
        void* pixels;
        int pitch;
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, spr_get_color_buffer(ctx), WINDOW_WIDTH * WINDOW_HEIGHT * 4);
        SDL_UnlockTexture(texture);
        
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
    
    spr_shutdown(ctx);
    stl_free(mesh);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
