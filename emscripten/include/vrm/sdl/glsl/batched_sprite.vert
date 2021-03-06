attribute vec4 a_pos_tex_coords;
attribute vec4 a_color;
attribute float a_hue;

uniform mat4 u_projection_view;

varying vec4 v_color;
varying vec2 v_tex_coords;
varying float v_hue;

void main()
{
    v_tex_coords = a_pos_tex_coords.zw;
    v_color = a_color;
    v_hue = a_hue;

    gl_Position = u_projection_view * vec4(a_pos_tex_coords.xy, 0.0, 1.0);
}
