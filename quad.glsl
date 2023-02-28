@module quad

@vs vs
in vec2 position;
in vec2 texcoord0;
out vec2 uv;
out vec2 pos;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
    pos = position;
}
@end

@fs fs
uniform sampler2D tex;
uniform fs_params {
    vec4 tint;

    // both in clip space
    vec2 clip_ul;
    vec2 clip_lr;
};

in vec2 uv;
in vec2 pos;
out vec4 frag_color;


void main() {
    // clip space is from [-1,1] [left, right]of screen on X, and [-1,1] [bottom, top] of screen on Y
    if(pos.x < clip_ul.x || pos.x > clip_lr.x || pos.y < clip_lr.y || pos.y > clip_ul.y) discard;
    frag_color = texture(tex, uv) * tint;
    //frag_color = vec4(pos.x,0.0,0.0,1.0);
}
@end

@program program vs fs 
