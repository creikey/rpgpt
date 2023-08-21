@module threedee

@ctype mat4 Mat4
@ctype vec4 Vec4
@ctype vec3 Vec3
@ctype vec2 Vec2

@block inverse_functions
	// Webgl 1 doesn't have inverse() but we need it, pulled from https://github.com/glslify/glsl-inverse/blob/master/index.glsl
	mat4 my_inverse(mat4 m) {
	float
		a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3],
		a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3],
		a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3],
		a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3],

		b00 = a00 * a11 - a01 * a10,
		b01 = a00 * a12 - a02 * a10,
		b02 = a00 * a13 - a03 * a10,
		b03 = a01 * a12 - a02 * a11,
		b04 = a01 * a13 - a03 * a11,
		b05 = a02 * a13 - a03 * a12,
		b06 = a20 * a31 - a21 * a30,
		b07 = a20 * a32 - a22 * a30,
		b08 = a20 * a33 - a23 * a30,
		b09 = a21 * a32 - a22 * a31,
		b10 = a21 * a33 - a23 * a31,
		b11 = a22 * a33 - a23 * a32,

		det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

	return mat4(
		a11 * b11 - a12 * b10 + a13 * b09,
		a02 * b10 - a01 * b11 - a03 * b09,
		a31 * b05 - a32 * b04 + a33 * b03,
		a22 * b04 - a21 * b05 - a23 * b03,
		a12 * b08 - a10 * b11 - a13 * b07,
		a00 * b11 - a02 * b08 + a03 * b07,
		a32 * b02 - a30 * b05 - a33 * b01,
		a20 * b05 - a22 * b02 + a23 * b01,
		a10 * b10 - a11 * b08 + a13 * b06,
		a01 * b08 - a00 * b10 - a03 * b06,
		a30 * b04 - a31 * b02 + a33 * b00,
		a21 * b02 - a20 * b04 - a23 * b00,
		a11 * b07 - a10 * b09 - a12 * b06,
		a00 * b09 - a01 * b07 + a02 * b06,
		a31 * b01 - a30 * b03 - a32 * b00,
		a20 * b03 - a21 * b01 + a22 * b00) / det;
	}
@end


// for this block, define a variable called `model_space_pos` to be used as an input
@block vs_compute_light_output
	world_space_frag_pos = model * vec4(model_space_pos, 1.0);
	vec4 frag_pos = view * world_space_frag_pos;

	//@Speed I think we can just take the third row here and be fine.
	light_dir = normalize(my_inverse(directional_light_space_matrix) * vec4(0.0, 0.0, -1.0, 0.0)).xyz;

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

@include_block inverse_functions

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
	float wobble_factor;
	float time;
	float seed;
	vec3 wobble_world_source;
};

@include_block inverse_functions

void main() {
	//vec3 transformed_pos = vec3(pos_in.x, pos_in.y + sin(pos_in.x * 5.0 + pos_in.y * 9.0 + time*1.9)*0.045, pos_in.z);

	vec3 untransformed_world_pos = (model * vec4(pos_in, 1.0)).xyz;

	vec3 away = normalize(untransformed_world_pos - wobble_world_source);
	float t = time + seed;
	vec3 transformed_pos = pos_in + away * sin(t*20.0 + pos_in.y*3.0) * pos_in.y*0.25 * wobble_factor * 0.0;
	pos = transformed_pos;
	uv = uv_in;

	gl_Position = projection * view * model * vec4(transformed_pos, 1.0);

	vec3 model_space_pos = (vec4(transformed_pos, 1.0f)).xyz;
	@include_block vs_compute_light_output
}
@end

@fs fs

uniform sampler2D tex;
uniform sampler2D shadow_map;

