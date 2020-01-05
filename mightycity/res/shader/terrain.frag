#version 450
#extension GL_ARB_separate_shader_objects : enable


layout (location = 0) in vec3 pass_normal;
layout (location = 1) in vec3 pass_frag_pos;
layout (location = 2) in vec3 pass_color;
layout (location = 3) in vec4 shadow_space;

// layout (binding = 1) uniform sampler2D tex_sampler;

layout (binding = 1) uniform LightData {
	vec3 light_dir;
	vec3 view_pos;
} light;

layout (binding = 3) uniform sampler2D shadow_depth;

layout (location = 0) out vec4 color;

const vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

#define ambient_strength 0.15f

float sample_shadow(vec4 shadow_coord, vec2 offset, float bias) {
	float shadow = 1.0;
	if ( shadow_coord.z > -1.0 && shadow_coord.z < 1.0 ) {
		// shadow_coord = shadow_coord * 0.5f + 0.5f;
		//shadow_coord.x = shadow_coord.x * 0.5f + 0.5f;
		//shadow_coord.y = shadow_coord.y * 0.5f + 0.5f;
		float dist = texture( shadow_depth, shadow_coord.st + offset).r;
		//float bias = max(0.01 * (1.0 - dot(pass_normal, light_dir)), 0.001);  
		if ( shadow_coord.w > 0.0 && dist < shadow_coord.z - bias) {
			shadow = ambient_strength;
		}
	}
	return shadow;
}

float calculate_shadow(vec4 shadow_coord, float bias) {

	ivec2 texture_size = textureSize(shadow_depth, 0);
	float scale = 1.5f;
	float dx = scale * 1.0 / float(texture_size.x);
	float dy = scale * 1.0 / float(texture_size.y);

	float shadow_factor = 0.0f;
	int count = 0;
	int range = 2;

	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadow_factor += sample_shadow(shadow_coord, vec2(dx * x, dy * y), bias);
			count++;
		}
	}

	return shadow_factor / count;
}


void main() {

	vec3 sampled_color = pass_color; // */= vec3(155.f / 255.f, 116.f / 255.f, 82.f / 255.f);

	// vec4 sampled_color = texture(tex_sampler, pass_tex);
	
	vec3 ambient = ambient_strength * light_color;

	vec3 normal = normalize(pass_normal);
	// vec3 light_dir = normalize(vec3(light.pos) - pass_frag_pos);
	vec3 light_dir = -light.light_dir;
	
	float diff = max(dot(normal, light_dir), 0.0f);
	vec3 diffuse = diff * light_color;

	float specular_strength = 0.3f;

	vec3 view_dir = normalize(vec3(light.view_pos) - pass_frag_pos);
	vec3 reflect_dir = reflect(-light_dir, normal);

	float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), 0.25);
	vec3 specular = specular_strength * spec * light_color;

	float bias = max(0.01 * (1.0 - dot(normal, light_dir)), 0.001);
	#if 1
	float shadow = calculate_shadow(shadow_space / shadow_space.w, bias);
	#else
	float shadow = 1.0f;
	#endif
	
	vec3 result = (ambient + shadow * (diffuse + specular)) * vec3(sampled_color);
	color = vec4(result, 1.0);
}
