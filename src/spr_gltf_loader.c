#include "spr_gltf_loader.h"
#include <stdio.h>
#include <stdlib.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

spr_mesh_t* spr_load_gltf(const char* filename) {
    printf("Loading glTF: %s\n", filename);
    /* Stub implementation */
    return NULL;
}