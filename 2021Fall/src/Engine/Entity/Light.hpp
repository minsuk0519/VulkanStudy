#pragma once
#include "Engine/Entity/Object.hpp"

struct LightData
{
	glm::vec3 ambient;
	float attenuationC1;
	glm::vec3 diffuse;
	float attenuationC2;
	glm::vec3 specular;
	float attenuationC3;

	glm::vec3 position;
	float theta;
	glm::vec3 direction;
	float phi;
	
	float falloff;

	int type;
};

class Light : public Object
{
public:
	Light();

	friend class Graphic;

	virtual void init() override = 0;
	virtual void update(float dt) override = 0;
	virtual void close() override = 0;

	virtual void* GetLightDataPointer(glm::mat4 viewMat) = 0;
protected:

	LightData lightdata;
};

class PointLight : public Light
{
public:
	PointLight();

	virtual void init() override;
	virtual void update(float dt) override;
	virtual void close() override;

	virtual void* GetLightDataPointer(glm::mat4 viewMat) override;
private:

};