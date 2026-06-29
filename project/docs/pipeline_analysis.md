# PS2 Graphics Demo — Pipeline Analysis & Benchmarking Guide

## Table of Contents

1. [Rendering Pipeline Overview](#1-rendering-pipeline-overview)
2. [Key Components](#2-key-components)
3. [Pipeline Flow Diagram](#3-pipeline-flow-diagram)
4. [Bottlenecks](#4-bottlenecks)
5. [Optimization Recommendations](#5-optimization-recommendations)
6. [Benchmarking Strategy](#6-benchmarking-strategy)
7. [Reference: VU1 Instruction Costs](#7-reference-vu1-instruction-costs)

---

## 1. Rendering Pipeline Overview

The engine has two renderers but uses **Path 1 (VU1 microprogram)** as the active path:

| Renderer | Program | Features | VU1 Usage |
|----------|---------|----------|-----------|
| **Path 1** (`Path1Renderer3D`) | `draw3d_triangle.vcl` | Blinn-Phong (ambient + diffuse + specular), clip-space backface culling, per-vertex lighting | Full VU1 program with DMA/GIF |
| **Path 3** (`Path3Renderer3D`) | EE-only | Ambient + diffuse only, CPU-side backface culling via cross product | VU0 inline asm for clipping only |

### Active Pipeline: Path 1 (VU1)

```
EE Core (main.cpp)
  │
  ├─► Model::Update()          — World matrices (EE scalar)
  ├─► Path1Renderer3D::RenderFrame()
  │     │
  │     ├─► For each model → mesh → chunk of 81 vertices:
  │     │     ├─ Build VIF1 packet:
  │     │     │   ├─ MVP matrix (4 qwords)
  │     │     │   ├─ Viewport scale + RGBA base (2 qwords)
  │     │     │   ├─ Model matrix (4 qwords)
  │     │     │   ├─ Light direction + intensities (2 qwords)
  │     │     │   ├─ TEX0 config + camera position (2 qwords)  ← per chunk!
  │     │     │   ├─ Vertex positions (N qwords)
  │     │     │   ├─ Vertex normals (N qwords)
  │     │     │   ├─ Texture STQ coords (N qwords)
  │     │     │   └─ MSCAL/MSCNT + END tag
  │     │     └─ DMA VIF1 → VU1 double-buffer
  │     │
  │     ▼
  │   VU1 (draw3d_triangle.vcl) — per triangle:
  │     ├─ Load 3 vertices, 3 STQs, 3 normals
  │     ├─ MVP transform (12 MAC ops)
  │     ├─ Clip detection + ADC tagging (speculative)
  │     ├─ Perspective divide + viewport transform
  │     ├─ Clip-space backface culling (2D cross product)
  │     ├─ FTOI4 → 12:4 fixed-point GS format
  │     ├─ XYZ2 + STQ early store (3 SQ each)
  │     │
  │     ├─ IF backface (IBGTZ → culled:):
  │     │   └─ ISW.w 0x8000 → ADC bit set, skip GS draw
  │     │
  │     └─ ELSE (visible):
  │         ├─ Blinn-Phong lighting (~70+ instr/vertex):
  │         │   ├─ Normal → world space + normalize (RSQRT, 7-cycle pipe)
  │         │   ├─ dot(N, L) → diffuse
  │         │   ├─ Vertex → world space
  │         │   ├─ toCamera = normalize(cameraPos - worldPos)
  │         │   ├─ halfVec = normalize(toCamera + toLight)
  │         │   ├─ specular = dot(N, H)³² (5 chained MUL.x squaring)
  │         │   └─ Combine: ambient + diffuse + specular → clamp → FTOI0
  │         └─ SQ RGBAQ × 3 → output buffer
  │
  │   XGKICK → GS via GIF DMA
  │
  ▼
GS Rasterizer:
  ├─ Gouraud shading (RGBAQ interpolation)
  ├─ Perspective-correct texturing (STQ)
  ├─ Z-buffer test + write
  ├─ Alpha blending
  └─ ADC check: skip triangle if w >= 0x8000
```

---

## 2. Key Components

### VU1 Data Memory Layout (4 KB total)

```
VU1 DMEM (256 qwords)
┌─────────────────────────────────────┐
│ Constants (15 qwords):              │
│  MVP matrix (4), Scale/RGBA (2),    │
│  Model matrix (4), Light dir (1),   │
│  Light intensities (1), GIF tag (1),│
│  TEX0 config (1), Camera pos (1)    │
│  Address: 0–14                      │
├─────────────────────────────────────┤
│ VIF1 Double-Buffer 0 (~120 qwords)  │
│  Input:  verts + normals + STQ      │
│  Output: ST + RGBAQ + XYZ2          │
├─────────────────────────────────────┤
│ VIF1 Double-Buffer 1 (~120 qwords)  │
│  (same layout, alternates per chunk)│
└─────────────────────────────────────┘
```

### Chunking Strategy

| Parameter | Value |
|-----------|-------|
| Max vertices per VIF packet | 81 (`MAX_VERTEXDATA_PER_VIF_PACKET`) |
| Max triangles per chunk | 27 (81 ÷ 3) |
| Buffer capacity | ~504 qwords per double-buffer |
| Header overhead | 2 qwords (vertCount + GIF tag) |
| Output GIF tags | 3 qwords (GIFAD + TEX0 + primTag) |
| Vertex input format | 3 qwords/vertex (position + normal + STQ) |
| Vertex output format | 3 qwords/vertex (ST + RGBAQ + XYZ2) |

### Double-Buffering Scheme

```
Layer 1 — EE-Side Packets:
  dynamicPacket[0] and dynamicPacket[1] alternate (ping-pong)
  While DMA transmits one, EE fills the other

Layer 2 — VIF1 Data Memory:
  VIF1 auto-toggles base address via TOP register
  MSCAL (start) for first chunk, MSCNT (continue) for subsequent

Pipeline Timeline:
  EE fill pkt[N] → DMA VIF1 → VU1 process → XGKICK → GS draw
  EE fill pkt[N+1] ───────────────────────────────► (parallel)
```

### Data Format

**Input (EE → VU1):**
```
Qword 0:         Vertex count (u32)
Qword 1:         GIF Tag (PACKED, register chain)
Qword 2..N+1:    Vertex positions (Vec4 × N)
Qword N+2..2N+1: Vertex normals (Vec4 × N)
Qword 2N+2..3N+1: Texture STQ coords (Vec4 × N)
```

**Output (VU1 → GS via XGKICK):**
```
Per vertex (3 qwords):
  Qword 0: ST (texture coordinates)
  Qword 1: RGBAQ (color + texture Q)
  Qword 2: XYZ2 (screen position, w = ADC bit)
```

---

## 3. Pipeline Flow Diagram

```
┌──────────────────────────────────────────────────┐
│ EE Core                                          │
│  CalculateLightingSpecular macro expanded inline │
│  because vclpp doesn't support nested macros     │
│                                                  │
│  RenderChunck() per 81 vertices:                 │
│    ┌─ Pack constants (same every chunk!)         │
│    ├─ Pack vertex data                           │
│    ├─ Add MSCAL/MSCNT                            │
│    └─ DMA VIF1                                   │
└──────────────────┬───────────────────────────────┘
                   │ VIF1 DMA
                   ▼
┌──────────────────────────────────────────────────┐
│ VU1 — draw3d_triangle.vcl                        │
│                                                  │
│  setup:  Load 15 constants from DMEM[0..14]      │
│  begin:  XTOP → read packet header               │
│  loop:   Per triangle (3 vertices):              │
│    ├─ Load: 3 vertices, 3 STQs                   │
│    ├─ MVP (4 MUL/MADD per vertex)                │
│    ├─ CLIPw + ADC tag (speculative)              │
│    ├─ DIV + MULQ → perspective divide            │
│    ├─ Clip-space backface culling (2D cross)     │
│    ├─ FTOI4 → GS fixed-point                     │
│    ├─ SQ XYZ2 + STQ (early store)                │
│    ├─ IF backface: ISW.w 0x8000, skip lighting   │
│    └─ ELSE: Lighting (3× specular, ~70 instr/v)  │
│        ├─ Normal → world + normalize             │
│        ├─ N·L → diffuse                          │
│        ├─ Vertex → world → toCamera → normalize  │
│        ├─ Half-vec → normalize                   │
│        ├─ dot(N,H)³² → specular                  │
│        ├─ ambient + diffuse + specular → clamp   │
│        └─ FTOI0 → RGBA                           │
│  store:  SQ RGBAQ × 3                            │
│  loopend: XGKICK → GS                            │
└──────────────────┬───────────────────────────────┘
                   │ GIF DMA
                   ▼
┌──────────────────────────────────────────────────┐
│ Graphics Synthesizer (GS)                        │
│  ├─ Gouraud shade triangles                      │
│  ├─ Perspective-correct texture mapping          │
│  ├─ Z-buffer test (GREATER_EQUAL)                │
│  ├─ Alpha blend (Cs*As + Cd*(128-As))            │
│  └─ ADC check → skip culled triangles            │
└──────────────────┬───────────────────────────────┘
                   │
                   ▼
         640×448 Framebuffer (PSM_24)
         Double-buffered, interlaced NTSC
```

---

## 4. Bottlenecks

### 🔴 Critical

#### 4.1 VU1 Lighting Cost — ~70+ instructions per vertex

The `CalculateLightingSpecular` macro (`src/vu1programs/vcl_macros.i`) dominates VU1 time:

| Operation | Instructions | Pipeline Stalls |
|-----------|-------------|-----------------|
| Normal → world space | 4 MUL/MADD | 0 |
| Normalize (normal) | 3 MUL, 2 ADD, **RSQRT**, MUL | **~7 (Q latency)** |
| dot(N,L) → diffuse | 3 MUL, 2 ADD, MAX, MUL | 0 |
| Vertex → world | 4 MUL/MADD | 0 |
| Normalize (toCamera) | 3 MUL, 2 ADD, **RSQRT**, MUL | **~7** |
| Normalize (halfVec) | 3 MUL, 2 ADD, **RSQRT**, MUL | **~7** |
| dot(N,H)³² → specular | 3 MUL, 2 ADD, 5 MUL.x, MUL.x, MAX, MINI | 0 |
| Combine + clamp | MUL, MADD, MADD, MINI, ADD, MAX, FTOI0 | 0 |
| **Total per vertex** | **~70+** | **~21 stall cycles** |
| **Total per triangle** | **~210+** | **~63 stall cycles** |

For comparison, `draw3d.vcl` (ambient + diffuse only): **~30 instructions/vertex**.

#### 4.2 Tiny Chunk Size — 27 triangles per DMA batch

VU1's 4 KB DMEM limits chunks to 81 vertices (27 triangles). Each chunk incurs:
- DMA setup/teardown overhead
- VU1 MSCAL/MSCNT overhead
- Redundant constant re-upload

#### 4.3 Per-Chunk Constant Re-upload

In `src/renderer/path1renderer3d.cpp:RenderChunck()`, **every single chunk** re-uploads:
- MVP matrix (4 qwords)
- Static packet: viewport scale + RGBA (2 qwords) — never changes
- Model matrix (4 qwords) — same across chunks of the same mesh
- Light direction + intensities (2 qwords)
- TEX0 config + camera position (2 qwords)

The code acknowledges this: `// TODO: try sending this only once`

#### 4.4 Deindexed Triangle Lists

Models in `src/renderer/model.cpp` are stored as flat deindexed triangle lists. Each shared vertex is duplicated (typically 2-3×). This inflates:
- DMA upload bytes (positions + normals + STQ all duplicated)
- VU1 processing (same vertex processed multiple times)

#### 4.5 No CPU-Side Frustum Culling

All models are unconditionally sent to VU1. No bounding volume tests exist before the upload loop in `Path1Renderer3D::RenderFrame()`.

### 🟡 Medium

#### 4.6 EE DMA Serialization

`RenderChunck()` calls both `dma_channel_wait(DMA_CHANNEL_VIF1, 0)` and `dma_channel_wait(DMA_CHANNEL_GIF, 0)` before each send, serializing the EE on DMA completion.

#### 4.7 No Occlusion Culling or LOD

No hierarchical Z, occlusion queries, or level-of-detail — all triangles at full detail always.

#### 4.8 EE Scalar Matrix Math

`Model::Update()` and matrix multiplications run on the EE's scalar unit. VU0 macro mode could accelerate this.

### 🟢 Lower

#### 4.9 GS Fill Rate

At 640×448 with overdraw, the GS can become the bottleneck on fill-heavy scenes (large triangles, transparent overlaps).

#### 4.10 Static `DrawEnable` Bits

Primitive config is hardcoded in both renderers — fogging, anti-aliasing, and alpha blending are always enabled even when not needed.

---

## 5. Optimization Recommendations

### High Impact / Low Effort

| # | Change | Files | Expected Gain |
|---|--------|-------|---------------|
| **1** | **Simplify VU1 lighting** — Use `draw3d.vcl` (ambient+diffuse) or make specular a compile-time toggle | `draw3d_triangle.vcl`, `main.cpp` | **2–3× vertex throughput** |
| **2** | **Add frustum culling** — Compute AABB per model, test against frustum planes on EE before VU1 upload | `path1renderer3d.cpp:RenderFrame()` | **Skips invisible models entirely** |
| **3** | **De-duplicate constant upload** — Skip re-uploading static packet + model matrix for consecutive chunks of the same mesh | `path1renderer3d.cpp:RenderChunck()` | **~15–20% per-chunk DMA reduction** |

### Medium Impact

| # | Change | Files | Expected Gain |
|---|--------|-------|---------------|
| **4** | **Increase chunk size** — Try > 81 if tighter packing allows; reduce GIF tag overhead | `path1renderer3d.cpp:87` | **~10–20% fewer chunks** |
| **5** | **LOD system** — Use lower-poly meshes for distant objects | `model.cpp`, new LOD class | **Proportional to camera distance** |
| **6** | **VU0 macro-mode** — Offload EE matrix ops to VU0 for `Model::Update()` and MVP computation | `model.cpp`, `mat4.cpp` | **Frees EE time for more scene setup** |
| **7** | **Skip per-chunk texture upload** — Only update TEX0 when the active texture actually changes between models | `path1renderer3d.cpp:RenderChunck()` | **~2 qwords saved per chunk** |

### Lower Impact / Long-Term

| # | Change | Files | Expected Gain |
|---|--------|-------|---------------|
| **8** | **Indexed geometry** — Use GIF indexed primitives; requires VU1 program changes | `model.cpp`, VU1 programs | **–50–66% vertex DMA** |
| **9** | **Triangle strips** — Convert deindexed tris to strips | `model.cpp`, VU1 programs | **~33% fewer vertices per mesh** |
| **10** | **Drop resolution** — 512×448 or 320×224 to reduce GS fill rate pressure | `DrawingEnvironment.cpp` | **–35–75% pixels** |
| **11** | **GS mipmapping** — Enable MIPMAP in TEX0 for distant objects | `Texture.cpp` | **Reduced texture cache thrashing** |
| **12** | **Conditional GS features** — Disable alpha blending and fogging per-primitive when not needed | Both renderers | **Minor GS savings** |

### Quickest Win

> **Switch from `draw3d_triangle.vcl` to `draw3d.vcl`** (or make specular lighting optional). The specular calculation alone accounts for roughly **half of all VU1 instructions per triangle**. For non-shiny surfaces at PS2 resolutions, this is the single highest-impact change.

---

## 6. Benchmarking Strategy

### 6.1 Current Instrumentation

| Tool | Location | What It Measures | Resolution |
|------|----------|------------------|------------|
| `Deltawatch` | `tools/Deltawatch.hpp` | EE-side elapsed time | Milliseconds (std::chrono) |
| `scr_printf` | `path1renderer3d.cpp:234` | Triangles/frame + display list prep time | On-screen overlay |
| `trianglesRendered` | Both renderers | Per-frame triangle count | Counter only |

### 6.2 What's Missing

The current setup measures only two coarse metrics (frame time, prep time). For systematic optimization, you need per-stage breakdowns.

### 6.3 Recommended Metrics

#### Throughput

| Metric | Unit | How to Measure |
|--------|------|----------------|
| Triangles/frame | count | Already tracked |
| Triangles/second | tris/s | `trianglesPerFrame / frameTimeMs * 1000` |
| Vertices/second | verts/s | `verticesPerFrame / frameTimeMs * 1000` |
| Pixels/frame (GS) | pixels | `640 × 448 × estimated_overdraw` |
| DMA bytes/frame | bytes | Sum VIF1 + GIF transfer sizes |
| Chunks/frame | count | `totalVertices / 81` |

#### Per-Stage Latency

| Stage | Metric | How to Measure |
|-------|--------|----------------|
| **EE Setup** | Model::Update() + packet building | `Deltawatch` around `for (model)` + `RenderFrame` preamble |
| **VIF1 DMA** | Upload time per chunk | `Deltawatch` around `dma_channel_send_packet2` → `dma_channel_wait` |
| **VU1 Processing** | Compute time per chunk | VU1 TPC register (cycle counter) or MSCAL → XGKICK gap |
| **GS Rasterization** | Pixel fill time | `Deltawatch` from `draw_wait_finish()` to next `graph_wait_vsync()` |

#### Efficiency Ratios

| Metric | Target | Description |
|--------|--------|-------------|
| Backface cull ratio | Higher is better | `triangles_culled / total_triangles` |
| Frustum clip ratio | Lower is better | How many vertices hit `CLIPw` flags |
| Constants-to-data ratio | Lower is better | `bytes_constants / bytes_vertex_data` per chunk |
| VU1 utilization | Higher is better | `VU1_busy_time / frame_time` |

### 6.4 On-Screen Benchmark HUD

Replace the current two-line debug overlay with a structured heads-up display:

```
┌────────────────────────────────────────┐
│ FPS: 59.8  │ Frame: 16.7ms             │
│ Tri: 12,420│ Vert: 37,260│ Chunks: 460 │
│ Cull: 34%  │ Const/Data: 12%            │
│ EE: 3.2ms  │ VIF: 1.8ms │ GS: 11.7ms   │
│ VU1: 8.4ms │ DMA: 1.2mb  │ Overdraw:~2x │
└────────────────────────────────────────┘
```

Toggle modes via controller: **off → minimal (FPS+tris) → full breakdown → per-stage detail**

### 6.5 PS2 Hardware Timers

Replace or supplement `std::chrono` with EE hardware timers for cycle-accurate measurements:

```cpp
// EE Timer 0 — runs at busclk/16 ≈ 18.432 MHz
#define EE_TIMER0_COUNT   (*((volatile u32*)0x10000000))
#define EE_TIMER0_MODE    (*((volatile u32*)0x10000010))

// VU1 TPC register — cycle counter
#define VU1_TPC           (*((volatile u32*)0x10004C00))  // approximate
```

Benefits over `std::chrono`:
- Cycle-accurate at 294.912 MHz (EE) / 147.456 MHz (VU1)
- Can measure VU1 stall cycles and DMA stall cycles
- Zero measurement overhead (hardware register reads)

### 6.6 Automated Benchmarking Harness

For reproducible before/after comparisons:

```
Requirements:
  1. Fixed camera path (pre-recorded spline, no controller input)
  2. Fixed model set at known positions
  3. N warmup frames (discard), then M measured frames
  4. Dump metrics to CSV via ps2link/ps2client stdout
  5. Multiple trials → mean/stdev per metric
```

**Example CSV output:**
```csv
frame,triangles,ee_ms,dma_ms,vu1_ms,gs_ms,backface_cull_pct,chunks
0,12420,3.2,1.8,8.4,11.7,34,460
1,12418,3.1,1.8,8.3,11.6,34,460
2,12422,3.2,1.9,8.4,11.7,33,461
...
```

### 6.7 Stress-Test Scenarios

Systematically vary one variable at a time to isolate bottlenecks:

| Scenario | What It Tests |
|----------|---------------|
| **1 large model** (100K tris, fills screen) | GS fill rate limit |
| **Many small models** (1000 cubes, 12 tris each) | Chunk/DMA overhead dominates |
| **All backfaces** (camera inside a sphere) | Culling effectiveness + GS ADC skip cost |
| **No textures** (solid color triangles) | Texture cache / TEX0 switching overhead |
| **Ambient-only lighting** (diffuse=0, specular=0) | VU1 lighting cost vs everything else |
| **Static camera, rotating model** | Isolates VU1 cost (no GS fill variation) |
| **Non-interlaced, low-res** (320×224) | Baseline GS fill rate |
| **Overdraw torture** (many overlapping transparent quads) | GS blending + Z-buffer bandwidth |

### 6.8 Suggested Benchmark Workflow

```
1. Profile current state → save baseline metrics
2. Change ONE variable (e.g., switch to draw3d.vcl)
3. Re-profile → compare to baseline
4. If improvement found, commit and repeat
5. If no improvement, revert and try next change
```

### 6.9 Practical First Steps

1. **Add per-stage timing to `main.cpp`** — wrap EE logic, DMA send, GS wait in separate `Deltawatch` calls
2. **Add backface cull counter** — count triangles that hit the `culled:` path (track on EE side or via GS feedback)
3. **Build stress-test model** — single OBJ with 50K+ triangles
4. **Create benchmark recording** — fixed camera path, dump metrics to serial/CSV
5. **Run before/after comparisons** for every pipeline change

---

## 7. Reference: VU1 Instruction Costs

### VU1 Throughput Estimates

At 147.456 MHz VU1 clock (upper + lower instruction dual-issue):

| Program | Instr/Triangle | Max Tris/Sec (theory) | Max Tris/Frame @60fps |
|---------|---------------|----------------------|------------------------|
| `draw3d.vcl` (diffuse only) | ~175 | ~842,000 | ~14,000 |
| `draw3d_triangle.vcl` (specular) | ~350 | ~421,000 | ~7,000 |

> If actual throughput is far below these numbers, the bottleneck is **DMA** or **GS**, not VU1.

### Pipeline-Affecting Instructions

| Instruction | Latency | Notes |
|-------------|---------|-------|
| DIV / RSQRT | **7 cycles** | Q register pipeline — instructions reading Q must be 7+ cycles later |
| MUL / MADD / MULA | 1 cycle | Full throughput, no stalls |
| ADD / SUB / MAX / MINI | 1 cycle | — |
| LQ / SQ | 1 cycle | VU data memory access |
| XGKICK | Variable | DMA to GS; VU1 stalls until transfer begins |
| CLIPw | 1 cycle | Updates clipping flag register |

### VU1 Register Constraints

| Register | Count | Notes |
|----------|-------|-------|
| VF (float vectors) | 32 | 128-bit each (4× float32) |
| VI (integers) | 16 | 32-bit each |
| ACC | 1 | Implicit dest for MULA; source for MADD/MSUB |
| Q | 1 | DIV/RSQRT result; **7-cycle pipeline** |
| I | 1 | Broadcast constant register |

---

## Key Files Reference

| File | Role |
|------|------|
| `src/main.cpp` | Main loop, model setup, lighting config |
| `src/renderer/path1renderer3d.cpp` | VU1 renderer — packet building, chunk dispatch |
| `src/renderer/path3renderer3d.cpp` | EE-only renderer (legacy/fallback) |
| `src/vu1programs/draw3d_triangle.vcl` | VU1 microprogram — Blinn-Phong, triangle-based |
| `src/vu1programs/draw3d.vcl` | VU1 microprogram — ambient+diffuse, vertex-based |
| `src/vu1programs/vcl_macros.i` | Shared VU1 macro library |
| `src/renderer/model.cpp` | OBJ loading via TinyObjLoader (deindexed) |
| `src/renderer/mesh.cpp` | Mesh data container (vertices, normals, texels) |
| `include/tools/Deltawatch.hpp` | ms-resolution timer |
| `include/light/Lighting.hpp` | EE-side lighting (used by Path 3 only) |
| `include/light/BaseLight.hpp` | Light configuration + packet serialization |
| `src/graphics/DrawingEnvironment.cpp` | GS register setup, framebuffer config |
| `docs/vu1_draw3d_triangle_analysis.md` | Detailed VU1 microprogram analysis |
