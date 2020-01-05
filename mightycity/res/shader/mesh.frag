#version 450
#extension GL_ARB_separate_shader_objects : enable
 
 layout (location = 0) in vec3 pass_normal;
 layout (location = 1) in vec3 pass_frag_pos;
 
 layout (binding = 1) uniform LightData {
	vec3 light_dir;
	vec3 view_pos;
 } light;

 layout (binding = 2) uniform Material {
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	float shininess;
 } material;

 layout (location = 0) out vec4 color;

 const vec3 light_color = vec3(1.0f, 1.0f, 1.0f);

 void main() {
	// vec3 sampled_color =  vec3(155.f / 255.f, 116.f / 255.f, 82.f / 255.f);

	// vec4 sampled_color = texture(tex_sampler, pass_tex);

	float ambient_strength = 0.15f;
	vec3 ambient = ambient_strength * light_color * material.ambient;

	vec3 normal = normalize(pass_normal);
	// vec3 light_dir = normalize(vec3(light.pos) - pass_frag_pos);
	vec3 light_dir = light.light_dir;
	
	float diff = max(dot(normal, light_dir), 0.15f);
	vec3 diffuse = diff * light_color * material.diffuse;

	float specular_strength = 0.3f;

	vec3 view_dir = normalize(vec3(light.view_pos) - pass_frag_pos);
	vec3 reflect_dir = reflect(-light_dir, normal);

	float spec = pow(max(dot(view_dir, reflect_dir), 0.0f), material.shininess);
	vec3 specular = specular_strength * spec * light_color * material.specular;

	vec3 result = (ambient + diffuse + specular);
	color = vec4(result, 1.0);
 }
