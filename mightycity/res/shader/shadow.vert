#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_pos;

layout (binding = 0) uniform CameraData {
	mat4 view;
	mat4 projection;
} camera;

layout (push_constant) uniform PushConstant {
	//vec2 pos;
	mat4 model;
} pc;

//
void main() {
	gl_Position = camera.projection * camera.view * pc.model * vec4(in_pos.xyz, 1);
}
