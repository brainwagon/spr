# Implementation Plan - Track: glTF 2.0 Loader

## Phase 1: Dependencies & Infrastructure
- [x] Task: Research and select a minimal JSON parser (e.g., `cgltf` or `parson`) compatible with the project's license and dependency philosophy. 17cdba8
    - [x] Sub-task: Verify candidate libraries against the "minimal dependencies" rule.
    - [x] Sub-task: Vendor the selected library into `src/` (or `src/external/`).
- [x] Task: Create `spr_gltf_loader.h` and `spr_gltf_loader.c` stubs. 0c5cbee
    - [x] Sub-task: Define the public API `spr_load_gltf`.
    - [x] Sub-task: Update `Makefile` to include the new source files.
- [ ] Task: Conductor - User Manual Verification 'Dependencies & Infrastructure' (Protocol in workflow.md)

## Phase 2: Basic Parsing & Geometry
- [ ] Task: Implement GLB header and chunk parsing.
    - [ ] Sub-task: Create a test GLB file (simple triangle).
    - [ ] Sub-task: Write a unit test to verify header parsing (magic, version, length).
    - [ ] Sub-task: Implement the GLB parsing logic.
- [ ] Task: Implement JSON structure parsing (Nodes, Meshes, Accessors).
    - [ ] Sub-task: Map glTF JSON objects to internal C structs.
    - [ ] Sub-task: Implement `bufferView` and `accessor` data extraction logic.
- [ ] Task: Implement Mesh Data Extraction.
    - [ ] Sub-task: Write logic to extract `POSITION` data into `spr_model_t` vertices.
    - [ ] Sub-task: Write logic to extract `INDICES` data into `spr_model_t` indices.
    - [ ] Sub-task: Write a test to load a simple untextured mesh (e.g., a cube) and verify vertex/index counts.
- [ ] Task: Conductor - User Manual Verification 'Basic Parsing & Geometry' (Protocol in workflow.md)

## Phase 3: Material & Texture Support
- [ ] Task: Implement Texture Loading for glTF.
    - [ ] Sub-task: Parse `images` and `textures` nodes.
    - [ ] Sub-task: Integrate with `stb_image` to load texture data from buffers (embedded) or URIs.
- [ ] Task: Implement Material Mapping.
    - [ ] Sub-task: Map `pbrMetallicRoughness` properties to `spr_material_t`.
    - [ ] Sub-task: Handle UV coordinates (`TEXCOORD_0`) extraction.
- [ ] Task: Conductor - User Manual Verification 'Material & Texture Support' (Protocol in workflow.md)

## Phase 4: Integration & Validation
- [ ] Task: Integrate into `spr_loader.c`.
    - [ ] Sub-task: Update the main `spr_load_model` function to detect `.gltf`/`.glb` extensions and dispatch to `spr_load_gltf`.
- [ ] Task: Update Viewer Application.
    - [ ] Sub-task: Allow the viewer to accept glTF files as command-line arguments.
- [ ] Task: Final Validation.
    - [ ] Sub-task: Test with the standard glTF sample models (Box, Duck, etc.).
    - [ ] Sub-task: Verify memory usage and check for leaks.
- [ ] Task: Conductor - User Manual Verification 'Integration & Validation' (Protocol in workflow.md)
