@module threedee

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

	world_space_frag_pos = model * vec4(pos_in, 1.0);
	vec4 frag_pos = view * world_space_frag_pos;
	gl_Position = projection * frag_pos;

	//@Speed I think we can just take the third row here and be fine.
	light_dir = normalize(inverse(directional_light_space_matrix) * vec4(0.0, 0.0, -1.0, 0.0)).xyz;

	light_space_fragment_position = directional_light_space_matrix * vec4(world_space_frag_pos.xyz, 1.0);
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
	
	}
}
@end

@program program vs fs
