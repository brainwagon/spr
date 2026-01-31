#include "spr_texture.h"
#include <stdlib.h>
#include <math.h>

#ifdef SPR_ENABLE_TEXTURES

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

spr_texture_t* spr_texture_load(const char* filename) {
    int w, h, n;
    /* Force 4 channels (RGBA) to simplify sampling? 
       Or handle native? 
       Let's handle native to save memory, or force 4 for speed/simplicity.
       User spec said "either single channel, rgb, or rgba".
       Let's stick to native loading for flexibility. 
    */
    unsigned char *data = stbi_load(filename, &w, &h, &n, 0);
    
    if (!data) {
        printf("SPR Texture: Failed to load %s (stbi error: %s)\n", filename, stbi_failure_reason());
        return NULL;
    }
    
    spr_texture_t* tex = (spr_texture_t*)malloc(sizeof(spr_texture_t));
    if (!tex) {
        stbi_image_free(data);
        return NULL;
    }
    
    tex->width = w;
    tex->height = h;
    tex->channels = n;
    tex->pixels = data;
    tex->sample_count = 0;
    
    return tex;
}

void spr_texture_free(spr_texture_t* tex) {
    if (tex) {
        if (tex->pixels) stbi_image_free(tex->pixels);
        free(tex);
    }
}

vec4_t spr_texture_sample(const spr_texture_t* tex, float u, float v, spr_stats_t* stats) {
    vec4_t c = {1.0f, 1.0f, 1.0f, 1.0f};
    if (stats) stats->texture_samples++;
    if (tex) ((spr_texture_t*)tex)->sample_count++;
    
    if (!tex || !tex->pixels) return c;
    
    /* Wrap UVs */
    u = u - floorf(u);
    v = v - floorf(v);
    
    /* Map to pixels */
    int x = (int)(u * tex->width);
    int y = (int)(v * tex->height);
    
    /* Clamp just in case float math pushes to width */
    if (x >= tex->width) x = tex->width - 1;
    if (y >= tex->height) y = tex->height - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    
    /* Flip Y? Textures often stored Top-to-Bottom, UV often Bottom-to-Top. */
    /* STL Viewer usually standard OpenGL is (0,0) Bottom-Left. */
    /* STB loads Top-to-Bottom. */
    /* Let's flip V for standard behavior. */
    y = (tex->height - 1) - y;
    
    int idx = (y * tex->width + x) * tex->channels;
    
    uint8_t* p = tex->pixels + idx;
    
    if (tex->channels == 1) {
        float val = p[0] / 255.0f;
        c.x = val; c.y = val; c.z = val; c.w = 1.0f;
    } else if (tex->channels == 2) {
        /* Grayscale + Alpha */
        float val = p[0] / 255.0f;
        float a = p[1] / 255.0f;
        c.x = val; c.y = val; c.z = val; c.w = a;
    } else if (tex->channels == 3) {
        c.x = p[0] / 255.0f;
        c.y = p[1] / 255.0f;
        c.z = p[2] / 255.0f;
        c.w = 1.0f;
    } else if (tex->channels == 4) {
        c.x = p[0] / 255.0f;
        c.y = p[1] / 255.0f;
        c.z = p[2] / 255.0f;
        c.w = p[3] / 255.0f;
    }
    
    return c;
}

#endif /* SPR_ENABLE_TEXTURES */
