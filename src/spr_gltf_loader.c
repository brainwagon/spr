#include "spr_gltf_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

/* Helper to get directory part of a path */
static char* get_directory(const char* path) {
    char* dir = strdup(path);
    if (!dir) return NULL;
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
        uint8_t* data = (uint8_t*)img->buffer_view->buffer->data + img->buffer_view->offset;
        return spr_texture_load_from_memory(data, (int)img->buffer_view->size);
    } else if (img->uri) {
        if (strncmp(img->uri, "data:", 5) == 0) return NULL;
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
    return NULL;
}

static mat4_t cgltf_to_mat4(const cgltf_float* m) {
    mat4_t out;
    /* glTF (column-major) to SPR (row-major) */
    out.m[0][0] = m[0];  out.m[0][1] = m[4];  out.m[0][2] = m[8];  out.m[0][3] = m[12];
    out.m[1][0] = m[1];  out.m[1][1] = m[5];  out.m[1][2] = m[9];  out.m[1][3] = m[13];
    out.m[2][0] = m[2];  out.m[2][1] = m[6];  out.m[2][2] = m[10]; out.m[2][3] = m[14];
    out.m[3][0] = m[3];  out.m[3][1] = m[7];  out.m[3][2] = m[11]; out.m[3][3] = m[15];
    return out;
}

static vec3_t transform_point(mat4_t m, vec3_t v) {
    vec4_t v4 = {v.x, v.y, v.z, 1.0f};
    vec4_t res = spr_mat4_mul_vec4(m, v4);
    vec3_t out = {res.x, res.y, res.z};
    return out;
}

static vec3_t transform_vector(mat4_t m, vec3_t v) {
    vec4_t v4 = {v.x, v.y, v.z, 0.0f};
    vec4_t res = spr_mat4_mul_vec4(m, v4);
    vec3_t out = {res.x, res.y, res.z};
    float len = sqrtf(out.x*out.x + out.y*out.y + out.z*out.z);
    if (len > 0) { out.x/=len; out.y/=len; out.z/=len; }
    return out;
}

static void process_node_count(cgltf_node* node, int* total_vertices, int* total_groups) {
    if (node->mesh) {
        for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i) {
            cgltf_primitive* prim = &node->mesh->primitives[i];
            if (prim->type != cgltf_primitive_type_triangles) continue;
            cgltf_accessor* pos_acc = NULL;
            for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
                if (prim->attributes[k].type == cgltf_attribute_type_position) { pos_acc = prim->attributes[k].data; break; }
            }
            if (!pos_acc) continue;
            int count = prim->indices ? (int)prim->indices->count : (int)pos_acc->count;
            *total_vertices += count;
            (*total_groups)++;
        }
    }
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        process_node_count(node->children[i], total_vertices, total_groups);
    }
}

static void process_node_extract(cgltf_node* node, mat4_t parent_transform, spr_mesh_t* mesh, 
                                 int* current_vertex, int* current_group, cgltf_data* data) {
    cgltf_float local_m[16];
    cgltf_node_transform_local(node, local_m);
    mat4_t local_transform = cgltf_to_mat4(local_m);
    mat4_t global_transform = spr_mat4_mul(parent_transform, local_transform);

    if (node->mesh) {
        for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i) {
            cgltf_primitive* prim = &node->mesh->primitives[i];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            cgltf_accessor* pos_acc = NULL;
            cgltf_accessor* norm_acc = NULL;
            cgltf_accessor* uv_acc = NULL;
            for (cgltf_size k = 0; k < prim->attributes_count; ++k) {
                if (prim->attributes[k].type == cgltf_attribute_type_position) pos_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_normal) norm_acc = prim->attributes[k].data;
                else if (prim->attributes[k].type == cgltf_attribute_type_texcoord) uv_acc = prim->attributes[k].data;
            }
            if (!pos_acc) continue;

            mesh->groups[*current_group].start_vertex = *current_vertex;
            int count = prim->indices ? (int)prim->indices->count : (int)pos_acc->count;
            mesh->groups[*current_group].vertex_count = count;
            
            if (prim->material) {
                int mat_idx = -1;
                for (size_t m_idx=0; m_idx<data->materials_count; ++m_idx) {
                    if (&data->materials[m_idx] == prim->material) { mat_idx = (int)m_idx; break; }
                }
                if (mat_idx != -1) mesh->groups[*current_group].material = &mesh->materials[mat_idx];
            }

            spr_vertex_t* vertices = (spr_vertex_t*)mesh->vertices;
            for (int k = 0; k < count; ++k) {
                cgltf_size idx = prim->indices ? cgltf_accessor_read_index(prim->indices, k) : (cgltf_size)k;
                spr_vertex_t* v = &vertices[(*current_vertex)++];
                
                vec3_t raw_pos;
                cgltf_accessor_read_float(pos_acc, idx, &raw_pos.x, 3);
                v->position = transform_point(global_transform, raw_pos);
                
                if (norm_acc) {
                    vec3_t raw_norm;
                    cgltf_accessor_read_float(norm_acc, idx, &raw_norm.x, 3);
                    v->normal = transform_vector(global_transform, raw_norm);
                } else {
                     vec3_t up = {0, 1, 0};
                     v->normal = transform_vector(global_transform, up);
                }
                
                if (uv_acc) {
                    cgltf_accessor_read_float(uv_acc, idx, &v->uv.x, 2);
                    v->uv.y = 1.0f - v->uv.y; /* Flip V */
                } else { v->uv.x=0; v->uv.y=0; }
                
                v->tangent.x=0; v->tangent.y=0; v->tangent.z=0; v->tangent.w=1;
            }
            (*current_group)++;
        }
    }
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        process_node_extract(node->children[i], global_transform, mesh, current_vertex, current_group, data);
    }
}

