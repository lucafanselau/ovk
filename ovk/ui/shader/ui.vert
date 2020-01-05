#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_tex;

layout (binding = 0) uniform Projection {
	mat4 proj;
} p;

layout (push_constant) uniform PushConstant {
	mat4 model;
} pc;

layout (location = 0) out vec2 pass_tex;
layout (location = 1) out vec4 pass_ndc;

void main() {
	gl_Position = p.proj * pc.model * vec4(in_pos, 0.0f, 1.0f);
	pass_ndc = pc.model * vec4(in_pos, 0.0f, 1.0f);
	pass_tex = in_tex;
}
