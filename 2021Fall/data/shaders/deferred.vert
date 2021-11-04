#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragPosition;

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	fragTexCoord = inTexCoord;
	fragPosition = inPosition;
}