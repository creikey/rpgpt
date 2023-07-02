@module armature

@vs vs
in vec3 pos_in;
in vec2 uv_in;
in vec4 indices_in; // is a sokol SG_VERTEXFORMAT_USHORT4N, a 16 bit unsigned integer treated as a floating point number due to webgl compatibility
in vec4 weights_in;

out vec3 pos;
out vec2 uv;

uniform vs_params {
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 bones[4];
};

void main() {
	vec4 total_position = vec4(0.0f);

	for(int i = 0; i < 4; i++)
	{
		float index_float = indices_in[i];
		int index = int(index_float * 65535.0);
		float weight = weights_in[i];

		vec4 local_position = bones[index] * vec4(pos_in, 1.0f);
		total_position += local_position * weight;
	}

	gl_Position = projection * view * model * total_position;
	//gl_Position = projection * view * model * vec4(pos_in, 1.0);

	pos = gl_Position.xyz;
	uv = uv_in;
}
@end

@fs fs
uniform sampler2D tex;
in vec3 pos;
in vec2 uv;
out vec4 frag_color;

void main() {
	vec4 col = texture(tex, uv);
	if(col.a < 0.5)
	{
		discard;
	}
	else
	{
		frag_color = vec4(col.rgb, 1.0);
	}
}
@end

@program program vs fs
