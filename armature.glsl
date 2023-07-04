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
	vec2 bones_tex_size;
};
uniform sampler2D bones_tex;

float decode_normalized_float32(vec4 v)
{
	float sign = 2.0 * v.x - 1.0;

	return sign * (v.z*255.0 + v.y);
}

void main() {
	vec4 total_position = vec4(0.0f);

	for(int bone_influence_index = 0; bone_influence_index < 4; bone_influence_index++)
	{
		float index_float = indices_in[bone_influence_index];
		int index = int(index_float * 65535.0);
		float weight = weights_in[bone_influence_index];

		float y_coord = (0.5 + index)/bones_tex_size.y;

		mat4 bone_mat;

		for(int row = 0; row < 4; row++)
		{
			for(int col = 0; col < 4; col++)
			{
				bone_mat[col][row] = decode_normalized_float32(texture(bones_tex, vec2((0.5 + col*4 + row)/bones_tex_size.x, y_coord)));
			}
		}

		vec4 local_position = bone_mat * vec4(pos_in, 1.0f);
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
