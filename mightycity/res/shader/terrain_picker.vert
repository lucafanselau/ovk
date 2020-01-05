#version 450 core
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
//
layout (binding = 0) uniform CameraData {
	mat4 view;
	mat4 projection;
} camera;

layout (push_constant) uniform PushConstant {
	vec2 pos;
} pc;

layout (location = 0) out vec3 encoded_color;

// TODO: Should be uniform
// NOTE: Totally random limit in one direction
const int max_chunks_1D = 32;
const int chunk_size = 8;
const int num_of_triangles = chunk_size * chunk_size * 2;

////layout (location = 0) out vec2 pass_tex;
//layout (location = 0) out vec3 pass_normal;
//layout (location = 1) out vec3 pass_frag_pos;
//layout (location = 2) out vec3 pass_color;
//
void main() {
	gl_Position = camera.projection * camera.view * vec4(in_pos.x + pc.pos.x, in_pos.y, in_pos.z + pc.pos.y, 1);

	// THIS IS BULLSHIT
	int triangle_index = int(floor(float(gl_VertexIndex)/ 3.f));
	// int tile_index = triangle_index / 2;

	int x = int(pc.pos.x) / chunk_size;
	int z = int(pc.pos.y) / chunk_size;
	
	// int z = tile_index % range;
	// int x = (tile_index - z) / range;

	float frange = float(max_chunks_1D);
	float fz = float(z) / frange;
	float fx = float(x) / frange;

	// float y = 0.0f;
	// if (triangle_index % 2 == 1) {
	// 	y = 1.0f;
	// }

	float y = float(triangle_index) / float(num_of_triangles);
	
	encoded_color = vec3(fx, y, fz);
	//	pass_frag_pos = in_pos; // Only because we do not transform local space into world space at the moment
	//		pass_normal = in_normal;
	//	pass_color = in_color;
	//  	pass_tex = in_tex;
	
}
