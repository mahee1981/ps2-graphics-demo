
;//--------------------------------------------------------------------
;// MatrixMultiplyVertex - Multiply "matrix" by "vertex", and output
;// the result in "vertexresult"
;//
;// Note: Apply rotation, scale and translation
;// Note: ACC register is modified
;//--------------------------------------------------------------------
#macro  MatrixMultiplyVertex: vertex_result, matrix, vertex
    mul            acc,           matrix[0], vertex[x]
    madd           acc,           matrix[1], vertex[y]
    madd           acc,           matrix[2], vertex[z]
    madd           vertex_result, matrix[3], vertex[w]
#endmacro

;//--------------------------------------------------------------------
;// VectorDotProduct - Calculate the dot product of "vector1" and
;// "vector2", and output to "dotproduct"[x]
;//--------------------------------------------------------------------
#macro   VectorDotProduct:  dotproduct, vector1, vector2
   mul.xyz        dotproduct, vector1,    vector2
   add.x          dotproduct, dotproduct, dotproduct[y]
   add.x          dotproduct, dotproduct, dotproduct[z]
#endmacro

#macro   VectorNormalize:   vecoutput, vector
   mul.xyz        vclsmlftemp, vector,     vector
   add.x          vclsmlftemp, vclsmlftemp, vclsmlftemp[y]
   add.x          vclsmlftemp, vclsmlftemp, vclsmlftemp[z]
   rsqrt          q,           vf00[w],     vclsmlftemp[x]
   sub.w          vecoutput,  vf00,        vf00
   mul.xyz        vecoutput,  vector,     q
#endmacro

;//--------------------------------------------------------------------
;// TriangleWinding - Compute winding of triangle relative to "eyepos"
;// result is nonzero if winding is CW (actually depends on your
;// coordinate system)
;// Thanks to David Etherton (Angel Studios) for that one
;//
;// Note: ACC register is modified
;//--------------------------------------------------------------------
#macro         TriangleWinding:   result, vert1, vert2, vert3, eyepos
    sub.xyz        tw_vert12, vert2, vert1
    sub.xyz        tw_vert13, vert3, vert1

    opmula.xyz     ACC,       tw_vert12, tw_vert13
    opmsub.xyz     tw_normal, tw_vert13, tw_vert12

    sub.xyz        tw_dot, eyepos, vert1

    mul.xyz        tw_dot, tw_dot, tw_normal
    add.x          tw_dot, tw_dot, tw_dot[y]
    add.x          tw_dot, tw_dot, tw_dot[z]

    fsand          result,   0x2
#endmacro

;//---------------------------------------------------------
;// ClipVertex - checks if the vertex is outside the viewing frustum. 
;// If it is, then the appropriate clipping flags are set.
;// 2 - Bitwise AND the clipping flags with 0x3FFFF, this makes sure that 
;//     we get the clipping judgement for the last three verts 
;//     (i.e. that make up the triangle we are about to draw)
;// 3 - Add 0x7FFF. If any of the clipping flags were set this will
;//     cause the triangle not to be drawn (any values above 0x8000
;//     that are stored in the w component of XYZ2 will set the ADC
;//     bit, which tells the GS not to perform a drawing kick on this
;//     triangle.
;//---------------------------------------------------------
#macro ClipVertex: t_vertex, t_destAddress, t_destAddressOffset, iADC
    clipw.xyz	t_vertex,   t_vertex	
    fcand       VI01,       0x3FFFF
    iaddiu      iADC,     VI01, 0x7FFF
    isw.w       iADC,     t_destAddressOffset(t_destAddress)
#endmacro


#macro PerformPerspectiveDivision: vertex, viewPortTransform, inStq, outStq
    div    q,    vf00[w],    vertex[w]
    mul.xyz    vertex,    vertex,    q
    mula.xyz    acc,    viewPortTransform,    vf00[w]
    madd.xyz    vertex,    vertex,    viewPortTransform
    mulq    outStq,    inStq,    q
#endmacro

#macro ClipSpaceBackfaceCulling: vertex0, vertex1, vertex2, cullingDecision
    sub   bfCull1,      vf00,   vf00     
    sub.xy   bfCull1,     vertex1,   vertex0
    sub   bfCull2,      vf00,   vf00  
    sub.xy   bfCull2,     vertex2,   vertex0
    mul   cullResult,      vf00,   vf00  
    mul.x     cullResult,     bfCull1,     bfCull2[y]
    mul.y     cullResult,     bfCull1,     bfCull2[x]
    sub.x     cullResult,     cullResult,     cullResult[y]
    ftoi0   cullResult,     cullResult
    mtir    intRes,     cullResult[x]
#endmacro


