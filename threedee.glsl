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

uniform texture2D bones_tex;
uniform sampler vs_skeleton_smp;

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
				bone_mat[col][row] = decode_normalized_float32(texture(sampler2D(bones_tex, vs_skeleton_smp), vec2((0.5 + col*4 + row)/bones_tex_size.x, y_coord)));
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

uniform texture2D tex;
uniform texture2D shadow_map;
uniform sampler fs_smp;
uniform samplerShadow fs_shadow_smp;

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

float do_shadow_sample(texture2D shadowMap, vec2 uv, float scene_depth, float n_dot_l) {
	{
		//WebGL does not support GL_CLAMP_TO_BORDER, or border colors at all it seems, so we have to check explicitly.
		//This will probably slow down other versions which do support texture borders, but the current system does
		// not provide a non-overly complex way to include/not-include this code based on the backend. So here it is.
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
			return 1.0;
	}
	float map_depth = decodeDepth(texture(sampler2D(shadowMap, fs_shadow_smp), uv));

    // float bias = max(0.03f * (1.0f - n_dot_l), 0.005f);
    // bias = clamp(bias, 0.0, 0.01);

	float offset_scale_N = sqrt(1 - n_dot_l*n_dot_l);
	float offset_scale_L = offset_scale_N / n_dot_l;
	float bias = 0.0004 * offset_scale_N + 0.0001 * offset_scale_L;

	map_depth += bias;

    return step(scene_depth, map_depth);
}


