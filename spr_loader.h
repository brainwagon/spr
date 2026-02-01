#ifndef SPR_LOADER_H
#define SPR_LOADER_H

#include "spr_texture.h"
#include "spr.h" /* For vec3_t */

typedef enum {
    SPR_MESH_STL,      /* Uses stl_vertex_t (legacy) */
    SPR_MESH_OBJ       /* Uses spr_vertex_t (standard) */
} spr_mesh_type_t;

/* Material Definition (Wavefront MTL) */
typedef struct {
    char name[64];
    vec3_t Ka; /* Ambient */
    vec3_t Kd; /* Diffuse */
    vec3_t Ks; /* Specular */
    vec3_t Ke; /* Emissive */
    float Ns;  /* Specular Exponent (Shininess) */
    float d;   /* Dissolve (Opacity) */
    
    spr_texture_t* map_Kd;   /* Diffuse Map */
    spr_texture_t* map_Ks;   /* Specular Map */
    spr_texture_t* map_Ns;   /* Specular Exponent Map */
    spr_texture_t* map_d;    /* Dissolve/Opacity Map */
    spr_texture_t* map_Ke;   /* Emissive Map */
    spr_texture_t* map_Bump; /* Bump Map (Height) */
    spr_texture_t* norm;     /* Normal Map */
} spr_material_t;

/* A sub-mesh using a specific material */
typedef struct {
    spr_material_t* material; /* Pointer to material in mesh->materials list (or NULL) */
    int start_vertex;
    int vertex_count;
} spr_mesh_group_t;

typedef struct {
    spr_mesh_type_t type;
    
    /* Vertex Data */
    int vertex_count;
    void* vertices;         /* Pointer to array (stl_vertex_t* or spr_vertex_t*) */
    
    /* OBJ: Groups and Materials */
    int group_count;
    spr_mesh_group_t* groups;
    
    int material_count;
    spr_material_t* materials; /* Storage for loaded materials */
    
    /* STL/Legacy: Simple Texture */
    spr_texture_t* texture; /* Diffuse texture (owned by mesh if not using groups) */
} spr_mesh_t;

spr_mesh_t* spr_load_mesh(const char* filename);
void spr_free_mesh(spr_mesh_t* mesh);

#endif /* SPR_LOADER_H */
