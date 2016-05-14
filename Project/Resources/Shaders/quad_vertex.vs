#version 330

uniform mat4 viewProjMatrix;
uniform mat4 modelMatrix;

layout (location=0) in vec4 vertexPos;
layout (location=1) in vec4 colorRGB;

out vec4 colour;

void main(void) {
	colour = colorRGB;

	gl_Position = viewProjMatrix * modelMatrix * vertexPos;
}
