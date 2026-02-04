# Specification: glTF Node Transform Support

## 1. Overview
The current glTF loader implementation ignores the scene hierarchy and node transformations (translation, rotation, scale), leading to incorrect rendering of complex models where meshes are positioned by nodes. This track addresses this by implementing full scene graph traversal and applying world-space transformations to vertices during loading.

## 2. Goals
- **Scene Traversal:** Iterate through the glTF scene's root nodes and recursively visit children.
- **Transform Calculation:** Compute the local transform matrix for each node and accumulate it with the parent's world transform.
- **Vertex Transformation:** Apply the calculated world transform to vertex positions and normals/tangents during mesh extraction.
- **Correctness:** Ensure models like `ABeautifulGame.glb` render with correct geometry placement.

## 3. Detailed Requirements
- **Recursion:** Use a recursive function `process_node(node, parent_transform)` to handle the hierarchy.
- **Matrix Math:**
    - Convert `cgltf` float arrays to `mat4_t`.
    - Use `spr_mat4_mul` for transform accumulation.
    - Transform `POSITION` by the full matrix.
    - Transform `NORMAL` and `TANGENT` by the rotation/scale part (inverse transpose for non-uniform scale, though for MVP uniform scale assumption might suffice, or just using the rotation part if scale is uniform). *Refinement: Use full matrix for normal but re-normalize, or better, calculate normal matrix.*
- **Flattening:** Continue to flatten the entire scene into a single `spr_mesh_t` for the current renderer architecture.

## 4. Verification
- **Visual:** `viewer ABeautifulGame.glb` should show the complete, coherent model.
- **Data:** Vertex counts should match the full scene (already fixed in previous step, but transforms must be correct).
