# VU1 `draw3d_triangle.vcl` — Technical Analysis

## Overview

`draw3d_triangle.vcl` is a **VCL** (VU1 Control Language) microprogram that runs on the PlayStation 2's **Vector Unit 1 (VU1)**. It is an evolution of the simpler `draw3d.vcl` — whereas the original processes **one vertex at a time**, this program processes **one triangle (3 vertices) per loop iteration**, enabling **clip-space backface culling** before perspective division.

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
    ↓ [vcl — VCL Assembler]
draw3d_triangle.vsm     (final VU1 microcode)
```

- The `.vsm` binary is linked into the PS2 ELF via a `.section(\".vudata\")` attribute.
- At runtime, `path1Renderer->UploadVU1MicroProgram()` copies it into VU1's instruction memory.

---

## Memory Layout (Constants)

The program defines 12 quadword constants, loaded from VU1's `vi00`-relative data memory at program start:

| Constant | Address | Purpose |
|---|---|---|
| `MvpMatRow0` | 0 | MVP matrix row 0 |
| `MvpMatRow1` | 1 | MVP matrix row 1 |
| `MvpMatRow2` | 2 | MVP matrix row 2 |
| `MvpMatRow3` | 3 | MVP matrix row 3 |
| `Scale` | 4 | Viewport transform (scale + offset) |
| `RGBALoc` | 5 | Base RGBA color |
| `ModelMatRow0` | 6 | Model matrix row 0 |
| `ModelMatRow1` | 7 | Model matrix row 1 |
| `ModelMatRow2` | 8 | Model matrix row 2 |
| `ModelMatRow3` | 9 | Model matrix row 3 |
| `LightDirection` | 10 | Light direction vector (world space) |
| `LightIntensities` | 11 | Ambient (x), Diffuse (y), Specular (z) intensities |

The data following these constants is the **vertex buffer** sent by the EE core via DMA/GIF, starting at the address obtained from `XTOP`.

---

## Double-Buffering Strategy

The reference quad-buffering approach (Dr. Henry S. Fortuna) divides VU1's data memory into **4 buffers** (2 input + 2 output) and uses VIF1's `BASE`/`OFFSET` codes to remap addresses, creating a 3-way pipeline: upload data to one input buffer while the VU1 processes the other input buffer and the GS rasterizes from the output buffer. This project uses a **different memory layout** and a **simpler double-buffering scheme**.

### VU1 Data Memory Map

```
VU1 DMEM (4 KB = 256 qwords)
┌─────────────────────────────────────────────────┐
│ Constants (12 qwords):                         │
│  MVP matrix (4), Scale (1), RGBA (1),          │
│  Model matrix (4), Light direction (1),         │
│  Light intensities (1)                          │
│  Address: 0 — 11    (updated per chunk)         │
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
packet2_utils_vu_add_double_buffer(packet2, 12, (1024 - 12) / 2);
```
This reserves the first **12 qwords** for constants and splits the remaining data memory into **2 buffers** for vertex data. The EE re-uploads the MVP, model matrix, and light data to addresses 0–11 at the start of each chunk (since matrices change per model), so the constants are **updated per chunk**, not just at program start.

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

VIF1's double-buffer mode automatically alternates between the two data memory regions. The EE always unpacks data starting at address 0 (relative to the active buffer via the `TOP` register):

```cpp
u32 vifAddedBytes = 0; // zero because now we will use TOP register (double buffer)
```

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

### Comparison with Classic Quad-Buffering

| Feature | Classic Quad-Buffering (Fortuna) | This Project |
|---|---|---|
| **VU1 buffers** | 4 (2 input + 2 output) | 2 (VIF1 double buffer) |
| **EE-side packets** | 1 (single VIF packet) | 2 (ping-pong packets) |
| **VIF remapping** | `BASE`/`OFFSET` codes | VIF1 double-buffer mode (`TOP` register) |
| **Buffer size** | ~83 vertices each | ~81 vertices each (≈27 triangles) |
| **Constants location** | First 16 qwords of each buffer | First 12 qwords of DMEM (shared) |
| **Output buffer** | Separate from input | Same buffer (in-place transform) |
| **Program restart** | `MSCALL` (initial) / `MSCNT` (subsequent) | `MSCAL` / `MSCNT` |
| **Data flow** | Upload to input A → process → XGKICK output A | Upload to buffer → XTOP → process → XGKICK |

The project's approach is **simpler** — it uses VIF1's built-in double-buffer mode rather than manually managing 4 buffers via `BASE`/`OFFSET`. The EE-side packet double-buffering compensates for the reduced VU1 buffer count by allowing the EE to prepare the next chunk while DMA transmits the current one. The **in-place transform** design means that the input data is overwritten with output data as VU1 processes it — the `gifPacketStart` pointer points to the same memory region, and the output (GIF tag + processed vertices) is smaller than the input (positions + normals + STQ), so no data is corrupted during processing.

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

Across consecutive chunks, the three stages overlap: the EE prepares chunk N+1 while VIF1 uploads chunk N to VU1 DMEM; the VU1 processes chunk N while the GS rasterizes chunk N-1 via XGKICK. This creates a **3-stage pipeline** where EE packet fill, VIF1 DMA upload, and GS rasterization all run concurrently.

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

Note: The exact buffer capacity is determined by the VIF1 double-buffer configuration — the constant area (12 qwords) is excluded from VIF1's address remapping, and the remaining DMEM is split into 2 regions. With 81 vertices per chunk, the input (header + positions + normals + STQ ≈ 245 qwords) and output (GIF tag + 3N qwords ≈ 244 qwords) are stored in the same buffer, with the VU1 reading all input data before overwriting the buffer with output.

The buffer header for each chunk contains:
- **Qword 0**: Vertex count (integer)
- **Qword 1**: GIF tag defining the primitive type and expected register count

---

## Execution Flow

### 1. `setup:` — Load Constants

```
LQ      matrixMvp[0],      MvpMatRow0(vi00)
LQ      matrixMvp[1],      MvpMatRow1(vi00)
...
LQ      lightDirectionVec, LightDirection(vi00)
SUB.xyz lightDirectionVec, vf00, lightDirectionVec   ; Invert direction
LQ      lightIntensitiesVec, LightIntensities(vi00)
```

- Loads all 4 rows of the MVP matrix, the viewport scale, RGBA base color, model matrix, light direction (inverted), and light intensity factors into VF registers.
- **`LQ`** (Load Quadword): loads up to 4 fields of a VU FPR from adjacent words in VU data memory. The offset is in doublewords (16-byte units).
- `FCSET 0x000000` — zeroes the clipping flags register (required before using `CLIPw`).
- Initializes `dontDraw = 1 + 0x7FFF = 0x8000` — a sentinel value that, when stored in the `w` component of `XYZ2`, sets the **ADC** (ADjacent Context) bit in the GS, preventing the triangle from being drawn.

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
- The GIF tag (`primTag`) is stored at the start of the output buffer with **`SQI`** (Store Quadword Post-Increment): `[base] <- ft.dest; base += 8` (8 doublewords = 16 bytes = 1 quadword).

> **Important**: The `vertCount` represents the number of **vertices**, not triangles. Since triangles are 3 vertices each, the loop processes `vertCount / 3` triangles.

#### Official VU Instruction References (begin)

| Instruction | Format | Purpose |
|---|---|---|
| **XTOP** | `XTOP It` | VU1-only. Read the address of the micro-mode packet (GIF path control). |
| **ILW** | `ILW.dest ft, offset(base)` | Load up to 4 fields of a VU FPR from VU data cache at offset (in doublewords) from base. |
| **IADD** | `IADD Id, Is, It` | Integer add: `Id = Is + It` |
| **SQI** | `SQI.dest ft, base++` | Store up to 4 fields of a VU FPR to VU data cache with post-increment: `[base] <- ft.dest; base += 8` |

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

Transforms each vertex from model space to clip space using the MVP matrix. The macro expands to:

```
MUL     acc,           matrix[0], vertex[x]
MADD    acc,           matrix[1], vertex[y]
MADD    acc,           matrix[2], vertex[z]
MADD    vertex_result, matrix[3], vertex[w]
```

- **`MUL`** (Multiply): `ACC.field <- fs.field * ft.field`
- **`MADD`** (Multiply-Add): `fd.field <- ACC.field + (fs.field * ft.field)`
- Result: `vertex = MVP[0]*vx + MVP[1]*vy + MVP[2]*vz + MVP[3]*vw`

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

#### Official VU Instruction References (clipping)

| Instruction | Format | Purpose |
|---|---|---|
| **CLIPw** | `CLIPw.xyz ft, fs` | Update the clipping flag based on vertex vs. frustum plane comparisons. Clipping flag bits are set when vertex components exceed the `w` range. |
| **FCAND** | `FCAND It, imm24` | Logical AND of clipping flag with 24-bit immediate: `It <- clipping_flag AND imm24` |
| **ISW** | `ISW.dest ft, offset(base)` | Store up to 4 integer fields of a VU FPR to VU data cache at offset (in doublewords) from base. |

#### d. Perspective Division

```
PerformPerspectiveDivision{ vertex0, viewPortTransform, Stq0, modStq0 }
PerformPerspectiveDivision{ vertex1, viewPortTransform, Stq1, modStq1 }
PerformPerspectiveDivision{ vertex2, viewPortTransform, Stq2, modStq2 }
```

Expands to:

```
DIV     q,      vf00[w],    vertex[w]         ; Q = 1.0 / vertex.w
MUL.xyz vertex, vertex,     q                 ; xyz = xyz / w
MULA.xyz acc,   viewPortTransform, vf00[w]    ; acc = scale * 1.0
MADD.xyz vertex, vertex,   viewPortTransform  ; vertex = vertex * scale + scale
MULQ    outStq, inStq,     q                  ; perspective-correct STQ
```

- **`DIV Q, ft.e, fs.e`** — divides a field of one FPR by a field of another, storing in the **Q** register: `Q <- ft / fs`
- **`RSQRT Q, ft.e, fs.e`** — `Q <- ft / sqrt(fs)` (used in normal normalization later)
- The **Q register** is a 32-bit fixed-point register used for pipelined division and square root operations. Results from `DIV`/`RSQRT` are available after 7 cycles of latency.
- Performs the homogeneous divide: `x' = x/w`, `y' = y/w`, `z' = z/w`
- **`MULA`** (Multiply-Add to ACC only): `ACC.field <- fs.field * ft.field`
- **`MULQ`** (Multiply by Q): multiplies each field by the Q register value
- Applies viewport transform: maps NDC `[-1, 1]` to GS screen coordinates
- Corrects texture coordinates for perspective: `s' = s/w`, `t' = t/w`, `q' = q/w`

#### Official VU Instruction References (perspective divide)

| Instruction | Format | Purpose |
|---|---|---|
| **DIV** | `DIV Q, ft.e, fs.e` | Divide: `Q <- ft.e / fs.e` (7-cycle latency) |
| **MULA** | `MULA.dest fs, ft` | Multiply into accumulator: `ACC.field <- fs.field * ft.field` |
| **MADD** | `MADD.dest fd, fs, ft` | Multiply-add: `fd.field <- ACC.field + (fs.field * ft.field)` |

#### e. Clip-Space Backface Culling

```
ClipSpaceBackfaceCulling{ vertex0, vertex1, vertex2, intRes }
IBGTZ   intRes, culled
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
- **`FTOI0`** (Float To Integer, 0 decimal digits): converts floating-point to fixed-point integer by truncation. This discards the fractional part.
- **`MTIR`** (Move to Integer Register): `It <- fs.field` — moves one field of a VF register to a VI register.
- The sign of the cross product tells us the triangle's winding direction (CW vs CCW).
- Since the cross product is performed in clip space (before viewport transform), the sign convention matches the clip-space coordinate system: **positive = CW = backface** → culled.
- **`IBGTZ`** (Integer Branch if Greater Than Zero): `if It > 0, PC += imm` — branches to `culled:` if back-facing.
- This is more efficient than doing backface culling on the EE core, as it avoids the DMA round-trip for culled triangles.

#### f. Fixed-Point Conversion

```
FTOI4.xyz vertex0, vertex0
FTOI4.xyz vertex1, vertex1
FTOI4.xyz vertex2, vertex2
```

- **`FTOI4`** (Float To Integer, 4 decimal digits): converts floating-point to `12:4` fixed-point format (16 bits: 12 integer, 4 fractional bits). This is the format the GS expects for screen coordinates.
- Only `.xy` matters for the GS (which expects `XYZ2` in this format), but `.z` is also converted for the depth buffer.

#### g. Lighting Calculation (Ambient + Diffuse)

For each of the three vertices:

```
LQ      normal0, 0(normalData)
LQ      normal1, 1(normalData)
LQ      normal2, 2(normalData)
```

Then per vertex:

```
MatrixMultiplyVertex{ normal, matrixModel, normal }   ; Normal → world space
VectorNormalize{ normalOut, normal }                  ; Normalize
VectorDotProduct{ normalOut, normalOut, lightDirectionVec }  ; N·L

MAX.x   normalOut,        normalOut,      vf00[x]    ; clamp to 0+
MUL.x   normalOut,        normalOut,      lightIntensitiesVec[y]  ; × diffuse intensity

MUL.xyz  acc,             rgba,           lightIntensitiesVec[x]  ; ambient = base × ambientIntensity
MADD.xyz FinalCol,        rgba,           normalOut[x]            ; + base × (N·L) × diffuseIntensity
MINI.xyz FinalCol,        FinalCol,       rgba[w]                 ; Cap to 128
ADD.w    FinalCol,        vf00,           rgba[w]                 ; Alpha = 128
MAX.xyz  FinalCol,        FinalCol,       vf00[x]                 ; Clamp to 0+

FTOI0    FinalCol,        FinalCol                                ; Convert to 8-bit unsigned
```

##### Vector Normalize Macro

```
MUL.xyz   vclsmlftemp, vector,   vector           ; dot = v·v (squared length)
ADD.x     vclsmlftemp, vclsmlftemp, vclsmlftemp[y]
ADD.x     vclsmlftemp, vclsmlftemp, vclsmlftemp[z]
RSQRT     q,           vf00[w],   vclsmlftemp[x]  ; Q = 1.0 / sqrt(length²)
SUB.w     vecoutput,   vf00,      vf00            ; zero w
MUL.xyz   vecoutput,   vector,    q               ; normalized = vector * Q
```

- **`RSQRT Q, ft.e, fs.e`** — `Q <- ft.e / sqrt(fs.e)`. Here: `Q = 1.0 / sqrt(dot)`, giving the normalization factor.
- The Q register is shared with `DIV`, so the RSQRT result must be consumed before any other Q-dependent instruction.

##### Vector Dot Product Macro

```
MUL.xyz   dotproduct, vector1, vector2
ADD.x     dotproduct, dotproduct, dotproduct[y]
ADD.x     dotproduct, dotproduct, dotproduct[z]
```

- Computes `dot = v1.x*v2.x + v1.y*v2.y + v1.z*v2.z` by summing x, y, and z components into x.

##### Lighting Model

**Ambient + Diffuse only** (no specular):

| Component | Formula |
|---|---|
| **Ambient** | `base_color × ambient_intensity` (always present) |
| **Diffuse** | `base_color × max(0, N·L) × diffuse_intensity` |
| **Alpha** | Constant 128 (0x80) |

- **`MAX`** (Maximum): `fd.field <- max(fs.field, ft.field)` — clamps `normalOut.x` to ≥ 0 (no negative diffuse).
- **`MINI`** (Minimum): `fd.field <- min(fs.field, ft.field)` — caps color at 128 (`rgba[w]`) to prevent oversaturation.
- **`ADD.w`**: sets alpha to `0 + 128 = 128`.
- Colors are capped at 128 (not 255) — likely to prevent oversaturation when combined with other effects.
- **`FTOI0`** converts the final color to 8-bit unsigned integer format for the GS.

#### Official VU Instruction References (lighting)

| Instruction | Format | Purpose |
|---|---|---|
| **RSQRT** | `RSQRT Q, ft.e, fs.e` | Reciprocal square root: `Q <- ft.e / sqrt(fs.e)` |
| **MAX** | `MAX.dest fd, fs, ft` | Maximum: `fd.field <- max(fs.field, ft.field)` |
| **MINI** | `MINI.dest fd, fs, ft` | Minimum: `fd.field <- min(fs.field, ft.field)` |
| **FTOI0** | `FTOI0.dest ft, fs` | Float to integer (0 decimal digits): truncates fractional part |
| **FTOI4** | `FTOI4.dest ft, fs` | Float to fixed-point (4 decimal digits): `12:4` format for GS coordinates |
| **MUL** | `MUL.dest fd, fs, ft` | Multiply: `fd.field <- fs.field * ft.field` |

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
- Execution **falls through** to `store:` — both culled and visible triangles reach the store path.

---

### 4. `store:` — Write Output Buffer

```
SQ   modStq0,   0(destAddress)            ; STQ for vertex 0
SQ   modStq1,   0+3(destAddress)          ; STQ for vertex 1
SQ   modStq2,   0+6(destAddress)          ; STQ for vertex 2
SQ   FinalCol0, 1(destAddress)            ; RGBAQ for vertex 0
SQ   FinalCol1, 1+3(destAddress)          ; RGBAQ for vertex 1
SQ   FinalCol2, 1+6(destAddress)          ; RGBAQ for vertex 2
SQ.xyz vertex0, 2(destAddress)            ; XYZ2 for vertex 0
SQ.xyz vertex1, 2+3(destAddress)          ; XYZ2 for vertex 1
SQ.xyz vertex2, 2+6(destAddress)          ; XYZ2 for vertex 2
```

- **`SQ`** (Store Quadword): stores all 4 fields of a VF register to VU data memory.
- **`SQ.xyz`**: stores only x, y, z fields — leaving the `w` field intact. This is critical because:
  - For visible triangles: `vertex[w]` contains the perspective-correct `w` (pass-through from the homogeneous divide).
  - For culled triangles: `vertex[w]` was overwritten by the `ClipVertex` ADC marker (or the culled path's `ISW.w`).
- The GS reads the `w` component of `XYZ2` to determine the ADC bit — so this mechanism seamlessly enables or disables drawing per-vertex.
- Each vertex outputs **3 quadwords** in the `PACKED` GIF format with register chain `ST → RGBAQ → XYZ2`.
- The output for each triangle occupies **9 quadwords** (3 vertices × 3 qwords each).

#### Official VU Instruction References (store)

| Instruction | Format | Purpose |
|---|---|---|
| **SQ** | `SQ.dest ft, offset(base)` | Store up to 4 fields of a VU FPR to adjacent words in VU data cache. Offset in doublewords. |

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

#### Official VU Instruction References (loop control)

| Instruction | Format | Purpose |
|---|---|---|
| **IADDI** | `IADDI Id, Is, imm` | Integer add immediate (sign-extended): `Id = Is + imm` |
| **IBNE** | `IBNE Is, It, imm` | Integer branch if not equal: `if Is != It, PC += PC + imm` |

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
                      ──────────────────────────
Qword 1:              Vertex 0  →  ST (texture)
Qword 2:              Vertex 0  →  RGBAQ (color + texture Q)
Qword 3:              Vertex 0  →  XYZ2 (screen position + ADC)
                      ──────────────────────────
Qword 4:              Vertex 1  →  ST
Qword 5:              Vertex 1  →  RGBAQ
Qword 6:              Vertex 1  →  XYZ2
                      ──────────────────────────
Qword 7:              Vertex 2  →  ST
Qword 8:              Vertex 2  →  RGBAQ
Qword 9:              Vertex 2  →  XYZ2
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
| Lighting | Per-vertex | Per-vertex (same model) |
| Performance | Higher vertex throughput (1/loop) | Lower vertex throughput (3/loop), but skips invisible tris |

The primary advantage of the triangle-based approach is that **backface culling happens on VU1 without EE involvement**, saving DMA bandwidth for invisible triangles. The EE no longer needs to compute triangle winding or skip culled triangles when generating the display list.

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

---

## Optimization Notes

1. **Early ADC writes**: The clipping ADC values are written **before** the division and lighting pipeline. This is fine because the store step overwrites them with the properly computed `vertex[w]` (or they stay as ADC markers if culled).

2. **No specular**: The lighting is ambient + diffuse only. The `lightIntensitiesVec[z]` (specular intensity) is never referenced in the code.

3. **Color clamping at 128**: The `MINI.xyz FinalCol, FinalCol, rgba[w]` caps colors at 128 (which is `rgba[w]`), not 255. This is likely intentional — perhaps to prevent over-brightening or to match a specific color space for the target display.

4. **Backface culling in clip space**: The culling uses the **signed area** of the triangle in 2D clip space (xy after MVP but before perspective division). This is mathematically equivalent to screen-space culling but avoids the perspective divide for culled triangles.

5. **GIF packet format**: The `PACKED` format with 3 registers per vertex (`ST`, `RGBAQ`, `XYZ2`) is the most compact format for textured, Gouraud-shaded triangles. The `Q` component of `RGBAQ` is repurposed from the texture STQ's `q` value.

6. **ADC as a culling mechanism**: Using the GS's ADC bit for backface culling is an elegant hack. The GS processes all 3 vertices of the culled triangle but simply skips the drawing kick, treating the vertices as adjacent to the previous (visible) primitive.

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
│               ├── Lighting (per vertex):                        │
│               │   │  MUL + RSQRT + MUL → normalize normal      │
│               │   │  MUL + ADD → N·L dot product               │
│               │   │  MAX → clamp diffuse to 0+                 │
│               │   │  MUL × diffuse intensity                    │
│               │   │  MUL + MADD → ambient + diffuse color      │
│               │   │  MINI → cap at 128                         │
│               │   │  FTOI0 → 8-bit unsigned                    │
│               │                                                  │
│               └── SQ (ST | RGBAQ | XYZ2) × 3 verts → output    │
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
