
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
