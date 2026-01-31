# A-Buffer / Transparency Plan

## Goal
Implement Order-Independent Transparency (OIT) using an A-Buffer approach. The renderer will store a list of fragments per pixel, sorted by Z, and composite them using per-channel opacity.

## 1. Data Structures

### `spr.h` / `spr.c`
*   **`spr_fragment_t`**: Node structure.
    *   `float z`: Depth.
    *   `vec3_t color`: RGB color (float precision preferred for compositing).
    *   `vec3_t opacity`: RGB opacity (3-tuple as requested).
    *   `struct spr_fragment_t* next`: Linked list pointer.
*   **Context Updates**:
    *   `spr_fragment_t** fragment_buffer`: Array of pointers (size = width * height). Replaces/augments the Z-buffer.
    *   **Memory Pool**: A linear allocator (arena) to efficiently allocate fragments per frame without `malloc` overhead.
        *   `spr_fragment_t* fragment_pool`: Large array.
        *   `size_t pool_cursor`: Current offset.

## 2. API Changes

*   **`spr_fs_output_t`**: New struct for Fragment Shader return type (replacing `spr_color_t`).
    *   `vec3_t color`
    *   `vec3_t opacity`
*   **`spr_uniforms_set_opacity`**: Helper to set opacity uniform.
*   **`spr_resolve`**: New function to trigger the sort-and-composite pass.

## 3. Shader Updates

*   **Fragment Shaders**: Update signature to return `spr_fs_output_t`.
*   **Implementations**: Update `plastic`, `matte`, etc., to populate the output struct.
    *   Read `u->opacity` (new uniform).
    *   Output `color = generated_color * opacity` (pre-multiplied? User said "output... = generated * opacity").
    *   Wait, standard premultiplied alpha usually implies `color * alpha` stored in RGB. The user prompt says "output color should be generated color * opacity". So we store that.

## 4. Rasterizer Logic

*   **Pass 1 (Rasterization)**:
    *   Calculate Z and execute Fragment Shader.
    *   Allocate `spr_fragment_t` from pool.
    *   Fill data.
    *   Atomic insertion into linked list? (For SIMD/threading).
        *   Current SIMD rasterizer is single-threaded (just using SSE instructions). So simple linked list insertion `node->next = head[idx]; head[idx] = node;` is safe.
    *   **Optimization**: Do we keep Z-buffer?
        *   If we want to support mixing Opaque and Transparent objects efficiently:
            *   Draw Opaque first (write Z).
            *   Draw Transparent (test Z, but don't write Z? Or add to list?).
        *   The user says "each pixel will maintain a sorted list...". This implies everything goes into the list.
        *   *Decision*: For simplicity, we will push EVERYTHING into the list and sort later. (Pure A-buffer). This is slower but correct for OIT.

## 5. Resolve Pass (`spr_resolve`)

*   Iterate every pixel.
*   Get the list head.
*   **Sort**: Use Insertion Sort (efficient for small N). Sort by Z (Farthest to Nearest).
*   **Composite**:
    *   Initialize `accum` (Background color).
    *   Loop `node` from Far to Near:
        *   `accum.r = node.color.r + accum.r * (1.0 - node.opacity.r)`
        *   (Assumes `node.color` is already `base * opacity`, i.e., premultiplied).
*   Write `accum` to `ctx->fb.color_buffer`.

## Execution Steps
1.  Modify `spr.h` (Types, API).
2.  Modify `spr.c` (Context, Allocator, Rasterizer, Resolve).
3.  Modify `spr_shaders.h/c` (Signatures, Opacity logic).
4.  Modify `viewer.c` (Call `spr_resolve`, set opacity).
