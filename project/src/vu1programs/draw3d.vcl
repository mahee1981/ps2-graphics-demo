#vuprog
#include "src/vu1programs/vcl_macros.i"

#define MvpMatRow0 0 
#define MvpMatRow1 1 
#define MvpMatRow2 2 
#define MvpMatRow3 3 
#define Scale 4
#define RGBALoc 5
#define ModelMatRow0 6 
#define ModelMatRow1 7 
#define ModelMatRow2 8 
#define ModelMatRow3 9 
#define LightDirection 10 
#define LightIntensities 11 
#define LightColor 12 


.syntax new
.vu
.name VU1Draw3D
.init_vf_all
.init_vi_all


--enter
--endenter

setup:

    ; //////// --- initialize local variables -- ////////
    lq    matrixMvp[0],    MvpMatRow0(vi00)            ; load the first row of the matrix
    lq    matrixMvp[1],    MvpMatRow1(vi00)            ; load the second row of the matrix
    lq    matrixMvp[2],    MvpMatRow2(vi00)            ; load the third row of the matrix
    lq    matrixMvp[3],    MvpMatRow3(vi00)            ; load the fourth row of the matrix
    lq.xyz    viewPortTransform,    Scale(vi00)        ; load the scale vector
    lq    rgba,    RGBALoc(vi00)                       ; RGBA
    lq    matrixModel[0],    ModelMatRow0(vi00)        ; load the first row of the model matrix
    lq    matrixModel[1],    ModelMatRow1(vi00)        ; load the second row of the model matrix
    lq    matrixModel[2],    ModelMatRow2(vi00)        ; load the third row of the model matrix
    lq    matrixModel[3],    ModelMatRow3(vi00)        ; load the fourth row of the model matrix
    lq    lightDirectionVec,    LightDirection(vi00)        ; load the light direction vector
    sub.xyz    lightDirectionVec, vf00, lightDirectionVec  ; Invert the light direction vector
    lq    lightIntensitiesVec,    LightIntensities(vi00)        ; load the ambient, diffuse and specular intensity

	fcset    0x000000                                       ; VCL won't let us use CLIP without first zeroing
                                                            ; the clip flags
    ;//////////// --- Load data 2 --- /////////////
    ; Updated dynamically
begin:
    xtop    iBase

    lq    primTag,    1(iBase)                              ; GIF tag - tell GS how much data we will send

    iaddiu    vertexData,    iBase,    2                    ; pointer to vertex data
    ilw.w    vertCount,    0(iBase)                         ; load vert count from scale vector
    iadd    normalData,    vertexData,    vertCount         ; pointer to normal data
    iadd    stqData,    normalData,    vertCount            ; pointer to stq
    iadd    gifPacketStart,    stqData,    vertCount        ; pointer for XGKICK, point data to start after all of the data that's been sent from EE core
    iadd    destAddress,    stqData,    vertCount           ; helper pointer for data inserting

    ;////////////////////////////////////////////

    ;/////////// --- Store tags --- /////////////
    sqi    primTag,    (destAddress++)                      ; prim + tell gs how many data will be
    ;////////////////////////////////////////////


    ;/////////////// --- Loop --- ///////////////
    iadd    vertexCounter,    iBase,    vertCount           ; loop vertCount times
loop:
    
    lq    vertex,    0(vertexData)                          ; load the vertex position data
    lq    normal,    0(normalData)
    lq    Stq,    0(stqData)                                ; load the texture coordinate data

    ;/////////////// --- Vertex Operations --- ///////////////

    MatrixMultiplyVertex{ vertex, matrixMvp, vertex }
                                                            ; macro from vcl_macros.i, preprocessed using vclpp
                                                            ; vclpp is very moody, and pay attention to spaces before
                                                            ; braces both in invocation and include files

    clipw.xyz    vertex,    vertex			                ; Dr. Fortuna: This instruction checks if the vertex is outside
                                                            ; the viewing frustum. If it is, then the appropriate
                                                            ; clipping flags are set
    fcand    VI01,    0x3FFFF                               ; Bitwise AND the clipping flags with 0x3FFFF, this makes
                                                            ; sure that we get the clipping judgement for the last three
                                                            ; verts (i.e. that make up the triangle we are about to draw)
    iaddiu    iADC,    VI01, 0x7FFF                         ; Add 0x7FFF. If any of the clipping flags were set this will
                                                            ; cause the triangle not to be drawn (any values above 0x8000
                                                            ; that are stored in the w component of XYZ2 will set the ADC
                                                            ; bit, which tells the GS not to perform a drawing kick on this
                                                            ; triangle.

    
    isw.w    iADC,    2(destAddress)
    
    div    q,    vf00[w],    vertex[w]                      ; perspective divide (1/vert[w]):
    mul.xyz    vertex,    vertex,    q
    mula.xyz    acc,    viewPortTransform,    vf00[w]       ; scale to GS screen space
    madd.xyz    vertex,    vertex,    viewPortTransform     ; multiply and add the scales -> vert = vert * scale + scale
    ftoi4.xyz    vertex,    vertex                          ; convert x and y to 12:4 fixed point format
    ;////////////////////////////////////////////


    ;//////////// --- Calculate light --- ////////////

    ; multiply normals by model matrix
    MatrixMultiplyVertex{ normal, matrixModel, normal }
    ; normalize the transformed vector
    VectorNormalize{ normalOut, normal }
    ; calculate the cosine between light direction and normal/diffuse impact
    VectorDotProduct{ normalOut, normalOut, lightDirectionVec }
    max.x    normalOut,    normalOut,    vf00[x]                  ; clamp the diffuseImpact 
    mul.x    normalOut,   normalOut,   lightIntensitiesVec[y]     ; multiply the diffuse factor by diffuse intensity

    mul.xyz    acc,    rgba,    lightIntensitiesVec[x]            ; Add the ambient light to final color
    madd.xyz    FinalCol,    rgba,    normalOut[x]                ; Add the diffuse lighting to final color
    mini.xyz    FinalCol,    FinalCol,     rgba[w]                ; Cap it to 128
    add.w    FinalCol,    vf00,    rgba[w]                        ; Set the alpha to 128 
    max.xyz     FinalCol,    FinalCol,    vf00[x]                 ; ensure that we send 0 or positive value

    ftoi0    FinalCol,    FinalCol                                ; convert to integer since that's the format expected
    ;////////////////////////////////////////////
 
    mulq    modStq,    Stq,    q                                  ; perform perspective correction on texture coordinates

    ;//////////// --- Store data --- ////////////
    sq    modStq,    0(destAddress)                         ; STQ
    sq    FinalCol,    1(destAddress)                       ; RGBA ; q is grabbed from stq
    sq.xyz    vertex,    2(destAddress)                     ; XYZ2


    iaddiu    vertexData,    vertexData,    1               ; move the vertex data pointer by one
    iaddiu    stqData,    stqData,    1                     ; move the texture data pointer by one
    iaddiu    normalData,    normalData,    1                     ; move the texture data pointer by one
    iaddiu    destAddress,    destAddress,    3             ;  move the output buffer address by 3

    iaddi    vertexCounter,    vertexCounter,    -1	        ; decrement the loop counter
    ibne    vertexCounter,    iBase,    loop	            ; and repeat if needed

loopend:

    xgkick    gifPacketStart                                ; dispatch to the GS rasterizer.
    --barrier
    --cont
    b    begin
--exit
--endexit
#endvuprog
