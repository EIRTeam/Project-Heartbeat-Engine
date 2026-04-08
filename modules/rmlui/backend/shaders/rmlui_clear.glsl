#[vertex]

#version 450

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec4 inColor0;
layout (location = 2) in vec2 inTexCoord0;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
}

#[fragment]

#version 450

layout(location = 0) out vec4 finalColor;

void main() {
	finalColor = vec4(0.0, 0.0, 0.0, 0.0);
}