#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inCoord;
layout(location = 2) in vec3 inNormal;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightProjViewModel;
    vec3 lightPos;
} ubo;

layout(location = 0) out vec4 outPos;
layout(location = 1) out vec2 outCoord;

void main() {
    outPos = vec4(inPosition, 1.0);
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    outPos = ubo.lightProjViewModel * vec4(inPosition, 1.0);
    outCoord = inCoord;
}