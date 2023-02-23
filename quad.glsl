@module quad

@vs vs
in vec2 position;
in vec2 texcoord0;
out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs
uniform sampler2D tex;
uniform fs_params {
    vec4 tint;
};

in vec2 uv;
out vec4 frag_color;


void main() {
    frag_color = texture(tex, uv) * tint;
}
@end

@program program vs fs
