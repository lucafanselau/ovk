#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;

layout (binding = 0) uniform CameraData {
	mat4 view;
	mat4 projection;
} camera;

layout (location = 0) out vec3 pass_normal;

void main() {
	gl_Position = camera.projection * camera.view * vec4(in_pos, 1.0f);
	pass_normal = in_normal;
}