uniform fs_params {
	int shadow_map_dimension;
	float how_much_not_to_blend_ground_color;
	int alpha_blend_int;
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
	float bias = 0.0004 * offset_scale_N + 0.0001 * offset_scale_L;

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

	bool alpha_blend = bool(alpha_blend_int);
	
	// desert lesbians
	/*
	if(how_much_not_to_blend_ground_color < 0.5)
	{
		float desertness = 1.0 - clamp(world_space_frag_pos.y/2.0, 0.0, 1.0);
		desertness = pow(desertness, 2.0);
		desertness *= 0.6;
		col.rgb = mix(col.rgb, vec3(206, 96, 33)/255.0, desertness);
	}
	*/

	//col.rgb = vec3(desertness, 0, 0);

	if(col.a < 0.1 && !alpha_blend)
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

		if(alpha_blend)
		{
			frag_color = vec4(col.rgb*lighting_factor, col.a);
		}
		else
		{
			frag_color = vec4(col.rgb*lighting_factor, 1.0);
		}
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

@vs vs_twodee
in vec3 position;
in vec2 texcoord0;
out vec2 uv;
out vec2 pos;

void main() {
    gl_Position = vec4(position.xyz, 1.0);
    uv = texcoord0;
    pos = position.xy;
}
@end

@fs fs_twodee
uniform sampler2D twodee_tex;
uniform twodee_fs_params {
    vec4 tint;

    // both in clip space
    vec2 clip_ul;
    vec2 clip_lr;

    float alpha_clip_threshold;

		vec2 tex_size;
};

in vec2 uv;
in vec2 pos;
out vec4 frag_color;


void main() {
    // clip space is from [-1,1] [left, right]of screen on X, and [-1,1] [bottom, top] of screen on Y
    if(pos.x < clip_ul.x || pos.x > clip_lr.x || pos.y < clip_lr.y || pos.y > clip_ul.y) discard;
    frag_color = texture(twodee_tex, uv) * tint;
    if(frag_color.a <= alpha_clip_threshold)
    {
     discard;
    }
    //frag_color = vec4(pos.x,0.0,0.0,1.0);
}
@end

@fs fs_twodee_outline
uniform sampler2D twodee_tex;
uniform twodee_fs_params {
    vec4 tint;

    // both in clip space
    vec2 clip_ul;
    vec2 clip_lr;

    float alpha_clip_threshold;

		vec2 tex_size;
};

in vec2 uv;
in vec2 pos;
out vec4 frag_color;


void main() {
    // clip space is from [-1,1] [left, right]of screen on X, and [-1,1] [bottom, top] of screen on Y
    if(pos.x < clip_ul.x || pos.x > clip_lr.x || pos.y < clip_lr.y || pos.y > clip_ul.y) discard;

		float left = texture(twodee_tex, uv + vec2(-1, 0)/tex_size).a;
		float right = texture(twodee_tex, uv + vec2(1, 0)/tex_size).a;
		float up = texture(twodee_tex, uv + vec2(0, 1)/tex_size).a;
		float down = texture(twodee_tex, uv + vec2(0, -1)/tex_size).a;

		if(
			false
			|| left > 0.1 && right < 0.1
			|| left < 0.1 && right > 0.1
			|| up < 0.1 && down > 0.1
			|| up > 0.1 && down < 0.1
		)
		{
			frag_color = vec4(1.0);
		}
		else
		{
			frag_color = vec4(0.0);
		}
}
@end

@fs fs_twodee_color_correction
uniform sampler2D twodee_tex;
uniform twodee_fs_params {
    vec4 tint;

    // both in clip space
    vec2 clip_ul;
    vec2 clip_lr;

    float alpha_clip_threshold;

		vec2 tex_size;
};

in vec2 uv;
in vec2 pos;
out vec4 frag_color;

// Black Box From https://github.com/armory3d/armory/blob/master/Shaders/std/tonemap.glsl
vec3 acesFilm(const vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d ) + e), 0.0, 1.0);
}

void main() {
    // clip space is from [-1,1] [left, right]of screen on X, and [-1,1] [bottom, top] of screen on Y
    if(pos.x < clip_ul.x || pos.x > clip_lr.x || pos.y < clip_lr.y || pos.y > clip_ul.y) discard;

		vec4 col = texture(twodee_tex, uv);

		col.rgb = acesFilm(col.rgb);

		frag_color = col;
}
@end


@fs fs_outline

uniform sampler2D tex;

in vec3 pos;
in vec2 uv;
in vec4 light_space_fragment_position;
in vec3 light_dir;
in vec4 world_space_frag_pos;

out vec4 frag_color;

void main() {
	vec4 col = texture(tex, uv);
	if(col.a < 0.5)
	{
		discard;
	}
	frag_color = vec4(vec3(1.0), col.a);
}
@end


@program mesh vs fs
@program armature vs_skeleton fs

@program mesh_shadow_mapping vs fs_shadow_mapping
@program armature_shadow_mapping vs_skeleton fs_shadow_mapping

@program mesh_outline vs fs_outline
@program armature_outline vs_skeleton fs_outline

@program twodee vs_twodee fs_twodee
@program twodee_outline vs_twodee fs_twodee_outline
@program twodee_colorcorrect vs_twodee fs_twodee_color_correction
