#ifndef STL_H
#define STL_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    float x, y, z;
    float nx, ny, nz; /* Normal for this vertex (or face) */
    uint16_t attr;    /* STL 16-bit attribute (often color) */
} stl_vertex_t;

typedef struct {
    int vertex_count;
    stl_vertex_t* vertices;
} stl_object_t;

stl_object_t* stl_load(const char* filename);
void stl_free(stl_object_t* obj);

#endif /* STL_H */
