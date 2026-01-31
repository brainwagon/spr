# Advanced A-Buffer Implementation Details

## Overview
The A-Buffer implementation has been upgraded from a simple linked-list to a sorted, front-to-back system with dynamic memory management and occlusion culling.

## 1. Memory Management
*   **Chunks**: Fragments are allocated in blocks (`spr_fragment_chunk_t`) of 4096 items to reduce `malloc` overhead.
*   **Free List**: Freed fragments are pushed to a `ctx->free_list` stack for immediate reuse.
*   **Lifecycle**:
    *   `alloc_fragment`: Checks free list -> current chunk -> new chunk.
    *   `spr_clear`: Resets the pool (currently frees all chunks except the head to ensure clean state).

## 2. Insertion Logic (`insert_fragment`)
*   **Sorting**: Fragments are inserted in **Ascending Z** order (Near to Far).
*   **Occlusion Culling (Early Z)**:
    *   As we traverse the list to find the insertion point, we accumulate opacity (`total_opacity`).
    *   If `min(total_opacity) > 0.999` (Threshold), the insertion point is fully occluded. The new fragment is discarded.
*   **Occlusion Culling (Behind)**:
    *   After inserting, if the new total opacity exceeds the threshold, all subsequent fragments in the list (which are behind the new one) are culled (freed to the free list).

## 3. Resolve Pass (`spr_resolve`)
*   **Front-to-Back Compositing**:
    *   Since the list is sorted Near-to-Far, we use Front-to-Back accumulation equations.
    *   `AccColor += LayerColor * (1.0 - AccOpacity)`
    *   `AccOpacity += LayerOpacity * (1.0 - AccOpacity)`
*   **Early Exit**: The loop breaks early if `AccOpacity` reaches saturation (> 0.999).

## 4. Tuning
*   `SPR_OPACITY_THRESHOLD`: Defined as `0.999f`.
*   `SPR_CHUNK_SIZE`: Defined as `4096`.
