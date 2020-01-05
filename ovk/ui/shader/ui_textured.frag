#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 pass_tex;
layout (location = 1) in vec4 pass_ndc;

layout (binding = 1) uniform sampler2D texture_sampler;

layout (push_constant) uniform FragmentPushConstant {
	layout (offset = 4 * 16) vec2 pos;
	layout (offset = 4 * 16 + 8) vec2 scale;
	layout (offset = 4 * 16 + 16) float radius;
} pc;

layout (location = 0) out vec4 color;

bool is_inside(vec2 a, vec2 b, vec2 value) {
	vec2 top_left = vec2(min(a.x, b.x), min(a.y, b.y));
	vec2 bottom_right = vec2(max(a.x, b.x), max(a.y, b.y));
 	return (value.x >= top_left.x && value.x <= bottom_right.x) && (value.y >= top_left.y && value.y <= bottom_right.y);
}

void main() {

	
	color = texture(texture_sampler, pass_tex);

	
	float r = pc.radius;
	if (r != 0.0f) {
		// Check if this fragment should be discarded
		vec2 fp = pass_ndc.xy;
		vec2 check_point = vec2(0.0f);
		vec2 bias = vec2(1.05f);

		vec2 scale[4] = vec2[]( -pc.scale,  vec2(pc.scale.x, -pc.scale.y), vec2(-pc.scale.x, pc.scale.y), pc.scale );
		vec2 r2[4] =    vec2[]( vec2(r, r), vec2(-r, r), vec2(r, -r), vec2(-r, -r) );

		for (int i = 0; i < 4; i++) {
			if (is_inside(pc.pos + bias * 0.5f * scale[i], pc.pos + 0.5f * scale[i] + r2[i], fp)) {
				check_point = pc.pos + 0.5f * scale[i] + r2[i];
				}
		}

		// Get distance to check point if there is one
		// color.gb = check_point;
		if (check_point != vec2(0.0f)) {
			float d = distance(check_point, fp);
			if (d > r) discard;
		}
	}
	// 

}
