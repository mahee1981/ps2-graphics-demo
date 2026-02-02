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
.name VU1Draw3DTriangle
.init_vf_all
.init_vi_all


--enter
--endenter

setup:

    ; //////// --- initialize local variables -- ////////
    lq    matrixMvp[0],    MvpMatRow0(vi00)                 ; load the first row of the matrix
    lq    matrixMvp[1],    MvpMatRow1(vi00)                 ; load the second row of the matrix
    lq    matrixMvp[2],    MvpMatRow2(vi00)                 ; load the third row of the matrix
    lq    matrixMvp[3],    MvpMatRow3(vi00)                 ; load the fourth row of the matrix

    lq.xyz    viewPortTransform,    Scale(vi00)             ; load the scale vector
    lq    rgba,    RGBALoc(vi00)                            ; RGBA

    lq    matrixModel[0],    ModelMatRow0(vi00)             ; load the first row of the model matrix
    lq    matrixModel[1],    ModelMatRow1(vi00)             ; load the second row of the model matrix
    lq    matrixModel[2],    ModelMatRow2(vi00)             ; load the third row of the model matrix
    lq    matrixModel[3],    ModelMatRow3(vi00)             ; load the fourth row of the model matrix

    lq    lightDirectionVec,    LightDirection(vi00)        ; load the light direction vector
    sub.xyz    lightDirectionVec, vf00, lightDirectionVec   ; Invert the light direction vector
    lq    lightIntensitiesVec,    LightIntensities(vi00)    ; load the ambient, diffuse and specular intensity

	fcset    0x000000                                       ; VCL won't let us use CLIP without first zeroing
                                                            ; the clip flags
	iaddiu    dontDraw,    vi00,    1                       ; used for backface culling, a constant that
	iaddiu    dontDraw,    dontDraw,    0x7FFF              ; ignores the triangle and cuts it from being drawn
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
    
    lq    vertex0,    0(vertexData)                          ; load the vertex position data
    lq    vertex1,    1(vertexData)                          ; load the vertex position data
    lq    vertex2,    2(vertexData)                          ; load the vertex position data
    lq    Stq0,    0(stqData)                                ; load the texture coordinate data
    lq    Stq1,    1(stqData)                                ; load the texture coordinate data
    lq    Stq2,    2(stqData)                                ; load the texture coordinate data

    ;/////////////// --- Vertex Operations --- ///////////////

    MatrixMultiplyVertex{ vertex0, matrixMvp, vertex0 }
    MatrixMultiplyVertex{ vertex1, matrixMvp, vertex1 }
    MatrixMultiplyVertex{ vertex2, matrixMvp, vertex2 }
                                                            ; macro from vcl_macros.i, preprocessed using vclpp
                                                            ; vclpp is very moody, and pay attention to spaces before
                                                            ; braces both in invocation and include files

    ClipVertex{ vertex0, destAddress, 2, iADC }
    ClipVertex{ vertex1, destAddress, 2+3, iADC }
    ClipVertex{ vertex2, destAddress, 2+6, iADC }


    PerformPerspectiveDivision{ vertex0, viewPortTransform, Stq0, modStq0 }
    PerformPerspectiveDivision{ vertex1, viewPortTransform, Stq1, modStq1 }
    PerformPerspectiveDivision{ vertex2, viewPortTransform, Stq2, modStq2 }

    
    ClipSpaceBackfaceCulling{ vertex0, vertex1, vertex2, intRes }
    ibgtz    intRes, culled

    ftoi4.xyz    vertex0,    vertex0                          ; convert x and y to 12:4 fixed point format
    ftoi4.xyz    vertex1,    vertex1                          ; convert x and y to 12:4 fixed point format
    ftoi4.xyz    vertex2,    vertex2                          ; convert x and y to 12:4 fixed point format
    
    ;////////////////////////////////////////////

    lq    normal0,    0(normalData)
    lq    normal1,    1(normalData)
    lq    normal2,    2(normalData)


    ;//////////// --- Calculate light --- ////////////

    MatrixMultiplyVertex{ normal0, matrixModel, normal0 }
    VectorNormalize{ normal0Out, normal0 }
    VectorDotProduct{ normal0Out, normal0Out, lightDirectionVec }
    max.x    normal0Out,    normal0Out,    vf00[x]                  ; clamp the diffuseImpact 
    mul.x    normal0Out,   normal0Out,   lightIntensitiesVec[y]     ; multiply the diffuse factor by diffuse intensity

    mul.xyz    acc,    rgba,    lightIntensitiesVec[x]            ; Add the ambient light to final color
    madd.xyz    FinalCol0,    rgba,    normal0Out[x]                ; Add the diffuse lighting to final color
    mini.xyz    FinalCol0,    FinalCol0,     rgba[w]                ; Cap it to 128
    add.w    FinalCol0,    vf00,    rgba[w]                        ; Set the alpha to 128 
    max.xyz     FinalCol0,    FinalCol0,    vf00[x]                 ; ensure that we send 0 or positive value

    ftoi0    FinalCol0,    FinalCol0                                ; convert to integer since that's the format expected

    MatrixMultiplyVertex{ normal1, matrixModel, normal1 }
    VectorNormalize{ normal1Out, normal1 }
    VectorDotProduct{ normal1Out, normal1Out, lightDirectionVec }
    max.x    normal1Out,    normal1Out,    vf00[x]                  ; clamp the diffuseImpact 
    mul.x    normal1Out,   normal1Out,   lightIntensitiesVec[y]     ; multiply the diffuse factor by diffuse intensity

    mul.xyz    acc,    rgba,    lightIntensitiesVec[x]            ; Add the ambient light to final color
    madd.xyz    FinalCol1,    rgba,    normal1Out[x]                ; Add the diffuse lighting to final color
    mini.xyz    FinalCol1,    FinalCol1,     rgba[w]                ; Cap it to 128
    add.w    FinalCol1,    vf00,    rgba[w]                        ; Set the alpha to 128 
    max.xyz     FinalCol1,    FinalCol1,    vf00[x]                 ; ensure that we send 0 or positive value

    ftoi0    FinalCol1,    FinalCol1                                ; convert to integer since that's the format expected

    MatrixMultiplyVertex{ normal2, matrixModel, normal2 }
    VectorNormalize{ normal2Out, normal2 }
    VectorDotProduct{ normal2Out, normal2Out, lightDirectionVec }
    max.x    normal2Out,    normal2Out,    vf00[x]                  ; clamp the diffuseImpact 
    mul.x    normal2Out,   normal2Out,   lightIntensitiesVec[y]     ; multiply the diffuse factor by diffuse intensity

    mul.xyz    acc,    rgba,    lightIntensitiesVec[x]            ; Add the ambient light to final color
    madd.xyz    FinalCol2,    rgba,    normal2Out[x]                ; Add the diffuse lighting to final color
    mini.xyz    FinalCol2,    FinalCol2,     rgba[w]                ; Cap it to 128
    add.w    FinalCol2,    vf00,    rgba[w]                        ; Set the alpha to 128 
    max.xyz     FinalCol2,    FinalCol2,    vf00[x]                 ; ensure that we send 0 or positive value

    ftoi0    FinalCol2,    FinalCol2                                ; convert to integer since that's the format expected
    b   store
    ;////////////////////////////////////////////
