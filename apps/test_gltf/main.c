#include "spr_gltf_loader.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "cgltf.h"

/* Smallest possible valid PNG (1x1 white pixel) */
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
    assert(tex != NULL);
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

void test_spr_load_gltf_textured() {
    printf("Testing spr_load_gltf with textured_triangle.glb...\n");
    
    spr_mesh_t* mesh = spr_load_gltf("apps/test_gltf/textured_triangle.glb");
    assert(mesh != NULL);
    assert(mesh->vertex_count == 3);
    assert(mesh->material_count == 1);
    
    spr_material_t* mat = &mesh->materials[0];
    /* baseColorFactor: [1.0, 0.5, 0.5, 1.0] */
    assert(mat->Kd.x > 0.99f);
    assert(mat->Kd.y > 0.49f && mat->Kd.y < 0.51f);
    
    assert(mat->map_Kd != NULL);
    assert(mat->map_Kd->width == 1);
    
    printf("Pass: spr_load_gltf correctly extracted materials and embedded textures.\n");
    
    spr_free_mesh(mesh);
    printf("Pass: Cleaned up textured mesh.\n");
}

int main() {
    printf("Running glTF Loader Tests...\n");

    test_texture_from_memory();
    test_spr_load_gltf_textured();

    printf("glTF Loader Tests Passed.\n");
    return 0;
}