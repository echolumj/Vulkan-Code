//#version 450
//#extension GL_ARB_separate_shader_objects : enable

//layout(location = 0) in vec3 fragColor;

//layout(location = 0) out vec4 outColor;

//void main() {
//    outColor = vec4(fragColor, 1.0);
//}

#version 450
 
layout (location = 0) in VertexInput {
  vec4 color;
} vertexInput;

layout(location = 0) out vec4 outFragColor;
 

void main()
{
	outFragColor = vertexInput.color;
}