culled:
   isw.w    dontDraw, 2(destAddress)
   isw.w    dontDraw, 2+3(destAddress)
   isw.w    dontDraw, 2+6(destAddress)
   move     FinalCol0, vf00
   move     FinalCol1, vf00
   move     FinalCol2, vf00

    ;//////////// --- Store data --- ////////////
store:
    sq    modStq0,    0(destAddress)                         ; STQ
    sq    modStq1,    0+3(destAddress)                         ; STQ
    sq    modStq2,    0+6(destAddress)                         ; STQ
    sq    FinalCol0,    1(destAddress)                       ; RGBA ; q is grabbed from stq
    sq    FinalCol1,    1+3(destAddress)                       ; RGBA ; q is grabbed from stq
    sq    FinalCol2,    1+6(destAddress)                       ; RGBA ; q is grabbed from stq
    sq.xyz    vertex0,    2(destAddress)                     ; XYZ2
    sq.xyz    vertex1,    2+3(destAddress)                     ; XYZ2
    sq.xyz    vertex2,    2+6(destAddress)                     ; XYZ2


    iaddiu    vertexData,    vertexData,    3               ; move the vertex data pointer by one
    iaddiu    stqData,    stqData,    3                     ; move the texture data pointer by one
    iaddiu    normalData,    normalData,    3                     ; move the texture data pointer by one
    iaddiu    destAddress,    destAddress,    9             ;  move the output buffer address by 3

    iaddi    vertexCounter,    vertexCounter,    -3	        ; decrement the loop counter
    ibne    vertexCounter,    iBase,    loop	            ; and repeat if needed

loopend:

    xgkick    gifPacketStart                                ; dispatch to the GS rasterizer.
    --barrier
    --cont
    b    begin
--exit
--endexit
#endvuprog
