# Possible Optimizations — Path1 VU1 Rendering Pipeline

Current throughput: **~1.4 million triangles/second**

---

## 1. High Impact / Low Effort

~~### 1.1 Eliminate Per-Chunk Constant Re-upload ~~ Done

`RenderChunck()` re-uploads **14 qwords of constants** (MVP, static packet, model matrix, light, TEX0, camera) for every 27-triangle chunk — even when they don't change between chunks of the same mesh.

**Fix**: Upload constants once per mesh (or per model) using a separate VIF unpack to the non-double-buffered region (addresses 0–14), then only send vertex data in subsequent chunks.

**Expected gain**: ~15–20% DMA bandwidth reduction per chunk + less EE packet-building time.

---

### 1.2 Reduce VU1 Lighting Cost

The `CalculateLightingSpecular` macro dominates VU1 time — 3 RSQRT stalls (7 cycles each = 21 stall cycles per vertex, 63 per triangle) plus ~70 instructions per vertex.

Options:
- **Drop specular for non-shiny surfaces** — Use `draw3d.vcl` (ambient+diffuse only, ~30 instr/vertex vs ~70). This alone could yield **2× vertex throughput**.
- **Reduce specular power** — 5 chained `MUL.x` gives pow(N·H, 32). Going to pow(8) (3 muls) saves instructions and reduces pressure near RSQRT stalls.
- **Interleave RSQRT waits** — The 3 normalizations are sequential. Restructure to issue RSQRT for vertex N+1's normal while computing specular for vertex N.
- **Move specular to a texture** (environment/specular map) — eliminates 2 RSQRTs per vertex entirely.
- **Pre-normalize normals at load time** — eliminates the normal→world normalize RSQRT (the 3rd one).

---

### 1.3 CPU-Side Frustum Culling (Bounding Sphere/AABB)

Currently all models are unconditionally dispatched to VU1. A simple bounding-sphere test against the 6 frustum planes on the EE would skip entire models/meshes that are off-screen.

**Cost**: ~20 MUL+ADD per model.  
**Savings**: Eliminates all DMA + VU1 + GS cost for invisible objects.

---

## 2. DMA Transfer Optimizations

### 2.1 VIF Unpack Packing (Smaller Formats on the Bus)

VIF unpack decompresses data back to full 128-bit qwords in VU1 data memory. This saves **DMA bandwidth** but **not** VU1 DMEM space — the VU1 program doesn't need to change.

| Data | Current Format | Packed Format | Bus Saving |
|---|---|---|---|
| ST coords | V4-32 (S, T, 1.0, 0.0) | V2-32 (S, T only) | 50% per vertex |
| Normals | V4-32 (nx, ny, nz, 0) | V3-32 | 25% per vertex |
| Positions | V4-32 (x, y, z, 1.0) | V3-32 + ROW mode (w=1.0 via STROW) | 25% per vertex |

**Notes**:
- Positions need VIF ROW register (`STROW` sets ROW.w = 1.0, then unpack with R flag adds ROW to missing components).
- Normals are clean candidates — w=0 is exactly what VIF gives with V3-32.
- ST coords are the best candidate — S and T only, Q=1.0 can be reconstructed in VU1 or via ROW mode.

---

### 2.2 Remove DMA Serialization

Current code waits for **both** DMA channels before every send:
```cpp
dma_channel_wait(DMA_CHANNEL_VIF1, 0);
dma_channel_wait(DMA_CHANNEL_GIF, 0);
```

The GIF wait is unnecessary — VIF1 and GIF are independent paths. The VIF1 wait could be moved earlier (only wait when about to reuse the same packet buffer). This breaks the designed 3-stage pipeline overlap.

---

## 3. Triangle Strips

### 3.1 Why Strips Help

| Format | Vertices for N triangles |
|---|---|
| Triangle list | 3N |
| Triangle strip | N + 2 (ideal), ~1.5–1.8N with degenerate connectors |

The GS has native `PRIM_TRIANGLE_STRIP` mode with automatic winding alternation.

### 3.2 Transitioning to Strips

**Step 1 — Stripify at load time:**
Use a stripification algorithm (NvTriStrip or greedy strip builder) after loading OBJ. Store strips + restart indices instead of flat triangle lists.

**Step 2 — Modify VU1 program:**

- **Option A (simple)**: Change `PRIM_TRIANGLE` → `PRIM_TRIANGLE_STRIP` in GIF tag. VU1 still outputs 1 vertex at a time. For degenerate connectors, set ADC bit. Lighting still runs per-vertex but fewer total vertices.
- **Option B (advanced)**: Cache last 2 transformed + lit vertices. Per iteration: transform 1 new vertex, cull using 3-vertex window, light 1 vertex only. Reduces VU1 lighting from 3× to 1× per triangle.

