@module threedee

@vs vs
in vec3 position;
out vec3 pos;

uniform vs_params {
	mat4 model;
	mat4 view;
	mat4 projection;
};

void main() {
	pos = position;
	gl_Position = projection * view * model * vec4(position, 1.0);
}
@end

@fs fs
in vec3 pos;
out vec4 frag_color;

void main() {
	frag_color = vec4(pos, 1.0);
}
@end

@program program vs fs
