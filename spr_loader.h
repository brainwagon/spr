#ifndef SPR_LOADER_H
#define SPR_LOADER_H

#include "spr_texture.h"

typedef enum {
    SPR_MESH_STL,      /* Uses stl_vertex_t (legacy) */
    SPR_MESH_OBJ       /* Uses spr_vertex_t (standard) */
} spr_mesh_type_t;

typedef struct {
    spr_mesh_type_t type;
    int vertex_count;
    void* vertices;         /* Pointer to array (stl_vertex_t* or spr_vertex_t*) */
    spr_texture_t* texture; /* Diffuse texture (owned by mesh) */
} spr_mesh_t;

spr_mesh_t* spr_load_mesh(const char* filename);
void spr_free_mesh(spr_mesh_t* mesh);

#endif /* SPR_LOADER_H */
