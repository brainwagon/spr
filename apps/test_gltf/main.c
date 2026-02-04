#include "spr_gltf_loader.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "cgltf.h"

void test_basic_parsing() {
    printf("Testing basic GLB parsing with triangle.glb...\n");
    
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, "apps/test_gltf/triangle.glb", &data);
    
    assert(result == cgltf_result_success);
    assert(data != NULL);
    assert(data->asset.version != NULL);
    assert(strcmp(data->asset.version, "2.0") == 0);
    
    printf("Pass: Successfully parsed triangle.glb asset version.\n");
    
    /* Load buffers */
    result = cgltf_load_buffers(&options, data, "apps/test_gltf/triangle.glb");
    assert(result == cgltf_result_success);
    
    assert(data->meshes_count == 1);
    assert(data->meshes[0].primitives_count == 1);
    
    printf("Pass: Successfully loaded buffers and found mesh.\n");
    
    cgltf_free(data);
}

void test_spr_load_gltf() {
    printf("Testing spr_load_gltf with triangle.glb...\n");
    
    spr_mesh_t* mesh = spr_load_gltf("apps/test_gltf/triangle.glb");
    assert(mesh != NULL);
    assert(mesh->vertex_count == 3);
    assert(mesh->group_count == 1);
    assert(mesh->vertices != NULL);
    
    spr_vertex_t* vertices = (spr_vertex_t*)mesh->vertices;
    /* First vertex of our generated triangle: -1.0, -1.0, 0.0 */
    assert(vertices[0].position.x == -1.0f);
    assert(vertices[0].position.y == -1.0f);
    assert(vertices[0].position.z == 0.0f);
    
    printf("Pass: spr_load_gltf correctly extracted vertices.\n");
    
    /* Cleanup (we should implement spr_free_mesh correctly if it doesn't handle this) */
    /* Looking at spr_loader.h, there is spr_free_mesh. */
    spr_free_mesh(mesh);
    printf("Pass: Cleaned up mesh.\n");
}

int main() {
    printf("Running glTF Loader Tests...\n");

    /* Test with non-existent file */
    spr_mesh_t* mesh = spr_load_gltf("non_existent.gltf");
    assert(mesh == NULL);
    printf("Pass: Loading non-existent file returns NULL.\n");

    test_basic_parsing();
    test_spr_load_gltf();

    printf("glTF Loader Tests Passed.\n");
    return 0;
}