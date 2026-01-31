#include "spr_loader.h"
#include "stl.h"
#include "spr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024

/* --- Helpers --- */

static char* get_base_dir(const char* path) {
    char* dir = strdup(path);
    char* last_slash = strrchr(dir, '/');
    if (last_slash) {
        *(last_slash + 1) = '\0'; /* Keep the slash */
    } else {
        free(dir);
        return strdup("./");
    }
    return dir;
}

static char* concat_path(const char* dir, const char* file) {
    char clean_file[MAX_LINE];
    strncpy(clean_file, file, MAX_LINE-1);
    char* p = clean_file;
    while (*p) {
        if (*p == '\r' || *p == '\n') *p = '\0';
        p++;
    }
    
    int len = strlen(dir) + strlen(clean_file) + 1;
    char* out = (char*)malloc(len);
    snprintf(out, len, "%s%s", dir, clean_file);
    return out;
}

/* --- Dynamic Array --- */

typedef struct {
    void* data;
    int count;
    int capacity;
    size_t element_size;
} dyn_array_t;

static void da_init(dyn_array_t* da, size_t size) {
    da->data = NULL;
    da->count = 0;
    da->capacity = 0;
    da->element_size = size;
}

static void* da_push(dyn_array_t* da) {
    if (da->count >= da->capacity) {
        da->capacity = (da->capacity == 0) ? 128 : da->capacity * 2;
        da->data = realloc(da->data, da->capacity * da->element_size);
    }
    return (char*)da->data + (da->count++ * da->element_size);
}

static void da_free(dyn_array_t* da) {
    if (da->data) free(da->data);
}

static void* da_get(dyn_array_t* da, int idx) {
    if (idx < 0 || idx >= da->count) return NULL;
    return (char*)da->data + (idx * da->element_size);
}

/* --- MTL Loader --- */

static void parse_mtl(const char* mtl_path, dyn_array_t* materials) {
    FILE* f = fopen(mtl_path, "r");
    if (!f) return;
    
    char line[MAX_LINE];
    char* dir = get_base_dir(mtl_path);
    spr_material_t* current_mat = NULL;
    
    printf("SPR Loader: Parsing MTL %s...\n", mtl_path);
    
    while (fgets(line, sizeof(line), f)) {
        char* ptr = line;
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '#' || *ptr == '\n' || *ptr == '\0') continue;
        
        if (strncmp(ptr, "newmtl ", 7) == 0) {
            ptr += 7;
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            char* name_end = ptr;
            while (*name_end && !isspace(*name_end)) name_end++;
            *name_end = '\0';
            
            /* Add new material */
            current_mat = da_push(materials);
            memset(current_mat, 0, sizeof(spr_material_t));
            strncpy(current_mat->name, ptr, 63);
            
            /* Defaults */
            current_mat->d = 1.0f;
            current_mat->Ns = 10.0f;
            current_mat->Kd.x = 0.8f; current_mat->Kd.y = 0.8f; current_mat->Kd.z = 0.8f; /* Default Gray */
            
        } else if (!current_mat) {
            continue;
        } else if (strncmp(ptr, "Ka", 2) == 0 && isspace(ptr[2])) {
            sscanf(ptr+2, "%f %f %f", &current_mat->Ka.x, &current_mat->Ka.y, &current_mat->Ka.z);
        } else if (strncmp(ptr, "Kd", 2) == 0 && isspace(ptr[2])) {
            sscanf(ptr+2, "%f %f %f", &current_mat->Kd.x, &current_mat->Kd.y, &current_mat->Kd.z);
        } else if (strncmp(ptr, "Ks", 2) == 0 && isspace(ptr[2])) {
            sscanf(ptr+2, "%f %f %f", &current_mat->Ks.x, &current_mat->Ks.y, &current_mat->Ks.z);
        } else if (strncmp(ptr, "Ns", 2) == 0 && isspace(ptr[2])) {
            sscanf(ptr+2, "%f", &current_mat->Ns);
        } else if (strncmp(ptr, "d", 1) == 0 && isspace(ptr[1])) {
            sscanf(ptr+1, "%f", &current_mat->d);
        } else if (strncmp(ptr, "map_Kd", 6) == 0 && isspace(ptr[6])) {
            ptr += 6; while (*ptr == ' ' || *ptr == '\t') ptr++;
            char* path = concat_path(dir, ptr);
            current_mat->map_Kd = spr_texture_load(path);
            /* If Kd is near-black but map is present, default Kd to White to allow texture to show */
            if (current_mat->map_Kd && current_mat->Kd.x < 0.01f && current_mat->Kd.y < 0.01f && current_mat->Kd.z < 0.01f) {
                current_mat->Kd.x = 1.0f; current_mat->Kd.y = 1.0f; current_mat->Kd.z = 1.0f;
            }
            free(path);
        } else if (strncmp(ptr, "map_Ks", 6) == 0 && isspace(ptr[6])) {
            ptr += 6; while (*ptr == ' ' || *ptr == '\t') ptr++;
            char* path = concat_path(dir, ptr);
            current_mat->map_Ks = spr_texture_load(path);
            free(path);
        }
    }
    
    free(dir);
    fclose(f);
}

