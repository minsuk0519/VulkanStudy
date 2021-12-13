#pragma once

class ObjectManager;
class Camera;

class Level
{
public:
	friend class LevelManager;

	void init();
	void update(float dt);
	void close();
private:
	ObjectManager* objManager = nullptr;

	Camera* camera = nullptr;
};