spr_mesh_t* spr_load_gltf(const char* filename) {
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success) return NULL;
    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) { cgltf_free(data); return NULL; }

    int total_vertices = 0;
    int total_groups = 0;
    if (data->scene) {
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) process_node_count(data->scene->nodes[i], &total_vertices, &total_groups);
    } else if (data->scenes_count > 0) {
        for (cgltf_size i = 0; i < data->scenes[0].nodes_count; ++i) process_node_count(data->scenes[0].nodes[i], &total_vertices, &total_groups);
    }

    if (total_vertices == 0) { cgltf_free(data); return NULL; }

    spr_mesh_t* mesh = (spr_mesh_t*)calloc(1, sizeof(spr_mesh_t));
    mesh->type = SPR_MESH_OBJ;
    mesh->vertex_count = total_vertices;
    mesh->vertices = malloc(sizeof(spr_vertex_t) * mesh->vertex_count);
    mesh->group_count = total_groups;
    mesh->groups = (spr_mesh_group_t*)calloc(total_groups, sizeof(spr_mesh_group_t));
    
    mesh->material_count = (int)data->materials_count;
    mesh->materials = (spr_material_t*)calloc(mesh->material_count, sizeof(spr_material_t));
    for (size_t i = 0; i < data->materials_count; ++i) {
        cgltf_material* gmat = &data->materials[i];
        spr_material_t* mat = &mesh->materials[i];
        if (gmat->name) strncpy(mat->name, gmat->name, 63);
        else sprintf(mat->name, "material_%d", (int)i);
        mat->Kd.x = 1; mat->Kd.y = 1; mat->Kd.z = 1; mat->d = 1;
        if (gmat->has_pbr_metallic_roughness) {
            mat->Kd.x = gmat->pbr_metallic_roughness.base_color_factor[0];
            mat->Kd.y = gmat->pbr_metallic_roughness.base_color_factor[1];
            mat->Kd.z = gmat->pbr_metallic_roughness.base_color_factor[2];
            mat->d = gmat->pbr_metallic_roughness.base_color_factor[3];
            mat->map_Kd = load_gltf_texture(gmat->pbr_metallic_roughness.base_color_texture.texture, filename);
        }
        if (gmat->normal_texture.texture) mat->norm = load_gltf_texture(gmat->normal_texture.texture, filename);
    }

    int current_vertex = 0;
    int current_group = 0;
    mat4_t root_transform = spr_mat4_identity();
    if (data->scene) {
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) process_node_extract(data->scene->nodes[i], root_transform, mesh, &current_vertex, &current_group, data);
    } else if (data->scenes_count > 0) {
         for (cgltf_size i = 0; i < data->scenes[0].nodes_count; ++i) process_node_extract(data->scenes[0].nodes[i], root_transform, mesh, &current_vertex, &current_group, data);
    }

    printf("Loaded glTF: %d triangles (%d vertices), %d groups, %d materials\n", mesh->vertex_count / 3, mesh->vertex_count, mesh->group_count, mesh->material_count);
    cgltf_free(data);
    return mesh;
}