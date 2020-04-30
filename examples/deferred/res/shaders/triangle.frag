#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 pass_normal;

layout (location = 0) out vec4 color;

const vec3 material_color = vec3(0.639282, 0.483062, 0.269731);

const vec3 light_dir = normalize(vec3(5.0f, 8.0f, 3.0f));
const vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

void main() {
	color = vec4(0.83f, 0.12f, 0.23f, 1.0);
	float ambient_strength = 0.15f;
	vec3 ambient = ambient_strength * light_color * material_color;

	vec3 normal = normalize(pass_normal);
	// vec3 light_dir = normalize(vec3(light.pos) - pass_frag_pos);
	// vec3 light_dir = light.light_dir;
	
	float diff = max(dot(normal, light_dir), 0.15f);
	vec3 diffuse = diff * light_color * material_color;

	// float specular_strength = 0.3f;

	// vec3 view_dir = normalize(vec3(light.view_pos) - pass_frag_pos);
	// vec3 reflect_dir = reflect(-light_dir, normal);

	// float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), 2);
	// vec3 specular = specular_strength * spec * light_color * material_color;

	vec3 result = (ambient + diffuse);
	color = vec4(result, 1.0);
	
}
