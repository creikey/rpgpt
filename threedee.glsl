@module threedee

@ctype mat4 Mat4
@ctype vec4 Vec4
@ctype vec3 Vec3
@ctype vec2 Vec2


// for this block, define a variable called `model_space_pos` to be used as an input
@block vs_compute_light_output
	world_space_frag_pos = model * vec4(model_space_pos, 1.0);
	vec4 frag_pos = view * world_space_frag_pos;

	//@Speed I think we can just take the third row here and be fine.
	light_dir = normalize(inverse(directional_light_space_matrix) * vec4(0.0, 0.0, -1.0, 0.0)).xyz;

	light_space_fragment_position = directional_light_space_matrix * vec4(world_space_frag_pos.xyz, 1.0);

@end

@vs vs_skeleton
in vec3 pos_in;
in vec2 uv_in;
in vec4 indices_in; // is a sokol SG_VERTEXFORMAT_USHORT4N, a 16 bit unsigned integer treated as a floating point number due to webgl compatibility
in vec4 weights_in;

out vec3 pos;
out vec2 uv;
out vec4 light_space_fragment_position;
out vec3 light_dir;
out vec4 world_space_frag_pos;

uniform skeleton_vs_params {
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 directional_light_space_matrix;
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

	vec3 model_space_pos = (total_position).xyz;
	@include_block vs_compute_light_output
}
@end

@vs vs
in vec3 pos_in;
in vec2 uv_in;

out vec3 pos;
out vec2 uv;
out vec4 light_space_fragment_position;
out vec3 light_dir;
out vec4 world_space_frag_pos;

uniform vs_params {
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 directional_light_space_matrix;	
};

void main() {
	pos = pos_in;
	uv = uv_in;

	gl_Position = projection * view * model * vec4(pos_in, 1.0);

	vec3 model_space_pos = (vec4(pos_in, 1.0f)).xyz;
	@include_block vs_compute_light_output
}
@end

@fs fs

uniform sampler2D tex;
uniform sampler2D shadow_map;

uniform fs_params {
	int shadow_map_dimension;
};

in vec3 pos;
in vec2 uv;
in vec4 light_space_fragment_position;
in vec3 light_dir;
in vec4 world_space_frag_pos;

out vec4 frag_color;

float decodeDepth(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));
}

float do_shadow_sample(sampler2D shadowMap, vec2 uv, float scene_depth, float n_dot_l) {
	{
		//WebGL does not support GL_CLAMP_TO_BORDER, or border colors at all it seems, so we have to check explicitly.
		//This will probably slow down other versions which do support texture borders, but the current system does
		// not provide a non-overly complex way to include/not-include this code based on the backend. So here it is.
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
			return 1.0;
	}
	float map_depth = decodeDepth(texture(shadowMap, uv));

    // float bias = max(0.03f * (1.0f - n_dot_l), 0.005f);
    // bias = clamp(bias, 0.0, 0.01);

	float offset_scale_N = sqrt(1 - n_dot_l*n_dot_l);
	float offset_scale_L = offset_scale_N / n_dot_l;
	float bias = 0.0002 * offset_scale_N + 0.0001 * offset_scale_L;

	map_depth += bias;

    return step(scene_depth, map_depth);
}


float bilinear_shadow_sample(sampler2D shadowMap, vec2 uv, int texture_width, int texture_height, float scene_depth_light_space, float n_dot_l) {
	vec2 texture_dim = vec2(float(texture_width), float(texture_height));
	vec2 texel_dim  = vec2(1.0 / float(texture_width ), 1.0 / float(texture_height));

	vec2 texel_uv = uv * vec2(texture_dim);
	vec2 texel_uv_floor = floor(texel_uv) * texel_dim;
	vec2 texel_uv_ceil  =  ceil(texel_uv) * texel_dim;


	vec2 uv_0 = texel_uv_floor;
	vec2 uv_1 = vec2(texel_uv_ceil.x , texel_uv_floor.y);
	vec2 uv_2 = vec2(texel_uv_floor.x, texel_uv_ceil.y );
	vec2 uv_3 = vec2(texel_uv_ceil.x , texel_uv_ceil.y );

	float bl = do_shadow_sample(shadowMap, uv_0, scene_depth_light_space, n_dot_l);
	float br = do_shadow_sample(shadowMap, uv_1, scene_depth_light_space, n_dot_l);
	float tl = do_shadow_sample(shadowMap, uv_2, scene_depth_light_space, n_dot_l);
	float tr = do_shadow_sample(shadowMap, uv_3, scene_depth_light_space, n_dot_l);

	vec2 interp = fract(texel_uv);

	float bot = mix(bl, br, interp.x);
	float top = mix(tl, tr, interp.x);
	float result = mix(bot, top, interp.y);

	return result;
}

float calculate_shadow_factor(sampler2D shadowMap, vec4 light_space_fragment_position, float n_dot_l) {
	float shadow = 1.0;

	vec3 projected_coords = light_space_fragment_position.xyz / light_space_fragment_position.w;

	if(projected_coords.z > 1.0)
    	return shadow;

	projected_coords = projected_coords * 0.5f + 0.5f;

    float current_depth = projected_coords.z;

	vec2 shadow_uv = projected_coords.xy;

	float texel_step_size = 1.0 / float(shadow_map_dimension);

	for (int x=-2; x<=2; x++) {
        for (int y=-2; y<=2; y++) {
            vec2 off = vec2(x*texel_step_size, y*texel_step_size);
			// shadow += do_shadow_sample(shadowMap, shadow_uv+off, current_depth);
            shadow += bilinear_shadow_sample(shadowMap, shadow_uv+off, shadow_map_dimension, shadow_map_dimension, current_depth, n_dot_l);
        }
    }
    shadow /= 25.0;



	return shadow;
}


void main() {
	vec4 col = texture(tex, uv);
	if(col.a < 0.5)
	{
		discard;
	}
	else
	{

		vec3 normal = normalize(cross(dFdx(world_space_frag_pos.xyz), dFdy(world_space_frag_pos.xyz)));

		float n_dot_l = clamp(dot(normal, light_dir), 0.0, 1.0);
		float shadow_factor = calculate_shadow_factor(shadow_map, light_space_fragment_position, n_dot_l);

		float lighting_factor = shadow_factor * n_dot_l;
		lighting_factor = lighting_factor * 0.5 + 0.5;

		frag_color = vec4(col.rgb*lighting_factor, 1.0);
		//frag_color = vec4(col.rgb, 1.0);
	
	}
}
@end

@fs fs_shadow_mapping

uniform sampler2D tex;

in vec3 pos;
in vec2 uv;
in vec4 light_space_fragment_position;
in vec3 light_dir;
in vec4 world_space_frag_pos;

out vec4 frag_color;

vec4 encodeDepth(float v) {
    vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
    enc = fract(enc);
    enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
    return enc;
}

void main() {
	vec4 col = texture(tex, uv);
	if(col.a < 0.5)
	{
		discard;
	}
	
	float depth = gl_FragCoord.z;
	frag_color = encodeDepth(depth);
}
@end

@program mesh vs fs
@program armature vs_skeleton fs
@program mesh_shadow_mapping vs fs_shadow_mapping
@program armature_shadow_mapping vs_skeleton fs_shadow_mapping
