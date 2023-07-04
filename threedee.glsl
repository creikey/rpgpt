@module threedee

@vs vs
in vec3 pos_in;
in vec2 uv_in;

out vec3 pos;
out vec2 uv;
out vec4 light_space_fragment_position;


uniform vs_params {
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 directional_light_space_matrix;	
};

void main() {
	pos = pos_in;
	uv = uv_in;

	vec4 world_space_frag_pos = model * vec4(pos_in, 1.0);
	vec4 frag_pos = view * world_space_frag_pos;
	gl_Position = projection * frag_pos;

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

out vec4 frag_color;

float decodeDepth(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));
}

float do_shadow_sample(sampler2D shadowMap, vec2 uv, float scene_depth) {
	float map_depth = decodeDepth(texture(shadowMap, uv));
	map_depth += 0.001;//bias to counter self-shadowing
    return step(scene_depth, map_depth);
}


float calculate_shadow_factor(sampler2D shadowMap, vec4 light_space_fragment_position) {
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
            shadow += do_shadow_sample(shadowMap, shadow_uv+off, current_depth);
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
		vec3 light_dir = normalize(vec3(1, -1, 0));

		float shadow_factor = calculate_shadow_factor(shadow_map, light_space_fragment_position);
		shadow_factor = shadow_factor * 0.5 + 0.5;

		frag_color = vec4(col.rgb*shadow_factor, 1.0);
	}
}
@end

@program program vs fs
