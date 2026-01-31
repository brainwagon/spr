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
    /* file might have newline/whitespace at end */
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

/* --- OBJ Loader --- */

typedef struct {
    int v_idx;
    int vt_idx;
    int vn_idx;
} obj_index_t;

static spr_texture_t* load_mtl(const char* mtl_path) {
    FILE* f = fopen(mtl_path, "r");
    if (!f) return NULL;
    
    char line[MAX_LINE];
    char* dir = get_base_dir(mtl_path);
    spr_texture_t* tex = NULL;
    
    while (fgets(line, sizeof(line), f)) {
        char* ptr = line;
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '#' || *ptr == '\n' || *ptr == '\0') continue;
        
        if (strncmp(ptr, "map_Kd", 6) == 0) {
            /* Found texture map */
            ptr += 6;
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            char* tex_path = concat_path(dir, ptr);
            printf("SPR Loader: Loading texture %s\n", tex_path);
            tex = spr_texture_load(tex_path);
            free(tex_path);
            /* Only load first one found */
            break; 
        }
    }
    
    free(dir);
    fclose(f);
    return tex;
}

static spr_mesh_t* load_obj(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Failed to open OBJ: %s\n", filename);
        return NULL;
    }
    
    printf("SPR Loader: Parsing OBJ %s...\n", filename);
    
    dyn_array_t pos_list; da_init(&pos_list, sizeof(vec3_t));
    dyn_array_t uv_list;  da_init(&uv_list, sizeof(vec2_t));
    dyn_array_t norm_list; da_init(&norm_list, sizeof(vec3_t));
    dyn_array_t vertices; da_init(&vertices, sizeof(spr_vertex_t));
    
    spr_texture_t* texture = NULL;
    char* base_dir = get_base_dir(filename);
    
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
            if (!texture) texture = load_mtl(mtl_path);
            free(mtl_path);
        } else if (strncmp(ptr, "f ", 2) == 0) {
            /* Parse Face */
            dyn_array_t indices; da_init(&indices, sizeof(obj_index_t));
            ptr += 2;
            
            while (*ptr) {
                while (*ptr == ' ' || *ptr == '\t') ptr++;
                if (*ptr == '\n' || *ptr == '\0') break;
                
                obj_index_t idx = {0, 0, 0};
                int consumed;
                
                /* Scan v/vt/vn */
                if (sscanf(ptr, "%d/%d/%d%n", &idx.v_idx, &idx.vt_idx, &idx.vn_idx, &consumed) == 3) {
                    /* Good */
                } else if (sscanf(ptr, "%d//%d%n", &idx.v_idx, &idx.vn_idx, &consumed) == 2) {
                    /* v//vn */
                } else if (sscanf(ptr, "%d/%d%n", &idx.v_idx, &idx.vt_idx, &consumed) == 2) {
                    /* v/vt */
                } else if (sscanf(ptr, "%d%n", &idx.v_idx, &consumed) == 1) {
                    /* v */
                } else {
                    break;
                }
                
                obj_index_t* new_idx = da_push(&indices);
                *new_idx = idx;
                ptr += consumed;
            }
            
            /* Triangulate (Fan) */
            if (indices.count >= 3) {
                obj_index_t* idx_list = (obj_index_t*)indices.data;
                for (int i = 1; i < indices.count - 1; ++i) {
                    obj_index_t tri[3];
                    tri[0] = idx_list[0];
                    tri[1] = idx_list[i];
                    tri[2] = idx_list[i+1];
                    
                    for (int k=0; k<3; ++k) {
                        spr_vertex_t* out_v = da_push(&vertices);
                        
                        /* Position (Required) */
                        vec3_t* p = da_get(&pos_list, tri[k].v_idx - 1);
                        if (p) out_v->position = *p;
                        else { out_v->position.x=0; out_v->position.y=0; out_v->position.z=0; }
                        
                        /* Normal */
                        vec3_t* n = da_get(&norm_list, tri[k].vn_idx - 1);
                        if (n) out_v->normal = *n;
                        else { out_v->normal.x=0; out_v->normal.y=1; out_v->normal.z=0; }
                        
                        /* UV */
                        vec2_t* uv = da_get(&uv_list, tri[k].vt_idx - 1);
                        if (uv) out_v->uv = *uv;
                        else { out_v->uv.x=0; out_v->uv.y=0; }
                    }
                }
            }
            da_free(&indices);
        }
    }
    
    fclose(f);
    free(base_dir);
    da_free(&pos_list);
    da_free(&uv_list);
    da_free(&norm_list);
    
    spr_mesh_t* mesh = (spr_mesh_t*)malloc(sizeof(spr_mesh_t));
    mesh->type = SPR_MESH_OBJ;
    mesh->vertex_count = vertices.count;
    mesh->vertices = vertices.data; /* Transferred ownership */
    mesh->texture = texture;
    
    printf("Loaded OBJ: %d vertices, Texture: %s\n", mesh->vertex_count, texture ? "Yes" : "No");
    
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
        mesh->vertices = stl->vertices; /* Transferred ownership */
        mesh->texture = NULL;
        free(stl); /* Free wrapper, keep vertices */
        return mesh;
    }
    return NULL;
}

void spr_free_mesh(spr_mesh_t* mesh) {
    if (mesh) {
        if (mesh->vertices) free(mesh->vertices);
        if (mesh->texture) spr_texture_free(mesh->texture);
        free(mesh);
    }
}