static spr_material_t* find_material(dyn_array_t* materials, const char* name) {
    for (int i=0; i<materials->count; ++i) {
        spr_material_t* mat = (spr_material_t*)da_get(materials, i);
        if (strcmp(mat->name, name) == 0) return mat;
    }
    return NULL;
}

/* --- OBJ Loader --- */

typedef struct {
    int v_idx, vt_idx, vn_idx;
} obj_index_t;

static spr_mesh_t* load_obj(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Failed to open OBJ: %s\n", filename);
        return NULL;
    }
    
    dyn_array_t pos_list; da_init(&pos_list, sizeof(vec3_t));
    dyn_array_t uv_list;  da_init(&uv_list, sizeof(vec2_t));
    dyn_array_t norm_list; da_init(&norm_list, sizeof(vec3_t));
    
    dyn_array_t vertices; da_init(&vertices, sizeof(spr_vertex_t));
    dyn_array_t groups;   da_init(&groups, sizeof(spr_mesh_group_t));
    dyn_array_t materials; da_init(&materials, sizeof(spr_material_t));
    
    char* base_dir = get_base_dir(filename);
    
    /* Default Group */
    spr_mesh_group_t current_group;
    current_group.material = NULL;
    current_group.start_vertex = 0;
    current_group.vertex_count = 0;
    
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char* ptr = line;
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '#' || *ptr == '\n' || *ptr == '\0') continue;
        
        if (strncmp(ptr, "v ", 2) == 0) {
            vec3_t* v = da_push(&pos_list);
            sscanf(ptr+2, "%f %f %f", &v->x, &v->y, &v->z);
        } else if (strncmp(ptr, "vt ", 3) == 0) {
            vec2_t* vt = da_push(&uv_list);
            sscanf(ptr+3, "%f %f", &vt->x, &vt->y);
        } else if (strncmp(ptr, "vn ", 3) == 0) {
            vec3_t* vn = da_push(&norm_list);
            sscanf(ptr+3, "%f %f %f", &vn->x, &vn->y, &vn->z);
        } else if (strncmp(ptr, "mtllib ", 7) == 0) {
            ptr += 7;
            char* mtl_path = concat_path(base_dir, ptr);
            parse_mtl(mtl_path, &materials);
            free(mtl_path);
        } else if (strncmp(ptr, "usemtl ", 7) == 0) {
            ptr += 7;
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            char* name_end = ptr;
            while (*name_end && !isspace(*name_end)) name_end++;
            *name_end = '\0';
            
            if (current_group.vertex_count > 0) {
                /* Push current */
                spr_mesh_group_t* g = da_push(&groups);
                *g = current_group;
                /* Start new */
                current_group.start_vertex += current_group.vertex_count;
                current_group.vertex_count = 0;
            }
            
            current_group.material = find_material(&materials, ptr);
            
        } else if (strncmp(ptr, "f ", 2) == 0) {
            dyn_array_t indices; da_init(&indices, sizeof(obj_index_t));
            ptr += 2;
            
            while (*ptr) {
                while (*ptr == ' ' || *ptr == '\t') ptr++;
                if (*ptr == '\n' || *ptr == '\0') break;
                
                obj_index_t idx = {0, 0, 0};
                int consumed;
                
                if (sscanf(ptr, "%d/%d/%d%n", &idx.v_idx, &idx.vt_idx, &idx.vn_idx, &consumed) == 3) {}
                else if (sscanf(ptr, "%d//%d%n", &idx.v_idx, &idx.vn_idx, &consumed) == 2) {}
                else if (sscanf(ptr, "%d/%d%n", &idx.v_idx, &idx.vt_idx, &consumed) == 2) {}
                else if (sscanf(ptr, "%d%n", &idx.v_idx, &consumed) == 1) {}
                else break;
                
                obj_index_t* new_idx = da_push(&indices);
                *new_idx = idx;
                ptr += consumed;
            }
            
            if (indices.count >= 3) {
                obj_index_t* idx_list = (obj_index_t*)indices.data;
                for (int i = 1; i < indices.count - 1; ++i) {
                    obj_index_t tri[3];
                    tri[0] = idx_list[0];
                    tri[1] = idx_list[i];
                    tri[2] = idx_list[i+1];
                    
                    for (int k=0; k<3; ++k) {
                        spr_vertex_t* out_v = da_push(&vertices);
                        current_group.vertex_count++;
                        
                        vec3_t* p = da_get(&pos_list, tri[k].v_idx - 1);
                        if (p) out_v->position = *p;
                        
                        vec3_t* n = da_get(&norm_list, tri[k].vn_idx - 1);
                        if (n) out_v->normal = *n;
                        else { out_v->normal.x=0; out_v->normal.y=1; out_v->normal.z=0; }
                        
                        vec2_t* uv = da_get(&uv_list, tri[k].vt_idx - 1);
                        if (uv) out_v->uv = *uv;
                        else { out_v->uv.x=0; out_v->uv.y=0; }
                    }
                }
            }
            da_free(&indices);
        }
    }
    
    /* Push final group */
    if (current_group.vertex_count > 0) {
        spr_mesh_group_t* g = da_push(&groups);
        *g = current_group;
    }
    
    fclose(f);
    free(base_dir);
    da_free(&pos_list);
    da_free(&uv_list);
    da_free(&norm_list);
    
    spr_mesh_t* mesh = (spr_mesh_t*)malloc(sizeof(spr_mesh_t));
    mesh->type = SPR_MESH_OBJ;
    mesh->vertex_count = vertices.count;
    mesh->vertices = vertices.data;
    mesh->group_count = groups.count;
    mesh->groups = groups.data;
    mesh->material_count = materials.count;
    mesh->materials = materials.data;
    mesh->texture = NULL;
    
    printf("Loaded OBJ: %d vertices, %d groups, %d materials\n", 
           mesh->vertex_count, mesh->group_count, mesh->material_count);
    
    return mesh;
}

