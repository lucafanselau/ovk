#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;
 
layout (binding = 0) uniform CameraData {
	mat4 view;
	mat4 projection;
} camera;

layout (push_constant) uniform PushConstant {
  mat4 model;
} pc;

////layout (location = 0) out vec2 pass_tex;
layout (location = 0) out vec3 pass_normal;
layout (location = 1) out vec3 pass_frag_pos;
//layout (location = 2) out vec3 pass_color;
//

// layout (location = 0) out vec3 pass_normal;
// layout (location = 1) out vec3 pass_frag_pos;
// layout (location = 2) out vec3 pass_color;
// layout (location = 3) out vec4 shadow_space;


void main() {
	
	// mat4 model = mat4(
	// 	pc.scale.x, 0, 0, 0,
	// 	0, pc.scale.y, 0, 0,
	// 	0, 0, pc.scale.z, 0,
	// 	pc.translate.xyz, 1
	// );

	gl_Position = camera.projection * camera.view * pc.model * vec4(in_pos, 1);
	pass_frag_pos = vec3(pc.model * vec4(in_pos, 1.0f)); // Only because we do not transform local space into world space at the moment
	pass_normal = mat3(transpose(inverse(pc.model))) * in_normal;
//	pass_color = in_color;
////	pass_tex = in_tex;
}
