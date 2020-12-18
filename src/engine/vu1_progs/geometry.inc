; Load GS offsets to center xformed vertex in gs coord space and color clamping constant
#macro LoadConstants: gs_offsets, depth_bits_to_gs
    loi			2047.5
    addi.xy		gs_offsets, vf00, i
    mr32.z		gs_offsets, depth_bits_to_gs
    loi			12583167.0  ; Clamp colors to 255 (255 + 12582912)
    maxi.w		gs_offsets, vf00, i
#endmacro

; Fix clip scale and value to clip against (2048 - arbitrary)
#macro FixClipConstant: clip_scale
    loi			2048.0
    maxi.w		clip_scale, vf00, i
#endmacro
