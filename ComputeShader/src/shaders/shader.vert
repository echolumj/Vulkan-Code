#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 3) uniform UniformBufferObject
{
	mat4 modelMat;
	mat4 viewMat;
	mat4 projMat;
}ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
	gl_Position = ubo.projMat * ubo.viewMat * ubo.modelMat * vec4(inPosition, 0.0, 1.0);
	fragColor = inColor;
}