**Step 3 — Backface culling in strip mode:**
Winding alternates every triangle. Flip the sign comparison on odd-numbered triangles (one extra `IADD` + conditional negate on the cross product).

---

## 4. Multiple Lights — How PS2 Games Did It

### 4.1 Common Industry Approaches

| Strategy | VU1 Cost/Vertex | DMA Cost | GS Cost | Used By |
|---|---|---|---|---|
| Pre-baked vertex colors / lightmaps | ~5 instr (transform only) | 1× | 1× | Most static geometry |
| Single directional on VU1 + env map for specular | ~15 instr | 1× | 1× | Gran Turismo 3, Jak & Daxter |
| Multi-pass additive blending | ~15 instr × N passes | N× | N× fill | Later-gen dynamic lights |
| EE computes lighting → VU1 passthrough | ~5 instr on VU1 | 1× | 1× | God of War, Shadow of the Colossus |
| VU1 diffuse + specular via texture | ~15 instr | 1× | 1× | Ratchet & Clank |

### 4.2 Multi-Pass Pattern for Multiple Dynamic Lights

```
For each mesh:
  Pass 0 (base): MVP transform + ambient + primary light (diffuse only)
    → Triangle strip, opaque write to framebuffer
  Pass 1..N (additive): MVP transform + one light each
    → Same strip, alpha blend ADD onto framebuffer
    → Z-test = EQUAL (no Z-write, only shade already-visible pixels)
```

Why this works with strips:
- Each pass processes the same strip (vertex order doesn't change)
- VU1 program per pass is simple (transform + single dot product)
- GS does accumulation via blending hardware
- Diminishing returns after 2–3 lights at PS2 resolution

### 4.3 Cost Comparison

| Approach | VU1 cost/vertex | DMA cost | GS cost |
|---|---|---|---|
| Current (1 light, full specular) | ~70 instr | 1× | 1× |
| Strip + diffuse only | ~15 instr | 0.5× | 1× |
| Strip + multi-pass (3 lights) | ~15 instr × 3 passes | 1.5× | 3× fill |
| EE lighting + strip passthrough | ~5 instr on VU1 | 0.5× | 1× |

---

## 5. Medium Impact Optimizations

### 5.1 VU0 Macro Mode for EE Matrix Math

`Model::Update()` does scalar matrix multiplications (rotateX, rotateY, rotateZ, translate, then MVP = world × viewProj). VU0 inline assembly can do 4×4 matrix multiply in ~16 instructions vs dozens of scalar FP ops. Frees EE time for packet building.

### 5.2 Skip Redundant TEX0 Per Chunk

TEX0 GIF register + `gifAdTag` are written to the output buffer for every chunk. If all chunks in a mesh use the same texture, send TEX0 once at the start and remove it from per-chunk output (saves 2 qwords of output + GIF AD setup).

### 5.3 Increase Chunk Size

At 81 vertices (27 triangles), per-chunk overhead is amortized over very few triangles. The double-buffer is `(1024 - 15) / 2 = 504 qwords`. Current max is ~83 vertices — already near the limit. To increase:
- Separate input/output buffers (triple-buffer scheme)
- Or reduce per-vertex data width via VIF packing

---

## 6. Long-Term Architectural

### 6.1 Indexed Geometry

Typical models have 2–3× vertex duplication in deindexed format. Indexed format cuts DMA by 50–66% and avoids reprocessing shared vertices. Requires VU1 to dereference indices — more complex program but massive bandwidth win.

### 6.2 LOD System

Swap lower-poly meshes for distant objects. Simple distance-based LOD with 2–3 levels per model.

### 6.3 EE-Side Backface Pre-Cull

For closed meshes (>50% backfaces), pre-cull on EE and don't send those triangles at all. Trade: CPU time vs DMA bandwidth. Only worthwhile if DMA is the bottleneck.

---

## Recommended Priority Path

1. **Constant dedup** — easiest win, directly reduces per-chunk overhead
2. **Remove GIF wait** — one-line change, improves pipeline overlap
3. **Frustum culling** — low effort, eliminates entire invisible models
4. **Move specular to texture + pre-normalize normals** — eliminates 2/3 RSQRT stalls
5. **Triangle strips with diffuse-only VU1** — vertex throughput jumps ~3–4×
6. **Multi-pass for additional lights** — scales cleanly with strip architecture

This progression mirrors how Jak & Daxter and Ratchet & Clank achieved their polygon counts on the same hardware.
