#[vertex]

#version 450

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec4 inColor0;
layout (location = 2) in vec2 inTexCoord0;

layout(location = 0) out vec2 fragTexCoord;

void main() {
	fragTexCoord = inTexCoord0;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}

#[fragment]

#version 450

layout(set = 0, binding = 0) uniform sampler2D _tex;
layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 finalColor;

void main() {
	finalColor = texture(_tex, fragTexCoord);
}