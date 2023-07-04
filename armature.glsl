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

// in textures, color elements are delivered as unsigend normalize floats
// in [0, 1]. This makes them into [-1, 1] as the bone matrices require 
// such values to be correct
vec4 make_signed_again(vec4 v) {
	v.x = 2.0 * v.x - 1.0;
	v.y = 2.0 * v.y - 1.0;
	v.z = 2.0 * v.z - 1.0;
	v.w = 2.0 * v.w - 1.0;
	return v;
}

void main() {
	vec4 total_position = vec4(0.0f);

	for(int bone_influence_index = 0; bone_influence_index < 4; bone_influence_index++)
	{
		float index_float = indices_in[bone_influence_index];
		int index = int(index_float * 65535.0);
		float weight = weights_in[bone_influence_index];

		float y_coord = (0.5 + index)/bones_tex_size.y;
		
		vec4 col0 = texture(bones_tex, vec2((0.5 + 0)/bones_tex_size.x, y_coord));
		vec4 col1 = texture(bones_tex, vec2((0.5 + 1)/bones_tex_size.x, y_coord));
		vec4 col2 = texture(bones_tex, vec2((0.5 + 2)/bones_tex_size.x, y_coord));
		vec4 col3 = texture(bones_tex, vec2((0.5 + 3)/bones_tex_size.x, y_coord));

		col0 = make_signed_again(col0);
		col1 = make_signed_again(col1);
		col2 = make_signed_again(col2);
		col3 = make_signed_again(col3);

		mat4 bone_mat = mat4(col0, col1, col2, col3);

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
