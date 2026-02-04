#include "spr_gltf_loader.h"
#include <stdio.h>
#include <assert.h>

int main() {
    printf("Running glTF Loader Tests...\n");

    /* Test with non-existent file */
    spr_mesh_t* mesh = spr_load_gltf("non_existent.gltf");
    assert(mesh == NULL);
    printf("Pass: Loading non-existent file returns NULL.\n");

    printf("glTF Loader Tests Passed (Stub phase).\n");
    return 0;
}