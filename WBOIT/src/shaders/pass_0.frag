#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float number;

void main() {
	float alpha = 0.5;
	outColor = vec4(fragColor * alpha, alpha);
    number = 1.0 - alpha;
}