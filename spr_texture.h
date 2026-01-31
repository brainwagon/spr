#ifndef SPR_TEXTURE_H
#define SPR_TEXTURE_H

#include "spr.h" /* For vec4_t */
#include <stdint.h>

#ifdef SPR_ENABLE_TEXTURES

typedef struct {
    int width;
    int height;
    int channels;    /* 1 (Gray), 3 (RGB), 4 (RGBA) */
    uint8_t* pixels; /* Raw data */
} spr_texture_t;

/* Returns NULL if failed or if texturing is disabled */
spr_texture_t* spr_texture_load(const char* filename);

void spr_texture_free(spr_texture_t* tex);

/* Point sampling: maps u,v (0..1) to pixel coordinates. Handles wrapping. */
/* Returns normalized RGBA (0.0 - 1.0) */
vec4_t spr_texture_sample(const spr_texture_t* tex, float u, float v);

#else

/* Dummy struct to allow compilation without textures */
typedef struct { int unused; } spr_texture_t;

static inline spr_texture_t* spr_texture_load(const char* f) { (void)f; return NULL; }
static inline void spr_texture_free(spr_texture_t* t) { (void)t; }
static inline vec4_t spr_texture_sample(const spr_texture_t* t, float u, float v) { 
    (void)t; (void)u; (void)v;
    vec4_t c = {1,1,1,1}; return c; 
}

#endif /* SPR_ENABLE_TEXTURES */

#endif /* SPR_TEXTURE_H */
