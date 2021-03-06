#version 450

#include "settings.glsl"
#include "light.glsl"

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout (binding = 3) uniform sampler2D texPosition;
layout (binding = 4) uniform sampler2D texNormal;
layout (binding = 5) uniform sampler2D texAlbedo;

void main()
{
	if(setting.deferredType == 0)	
	{
		outColor = texture(texPosition, fragTexCoord);
		return;
	}
	else if(setting.deferredType == 1) 
	{
		outColor = texture(texNormal, fragTexCoord);
		return;
	}
	else if(setting.deferredType == 2)
	{
		outColor = texture(texAlbedo, fragTexCoord);
		return;
	}

	vec4 tex1 = texture(texPosition, fragTexCoord);
	vec4 tex2 = texture(texNormal, fragTexCoord);
	vec3 albedo = texture(texAlbedo, fragTexCoord).rgb;
	albedo = pow(albedo, vec3(2.2));
	vec3 pos = tex1.rgb;
	float metal = tex1.a;
	vec3 norm = tex2.rgb;
	float roughness = tex2.a;

	if(length(norm) == 0.0)
	{
		discard;
	}

	vec3 color;

	if(setting.computationType == 0)
	{
		color = ComputePBR(pos, norm, metal, roughness, albedo);
	}
	else
	{
		color = computeLight(pos, norm);
	}

	color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  

	outColor = vec4(color, 1.0);
}