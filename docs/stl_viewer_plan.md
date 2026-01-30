# STL Viewer Development Plan

## Goal
Create a real-time interactive STL model viewer using the `spr` library and **SDL2** for window management and input.

## Libraries
-   **SPR**: Existing software polygon renderer.
-   **SDL2**: For creating a window, handling mouse/keyboard events, and displaying the `spr` framebuffer.

## Features
1.  **STL Loading**: Support for reading STL files (Binary and/or ASCII) and converting them into a format `spr` can consume (`vertex_t` array).
    *   *Note: The `cat` directory currently contains an .obj file, so we will implement an STL loader as requested, but we may need to convert the cat model or use a procedural test object if no STL is provided.*
2.  **Interactive View Control**:
    *   **Orbit**: Left Mouse Button Drag (Rotates model around center).
    *   **Zoom**: Mouse Wheel (Scales model or moves camera).
    *   **Pan**: Right Mouse Button Drag (Moves model in screen plane).
3.  **Rendering**:
    *   **Shader**: "Plastic" look.
        *   Color: Grey (e.g., RGB 180, 180, 180).
        *   Lighting: Directional Light (Fixed).
        *   Components: Ambient + Diffuse (N dot L) + Specular (Phong).
    *   **Mesh**: Calculated per-face normals (flat shading) or averaged vertex normals if smooth shading is desired. For a simple STL viewer, flat normals (per triangle) are standard and easiest since STL doesn't store normals reliable or connectivity.

## Architecture

### 1. `stl.h` / `stl.c`
- [x] **Structs**: `stl_object_t` containing vertex count and pointer to vertices/normals.
- [x] **Functions**: `stl_load(filename)`: Detects ASCII vs Binary, parses, calculates normals (if missing or for flat shading), returns object.

### 2. `viewer.c` (Main Application)
- [x] **Setup**: Init SDL2, Init `spr`.
- [x] **State**: `view_rot_x`, `view_rot_y`, `view_dist`, `view_pan_x`, `view_pan_y`.
- [x] **Loop**:
    1.  Handle SDL Events (Mouse Motion, Wheel, Quit).
    2.  Update Transform Matrices in `spr` based on State.
    3.  Clear Buffer.
    4.  Draw STL using `spr_draw_triangles` and the Plastic Shader.
    5.  Update SDL Texture with `spr` framebuffer.
    6.  Render SDL Texture.

## Implementation Steps

### Step 1: SDL2 Integration
- [x] Create a basic main loop that opens a window and displays the `spr` test output (e.g., the pyramid) in real-time.

### Step 2: STL Loader
- [x] Implement a loader for Binary STL (simplest and most common).
- [x] Add support for generating flat normals for the triangles.

### Step 3: Interaction
- [x] Implement the mouse event handling to update rotation and zoom variables.
- [x] Apply these variables to the `spr` view matrix (`spr_lookat`, `spr_rotate`, etc.).

### Step 4: Plastic Shader
- [x] Implement a new Vertex/Fragment shader pair in `viewer.c`.
- [x] **Vertex Shader**: Transform position, pass normal to fragment shader.
- [x] **Fragment Shader**: Calculate lighting:
    `LightDir = normalize(1, 1, 1)`
    `Diffuse = max(dot(N, L), 0)`
    `Specular = pow(max(dot(R, V), 0), Shininess)`
    `Color = Grey * (Ambient + Diffuse) + White * Specular`

### Step 5: Verification
- [x] Load a test STL (or a generated one if file missing) and verify controls and rendering.