#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 in_pos;


void main() {
	gl_Position = vec4(in_pos, 0.0f, 1.0f);
}
