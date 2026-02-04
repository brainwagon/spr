#include "spr_gltf_loader.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "cgltf.h"

/* Smallest possible valid PNG (1x1 red pixel) */
const unsigned char tiny_png[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
  0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00,
  0x0c, 0x49, 0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff, 0xff, 0x3f,
  0x00, 0x05, 0xfe, 0x02, 0xfe, 0xdc, 0x44, 0x74, 0x06, 0x00, 0x00, 0x00,
  0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

void test_texture_from_memory() {
    printf("Testing spr_texture_load_from_memory...\n");
    spr_texture_t* tex = spr_texture_load_from_memory(tiny_png, sizeof(tiny_png));
    printf("Channels: %d, Pixel: %d %d %d\n", tex->channels, tex->pixels[0], tex->pixels[1], tex->pixels[2]);
    assert(tex->width == 1);
    assert(tex->height == 1);
    assert(tex->channels == 3);
    assert(tex->pixels[0] == 255);
    assert(tex->pixels[1] == 255);
    assert(tex->pixels[2] == 255);
    spr_texture_free(tex);
    printf("Pass: Successfully loaded 1x1 PNG from memory.\n");
}

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
    assert(vertices[0].position.x == -1.0f);
    assert(vertices[0].position.y == -1.0f);
    assert(vertices[0].position.z == 0.0f);
    
    printf("Pass: spr_load_gltf correctly extracted vertices.\n");
    
    spr_free_mesh(mesh);
    printf("Pass: Cleaned up mesh.\n");
}

int main() {
    printf("Running glTF Loader Tests...\n");

    /* Test with non-existent file */
    spr_mesh_t* mesh = spr_load_gltf("non_existent.gltf");
    assert(mesh == NULL);
    printf("Pass: Loading non-existent file returns NULL.\n");

    test_texture_from_memory();
    test_basic_parsing();
    test_spr_load_gltf();

    printf("glTF Loader Tests Passed.\n");
    return 0;
}