float bilinear_shadow_sample(texture2D shadowMap, vec2 uv, int texture_width, int texture_height, float scene_depth_light_space, float n_dot_l) {
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

float calculate_shadow_factor(texture2D shadowMap, vec4 light_space_fragment_position, float n_dot_l) {
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
	vec4 col = texture(sampler2D(tex, fs_smp), uv);

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

	{

		vec3 normal = normalize(cross(dFdx(world_space_frag_pos.xyz), dFdy(world_space_frag_pos.xyz)));

		float n_dot_l = clamp(dot(normal, light_dir), 0.0, 1.0);
		float shadow_factor = calculate_shadow_factor(shadow_map, light_space_fragment_position, n_dot_l);

		float lighting_factor = shadow_factor * n_dot_l;
		lighting_factor = lighting_factor * 0.5 + 0.5;

		if (!alpha_blend)
		{
			float _Cutoff = 0.75; // Change this! it is tuned for existing bushes and TreeLayer leaves 2023-08-23
			col.a = (col.a - _Cutoff) / max(fwidth(col.a), 0.0001) + 0.5;
		}
	
		frag_color = vec4(col.rgb*lighting_factor, col.a);
	}
}
@end

@fs fs_shadow_mapping

uniform texture2D tex;
uniform sampler fs_shadow_mapping_smp;

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
	vec4 col = texture(sampler2D(tex, fs_shadow_mapping_smp), uv);
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
uniform texture2D twodee_tex;
uniform sampler fs_twodee_smp;

uniform twodee_fs_params {
    vec4 tint;

    // both in clip space
    vec2 clip_ul;
    vec2 clip_lr;

    float alpha_clip_threshold;
	float time;

		vec2 tex_size;
		vec2 screen_size;

		float flip_and_swap_rgb;
};

in vec2 uv;
in vec2 pos;
out vec4 frag_color;


void main() {
    // clip space is from [-1,1] [left, right]of screen on X, and [-1,1] [bottom, top] of screen on Y
    if(pos.x < clip_ul.x || pos.x > clip_lr.x || pos.y < clip_lr.y || pos.y > clip_ul.y) discard;
	vec2 real_uv = uv;
	if (flip_and_swap_rgb > 0) real_uv.y = 1 - real_uv.y;
	frag_color = texture(sampler2D(twodee_tex, fs_twodee_smp), real_uv) * tint;
	if (flip_and_swap_rgb > 0) frag_color.rgb = frag_color.bgr;
    if(frag_color.a <= alpha_clip_threshold)
    {
     discard;
    }
    //frag_color = vec4(pos.x,0.0,0.0,1.0);
}
@end

@fs fs_twodee_outline
uniform texture2D twodee_tex;
uniform sampler fs_twodee_outline_smp;

uniform twodee_fs_params {
    vec4 tint;

    // both in clip space
    vec2 clip_ul;
    vec2 clip_lr;

    float alpha_clip_threshold;
	float time;

		vec2 tex_size;
		vec2 screen_size;

		float flip_and_swap_rgb;
};

in vec2 uv;
in vec2 pos;
out vec4 frag_color;


void main() {
    // clip space is from [-1,1] [left, right]of screen on X, and [-1,1] [bottom, top] of screen on Y
    if(pos.x < clip_ul.x || pos.x > clip_lr.x || pos.y < clip_lr.y || pos.y > clip_ul.y) discard;
	
	vec2 real_uv = uv;
	if (flip_and_swap_rgb > 0) real_uv.y = 1 - real_uv.y;

		// 5-tap tent filter: centre, left, right, up, down
		float c = texture(sampler2D(twodee_tex, fs_twodee_outline_smp), real_uv).a;
		float l = texture(sampler2D(twodee_tex, fs_twodee_outline_smp), real_uv + vec2(-1, 0)/tex_size).a;
		float r = texture(sampler2D(twodee_tex, fs_twodee_outline_smp), real_uv + vec2(+1, 0)/tex_size).a;
		float u = texture(sampler2D(twodee_tex, fs_twodee_outline_smp), real_uv + vec2(0, +1)/tex_size).a;
		float d = texture(sampler2D(twodee_tex, fs_twodee_outline_smp), real_uv + vec2(0, -1)/tex_size).a;

		// if centre pixel is ~1, it is inside a shape.
		// if centre pixel is ~0, it is outside a shape.
		// if it is in the middle, it is near an MSAA-resolved edge.
		// we parallel-compute the inside AND outside glows, and then lerp using c.
		float lerp_t = c;

		// buffer is linear-space to be ACES-corrected later, but MSAA happens in gamma space;
		// I want a gamma-space blend; so I cheaply do pow(x, 2) by computing x*x.
		c *= c; l *= l; r *= r; u *= u; d *= d;

		float accum_o =     (c + l + r + u + d);
		float accum_i = 5 - (c + l + r + u + d);
		accum_o = 0.3 * accum_o;
		accum_i = 0.3 * accum_i;
		accum_o = clamp(accum_o, 0, 1);
		accum_i = clamp(accum_i, 0, 1);
		accum_o = sqrt(accum_o); // cheap gamma-undo
		accum_i = sqrt(accum_i); // cheap gamma-undo

		float accum = mix(accum_o, accum_i, lerp_t);
		frag_color = vec4(1, 1, 1, accum);
}
@end

@fs fs_twodee_color_correction
uniform texture2D twodee_tex;
uniform sampler fs_twodee_color_correction_smp;

uniform twodee_fs_params {
    vec4 tint;

    // both in clip space
    vec2 clip_ul;
    vec2 clip_lr;

    float alpha_clip_threshold;
	float time;

		vec2 tex_size;
		vec2 screen_size;

		float flip_and_swap_rgb;
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

@include tuning.h

void main() {
    // clip space is from [-1,1] [left, right]of screen on X, and [-1,1] [bottom, top] of screen on Y
    if(pos.x < clip_ul.x || pos.x > clip_lr.x || pos.y < clip_lr.y || pos.y > clip_ul.y) discard;

	vec2 real_uv = uv;
	if (flip_and_swap_rgb > 0) real_uv.y = 1 - real_uv.y;

		vec4 col = texture(sampler2D(twodee_tex, fs_twodee_color_correction_smp), real_uv);
		if (flip_and_swap_rgb > 0) col.rgb = col.bgr;

		col.rgb = acesFilm(col.rgb);

		#if (FILM_GRAIN_STRENGTH > 0)
		{ // Film grain
			vec2 grain_uv = gl_FragCoord.xy / screen_size.xy;
			float x = grain_uv.x * grain_uv.y * time * 24 + 100.0;
			vec3 noise = vec3(mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)) * 100.0;
			col.rgb += (noise - 0.5) * (FILM_GRAIN_STRENGTH * 0.01);
			col.rgb *= (1 - FILM_GRAIN_STRENGTH * 0.01);
		}
		#endif
		#if (CONTRAST_BOOST_MIN > 0 || CONTRAST_BOOST_MAX < 255)
		{ // Hard-clip contrast levels
			float min = CONTRAST_BOOST_MIN;
			float max = CONTRAST_BOOST_MAX;
			col.rgb -= min/255;
			col.rgb *= 255/(max-min);
		}
		#endif
		#if (VIGNETTE_STRENGTH > 0)
		{ // Vignette
			col.rgb *= clamp((2 - VIGNETTE_STRENGTH * 0.01) - length(gl_FragCoord.xy / screen_size.xy - vec2(0.5)), 0, 1);
		}
		#endif
		#if (CROSS_PROCESS_STRENGTH > 0)
		{ // Cross-process
			float cross_process_strength = CROSS_PROCESS_STRENGTH * 0.01;
			col.rg *= (col.rg * ((-cross_process_strength) * col.rg + (-1.5 * (-cross_process_strength))) + (0.5 * (-cross_process_strength) + 1));
			col.b  *= (col.b  * ((+cross_process_strength) * col.b  + (-1.5 * (+cross_process_strength))) + (0.5 * (+cross_process_strength) + 1));
		}
		#endif
		// col.rgb = clamp(col.rgb, 0, 1);

		frag_color = col;
}
@end


@fs fs_outline

uniform texture2D tex;
uniform sampler fs_outline_smp;

in vec3 pos;
in vec2 uv;
in vec4 light_space_fragment_position;
in vec3 light_dir;
in vec4 world_space_frag_pos;

out vec4 frag_color;

void main() {
	vec4 col = texture(sampler2D(tex, fs_outline_smp), uv);
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
