#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_pos;
//layout (location = 1) in vec2 in_tex;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
//
layout (binding = 0) uniform CameraData {
	mat4 view;
	mat4 projection;
} camera;

layout (binding = 2) uniform ShadowData {
	mat4 view;
	mat4 projection;
} shadow_data;

layout (push_constant) uniform PushConstant {
	vec2 pos;
} pc;

//layout (location = 0) out vec2 pass_tex;
layout (location = 0) out vec3 pass_normal;
layout (location = 1) out vec3 pass_frag_pos;
layout (location = 2) out vec3 pass_color;
layout (location = 3) out vec4 shadow_space;
//

const mat4 bias_mat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0
);

void main() {
	gl_Position = camera.projection * camera.view * vec4(in_pos.x + pc.pos.x, in_pos.y, in_pos.z + pc.pos.y, 1);
	pass_frag_pos = in_pos; // Only because we do not transform local space into world space at the moment
	pass_normal = in_normal;
	pass_color = in_color;
	shadow_space = bias_mat * shadow_data.projection * shadow_data.view * vec4(in_pos.x + pc.pos.x, in_pos.y, in_pos.z + pc.pos.y, 1);
//	pass_tex = in_tex;
	
}
