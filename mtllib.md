## Overview 🌈
- In Wavefront OBJ, `mtllib` tells the OBJ file which material library file(s) to load.
- Materials are defined in one or more `.mtl` files using `newmtl ...`.
- The `usemtl` command in the OBJ assigns a named material to subsequent geometry (faces/lines) until another `usemtl` appears.

---

## `mtllib` in OBJ 📚
- Syntax:
  - `mtllib <file1.mtl> [file2.mtl ...]`
  - Example: `mtllib materials.mtl shared/common.mtl`
- Purpose:
  - Declares one or more MTL files that define material names used by `usemtl`.
- Placement:
  - Typically near the top of the OBJ. Can appear multiple times; materials accumulate.
- Path rules:
  - Paths are usually relative to the OBJ file’s location.
  - Prefer forward slashes (`/`) and avoid spaces in filenames for compatibility.
- Multiple files:
  - You can reference several MTL files; all `newmtl` entries across them become available.

---

## `usemtl` in OBJ 🎨
- Syntax:
  - `usemtl <material_name>`
- Scope:
  - Applies to all subsequent elements (faces `f`, lines `l`, curves) until changed.
  - Works independently of object (`o`) or group (`g`) declarations.
- Behavior:
  - If a `usemtl` references a name not found in loaded MTL(s), viewers typically fall back to a default material (no crash).
- Tip:
  - Per-face material assignment is supported by placing `usemtl` between face blocks.

---

## Inside the .MTL file 🧪
- Materials are defined by name:
  - `newmtl <name>` starts a new material block.
- Common properties (Phong/Blinn era; not PBR):
  - `Ka r g b` — ambient color (often ignored by modern viewers)
  - `Kd r g b` — diffuse color
  - `Ks r g b` — specular color
  - `Ke r g b` — emissive color
  - `Ns value` — specular exponent (0–1000 typical)
  - `d value` — dissolve (opacity): 1.0 = opaque, 0.0 = fully transparent
  - `Tr value` — transparency (1 − d); some exporters use this instead of `d`
  - `Ni value` — index of refraction (for refraction-capable illum modes)
  - `Tf r g b` — transmission filter (color tint through transparent material)
  - `illum n` — illumination model (see below)
- Texture maps (filenames relative to the MTL file’s location in most tools):
  - `map_Kd <file>` — diffuse/albedo map
  - `map_Ks <file>` — specular color map
  - `map_Ns <file>` — shininess map
  - `map_d <file>` — opacity map (alpha)
  - `map_Ke <file>` — emission map
  - `map_Bump` or `bump <file>` — bump map (height)
  - `norm <file>` — normal map (nonstandard but widely used)
  - `refl <file>` — reflection/environment map
- Texture options (placed between the keyword and filename; support varies):
  - `-o u v w` offset, `-s u v w` scale, `-bm s` bump scale, `-clamp on|off`, `-blendu on|off`, `-blenv on|off`, `-imfchan r|g|b|a|l|z`
- Illumination models (common values):
  - `illum 0` — color only (no lighting)
  - `illum 1` — diffuse only
  - `illum 2` — diffuse + specular (most common)
  - Higher modes add reflection/refraction variants (support varies by viewer)

---

## Minimal Example 🧩

OBJ (model.obj):
```
# A cube with two materials
mtllib materials.mtl

o Cube
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
v 0 0 1
v 1 0 1
v 1 1 1
v 0 1 1

vt 0 0
vt 1 0
vt 1 1
vt 0 1

vn 0 0 -1
vn 0 0 1
vn 0 -1 0
vn 0 1 0
vn -1 0 0
vn 1 0 0

usemtl RedPaint
f 1/1/1 2/2/1 3/3/1 4/4/1

usemtl Metal
f 5/1/2 8/4/2 7/3/2 6/2/2
```

MTL (materials.mtl):
```
# Red diffuse paint
newmtl RedPaint
Kd 0.8 0.1 0.1
Ks 0.02 0.02 0.02
Ns 10
illum 2
map_Kd textures/red_diffuse.png

# Brushed metal-ish
newmtl Metal
Kd 0.6 0.6 0.6
Ks 0.9 0.9 0.9
Ns 300
illum 2
map_Ks textures/metal_spec.png
norm textures/metal_normal.png
```

---

## Best Practices ✅
- Keep `mtllib` near the top; use relative paths.
- Ensure `newmtl` names match exactly the names used by `usemtl` (case-sensitive).
- Avoid duplicate `newmtl` names across multiple MTLs; if unavoidable, ensure consistent definitions.
- Prefer forward slashes in paths and avoid spaces to maximize compatibility.
- Provide UVs (`vt`) when using texture maps; many viewers require them.
- For transparency, use either `d` or `map_d` (alpha). Some tools ignore `Tr`.
- Expect variation: MTL is loosely standardized; different viewers may handle `Ke`, `norm`, `refl`, and high `illum` modes inconsistently.

---

## Common Gotchas 🐞
- Missing textures: Paths in MTL are often relative to the MTL file, not the OBJ.
- PBR mismatch: MTL is not PBR-native. Conversions to/from metallic/roughness workflows are approximations.
- `Ns` scale differences: Some tools remap shininess differently.
- No per-vertex material: Materials are per-face. Split by `usemtl` as needed.
- Unicode/whitespace: Some legacy loaders fail on spaces or non-ASCII paths.

---

## Quick FAQ ❓
- Can I have multiple `mtllib` lines? Yes; materials accumulate.
- What if `usemtl` appears before `mtllib`? Loaders usually allow it as long as the MTL is declared somewhere before parsing completes; safest to declare `mtllib` first.
- Can one face mix materials? No. Assign materials per face only.
- Are normal maps standard? Not in original spec; `norm` and `bump` are de facto extensions with broad support.
