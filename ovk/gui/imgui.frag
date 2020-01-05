#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec4 pass_color;
layout (location = 1) in vec2 pass_tex;

layout (binding = 0) uniform sampler2D texture_sampler;

layout (location = 0) out vec4 color;

void main() {
	color = pass_color * texture(texture_sampler, pass_tex);
}