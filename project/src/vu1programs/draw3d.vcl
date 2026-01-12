#vuprog
#include "src/vu1programs/vcl_macros.i"

#define MvpMatRow0 0 
#define MvpMatRow1 1 
#define MvpMatRow2 2 
#define MvpMatRow3 3 


.syntax new
.vu
.name VU1Draw3D
.init_vf_all
.init_vi_all


--enter
--endenter

setup:

    ; //////// --- initialize local variables -- ////////
    lq matrixMvp[0], MvpMatRow0(vi00)           ; load the first row of the matrix
    lq matrixMvp[1], MvpMatRow1(vi00)           ; load the second row of the matrix
    lq matrixMvp[2], MvpMatRow2(vi00)           ; load the third row of the matrix
    lq matrixMvp[3], MvpMatRow3(vi00)           ; load the fourth row of the matrix

    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
    xtop    iBase

    lq.xyz  viewPortTransform,  0(iBase)        ; load program params
                                                ; float : X, Y, Z - scale vector that we will use to scale the verts after projecting them.
                                                ; float : W - vert count.
    lq      primTag,        1(iBase)            ; GIF tag - tell GS how much data we will send
    lq      rgba,           2(iBase)             ; RGBA

    iaddiu  vertexData,     iBase,      3           ; pointer to vertex data
    ilw.w   vertCount,      0(iBase)                ; load vert count from scale vector
    iadd    stqData,        vertexData, vertCount   ; pointer to stq
    iadd    gifPacketStart, stqData,    vertCount   ; pointer for XGKICK, point data to start after all of the data that's been sent from EE core
    iadd    destAddress,    stqData,    vertCount   ; helper pointer for data inserting
    ;////////////////////////////////////////////

    ;/////////// --- Store tags --- /////////////
    sqi primTag,    (destAddress++) ; prim + tell gs how many data will be
    ;////////////////////////////////////////////

	fcset   0x000000                            ; VCL won't let us use CLIP without first zeroing
                                                ; the clip flags

    ;/////////////// --- Loop --- ///////////////
    iadd vertexCounter, iBase, vertCount ; loop vertCount times
loop:
    
    lq vertex, 0(vertexData)                      ; load the vertex position data
    lq Stq,  0(stqData)                         ; load the texture coordinate data

    MatrixMultiplyVertex{ vertex, matrixMvp, vertex }  
                                                ; macro from vcl_macros.i, preprocessed using vclpp
                                                ; vclpp is very moody, and pay attention to spaces before
                                                ; braces both in invocation and include files

    clipw.xyz	vertex, vertex			        ; Dr. Fortuna: This instruction checks if the vertex is outside
                                                ; the viewing frustum. If it is, then the appropriate
                                                ; clipping flags are set
    fcand		VI01, 0x3FFFF                   ; Bitwise AND the clipping flags with 0x3FFFF, this makes
                                                ; sure that we get the clipping judgement for the last three
                                                ; verts (i.e. that make up the triangle we are about to draw)
    iaddiu		iADC, VI01, 0x7FFF              ; Add 0x7FFF. If any of the clipping flags were set this will
                                                ; cause the triangle not to be drawn (any values above 0x8000
                                                ; that are stored in the w component of XYZ2 will set the ADC
                                                ; bit, which tells the GS not to perform a drawing kick on this
                                                ; triangle.

    
    isw.w		iADC,   2(destAddress)
    
    div         q,      vf00[w],    vertex[w]   ; perspective divide (1/vert[w]):
    mul.xyz     vertex, vertex,     q
    mula.xyz    acc,    viewPortTransform,      vf00[w]     ; scale to GS screen space
    madd.xyz    vertex, vertex,     viewPortTransform       ; multiply and add the scales -> vert = vert * scale + scale
    ftoi4.xy   vertex, vertex                  ; convert x and y to 12:4 fixed point format
    ftoi0.z   vertex, vertex                   ; convert z to an integer
    ;////////////////////////////////////////////
 
    mulq modStq, Stq, q

    ;//////////// --- Store data --- ////////////
    sq modStq,      0(destAddress)         ; STQ
    sq rgba,        1(destAddress)      ; RGBA ; q is grabbed from stq
    sq.xyz vertex,  2(destAddress)      ; XYZ2


    iaddiu    vertexData,  vertexData, 1   ; move the vertex data pointer by one
    iaddiu    stqData, stqData,  1          ; move the texture data pointer by one
    iaddiu    destAddress, destAddress, 3 ;  move the output buffer address by 3

    iaddi   vertexCounter,  vertexCounter,  -1	; decrement the loop counter 
    ibne    vertexCounter,  iBase,   loop	; and repeat if needed

loopend:
    --barrier

    xgkick gifPacketStart ; dispatch to the GS rasterizer.

--exit
--endexit
#endvuprog
