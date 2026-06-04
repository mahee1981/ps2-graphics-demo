# VU1 `draw3d_triangle.vcl` — Technical Analysis

## Overview

`draw3d_triangle.vcl` is a **VCL** microprogram that runs on the PlayStation 2's **Vector Unit 1 (VU1)**. This program processes **one triangle (3 vertices) per loop iteration**, enabling **clip-space backface culling** before perspective division.

| Property | Value |
|---|---|
| Program name | `VU1Draw3DTriangle` |
| Source | `src/vu1programs/draw3d_triangle.vcl` |
| Macro includes | `src/vu1programs/vcl_macros.i` |
| Pre-processed output | `src/vu1programs/draw3d_triangle.vsm_pp` |
| Assembled output | `src/vu1programs/draw3d_triangle.vsm` |
| Loaded in | `src/main.cpp` as `VU1Draw3DTriangle_CodeStart` / `_CodeEnd` |
| Data memory (DMEM) | 4 KB (4096 bytes) |
| Instruction memory (IMEM) | 4 KB (1024 instructions) |

---

## Build Pipeline

The VCL source goes through two transformations:

```
draw3d_triangle.vcl
    ↓ [vclpp — VCL Pre-Processor]
draw3d_triangle.vsm_pp  (macros expanded, ready for assembler)
    ↓ [vcl — VCL]
draw3d_triangle.vsm     (VU1 assembly)
    ↓ [vcl — VSM Assembler]
draw3d_triangle.o     (final VU1 microcode)
```
Meaning that two tools are required: [VCLPP](https://github.com/glampert/vclppl) and [VCL - Vector Unit Command Line](https://ps2linux.no-ip.info/playstation2-linux.com/projects/vcl.html) for compiling the full binary.

- The `.o` binary is linked into the PS2 ELF via a `.section(\".vudata\")` attribute.
- At runtime, `path1Renderer->UploadVU1MicroProgram()` copies it into VU1's instruction memory.


---

## Memory Layout (Constants)

The program defines 15 quadword constants, loaded from VU1's `vi00`-relative data memory at program start:

| Constant | Address | Purpose |
|---|---|---|
| `MvpMatRow0` | 0 | MVP matrix row 0 |
| `MvpMatRow1` | 1 | MVP matrix row 1 |
| `MvpMatRow2` | 2 | MVP matrix row 2 |
| `MvpMatRow3` | 3 | MVP matrix row 3 |
| `Scale` | 4 | Viewport transform (scale + offset) |
| `RGBALoc` | 5 | Base RGBA color (per vertex, set to 128) |
| `ModelMatRow0` | 6 | Model matrix row 0 |
| `ModelMatRow1` | 7 | Model matrix row 1 |
| `ModelMatRow2` | 8 | Model matrix row 2 |
| `ModelMatRow3` | 9 | Model matrix row 3 |
| `LightDirection` | 10 | Light direction vector (world space) |
| `LightIntensities` | 11 | Ambient (x), Diffuse (y), Specular (z) intensities |
| `GifTagAd` | 12 | GIF tag for GS register configuration (texture select) |
| `GifTexSelect` | 13 | TEX0 GS register value (active texture) |
| `CameraPos` | 14 | Camera position in world space (xyz, w=0 unused) |

The data following these constants is the **vertex buffer** sent by the EE core via DMA/GIF, starting at the address obtained from `XTOP`.

---

## Double-Buffering Strategy

This project uses a **simple double-buffering scheme**.

### VU1 Data Memory Map

```
VU1 DMEM (4 KB = 256 qwords)
┌─────────────────────────────────────────────────┐
│ Constants (15 qwords):                         │
│  MVP matrix (4), Scale (1), RGBA (1),          │
│  Model matrix (4), Light direction (1),         │
│  Light intensities (1), GIF tag (1),            │
│  TEX0 config (1), Camera position (1)           │
│  Address: 0 — 14    (updated per chunk)         │
├─────────────────────────────────────────────────┤
│ VIF1 Double Buffer 0                           │
│  Header (2 qwords): vertCount + GIF Tag         │
│  Vertex data (positions, normals, STQ)          │
│  Output area (XGKICK buffer)                    │
├─────────────────────────────────────────────────┤
│ VIF1 Double Buffer 1                           │
│  (same layout as buffer 0)                      │
└─────────────────────────────────────────────────┘
```

VIF1 is configured with:
```cpp
packet2_utils_vu_add_double_buffer(packet2, 15, (1024 - 15) / 2);
```
This reserves the first **15 qwords** for constants and splits the remaining data memory into **2 buffers** for vertex data. The EE re-uploads the vertex count, MVP, model matrix, light data, texture id, and camera position to addresses 0–14 at the start of each chunk (since matrices change per model), so the constants are **updated per chunk**, not just at program start. 

### How the Double-Buffering Works

The system achieves pipelining through **two layers of double-buffering**:

#### 1. EE-Side Packet Double-Buffering

The `Path1Renderer3D` maintains two VIF packet buffers (`dynamicPacket[2]`) and alternates between them:

```cpp
packet2_t *dynamicPacket[2];
// ...
packet2_t *currentVifPacket = dynamicPacket[context];
// ... fill currentVifPacket ...
dma_channel_send_packet2(currentVifPacket, DMA_CHANNEL_VIF1, 1);
context = !context;  // switch to the other packet for next chunk
```

While DMA channel VIF1 is transmitting one packet to VU1's data memory, the EE core can **simultaneously fill the other packet** with the next chunk of vertex data. This hides the DMA setup latency. The VU1 program processes one chunk at a time (it cannot run concurrently with VIF1 writes into the same buffer), but the EE-side packet preparation overlaps with both the VIF1 DMA and the VU1's processing of the previous chunk.

#### 2. VIF1 Data Memory Double-Buffering

VIF1's double-buffer mode automatically alternates between the two data memory regions. The EE always unpacks data starting at address 0 relative to the active buffer via the `TOP` register.

VIF1 internally toggles the base address for each `MSCAL`/`MSCNT` call, so consecutive chunks automatically land in alternating memory regions.

#### 3. Micro-Program Start/Continue

For the first chunk of vertices in a frame, the EE sends an `MSCAL` (micro-program start) VIF code, which starts the VU1 program from the beginning:

```cpp
if (offset == 0)
    packet2_utils_vu_add_start_program(currentVifPacket, 0);  // MSCAL
else
    packet2_utils_vu_add_continue_program(currentVifPacket);   // MSCNT
```

For subsequent chunks, `MSCNT` (micro-program continue) resumes the VU1 program at its current loop, waiting at `begin:` for the next `XTOP` packet. This allows the VU1 to process multiple chunks per frame without restarting its constant setup.

### Overview of the Buffering Strategy

| Feature | Implementation |
|---|---|
| **VU1 buffers** | 4 (2 input + 2 output) |
| **EE-side packets** |  2 (ping-pong packets) |
| **VIF remapping** | VIF1 double-buffer mode (`TOP` register) |
| **Buffer size** | ~81 vertices each (≈27 triangles) |
| **Constants location** | First 15 qwords of DMEM (shared) |
| **Output buffer** | Same buffer (in-place transform) |
| **Program restart** | `MSCALL` (initial) / `MSCNT` (subsequent) |
| **Data flow** | Upload to buffer → XTOP → process → XGKICK |

The project uses VIF1's built-in double-buffer mode. The EE-side packet double-buffering compensates for the reduced VU1 buffer count by allowing the EE to prepare the next chunk while DMA transmits the current one. 

### Pipeline Timeline

```
Frame N:                                        Frame N+1:
EE fill pkt[0] ──► DMA VIF1 ──► VU1 process ──► XGKICK ──► GS draw
EE fill pkt[1] ──► DMA VIF1 ──► VU1 process ──► XGKICK ──► GS draw
   (parallel)         │              │                │           │
                      ▼              ▼                ▼           ▼
                    Time ──────────────────────────────────────────►

Chunk 1:  [EE fills pkt[0]] → [DMA to VU1 buf 0] → [VU1 processes buf 0] → [XGKICK to GS]
Chunk 2:                     [EE fills pkt[1]] → [DMA to VU1 buf 1] → [VU1 processes buf 1] → [XGKICK]
```

Across consecutive chunks, the three stages overlap: the EE prepares chunk N+1 while DMA uploads chunk N to VU1 DMEM via VIF1; the VU1 processes chunk N while the GS rasterizes chunk N-1 via XGKICK. This creates a **3-stage pipeline** where EE packet fill, VIF1 DMA upload, and GS rasterization all run concurrently.

### Chunking Strategy

The EE splits vertex data into chunks of **81 vertices** (27 triangles):

```cpp
constexpr u32 MAX_VERTEXDATA_PER_VIF_PACKET = 81;
// must be divisble by 3 so you avoid artifacts between batches
```

This ensures:
- Each chunk's data (input + output) fits within VU1's data memory per buffer
- The vertex count is always divisible by 3 (no partial triangles)
- Multiple models/meshes can be rendered in a single frame by sending multiple chunks

Note: The exact buffer capacity is determined by the VIF1 double-buffer configuration — the constant area (15 qwords) is excluded from VIF1's address remapping, and the remaining DMEM is split into 2 regions. With 81 vertices per chunk, the input (header + positions + normals + STQ ≈ 245 qwords) and output (GIF tag + 3N qwords ≈ 244 qwords) are stored in the same buffer.

The buffer header for each chunk sent to VU1's double-buffer area contains:
- **Qword 0**: Vertex count (integer) — read via `ILW.w`
- **Qword 1**: GIF tag — defines the primitive type and register count

---

## Execution Flow

### 1. `setup:` — Load Constants

```
LQ      matrixMvp[0],      MvpMatRow0(vi00)
LQ      matrixMvp[1],      MvpMatRow1(vi00)
...
LQ      lightDirectionVec, LightDirection(vi00)
SUB.xyz toLight, vf00, lightDirectionVec           ; Invert direction
LQ      lightIntensitiesVec, LightIntensities(vi00)
LQ      gifAdTag,          GifTagAd(vi00)
LQ      tex0Config,        GifTexSelect(vi00)
```

- Loads all 4 rows of the MVP matrix, the viewport scale, RGBA base color, model matrix, light direction (inverted and stored in `toLight`), and light intensity factors into VF registers.
- Also loads the two GIF tags (`gifAdTag` for GS register setup, `tex0Config` for texture activation) that will be stored in the output buffer.
- **`LQ`** (Load Quadword): loads up to 4 fields of a VU FPR from adjacent words in VU data memory. The offset is in doublewords (16-byte units).
- `FCSET 0x000000` — zeroes the clipping flags register (required before using `CLIPw`).
- Initializes `dontDraw = 1 + 0x7FFF = 0x8000` — a sentinel value that, when stored in the `w` component of `XYZ2`, sets the **ADC** bit in the GS XYZ2 register, preventing the triangle from being drawn.

#### Official VU Instruction References (setup)

| Instruction | Format | Purpose |
|---|---|---|
| **LQ** | `LQ.dest ft, offset(base)` | Load up to 4 fields of a VU FPR from adjacent words in VU data cache. Offset in doublewords. |
| **SUB** | `SUB.dest fd, fs, ft` | Subtract up to 4 matching fields: `fd.field <- fs.field - ft.field` |
| **FCSET** | `FCSET imm24` | Set the clipping flag to the value of a 24-bit immediate: `clipping_flag = imm24` |
| **IADDIU** | `IADDIU It, Is, imm` | Add immediate to integer register: `It = Is + imm` (no overflow check implied) |

---

### 2. `begin:` — Read Packet Header

```
XTOP    iBase
LQ      primTag,            1(iBase)
IADDIU  vertexData,         iBase,            2
ILW.w   vertCount,          0(iBase)
```

- **`XTOP`** (VU1-only) — reads the address of the start of the micro-mode packet (the vertex buffer sent from the EE via VIF1). It controls the VU1 path to the GIF.
- The EE core sets up the buffer as:
  - **Qword 0**: Vertex count (integer) — read via `ILW.w`
  - **Qword 1**: GIF tag — defines the primitive type and data format
  - **Qword 2 onwards**: Vertex data (3 vertices per triangle)
- Pointers are computed using **`IADD`** (Integer Add): `Id = Is + It`:
  - `vertexData` → start of vertex positions
  - `normalData` → `vertexData + vertCount`
  - `stqData` → `normalData + vertCount`
  - `gifPacketStart` / `destAddress` → `stqData + vertCount` (output buffer)
- The GIF tags (`giffAdTag`, `tex0Config`, `primTag`) are stored at the start of the output buffer with **`SQI`** (Store Quadword Post-Increment)

> **Important**: The `vertCount` represents the number of **vertices**, not triangles. Since triangles are 3 vertices each, the loop processes `vertCount / 3` triangles.

---

### 3. `loop:` — Triangle Processing (one iteration = one triangle)

Each loop iteration loads 3 vertices, 3 STQ texture coords, processes them, optionally discards the triangle, and stores the result.

#### a. Load Triangle Data

```
LQ   vertex0,   0(vertexData)
LQ   vertex1,   1(vertexData)
LQ   vertex2,   2(vertexData)
LQ   Stq0,      0(stqData)
LQ   Stq1,      1(stqData)
LQ   Stq2,      2(stqData)
```

- Loads 3 vertex positions (4 × 32-bit floats each) and 3 sets of texture coordinates.

#### b. MVP Transform

```
MatrixMultiplyVertex{ vertex0, matrixMvp, vertex0 }
MatrixMultiplyVertex{ vertex1, matrixMvp, vertex1 }
MatrixMultiplyVertex{ vertex2, matrixMvp, vertex2 }
```

Transforms each vertex from model space to clip space using the MVP matrix.

#### c. Clipping Detection (ADC tagging)

```
ClipVertex{ vertex0, destAddress, 2,     iADC }
ClipVertex{ vertex1, destAddress, 2+3,   iADC }
ClipVertex{ vertex2, destAddress, 2+6,   iADC }
```

The macro expands to:

```
CLIPw.xyz  vertex,   vertex
FCAND      VI01,     0x3FFFF
IADDIU     iADC,     VI01, 0x7FFF
ISW.w      iADC,     offset(destAddress)
```

- **`CLIPw.xyz`** — tests each component of the vertex against `vertex.w` to detect if the vertex is outside the view frustum. The `w` component defines the clip planes (e.g., `|x| > w`, `|y| > w`, `|z| > w`). Updates the clipping flag register.
- **`FCAND VI01, 0x3FFFF`** — performs a bitwise AND between the current clipping flag value and the immediate `0x3FFFF`, storing the result in `VI01`. This extracts the clip flags for the last 3 vertices.
- **`IADDIU iADC, VI01, 0x7FFF`** — if any clip flags were set, `iADC ≥ 0x8000`. This sets the ADC bit when stored as the `w` component of `XYZ2`, causing the GS to skip the vertex.
- **`ISW.w`** (Integer Store Word) — stores the integer ADC value at the `w` position in the output buffer.
- The ADC value is written **ahead of time** into the output buffer. If the triangle is visible, `vertex.w` (from the perspective division) will **overwrite** this during the store step. If culled, the ADC marker remains.

#### d. Perspective Division

```
PerformPerspectiveDivision{ vertex0, viewPortTransform, Stq0, Stq0 }
PerformPerspectiveDivision{ vertex1, viewPortTransform, Stq1, Stq1 }
PerformPerspectiveDivision{ vertex2, viewPortTransform, Stq2, Stq2 }
```

Expands to:

```
DIV     q,      vf00[w],    vertex[w]         ; Q = 1.0 / vertex.w
MUL.xyz vertex, vertex,     q                 ; xyz = xyz / w
MULA.xyz acc,   viewPortTransform, vf00[w]    ; acc = scale * 1.0
MADD.xyz vertex, vertex,   viewPortTransform  ; vertex = vertex * scale + scale
MULQ    outStq, inStq,     q                  ; perspective-correct STQ
```

- The perspective-corrected STQ overwrites `StqN` in-place (previously used a separate `modStqN` register).
- Applies viewport transform: maps NDC `[-1, 1]` to GS screen coordinates
- Corrects texture coordinates for perspective: `s' = s/w`, `t' = t/w`, `q' = q/w`

#### e. Clip-Space Backface Culling

```
ClipSpaceBackfaceCulling{ vertex0, vertex1, vertex2, intRes }
```

The macro:

```
SUB     bfCull1,    vf00,   vf00              ; zero
SUB.xy  bfCull1,    vertex1, vertex0          ; edge1 = v1 - v0
SUB     bfCull2,    vf00,   vf00              ; zero
SUB.xy  bfCull2,    vertex2, vertex0          ; edge2 = v2 - v0
MUL     cullResult, vf00,   vf00              ; zero
MUL.x   cullResult, bfCull1, bfCull2[y]       ; cross.x = edge1.x * edge2.y
MUL.y   cullResult, bfCull1, bfCull2[x]       ; cross.y = edge1.y * edge2.x
SUB.x   cullResult, cullResult, cullResult[y]  ; cross = cross.x - cross.y
FTOI0   cullResult, cullResult                 ; convert to integer
MTIR    intRes,     cullResult[x]              ; move to integer register
```

- Computes the **2D cross product** (signed area) of the triangle in clip space (before perspective divide): `(v1.xy - v0.xy) × (v2.xy - v0.xy)`.
- The sign of the cross product tells us the triangle's winding direction (CW vs CCW).
- Since the cross product is performed in clip space, the sign convention matches the clip-space coordinate system: **positive = CW = backface** → culled.
- **`IBGTZ`** (Integer Branch if Greater Than Zero): `if It > 0, PC += imm` — branches to `culled:` if back-facing (used after the STQ/XYZ2 stores below).

#### f. Fixed-Point Conversion

```
FTOI4.xyz vertex0, vertex0
FTOI4.xyz vertex1, vertex1
FTOI4.xyz vertex2, vertex2
```

- **`FTOI4`** (Float To Integer, 4 decimal digits): converts floating-point to `12:4` fixed-point format (16 bits: 12 integer, 4 fractional bits). This is the format the GS expects for screen coordinates.
- Only `.xy` matters for the GS (which expects `XYZ2` in this format), but `.z` is also converted for the depth buffer.

#### f2. Early Store of XYZ2 and STQ

```
SQ.xyz   vertex0,            2(destAddress)          ; XYZ2 for vertex 0
SQ.xyz   vertex1,            2+3(destAddress)        ; XYZ2 for vertex 1
SQ.xyz   vertex2,            2+6(destAddress)        ; XYZ2 for vertex 2
SQ       Stq0,               0(destAddress)          ; STQ for vertex 0
SQ       Stq1,               0+3(destAddress)        ; STQ for vertex 1
SQ       Stq2,               0+6(destAddress)        ; STQ for vertex 2
```

- The screen-space positions (`XYZ2`) and texture coordinates (`STQ`) are stored to the output buffer **before** the backface culling decision. This ensures the GS always knows the triangle geometry regardless of the culling outcome.
- If the triangle is later determined to be back-facing, the `ISW.w` instructions in the `culled:` path will overwrite the ADC bits (w component of XYZ2) to prevent the GS from drawing — but the position and texture data remain valid in the buffer.

#### g. Lighting Calculation (Ambient + Diffuse + Specular)

The program uses the **`CalculateLightingSpecular`** macro (defined in `vcl_macros.i`) to compute per-vertex Blinn-Phong lighting. All inner macro calls have been expanded inline because `vclpp` does not support multi-layer macro expansion.

For each of the three vertices:

```
LQ      normal0, 0(normalData)
LQ      normal1, 1(normalData)
LQ      normal2, 2(normalData)
LQ      cameraPos,         CameraPos(vi00)    ; camera position (world space)

CalculateLightingSpecular{ FinalCol0, normal0, baseVertex0, cameraPos, matrixModel, lightIntensitiesVec, rgba, toLight }
CalculateLightingSpecular{ FinalCol1, normal1, baseVertex1, cameraPos, matrixModel, lightIntensitiesVec, rgba, toLight }
CalculateLightingSpecular{ FinalCol2, normal2, baseVertex2, cameraPos, matrixModel, lightIntensitiesVec, rgba, toLight }
```

The macro takes 8 parameters: `outputColor`, `normal`, `baseVertex`, `cameraPos`, `matrixModel`, `lightIntensitiesVec`, `rgba`, and `toLight`. It performs the following steps internally:

**Step 1 — Normal to world space + normalize:**
```
Normal = matrixModel × Normal      ; MatrixMultiplyVertex (expanded inline)
Normal = normalize(Normal)         ; VectorNormalize (expanded inline)
```

**Step 2 — Diffuse component:**
```
N·L = dot(Normal, toLight)                                ; VectorDotProduct (expanded inline)
N·L = max(N·L, 0)                                          ; clamp
N·L = N·L × lightIntensitiesVec.y                          ; scale by diffuse intensity
```

**Step 3 — Specular component (Blinn-Phong half-vector model):**
```
vecWorld = matrixModel × baseVertex                        ; vertex position → world space
toCamera = cameraPos - vecWorld                            ; vector from vertex to camera
toCamera = normalize(toCamera)                             ; normalize
halfVec  = toCamera + toLight                              ; half-vector
halfVec  = normalize(halfVec)                              ; normalize
spec     = dot(halfVec, Normal)                            ; N·H
spec     = spec³²                                            ; specular power raised via 5 chained MUL.x (squaring each time)
spec     = spec × lightIntensitiesVec.z                    ; scale by specular intensity
spec     = max(spec, 0)                                    ; clamp
spec     = min(spec, 1)                                    ; cap at 1.0
```

**Step 4 — Combine lighting components:**
```
ambient  = rgba × lightIntensitiesVec.x                    ; ambient term
diffuse  = rgba × N·L                                      ; diffuse term
specular = rgba × spec                                     ; specular term
final    = clamp(ambient + diffuse + specular, 0, 128)    ; combine and cap
alpha    = 128                                             ; constant alpha
```

##### Lighting Model

**Ambient + Diffuse + Specular** (Blinn-Phong):

| Component | Formula |
|---|---|
| **Ambient** | `base_color × ambient_intensity` (always present) |
| **Diffuse** | `base_color × max(0, N·L) × diffuse_intensity` |
| **Specular** | `base_color × clamp(max(0, N·H)³², 0, 1) × specular_intensity` |
| **Alpha** | Constant 128 (0x80) |

Where:
- `N` = surface normal (world space, normalized)
- `L` = light direction (world space, inverted via `vf00 - lightDirectionVec` = from surface to light)
- `H` = half-vector `normalize(toCamera + toLight)` — approximates the reflection highlight
- Specular power = 32 (computed as `x³²` via 5 chained `MUL.x` squaring instructions: `x → x² → x⁴ → x⁸ → x¹⁶ → x³²`)

##### Scratch Registers and Preconditions

The macro uses shared scratch registers (`vclsmlftemp`, `toCamera`, `halfVec`, `vecWorld`, `specComponent`) that must not be used concurrently across macro invocations. All three vertex calls share these registers sequentially (no overlap from VU1's in-order execution).

The `toLight`, `rgba`, `lightIntensitiesVec`, and `matrixModel` registers must be pre-loaded before the macro is called. `cameraPos` is loaded once from constant address 14 before the three macro invocations.

- **`MAX`** (Maximum): `fd.field <- max(fs.field, ft.field)` — clamps to ≥ 0 (no negative values).
- **`MINI`** (Minimum): `fd.field <- min(fs.field, ft.field)` — caps color at 128 (`rgba[w]`) to prevent oversaturation.
- **`ADD.w`**: sets alpha to `0 + 128 = 128`.
- Colors are capped at 128 (not 255) — likely to prevent oversaturation when combined with other effects.
- **`FTOI0`** converts the final color to 8-bit unsigned integer format for the GS.

---

#### h. Culled Triangle Path

```
culled:
   ISW.w   dontDraw, 2(destAddress)      ; ADC = 0x8000 → skip vertex 0
   ISW.w   dontDraw, 2+3(destAddress)    ; ADC = 0x8000 → skip vertex 1
   ISW.w   dontDraw, 2+6(destAddress)    ; ADC = 0x8000 → skip vertex 2
   MOVE    FinalCol0, vf00               ; zero color
   MOVE    FinalCol1, vf00
   MOVE    FinalCol2, vf00
```

- Writes `0x8000` to the `w` component of each vertex's `XYZ2` slot via **`ISW.w`**, setting the ADC bit.
- The GS interprets ADC=1 as "this vertex is adjacent to the previous primitive, skip the drawing kick."
- The triangle is **silently skipped** — the GS doesn't even know it was a separate primitive.
- **`MOVE`** is a pseudo-op that copies one VF register to another (zeroing colors for the culled triangle).
- Execution **falls through** to `storeNkick:` — both culled and visible triangles reach the store path.

---

### 4. `storeNkick:` — Write Color to Output Buffer

```
SQ   FinalCol0, 1(destAddress)            ; RGBAQ for vertex 0
SQ   FinalCol1, 1+3(destAddress)          ; RGBAQ for vertex 1
SQ   FinalCol2, 1+6(destAddress)          ; RGBAQ for vertex 2
```

- **`SQ`** (Store Quadword): stores all 4 fields of a VF register (RGBA) to VU data memory.
- Only the **color qwords** are stored here. The `XYZ2` positions and `STQ` texture coordinates were already stored earlier (before the culling decision — see section 3.f2).
- The GS reads the `w` component of `XYZ2` to determine the ADC bit — set by the `ClipVertex` macro or overwritten by the `culled:` path's `ISW.w`.
- Each vertex outputs **3 quadwords** in the `PACKED` GIF format with register chain `ST → RGBAQ → XYZ2`.
- The output for each triangle occupies **9 quadwords** (3 vertices × 3 qwords each).

---

### 5. Pointer Advancement

```
IADDIU vertexData,    vertexData,    3
IADDIU stqData,      stqData,       3
IADDIU normalData,   normalData,    3
IADDIU destAddress,  destAddress,   9

IADDI  vertexCounter, vertexCounter, -3
IBNE   vertexCounter, iBase,         loop
```

- Advances all data pointers by 3 (one triangle worth of vertices).
- Advances `destAddress` by 9 (3 vertices × 3 quadwords per vertex).
- **`IADDI`** (Integer Add Immediate): `Id = Is + imm` — decrements the counter.
- **`IBNE`** (Integer Branch if Not Equal): `if Is != It, PC += imm` — loops back if more triangles remain.

---

### 6. `loopend:` — DMA Kick

```
XGKICK   gifPacketStart
--barrier
--cont
B   begin
```

- **`XGKICK`** (VU1-only) — sends the output buffer to the Graphics Synthesizer via the GIF (Graphics Interface) DMA channel.
- `--barrier` / `--cont` — VCL directives ensuring the DMA transfer completes before the next packet overwrites the buffer.
- **`B`** (Branch) — unconditional branch back to `begin:` to wait for the next micro-mode packet.
- This creates an infinite loop: `packet received → process → XGKICK → wait for next packet`.

---

## Data Format

### Input (EE → VU1 via GIF micro-mode)

The EE packs vertex data in a **deindexed triangle list** format:

```
Qword 0:       vertexCount (integer, read via ILW.w)
Qword 1:       GIF Tag (PACKED format, primitive descriptor)
Qword 2..N+1:  Vertex positions (Vec4, 3 per triangle)
Qword N+2..2N+1: Vertex normals (Vec4, 3 per triangle)
Qword 2N+2..3N+1: Texture coords STQ (Vec4, 3 per triangle)
```

Where `N = vertCount` (total vertices across all triangles).

### Output (VU1 → GS via XGKICK)

```
Qword 0:              GIF Tag
Qword 1:              GIF Tag
Qword 2:              GIF Tag
                      ──────────────────────────
Qword 3:              Vertex 0  →  ST (texture)
Qword 4:              Vertex 0  →  RGBAQ (color + texture Q)
Qword 5:              Vertex 0  →  XYZ2 (screen position + ADC)
                      ──────────────────────────
Qword 6:              Vertex 1  →  ST
Qword 7:              Vertex 1  →  RGBAQ
Qword 8:              Vertex 1  →  XYZ2
                      ──────────────────────────
Qword 9:              Vertex 2  →  ST
Qword 10:             Vertex 2  →  RGBAQ
Qword 12:             Vertex 2  →  XYZ2
(per triangle)
```

The GIF tag format is `GIF_PACKED` with the register chain: `ST → RGBAQ → XYZ2`.

---

## Key Differences from `draw3d.vcl`

| Feature | `draw3d.vcl` | `draw3d_triangle.vcl` |
|---|---|---|
| Vertices per loop | 1 | 3 |
| Backface culling | ❌ Done on EE core | ✅ Done in clip space on VU1 |
| Clip space culling | ❌ | ✅ (before perspective divide) |
| ADC handling | Per-vertex via `CLIPw` only | Per-vertex with backface override |
| Triangle rejection | Not possible — all verts drawn | Possible — `IBGTZ` branch to `culled:` |
| Lighting | Per-vertex (ambient + diffuse only) | Per-vertex (Blinn-Phong: ambient + diffuse + specular) |
| Performance | Higher vertex throughput (1/loop) | Lower vertex throughput (3/loop), but skips invisible tris |

The primary advantage of the triangle-based approach is that **backface culling happens on VU1 without EE involvement**, saving VU1 processing time for invisible triangles.

---

## VU Architecture Notes

### VU Register Files

| Register file | Count | Width | Purpose |
|---|---|---|---|
| **VF** (Floating Point) | 32 | 128-bit (× 4 × float32) | Vector data (positions, normals, colors, matrices) |
| **VI** (Integer) | 16 | 32-bit | Address pointers, loop counters, branch conditions |
| **ACC** (Accumulator) | 1 | 128-bit | Implicit destination for `MULA`, `OPMULA`; source for `MADD`, `MSUB` |
| **Q** | 1 | 32-bit | Pipelined divide/RSQRT result (7-cycle latency) |
| **I** | 1 | 32-bit | Constant register (broadcast) |
| **Clipping Flag** | 1 | 24-bit | Updated by `CLIPw`; read by `FCAND`, `FCGET`, `FCOR` |
| **Status Flag** | 1 | 12-bit | Updated by arithmetic; read by `FSAND`, `FSEQ`, `FSOR` |
| **MAC Flag** | 1 | 12-bit | Updated by MAC operations; read by `FMAND`, `FMEQ`, `FMOR` |

### Key Pipeline Considerations

1. **Q register latency**: `DIV` and `RSQRT` have a 7-cycle latency. Instructions reading Q (`MULQ`, `ADDQ`, etc.) must be scheduled 7 cycles after the DIV/RSQRT that writes it.
2. **ACC forwarding**: `MULA` writes to ACC, which can be read by `MADD`/`MSUB` in the next instruction (no stall needed).
3. **Dual-issue**: VU1 can issue both an upper (floating-point) and lower (integer/branch) instruction per cycle. The VCL compiler handles scheduling.
4. **CLIPw flag update**: The clipping flag is updated atomically by `CLIPw`. It accumulates results for the last 3 `CLIPw` operations, which is why `FCAND 0x3FFFF` is used to extract all three vertices' clip status.

However, this is handled by VCL (Vector Unit Command Line).

---

## Visual Pipeline Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│  EE Core (main.cpp)                                              │
│  ┌────────────────────────────────────┐                          │
│  │ Pack deindexed triangle data:     │                          │
│  │  positions + normals + STQ        │                          │
│  │  per frame                        │                          │
│  └──────────┬─────────────────────────┘                          │
│             │ GIF DMA (micro-mode)                               │
└─────────────┼────────────────────────────────────────────────────┘
              ▼
┌──────────────────────────────────────────────────────────────────┐
│  VU1 — draw3d_triangle microprogram                              │
│                                                                  │
│  setup:     ─── Load MVP, model, viewport, light constants       │
│               ┌──────────────────────────────────────────────┐   │
│               │ VF regs: matrixMvp[0..3], viewPortTransform │   │
│               │          rgba, matrixModel[0..3],            │   │
│               │          lightDirectionVec, lightIntensities │   │
│               │ VI regs: dontDraw = 0x8000                   │   │
│               └──────────────────────────────────────────────┘   │
│                                                                  │
│  begin:     ◄── Wait for packet (XTOP), compute pointers        │
│               ┌──────────────────────────────────────────────┐   │
│               │ VI regs: vertexData, normalData, stqData,   │   │
│               │          gifPacketStart, destAddress,        │   │
│               │          vertexCounter                        │   │
│               └──────────────────────────────────────────────┘   │
│                                                                  │
│  loop:     ─── Load 3 vertices, 3 STQ, 3 normals               │
│               │                                                  │
│               ├── MVP transform (MUL + MADD × 4 per vert)       │
│               │   model → clip space                            │
│               │                                                  │
│               ├── CLIPw + FCAND + IADDIU → write ADC tag        │
│               │   (speculative: overwritten if visible)         │
│               │                                                  │
│               ├── DIV + MUL + MULA + MADD → perspective ÷      │
│               │   clip → NDC → screen coords                    │
│               │                                                  │
│               ├── 2D cross product in clip space                │
│               │   ├── Backface (intRes > 0) ──► culled:       │
│               │   │   └── ISW.w 0x8000 to XYZ2[w] per vert    │
│               │   └── Visible ──► continue                     │
│               │                                                  │
│               ├── FTOI4 → 12:4 fixed-point (GS format)         │
│               │                                                  │
│               ├── SQ XYZ2 + SQ STQ (early store before culling) │
│               │                                                  │
│               ├── Lighting (per vertex, Blinn-Phong):           │
│               │   │  LQ cameraPos from constant (once)         │
│               │   │  CalculateLightingSpecular macro:           │
│               │   │   Normal → world space + normalize         │
│               │   │   N·L → diffuse                            │
│               │   │   vertex → toCamera → halfVec              │
│               │   │   N·H³² → specular                          │
│               │   │   ambient + diffuse + specular → color     │
│               │   │   Clamp, cap at 128, FTOI0                 │
│               │                                                  │
│               └── SQ RGBAQ × 3 verts → output                  │
│                                                                  │
│               └──→ IADDI -3, IBNE loop if more tris            │
│                                                                  │
│  loopend:   ─── XGKICK → DMA output to GS                      │
│                 └──→ B begin (wait for next packet)             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
              ▼
┌──────────────────────────────────────────────────────────────────┐
│  Graphics Synthesizer (GS)                                       │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ Rasterizes triangles with:                               │   │
│  │ • Perspective-correct texture mapping (via STQ)          │   │
│  │ • Gouraud shading (via RGBAQ per vertex)                 │   │
│  │ • Depth buffering (via XYZ2 fixed-point coords)          │   │
│  │ • ADC check: skip drawing kick if w >= 0x8000            │   │
│  └──────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

---

## VU Instruction Reference (Official Documentation)

The following table summarizes all unique VU instructions used in `draw3d_triangle.vcl`, with descriptions from the [official PS2 Vector Unit Instruction Manual](https://github.com/ps2dev/ps2-docs/blob/master/vectormanual.pdf).

| Mnemonic | Format | Operation | Used In |
|---|---|---|---|
| **LQ** | `LQ.dest ft, offset(base)` | Load 4 words from VU data memory[offset] to FPR `ft` | setup, begin, loop |
| **SQ** | `SQ.dest ft, offset(base)` | Store fields of FPR `ft` to VU data memory[offset] | store |
| **SQI** | `SQI.dest ft, base++` | Store and post-increment: `[base] <- ft; base += 8` | begin |
| **ILW.w** | `ILW.dest ft, offset(base)` | Load 1 word (integer) from VU data memory to FPR field | begin |
| **ISW.w** | `ISW.dest ft, offset(base)` | Store 1 word (integer) from FPR field to VU data memory | culled, clip macro |
| **IADD** | `IADD Id, Is, It` | Integer register: `Id = Is + It` | begin |
| **IADDIU** | `IADDIU It, Is, imm` | Integer add immediate (unsigned): `It = Is + imm` | setup, store |
| **IADDI** | `IADDI Id, Is, imm` | Integer add immediate (signed): `Id = Is + imm` | store |
| **IBNE** | `IBNE Is, It, imm` | Branch if `Is != It` | store |
| **IBGTZ** | `IBGTZ It, imm` | Branch if `It > 0` | loop |
| **XTOP** | `XTOP It` | Read micro-mode packet base address into `It` (VU1 only) | begin |
| **XGKICK** | `XGKICK Is` | DMA output buffer (at address `Is`) to GS (VU1 only) | loopend |
| **CLIPw.xyz** | `CLIPw.xyz ft, fs` | Update clipping flag: compare `ft.xyzw` vs `fs.w` | clip macro |
| **FCAND** | `FCAND It, imm24` | `It <- clipping_flag AND imm24` | clip macro |
| **FCSET** | `FCSET imm24` | `clipping_flag = imm24` | setup |
| **DIV** | `DIV Q, ft.e, fs.e` | `Q <- ft.e / fs.e` (7-cycle pipeline) | perspective macro |
| **RSQRT** | `RSQRT Q, ft.e, fs.e` | `Q <- ft.e / sqrt(fs.e)` (7-cycle pipeline) | normalize macro |
| **MUL** | `MUL.dest fd, fs, ft` | `fd.field <- fs.field * ft.field` | mvp, lighting, culling |
| **MADD** | `MADD.dest fd, fs, ft` | `fd.field <- ACC.field + (fs.field * ft.field)` | mvp, perspective, lighting |
| **MULA** | `MULA.dest fs, ft` | `ACC.field <- fs.field * ft.field` (accumulator only) | perspective |
| **MULQ** | `MULQ.dest fd, fs` | `fd.field <- fs.field * Q` | perspective macro |
| **SUB** | `SUB.dest fd, fs, ft` | `fd.field <- fs.field - ft.field` | setup, culling |
| **ADD** | `ADD.dest fd, fs, ft` | `fd.field <- fs.field + ft.field` | lighting |
| **MAX** | `MAX.dest fd, fs, ft` | `fd.field <- max(fs.field, ft.field)` | lighting |
| **MINI** | `MINI.dest fd, fs, ft` | `fd.field <- min(fs.field, ft.field)` | lighting |
| **MTIR** | `MTIR It, fs.field` | Move FPR field to integer register: `It <- fs.field` | culling macro |
| **OPMULA.xyz** | `OPMULA.xyz ACC, fs, ft` | Outer product (pre-increment) — cross product | winding macro |
| **OPMSUB.xyz** | `OPMSUB.xyz fd, fs, ft` | Outer product (post-decrement) — cross product | winding macro |
| **FTOI0** | `FTOI0.dest ft, fs` | Float to integer (0 fractional bits) | lighting |
| **FTOI4** | `FTOI4.dest ft, fs` | Float to fixed-point (4 fractional bits = 12:4 format) | loop |

> **Note**: The `OPMULA.xyz` and `OPMSUB.xyz` instructions are used in the `TriangleWinding` macro (defined in `vcl_macros.i`) but are not directly called in `draw3d_triangle.vcl` — the program uses its own `ClipSpaceBackfaceCulling` macro instead, which computes the 2D cross product manually via `SUB`, `MUL`, and `SUB`.

---

## References

- PS2 Vector Unit Instruction Manual (part of Official PS2 Hardware Documentation)
- VCL Pre-Processor and VCL Assembler documentation (ee-kit)
- GS User's Manual — XYZ2 format, ADC bit behavior, PACKED GIF tags
- EE Core User's Manual — VIF1, DMA, micro-mode packet setup
