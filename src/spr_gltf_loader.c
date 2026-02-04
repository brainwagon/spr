#include "spr_gltf_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

/* Helper to get directory part of a path */
static char* get_directory(const char* path) {
    char* dir = strdup(path);
    char* last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        free(dir);
        return NULL;
    }
    return dir;
}

static spr_texture_t* load_gltf_texture(cgltf_texture* gtex, const char* base_path) {
    if (!gtex || !gtex->image) return NULL;
    
    cgltf_image* img = gtex->image;
    
    if (img->buffer_view) {
        /* Embedded in GLB/buffer */
        uint8_t* data = (uint8_t*)img->buffer_view->buffer->data + img->buffer_view->offset;
        return spr_texture_load_from_memory(data, (int)img->buffer_view->size);
    } else if (img->uri) {
        /* External file */
        if (strncmp(img->uri, "data:", 5) == 0) {
            /* Base64 data URI - let cgltf handle decoding if we used its helper, 
               but for now we focus on files and GLB. cgltf_load_buffers handles data URIs. */
            return NULL; 
        } else {
            char path[1024];
            char* dir = get_directory(base_path);
            if (dir) {
                snprintf(path, sizeof(path), "%s/%s", dir, img->uri);
                free(dir);
            } else {
                snprintf(path, sizeof(path), "%s", img->uri);
            }
            return spr_texture_load(path);
        }
    }
    
    return NULL;
}

spr_mesh_t* spr_load_gltf(const char* filename) {
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    
    if (result != cgltf_result_success) {
        return NULL;
    }
    
    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) {
        cgltf_free(data);
        return NULL;
    }

    if (data->meshes_count == 0) {
        cgltf_free(data);
        return NULL;
    }

    /* For MVP, we take the first primitive of the first mesh */
    cgltf_mesh* gmesh = &data->meshes[0];
    if (gmesh->primitives_count == 0) {
        cgltf_free(data);
        return NULL;
    }

    cgltf_primitive* prim = &gmesh->primitives[0];
    
    /* Find attributes */
    cgltf_accessor* pos_acc = NULL;
    cgltf_accessor* norm_acc = NULL;
    cgltf_accessor* uv_acc = NULL;

    for (cgltf_size i = 0; i < prim->attributes_count; ++i) {
        if (prim->attributes[i].type == cgltf_attribute_type_position) {
            pos_acc = prim->attributes[i].data;
        } else if (prim->attributes[i].type == cgltf_attribute_type_normal) {
            norm_acc = prim->attributes[i].data;
        } else if (prim->attributes[i].type == cgltf_attribute_type_texcoord) {
            uv_acc = prim->attributes[i].data;
        }
    }

    if (!pos_acc || !prim->indices) {
        cgltf_free(data);
        return NULL;
    }

    /* Allocate spr_mesh_t */
    spr_mesh_t* mesh = (spr_mesh_t*)calloc(1, sizeof(spr_mesh_t));
    mesh->type = SPR_MESH_OBJ; /* Uses spr_vertex_t */
    
    int index_count = (int)prim->indices->count;
    mesh->vertex_count = index_count;
    mesh->vertices = malloc(sizeof(spr_vertex_t) * mesh->vertex_count);
    spr_vertex_t* vertices = (spr_vertex_t*)mesh->vertices;

    /* Extract data by indexed lookup (flattening) */
    for (int i = 0; i < index_count; ++i) {
        cgltf_size idx = cgltf_accessor_read_index(prim->indices, i);
        
        /* Position */
        cgltf_accessor_read_float(pos_acc, idx, &vertices[i].position.x, 3);
        
        /* Normal */
        if (norm_acc) {
            cgltf_accessor_read_float(norm_acc, idx, &vertices[i].normal.x, 3);
        } else {
            vertices[i].normal.x = 0; vertices[i].normal.y = 1; vertices[i].normal.z = 0;
        }
        
        /* UV */
        if (uv_acc) {
            cgltf_accessor_read_float(uv_acc, idx, &vertices[i].uv.x, 2);
        } else {
            vertices[i].uv.x = 0; vertices[i].uv.y = 0;
        }
        
        /* Tangent */
        vertices[i].tangent.x = 0; vertices[i].tangent.y = 0; vertices[i].tangent.z = 0; vertices[i].tangent.w = 1;
    }

    /* Handle Material */
    mesh->material_count = 1;
    mesh->materials = (spr_material_t*)calloc(1, sizeof(spr_material_t));
    spr_material_t* mat = &mesh->materials[0];
    strcpy(mat->name, "gltf_material");
    
    /* Default material properties */
    mat->Kd.x = 1; mat->Kd.y = 1; mat->Kd.z = 1;
    mat->d = 1.0f;

    if (prim->material) {
        cgltf_material* gmat = prim->material;
        if (gmat->has_pbr_metallic_roughness) {
            mat->Kd.x = gmat->pbr_metallic_roughness.base_color_factor[0];
            mat->Kd.y = gmat->pbr_metallic_roughness.base_color_factor[1];
            mat->Kd.z = gmat->pbr_metallic_roughness.base_color_factor[2];
            mat->d = gmat->pbr_metallic_roughness.base_color_factor[3];
            
            mat->map_Kd = load_gltf_texture(gmat->pbr_metallic_roughness.base_color_texture.texture, filename);
        }
        
        if (gmat->normal_texture.texture) {
            mat->norm = load_gltf_texture(gmat->normal_texture.texture, filename);
        }
    }

    /* Create a single group */
    mesh->group_count = 1;
    mesh->groups = (spr_mesh_group_t*)calloc(1, sizeof(spr_mesh_group_t));
    mesh->groups[0].start_vertex = 0;
    mesh->groups[0].vertex_count = mesh->vertex_count;
    mesh->groups[0].material = mat;

    cgltf_free(data);
    return mesh; 
}
