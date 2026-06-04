;////////////////////////////////////////////////////////////////////
;// --- MatrixMultiplyVertex ---
;////////////////////////////////////////////////////////////////////
;// Multiply "matrix" by "vertex", and output the result in "vertexresult"
;//
;// Note: Apply rotation, scale and translation
;// Note: ACC register is modified
;////////////////////////////////////////////////////////////////////
#macro MatrixMultiplyVertex: vertex_result, matrix, vertex
    mul         acc,           matrix[0], vertex[x]
    madd        acc,           matrix[1], vertex[y]
    madd        acc,           matrix[2], vertex[z]
    madd        vertex_result, matrix[3], vertex[w]
#endmacro

;////////////////////////////////////////////////////////////////////
;// --- VectorDotProduct ---
;////////////////////////////////////////////////////////////////////
;// Calculate the dot product of "vector1" and "vector2",
;// and output to "dotproduct"[x]
;////////////////////////////////////////////////////////////////////
#macro VectorDotProduct: dotproduct, vector1, vector2
    mul.xyz     dotproduct, vector1,    vector2
    add.x       dotproduct, dotproduct, dotproduct[y]
    add.x       dotproduct, dotproduct, dotproduct[z]
#endmacro

;////////////////////////////////////////////////////////////////////
;// --- VectorNormalize ---
;////////////////////////////////////////////////////////////////////
#macro VectorNormalize: vecoutput, vector
    mul.xyz     vclsmlftemp, vector,     vector
    add.x       vclsmlftemp, vclsmlftemp, vclsmlftemp[y]
    add.x       vclsmlftemp, vclsmlftemp, vclsmlftemp[z]
    rsqrt       q,           vf00[w],     vclsmlftemp[x]
    sub.w       vecoutput,   vf00,        vf00
    mul.xyz     vecoutput,   vector,      q
#endmacro

;////////////////////////////////////////////////////////////////////
;// --- TriangleWinding ---
;////////////////////////////////////////////////////////////////////
;// Compute winding of triangle relative to "eyepos"
;// result is nonzero if winding is CW (actually depends on your
;// coordinate system)
;// Thanks to David Etherton (Angel Studios) for that one
;//
;// Note: ACC register is modified
;////////////////////////////////////////////////////////////////////
#macro TriangleWinding: result, vert1, vert2, vert3, eyepos
    sub.xyz     tw_vert12, vert2,    vert1
    sub.xyz     tw_vert13, vert3,    vert1

    opmula.xyz  ACC,       tw_vert12, tw_vert13
    opmsub.xyz  tw_normal, tw_vert13, tw_vert12

    sub.xyz     tw_dot,    eyepos,   vert1

    mul.xyz     tw_dot,    tw_dot,   tw_normal
    add.x       tw_dot,    tw_dot,   tw_dot[y]
    add.x       tw_dot,    tw_dot,   tw_dot[z]

    fsand       result,    0x2
#endmacro

;////////////////////////////////////////////////////////////////////
;// --- ClipVertex ---
;////////////////////////////////////////////////////////////////////
;// checks if the vertex is outside the viewing frustum.
;// If it is, then the appropriate clipping flags are set.
;// 2 - Bitwise AND the clipping flags with 0x3FFFF, this makes sure
;//     that we get the clipping judgement for the last three verts
;//     (i.e. that make up the triangle we are about to draw)
;// 3 - Add 0x7FFF. If any of the clipping flags were set this will
;//     cause the triangle not to be drawn (any values above 0x8000
;//     that are stored in the w component of XYZ2 will set the ADC
;//     bit, which tells the GS not to perform a drawing kick on this
;//     triangle.
;////////////////////////////////////////////////////////////////////
#macro ClipVertex: t_vertex, t_destAddress, t_destAddressOffset, iADC
    clipw.xyz   t_vertex,               t_vertex
    fcand       VI01,                   0x3FFFF
    iaddiu      iADC,                   VI01,         0x7FFF
    isw.w       iADC,                   t_destAddressOffset(t_destAddress)
#endmacro

;////////////////////////////////////////////////////////////////////
;// --- PerformPerspectiveDivision ---
;////////////////////////////////////////////////////////////////////
#macro PerformPerspectiveDivision: vertex, viewPortTransform, inStq, outStq
    div         q,              vf00[w],           vertex[w]
    mul.xyz     vertex,         vertex,            q
    mula.xyz    acc,            viewPortTransform, vf00[w]
    madd.xyz    vertex,         vertex,            viewPortTransform
    mulq        outStq,         inStq,             q
#endmacro

;////////////////////////////////////////////////////////////////////
;// --- ClipSpaceBackfaceCulling ---
;////////////////////////////////////////////////////////////////////
#macro ClipSpaceBackfaceCulling: vertex0, vertex1, vertex2, cullingDecision
    sub         bfCull1,        vf00,      vf00
    sub.xy      bfCull1,        vertex1,   vertex0
    sub         bfCull2,        vf00,      vf00
    sub.xy      bfCull2,        vertex2,   vertex0
    mul         cullResult,     vf00,      vf00
    mul.x       cullResult,     bfCull1,   bfCull2[y]
    mul.y       cullResult,     bfCull1,   bfCull2[x]
    sub.x       cullResult,     cullResult, cullResult[y]
    ftoi0       cullResult,     cullResult
    mtir        intRes,         cullResult[x]
