/* quad vertex shader */
@vs vs
in vec2 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

/* quad fragment shader */
@fs fs
uniform sampler2D tex;

in vec2 uv;
out vec4 frag_color;


void main() {
    frag_color = texture(tex, uv);
}
@end

/* quad shader program */
@program quad vs fs
