#include "stl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper to read 32-bit float (Little Endian) */
/* Assuming host is Little Endian for simplicity (x86/ARM usually are) */

stl_object_t* stl_load(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open STL file");
        return NULL;
    }

    /* Check header to guess Binary vs ASCII. */
    char header[80];
    if (fread(header, 1, 80, f) != 80) {
        fclose(f);
        return NULL;
    }

    uint32_t triangle_count = 0;
    if (fread(&triangle_count, 4, 1, f) != 1) {
        fclose(f);
        return NULL;
    }

    /* Check file size */
    long current_pos = ftell(f);
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, current_pos, SEEK_SET);

    long expected_size = 80 + 4 + (long)triangle_count * 50;
    
    if (file_size == expected_size) {
        /* Binary STL */
        printf("Loading Binary STL: %u triangles\n", triangle_count);
        
        stl_object_t* obj = (stl_object_t*)malloc(sizeof(stl_object_t));
        if (!obj) { fclose(f); return NULL; }
        
        obj->vertex_count = triangle_count * 3;
        obj->vertices = (stl_vertex_t*)malloc(obj->vertex_count * sizeof(stl_vertex_t));
        if (!obj->vertices) { free(obj); fclose(f); return NULL; }

        for (uint32_t i = 0; i < triangle_count; ++i) {
            float normal[3];
            float v1[3], v2[3], v3[3];
            uint16_t attr;
            size_t res = 0;

            /* Read Normal */
            res += fread(normal, 4, 3, f);
            /* Read Vertices */
            res += fread(v1, 4, 3, f);
            res += fread(v2, 4, 3, f);
            res += fread(v3, 4, 3, f);
            /* Read Attribute */
            res += fread(&attr, 2, 1, f);

            if (res != (3 + 3 + 3 + 3 + 1)) {
                 /* Handle partial read if necessary */
            }

            /* Store */
            int idx = i * 3;
            
            /* Vertex 1 */
            obj->vertices[idx].x = v1[0];
            obj->vertices[idx].y = v1[1];
            obj->vertices[idx].z = v1[2];
            obj->vertices[idx].nx = normal[0];
            obj->vertices[idx].ny = normal[1];
            obj->vertices[idx].nz = normal[2];
            obj->vertices[idx].attr = attr;

            /* Vertex 2 */
            obj->vertices[idx+1].x = v2[0];
            obj->vertices[idx+1].y = v2[1];
            obj->vertices[idx+1].z = v2[2];
            obj->vertices[idx+1].nx = normal[0];
            obj->vertices[idx+1].ny = normal[1];
            obj->vertices[idx+1].nz = normal[2];
            obj->vertices[idx+1].attr = attr;

            /* Vertex 3 */
            obj->vertices[idx+2].x = v3[0];
            obj->vertices[idx+2].y = v3[1];
            obj->vertices[idx+2].z = v3[2];
            obj->vertices[idx+2].nx = normal[0];
            obj->vertices[idx+2].ny = normal[1];
            obj->vertices[idx+2].nz = normal[2];
            obj->vertices[idx+2].attr = attr;
        }
        
        fclose(f);
        return obj;

    } else {
        /* Assume ASCII or invalid. Let's try simple ASCII parsing. */
        printf("Trying ASCII STL loader...\n");
        /* Reset to start */
        fseek(f, 0, SEEK_SET);
        
        /* Count facets */
        char line[256];
        uint32_t facets = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "facet normal")) facets++;
        }
        
        if (facets == 0) {
            printf("Not a valid STL file.\n");
            fclose(f);
            return NULL;
        }
        
        printf("Found %u facets in ASCII.\n", facets);
        
        stl_object_t* obj = (stl_object_t*)malloc(sizeof(stl_object_t));
        if (!obj) { fclose(f); return NULL; }
        
        obj->vertex_count = facets * 3;
        obj->vertices = (stl_vertex_t*)malloc(obj->vertex_count * sizeof(stl_vertex_t));
        if (!obj->vertices) { free(obj); fclose(f); return NULL; }
        
        fseek(f, 0, SEEK_SET);
        
        int v_idx = 0;
        float nx=0, ny=0, nz=0;
        
        while (fgets(line, sizeof(line), f)) {
            char* ptr = line;
            while (*ptr == ' ' || *ptr == '\t') ptr++; /* Skip whitespace */
            
            if (strncmp(ptr, "facet normal", 12) == 0) {
                sscanf(ptr, "facet normal %f %f %f", &nx, &ny, &nz);
            } else if (strncmp(ptr, "vertex", 6) == 0) {
                float vx, vy, vz;
                sscanf(ptr, "vertex %f %f %f", &vx, &vy, &vz);
                if (v_idx < obj->vertex_count) {
                    obj->vertices[v_idx].x = vx;
                    obj->vertices[v_idx].y = vy;
                    obj->vertices[v_idx].z = vz;
                    obj->vertices[v_idx].nx = nx;
                    obj->vertices[v_idx].ny = ny;
                    obj->vertices[v_idx].nz = nz;
                    obj->vertices[v_idx].attr = 0;
                    v_idx++;
                }
            }
        }
        
        fclose(f);
        return obj;
    }
}

void stl_free(stl_object_t* obj) {
    if (obj) {
        if (obj->vertices) free(obj->vertices);
        free(obj);
    }
}