/* --- Public API --- */

spr_mesh_t* spr_load_mesh(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return NULL;
    
    if (strcasecmp(ext, ".obj") == 0) {
        return load_obj(filename);
    } else if (strcasecmp(ext, ".stl") == 0) {
        stl_object_t* stl = stl_load(filename);
        if (!stl) return NULL;
        
        spr_mesh_t* mesh = (spr_mesh_t*)malloc(sizeof(spr_mesh_t));
        mesh->type = SPR_MESH_STL;
        mesh->vertex_count = stl->vertex_count;
        mesh->vertices = stl->vertices; 
        
        /* Create 1 default group for STL */
        mesh->group_count = 1;
        mesh->groups = (spr_mesh_group_t*)malloc(sizeof(spr_mesh_group_t));
        mesh->groups[0].material = NULL;
        mesh->groups[0].start_vertex = 0;
        mesh->groups[0].vertex_count = mesh->vertex_count;
        
        mesh->material_count = 0;
        mesh->materials = NULL;
        mesh->texture = NULL;
        free(stl);
        return mesh;
    }
    return NULL;
}

void spr_free_mesh(spr_mesh_t* mesh) {
    if (mesh) {
        if (mesh->vertices) free(mesh->vertices);
        if (mesh->groups) free(mesh->groups);
        
        if (mesh->materials) {
            for (int i=0; i<mesh->material_count; ++i) {
                if (mesh->materials[i].map_Kd) spr_texture_free(mesh->materials[i].map_Kd);
                if (mesh->materials[i].map_Ks) spr_texture_free(mesh->materials[i].map_Ks);
            }
            free(mesh->materials);
        }
        
        if (mesh->texture) spr_texture_free(mesh->texture);
        free(mesh);
    }
}