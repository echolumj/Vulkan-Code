#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, binding = 0) uniform subpassInput blendImage;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput number;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 nearColor = subpassLoad(blendImage).rgb / subpassLoad(blendImage).a;
	float alpha = subpassLoad(number).r;
	nearColor = nearColor * (1.0f - alpha);
	outColor = vec4(nearColor, 1.0);
}