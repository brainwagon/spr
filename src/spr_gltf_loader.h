#ifndef SPR_GLTF_LOADER_H
#define SPR_GLTF_LOADER_H

#include "spr_loader.h"

/**
 * Loads a glTF 2.0 file (.gltf or .glb).
 * Currently supports static meshes and basic PBR materials.
 */
spr_mesh_t* spr_load_gltf(const char* filename);

#endif /* SPR_GLTF_LOADER_H */
