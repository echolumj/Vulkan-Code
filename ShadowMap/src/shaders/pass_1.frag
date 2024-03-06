#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D depthSampler;
layout(binding = 2) uniform sampler2D texSampler;

layout(location = 0) in vec4 outPos;
layout(location = 1) in vec2 outCoord;

layout(location = 0) out vec4 fragColor;

void main() {

    //current depth
    float curDepth = outPos.z / outPos.w;

    vec2 uv = (outPos.xy / outPos.w) * 0.5 + vec2(0.5);
    float minDepth = texture(depthSampler, uv).r;

    float notInShadow = 1.0;
    float bias = 0.001;
    if(curDepth > minDepth + bias)
        notInShadow = 0.1f;

    vec2 coord = outCoord;
    coord.y = 1.0 - coord.y;
    fragColor = texture(texSampler, coord).rgba * notInShadow;
    //fragColor = vec4(minDepth, curDepth, 0.0, 1.0);
    fragColor.a = 1.0f;
}