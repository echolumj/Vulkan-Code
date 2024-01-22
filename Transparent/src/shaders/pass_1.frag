#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(input_attachment_index = 0, binding = 0) uniform subpassInput blendImage;
layout(input_attachment_index = 1, binding = 1) uniform subpassInput number;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = subpassLoad(blendImage).rgba / subpassLoad(number).r;
	outColor.rgb = outColor.rgb * outColor.a;
}