#endmacro

;////////////////////////////////////////////////////////////////////
;// --- CalculateLightingSpecular ---
;////////////////////////////////////////////////////////////////////
;// Calculate ambient + diffuse + specular lighting for a single vertex.
;// All inner macros (MatrixMultiplyVertex, VectorNormalize,
;// VectorDotProduct) have been expanded inline because vclpp does not
;// support multi-layer macro expansion.
;//
;// Parameters:
;//   outputColor - register to store integer final color (FinalColN)
;//   normal      - vertex normal, transformed and normalized in-place
;//   baseVertex  - vertex position in object space
;//   diffuseOut  - register to store diffuse dot product intermediate
;//   cameraPos   - camera position (must be pre-loaded)
;//
;// Scratch registers used (shared across calls):
;//   vclsmlftemp, toCamera, halfVec, vecWorld, specComponent
;//
;// Preconditions:
;//   toLight, rgba, lightIntensitiesVec, matrixModel must be loaded.
;////////////////////////////////////////////////////////////////////
#macro CalculateLightingSpecular: outputColor, normal, baseVertex, cameraPos, matrixModel, lightIntensitiesVec, rgba, toLight

    mul         acc,           matrixModel[0], normal[x]
    madd        acc,           matrixModel[1], normal[y]
    madd        acc,           matrixModel[2], normal[z]
    madd        normal,        matrixModel[3], normal[w]
    mul.xyz     vclsmlftemp,   normal,         normal
    add.x       vclsmlftemp,   vclsmlftemp,    vclsmlftemp[y]
    add.x       vclsmlftemp,   vclsmlftemp,    vclsmlftemp[z]
    rsqrt       q,             vf00[w],        vclsmlftemp[x]
    sub.w       normal,        vf00,           vf00
    mul.xyz     normal,        normal,         q

    mul.xyz     diffuseOut,    normal,         toLight
    add.x       diffuseOut,    diffuseOut,     diffuseOut[y]
    add.x       diffuseOut,    diffuseOut,     diffuseOut[z]
    max.x       diffuseOut,    diffuseOut,     vf00[x]
    mul.x       diffuseOut,    diffuseOut,     lightIntensitiesVec[y]

    mul         acc,           matrixModel[0], baseVertex[x]
    madd        acc,           matrixModel[1], baseVertex[y]
    madd        acc,           matrixModel[2], baseVertex[z]
    madd        vecWorld,      matrixModel[3], baseVertex[w]

    sub.xyz     toCamera,      cameraPos,      vecWorld
    mul.xyz     vclsmlftemp,   toCamera,       toCamera
    add.x       vclsmlftemp,   vclsmlftemp,    vclsmlftemp[y]
    add.x       vclsmlftemp,   vclsmlftemp,    vclsmlftemp[z]
    rsqrt       q,             vf00[w],        vclsmlftemp[x]
    sub.w       toCamera,      vf00,           vf00
    mul.xyz     toCamera,      toCamera,       q

    add.xyz     halfVec,       toCamera,       toLight
    mul.xyz     vclsmlftemp,   halfVec,        halfVec
    add.x       vclsmlftemp,   vclsmlftemp,    vclsmlftemp[y]
    add.x       vclsmlftemp,   vclsmlftemp,    vclsmlftemp[z]
    rsqrt       q,             vf00[w],        vclsmlftemp[x]
    sub.w       halfVec,       vf00,           vf00
    mul.xyz     halfVec,       halfVec,        q

    mul.xyz     specComponent, halfVec,        normal
    add.x       specComponent, specComponent,  specComponent[y]
    add.x       specComponent, specComponent,  specComponent[z]
    mul.x       specComponent, specComponent,  specComponent
    mul.x       specComponent, specComponent,  specComponent
    mul.x       specComponent, specComponent,  specComponent
    mul.x       specComponent, specComponent,  specComponent
    mul.x       specComponent, specComponent,  specComponent
    mul.x       specComponent, specComponent,  lightIntensitiesVec[z]
    max.x       specComponent, specComponent,  vf00[x]
    mini.x      specComponent, specComponent,  vf00[w]

    mul.xyz     acc,           rgba,           lightIntensitiesVec[x]
    madd.xyz    acc,           rgba,           diffuseOut[x]
    madd.xyz    outputColor,   rgba,           specComponent[x]
    mini.xyz    outputColor,   outputColor,    rgba[w]
    add.w       outputColor,   vf00,           rgba[w]
    max.xyz     outputColor,   outputColor,    vf00[x]
    ftoi0       outputColor,   outputColor
#endmacro
