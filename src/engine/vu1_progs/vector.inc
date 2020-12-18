#macro VectorPerspectiveDivide: vertex
    div         q,      vf00[w],    vertex[w]
    mulq.xyz    vertex, vertex,     q
#endmacro

#macro VectorAddGSScales: output_vertex, input_vertex, gs_offsets
    add.xyz     output_vertex, input_vertex, gs_offsets
    ftoi4.xyz   output_vertex, output_vertex  
#endmacro

#macro VectorTexturePerspectiveCorrection: output_vertex, input_vertex
    mulq    output_vertex,  input_vertex,   q
#endmacro

#macro VectorStore: vertex, vumem, num
    sq  vertex, num(vumem)
#endmacro

#macro VectorClip: output_vertex, input_vertex, clip_scale
    mul.xyz		output_vertex, input_vertex, clip_scale
    clipw.xyz	output_vertex, clip_scale[w]
#endmacro

#macro VectorLoad: output_vertex, vumem, offset
    lq output_vertex, offset(vumem)
#endmacro