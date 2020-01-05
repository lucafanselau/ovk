#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_tex;

layout (location = 0) out vec2 pass_tex_coord;

layout (binding = 0) uniform Projection {
	mat4 proj;
} p;

void main() {
	gl_Position = p.proj * vec4(in_pos, 1.0f);
	pass_tex_coord = in_tex;
}
