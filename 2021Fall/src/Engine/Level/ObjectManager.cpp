#include "ObjectManager.hpp"

void ObjectManager::init()
{
}

void ObjectManager::update(float dt)
{
	for (auto& obj : objectList)
	{
		obj->update(dt);
	}
}

void ObjectManager::close()
{
	for (auto& obj : objectList)
	{
		obj->close();
		delete obj;
	}

	objectList.clear();
}