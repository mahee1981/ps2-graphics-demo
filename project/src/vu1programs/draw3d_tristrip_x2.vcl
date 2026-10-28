#vuprog
#include "src/vu1programs/vcl_macros.i"

#define MvpMatRow0       0
#define MvpMatRow1       1
#define MvpMatRow2       2
#define MvpMatRow3       3
#define Scale            4
#define RGBALoc          5
#define ModelMatRow0     6
#define ModelMatRow1     7
#define ModelMatRow2     8
#define ModelMatRow3     9
#define LightDirection   10
#define LightIntensities 11
;not used
#define LightColor       12
#define GifTagAd         12
#define GifTexSelect     13
#define CameraPos        14


.syntax new
.vu
.name VU1Draw3DTriStripV2
.init_vf_all
.init_vi_all


--enter
--endenter

setup:

    ;////////////////////////////////////////////////////////////
    ;// --- Initialize local variables ---
    ;////////////////////////////////////////////////////////////
    lq          matrixMvp[0],      MvpMatRow0(vi00)         ; load the first row of the matrix
    lq          matrixMvp[1],      MvpMatRow1(vi00)         ; load the second row of the matrix
    lq          matrixMvp[2],      MvpMatRow2(vi00)         ; load the third row of the matrix
    lq          matrixMvp[3],      MvpMatRow3(vi00)         ; load the fourth row of the matrix

    lq.xyz      viewPortTransform, Scale(vi00)              ; load the scale vector
    lq          rgba,              RGBALoc(vi00)             ; RGBA

    lq          matrixModel[0],    ModelMatRow0(vi00)       ; load the first row of the model matrix
    lq          matrixModel[1],    ModelMatRow1(vi00)       ; load the second row of the model matrix
    lq          matrixModel[2],    ModelMatRow2(vi00)       ; load the third row of the model matrix
    lq          matrixModel[3],    ModelMatRow3(vi00)       ; load the fourth row of the model matrix

    lq          lightDirectionVec, LightDirection(vi00)     ; load the light direction vector
    sub.xyz     toLight, vf00, lightDirectionVec            ; Invert the light direction vector
    lq          lightIntensitiesVec, LightIntensities(vi00) ; load the ambient, diffuse and specular intensity

    lq          gifAdTag,          GifTagAd(vi00)           ; load gif tag for selecting the texture;
    lq          tex0Config,        GifTexSelect(vi00)       ; load gif tag for selecting the texture;

    fcset       0x000000                                     ; VCL won't let us use CLIP without first zeroing
                                                            ; the clip flags
    iaddiu      dontDraw,          vi00,            1       ; used for backface culling, a constant that
    iaddiu      dontDraw,          dontDraw, 0x7FFF         ; ignores the triangle and cuts it from being drawn

    ;////////////////////////////////////////////////////////////
    ;// --- Load data from EE packet ---
    ;////////////////////////////////////////////////////////////
    ; Updated dynamically
begin:
    xtop        iBase

    lq          primTag,           1(iBase)                  ; GIF tag - tell GS how much data we will send

    iaddiu      vertexData,        iBase,            2      ; pointer to vertex data
    ilw.w       vertCount,         0(iBase)                 ; load vert count from scale vector
    iadd        normalData,        vertexData, vertCount    ; pointer to normal data
    iadd        stqData,           normalData,   vertCount  ; pointer to stq
    iadd        gifPacketStart,    stqData,      vertCount  ; pointer for XGKICK, point data to start after
                                                             ; all of the data that's been sent from EE core
    iadd        destAddress,       stqData,      vertCount  ; helper pointer for data inserting

    ;////////////////////////////////////////////////////////////
    ;// --- Store GIF tags ---
    ;////////////////////////////////////////////////////////////
    sqi         gifAdTag,          (destAddress++)           ; GIF tag to set a register in GS
    sqi         tex0Config,        (destAddress++)           ; set TEX0 register in GS (enable texture)
    sqi         primTag,           (destAddress++)           ; prim + tell gs how many data will be
    ;////////////////////////////////////////////////////////////


    ;////////////////////////////////////////////////////////////
    ;// --- Triangle loop: TWO triangles (6 vertices) per iteration ---
    ;// vertCount must be a multiple of 6.
    ;////////////////////////////////////////////////////////////
    iadd        vertexCounter,     iBase,        vertCount  ; loop vertCount times
