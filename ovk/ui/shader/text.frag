#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 pass_tex_coord;

layout (location = 0) out vec4 color;

layout (binding = 1) uniform sampler2D texture_sampler;

void main() {
	color = texture(texture_sampler, pass_tex_coord);
}
