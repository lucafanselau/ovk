#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 in_pos;
layout (location = 1) in vec2 in_tex;
layout (location = 2) in vec4 in_color;

layout (push_constant) uniform PushConstant {
	vec2 scale;
	vec2 translate;
} pc;

layout (location = 0) out vec4 pass_color;
layout (location = 1) out vec2 pass_tex;

void main() {
	pass_color = in_color;
	pass_tex = in_tex;
	gl_Position = vec4(in_pos * pc.scale + pc.translate, 0, 1);
}