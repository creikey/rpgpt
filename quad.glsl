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
    vec2 upper_left;
    vec2 lower_right;
};

in vec2 uv;
out vec4 frag_color;


void main() {
    vec2 actual_position = vec2(mix(upper_left.x, lower_right.x, uv.x), mix(upper_left.y, lower_right.y, uv.y));
    frag_color = texture(tex, actual_position) * tint;
}
@end

@program program vs fs