loop:

    ; ---- triangle A: vertices 0..2, triangle B: vertices 3..5 ----
    lq          baseVertex0,       0(vertexData)             ; load the vertex position data
    lq          baseVertex1,       1(vertexData)
    lq          baseVertex2,       2(vertexData)
    lq          baseVertex3,       3(vertexData)
    lq          baseVertex4,       4(vertexData)
    lq          baseVertex5,       5(vertexData)
    lq          Stq0,              0(stqData)                ; load the texture coordinate data
    lq          Stq1,              1(stqData)
    lq          Stq2,              2(stqData)
    lq          Stq3,              3(stqData)
    lq          Stq4,              4(stqData)
    lq          Stq5,              5(stqData)

    ;////////////////////////////////////////////////////////////
    ;// --- Vertex Operations (both triangles in one basic block,
    ;//     so VCL can interleave them) ---
    ;////////////////////////////////////////////////////////////

    MatrixMultiplyVertex{ vertex0, matrixMvp, baseVertex0 }
    MatrixMultiplyVertex{ vertex1, matrixMvp, baseVertex1 }
    MatrixMultiplyVertex{ vertex2, matrixMvp, baseVertex2 }
    MatrixMultiplyVertex{ vertex3, matrixMvp, baseVertex3 }
    MatrixMultiplyVertex{ vertex4, matrixMvp, baseVertex4 }
    MatrixMultiplyVertex{ vertex5, matrixMvp, baseVertex5 }

    ; clip flags must be shifted in vertex by vertex, so keep this order
    ClipVertex{ vertex0, destAddress, 2,    iADC }
    ClipVertex{ vertex1, destAddress, 2+3,  iADC }
    ClipVertex{ vertex2, destAddress, 2+6,  iADC }
    ClipVertex{ vertex3, destAddress, 2+9,  iADC }
    ClipVertex{ vertex4, destAddress, 2+12, iADC }
    ClipVertex{ vertex5, destAddress, 2+15, iADC }

    PerformPerspectiveDivision{ vertex0, viewPortTransform, Stq0, Stq0 }
    PerformPerspectiveDivision{ vertex1, viewPortTransform, Stq1, Stq1 }
    PerformPerspectiveDivision{ vertex2, viewPortTransform, Stq2, Stq2 }
    PerformPerspectiveDivision{ vertex3, viewPortTransform, Stq3, Stq3 }
    PerformPerspectiveDivision{ vertex4, viewPortTransform, Stq4, Stq4 }
    PerformPerspectiveDivision{ vertex5, viewPortTransform, Stq5, Stq5 }

    ClipSpaceBackfaceCulling{ vertex0, vertex1, vertex2, intRes0 }
    ClipSpaceBackfaceCulling{ vertex3, vertex4, vertex5, intRes1 }

    ftoi4.xyz   vertex0,            vertex0                  ; convert x and y to 12:4 fixed point format
    ftoi4.xyz   vertex1,            vertex1
    ftoi4.xyz   vertex2,            vertex2
    ftoi4.xyz   vertex3,            vertex3
    ftoi4.xyz   vertex4,            vertex4
    ftoi4.xyz   vertex5,            vertex5

    sq.xyz      vertex0,            2(destAddress)           ; XYZ2
    sq.xyz      vertex1,            2+3(destAddress)
    sq.xyz      vertex2,            2+6(destAddress)
    sq.xyz      vertex3,            2+9(destAddress)
    sq.xyz      vertex4,            2+12(destAddress)
    sq.xyz      vertex5,            2+15(destAddress)

    sq          Stq0,               0(destAddress)           ; STQ
    sq          Stq1,               0+3(destAddress)
    sq          Stq2,               0+6(destAddress)
    sq          Stq3,               0+9(destAddress)
    sq          Stq4,               0+12(destAddress)
    sq          Stq5,               0+15(destAddress)

    ;////////////////////////////////////////////////////////////
    ;// --- Per-triangle epilogues (tiny; same behaviour as before) ---
    ;////////////////////////////////////////////////////////////

    ; ---- triangle A ----
    ibgtz       intRes0,            culledA
    move        FinalCol0,          rgba
    ftoi0       FinalCol0,          FinalCol0
    b           storeA
culledA:
    isw.w       dontDraw,           2(destAddress)
    isw.w       dontDraw,           2+3(destAddress)
    isw.w       dontDraw,           2+6(destAddress)
    move        FinalCol0,          vf00
storeA:
    sq          FinalCol0,          1(destAddress)           ; RGBA ; q is grabbed from stq
    sq          FinalCol0,          1+3(destAddress)
    sq          FinalCol0,          1+6(destAddress)

    ; ---- triangle B ----
    ibgtz       intRes1,            culledB
    move        FinalCol1,          rgba
    ftoi0       FinalCol1,          FinalCol1
    b           storeB
culledB:
    isw.w       dontDraw,           2+9(destAddress)
    isw.w       dontDraw,           2+12(destAddress)
    isw.w       dontDraw,           2+15(destAddress)
    move        FinalCol1,          vf00
storeB:
    sq          FinalCol1,          1+9(destAddress)         ; RGBA ; q is grabbed from stq
    sq          FinalCol1,          1+12(destAddress)
    sq          FinalCol1,          1+15(destAddress)

    ;////////////////////////////////////////////////////////////
    ;// --- Advance pointers by two triangles ---
    ;////////////////////////////////////////////////////////////
    iaddiu      vertexData,        vertexData,       6        ; 6 vertices
    iaddiu      stqData,           stqData,          6        ; 6 stq
    iaddiu      destAddress,       destAddress,      18       ; 6 vertices * 3 qwords

    iaddi       vertexCounter,     vertexCounter,   -6        ; decrement the loop counter
    ibne        vertexCounter,     iBase,            loop     ; and repeat if needed

loopend:

    xgkick      gifPacketStart                                ; dispatch to the GS rasterizer.
    --barrier
    --cont
    b           begin
--exit
--endexit
#endvuprog
