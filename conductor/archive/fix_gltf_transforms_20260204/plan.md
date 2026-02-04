# Implementation Plan - Track: Fix glTF Transforms

## Phase 1: Scene Traversal & Transforms [checkpoint: 978b8d8]
- [x] Task: Implement matrix conversion helpers.
    - [x] Sub-task: Create `cgltf_to_mat4` helper.
    - [x] Sub-task: Create `transform_point` and `transform_vector` helpers.
- [x] Task: Implement recursive node processing.
    - [x] Sub-task: Replace linear mesh iteration with recursive `process_node` function.
    - [x] Sub-task: Implement pass 1 (counting vertices/indices) using recursion.
    - [x] Sub-task: Implement pass 2 (extracting and transforming vertices) using recursion.
- [x] Task: Conductor - User Manual Verification 'Scene Traversal & Transforms' (Protocol in workflow.md) 978b8d8
