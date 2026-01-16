
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
