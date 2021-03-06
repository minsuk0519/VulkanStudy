#include "Level.hpp"
#include "ObjectManager.hpp"
#include "Engine/Entity/Camera.hpp"
#include "Engine/Entity/Light.hpp"
#include "Engine/Input/Input.hpp"

void Level::init()
{
	objManager = new ObjectManager(this);

    camera = objManager->addObjectByTemplate<Camera>();
    camera->GetTransform().SetPosition(glm::vec3(0.0f, 3.0f, -5.0f));

    PointLight* newlight = objManager->addObjectByTemplate<PointLight>();
    newlight->setLightIndex(0);
    newlight->GetTransform().SetPosition(glm::vec3(0.0f, 4.0f, 0.0f));
    newlight->GetTransform().SetScale(glm::vec3(0.2f));
    newlight->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_LIGHT_OBJ, PROGRAM_ID::PROGRAM_ID_DIFFUSE, DRAWTARGET_INDEX::DRAWTARGET_CUBE, UniformBufferIndex::UNIFORM_LIGHT_OBJECT_MATRIX);
    newlight = objManager->addObjectByTemplate<PointLight>();
    newlight->setLightIndex(1);
    newlight->GetTransform().SetPosition(glm::vec3(0.0f, 10.0f, 5.0f));
    newlight->GetTransform().SetScale(glm::vec3(0.2f));
    newlight->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_LIGHT_OBJ, PROGRAM_ID::PROGRAM_ID_DIFFUSE, DRAWTARGET_INDEX::DRAWTARGET_CUBE, UniformBufferIndex::UNIFORM_LIGHT_OBJECT_MATRIX);
    newlight = objManager->addObjectByTemplate<PointLight>();
    newlight->setLightIndex(2);
    newlight->GetTransform().SetPosition(glm::vec3(-5.0f, 5.0f, 0.0f));
    newlight->GetTransform().SetScale(glm::vec3(0.2f));
    newlight->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_LIGHT_OBJ, PROGRAM_ID::PROGRAM_ID_DIFFUSE, DRAWTARGET_INDEX::DRAWTARGET_CUBE, UniformBufferIndex::UNIFORM_LIGHT_OBJECT_MATRIX);
    newlight = objManager->addObjectByTemplate<PointLight>();
    newlight->setLightIndex(3, true);
    newlight->GetTransform().SetPosition(glm::vec3(0.0f, 5.0f, -5.0f));
    newlight->GetTransform().SetScale(glm::vec3(0.2f));
    newlight->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_LIGHT_OBJ, PROGRAM_ID::PROGRAM_ID_DIFFUSE, DRAWTARGET_INDEX::DRAWTARGET_CUBE, UniformBufferIndex::UNIFORM_LIGHT_OBJECT_MATRIX);

    Object* newobj;
    /*newobj= objManager->addObject();
    newobj->GetTransform().SetScale(glm::vec3(1.0f));
    newobj->GetTransform().SetPosition(glm::vec3(-7.5f, 0.0f, 0.0f));
    newobj->SetUniform(ObjectUniform{ glm::mat4(1.0f), glm::vec3(0.955008f, 0.637427f, 0.538163f), 1.0f, 1.0f });
    newobj->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_OBJ, PROGRAM_ID::PROGRAM_ID_BASERENDER, DRAWTARGET_INDEX::DRAWTARGET_MODEL_INSTANCE);

    newobj = objManager->addObject();
    newobj->GetTransform().SetScale(glm::vec3(1.0f));
    newobj->GetTransform().SetPosition(glm::vec3(7.5f, 0.0f, 0.0f));
    newobj->SetUniform(ObjectUniform{ glm::mat4(1.0f), glm::vec3(1.0f, 0.765557f, 0.336057f), 1.0f, 1.0f });
    newobj->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_OBJ, PROGRAM_ID::PROGRAM_ID_BASERENDER, DRAWTARGET_INDEX::DRAWTARGET_MODEL_INSTANCE);*/

    newobj = objManager->addObject();
    newobj->GetTransform().SetScale(glm::vec3(30.0f, 0.1f, 30.0f));
    newobj->GetTransform().SetPosition(glm::vec3(0.0f, -2.0f, 15.0f));
    newobj->SetUniform(ObjectUniform{ glm::mat4(1.0f), glm::vec3(0.659777f, 0.608679f, 0.525649f), 1.0f, 1.0f });
    newobj->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_OBJ, PROGRAM_ID::PROGRAM_ID_BASERENDER, DRAWTARGET_INDEX::DRAWTARGET_CUBE, UniformBufferIndex::UNIFORM_OBJECT_MATRIX);

    newobj = objManager->addObject();
    newobj->GetTransform().SetScale(glm::vec3(0.1f, 15.0f, 30.0f));
    newobj->GetTransform().SetPosition(glm::vec3(-30.0f, 13.0f, 15.0f));
    newobj->SetUniform(ObjectUniform{ glm::mat4(1.0f), glm::vec3(0.659777f, 0.608679f, 0.525649f), 1.0f, 1.0f });
    newobj->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_OBJ, PROGRAM_ID::PROGRAM_ID_BASERENDER, DRAWTARGET_INDEX::DRAWTARGET_CUBE, UniformBufferIndex::UNIFORM_OBJECT_MATRIX);

    newobj = objManager->addObject();
    newobj->GetTransform().SetScale(glm::vec3(30.0f, 15.0f, 0.1f));
    newobj->GetTransform().SetPosition(glm::vec3(0.0f, 13.0f, 45.0f));
    newobj->SetUniform(ObjectUniform{ glm::mat4(1.0f), glm::vec3(0.659777f, 0.608679f, 0.525649f), 1.0f, 1.0f });
    newobj->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_OBJ, PROGRAM_ID::PROGRAM_ID_BASERENDER, DRAWTARGET_INDEX::DRAWTARGET_CUBE, UniformBufferIndex::UNIFORM_OBJECT_MATRIX);

    newobj = objManager->addObject();
    newobj->GetTransform().SetScale(glm::vec3(2.0f, 2.0f, 2.0f));
    newobj->GetTransform().SetPosition(glm::vec3(-20.0f, 0.0f, 20.0f));
    newobj->SetUniform(ObjectUniform{ glm::mat4(1.0f), glm::vec3(0.662124f, 0.654864f, 0.633732f), 1.0f, 1.0f });
    newobj->SetDrawBehavior(DESCRIPTORSET_INDEX::DESCRIPTORSET_ID_OBJ, PROGRAM_ID::PROGRAM_ID_BASERENDER, DRAWTARGET_INDEX::DRAWTARGET_MODEL, UniformBufferIndex::UNIFORM_OBJECT_MATRIX);
}

void Level::postinit()
{
    objManager->postinit();
}

void Level::update(float dt)
{
    float cam_speed = 5.0f;

    if (Input::isPressed(KeyBinding::KEY_SHIFT)) cam_speed = 30.0f;
    if (Input::isPressed(KeyBinding::KEY_UP)) camera->Move(cam_speed * dt, 0.0f);
    if (Input::isPressed(KeyBinding::KEY_DOWN)) camera->Move(-cam_speed * dt, 0.0f);
    if (Input::isPressed(KeyBinding::KEY_RIGHT)) camera->Move(0.0f, cam_speed * dt);
    if (Input::isPressed(KeyBinding::KEY_LEFT)) camera->Move(0.0f, -cam_speed * dt);
    if (Input::isPressed(KeyBinding::MOUSE_RIGHT)) camera->LookAround(Input::GetMouseMove().x * dt, Input::GetMouseMove().y * dt);

	objManager->update(dt);
}

void Level::close()
{
	objManager->close();
	delete objManager;
}

ObjectManager* Level::GetObjectManager() const
{
    return objManager;
}
