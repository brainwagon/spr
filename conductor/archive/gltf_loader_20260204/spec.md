# Specification: glTF 2.0 Loader for SPR

## 1. Overview
This track introduces support for the **glTF 2.0 (GL Transmission Format)** to `libspr`. glTF is the industry-standard "JPEG for 3D," offering a compact, interoperable format for 3D scenes. This implementation will focus on the **binary glTF (.glb)** format and **core JSON (.gltf)** support, enabling `libspr` to load static meshes, nodes, and basic physically-based materials (PBR) compatible with its existing pipeline.

## 2. Goals
- **Load Geometry:** Parse mesh data (vertices, normals, UVs, indices) from glTF/GLB files.
- **Scene Hierarchy:** Support the loading of scene nodes and their hierarchical transformations (local/world matrices).
- **Materials:** Map glTF PBR metallic-roughness materials to `spr_material` structures (Base Color -> Diffuse, etc.).
- **Integration:** seamlessly integrate with the existing `spr_loader` API or provide a parallel `spr_load_gltf` function.
- **Dependency Management:** Use a lightweight, single-header JSON parser (e.g., `cgltf` or `cJSON`) or a minimal custom parser to maintain the "minimal dependencies" philosophy.

## 3. Detailed Requirements

### 3.1. Parsing & Validation
- **JSON Parsing:** Parse the JSON structure of `.gltf` files.
- **Binary Parsing:** Handle the GLB binary chunk/header format.
- **Buffer Views & Accessors:** Correctly interpret `bufferViews` and `accessors` to extract raw vertex and index data.
- **Validation:** Basic validation of the magic number (for GLB) and version (2.0).

### 3.2. Geometry Support
- **Attributes:** Support `POSITION`, `NORMAL`, and `TEXCOORD_0`.
- **Primitives:** Support `TRIANGLES` mode. (Strip/Fan can be optional or converted).
- **Indices:** Support `unsigned short` and `unsigned int` indices.

### 3.3. Material Mapping
- **PBR Conversion:** Convert glTF's `pbrMetallicRoughness` model to SPR's lighting model:
    - `baseColorTexture` -> Diffuse Map
    - `baseColorFactor` -> Diffuse Color
    - `normalTexture` -> Normal Map (if supported by SPR shaders)
    - Ignore unsupported properties (e.g., occlusion, emission) for the MVP, or map them to best-fit placeholders.

### 3.4. API Changes
- **New Function:** `spr_model_t* spr_load_gltf(const char* filename);`
- **Error Handling:** Return `NULL` on failure and use the established internal error code mechanism.

## 4. Constraints
- **Performance:** Parsing should be efficient enough for runtime loading.
- **Memory:** Use existing `spr` memory management patterns (chunk allocators if applicable, or standard malloc/free for the loader transient data).
- **Standards:** Strictly adhere to the glTF 2.0 specification for the subset of features supported.

## 5. Testing Plan
- **Unit Tests:** Test parsing of minimal valid GLTF/GLB files.
- **Integration Tests:** Load standard sample models (e.g., "Box", "Duck", "DamagedHelmet") and verify vertex counts/material properties.
- **Visual Verification:** Update the `viewer` app to accept `.gltf`/`.glb` files and visually verify rendering against reference images.
