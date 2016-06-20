/*
Copyright(c) 2016 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES ===========================
#include "GameObject.h"
#include "Scene.h"
#include "../Core/GUIDGenerator.h"
#include "../Pools/GameObjectPool.h"
#include "../IO/Serializer.h"
#include "../Components/Transform.h"
#include "../Components/Mesh.h"
#include "../Components/MeshRenderer.h"
#include "../Components/Camera.h"
#include "../Components/Skybox.h"
#include "../Components/RigidBody.h"
#include "../Components/Collider.h"
#include "../Components/MeshCollider.h"
#include "../Components/Hinge.h"
#include "../Components/Script.h"
#include "../Components/LineRenderer.h"
//=====================================

using namespace std;

GameObject::GameObject()
{
	m_ID = GENERATE_GUID;
	m_name = "GameObject";
	m_isActive = true;
	m_hierarchyVisibility = true;

	GameObjectPool::GetInstance().AddGameObjectToPool(this);

	// add transform component
	m_transform = AddComponent<Transform>();
}

GameObject::~GameObject()
{
	// delete components
	map<string, IComponent*>::iterator it;
	for (it = m_components.begin(); it != m_components.end(); ++it)
	{
		// some components might use these pointers if they are not null
		// however when the gameobject is being destroyed they will become
		// dangling pointers, it's important to set them to null to avoid crashes.
		IComponent* component = it->second;
		component->g_gameObject = nullptr;
		component->g_transform = nullptr;

		delete component;
	}
	m_components.clear();

	m_ID = -1;
	m_name.clear();
	m_isActive = false;
	m_hierarchyVisibility = false;
}

void GameObject::Initialize(D3D11Device* d3d11Device, Scene* scene, MeshPool* meshPool, MaterialPool* materialPool, TexturePool* texturePool, ShaderPool* shaderPool, PhysicsEngine* physics, ScriptEngine* scriptEngine)
{
	m_D3D11Device = d3d11Device;
	m_scene = scene;
	m_meshPool = meshPool;
	m_materialPool = materialPool;
	m_texturePool = texturePool;
	m_shaderPool = shaderPool;
	m_physics = physics;
	m_scriptEngine = scriptEngine;
}

void GameObject::Update()
{
	if (!m_isActive)
		return;

	// update components
	map<string, IComponent*>::iterator it;
	for (it = m_components.begin(); it != m_components.end(); ++it)
		it->second->Update();
}

string GameObject::GetName()
{
	return m_name;
}

void GameObject::SetName(string name)
{
	m_name = name;
}

string GameObject::GetID()
{
	return m_ID;
}

void GameObject::SetID(string ID)
{
	m_ID = ID;
}

void GameObject::SetActive(bool active)
{
	m_isActive = active;
}

bool GameObject::IsActive()
{
	return m_isActive;
}

void GameObject::SetHierarchyVisibility(bool value)
{
	m_hierarchyVisibility = value;
}

bool GameObject::IsVisibleInHierarchy()
{
	return m_hierarchyVisibility;
}

void GameObject::Save()
{
	Serializer::SaveSTR(m_ID);
	Serializer::SaveSTR(m_name);
	Serializer::SaveBool(m_isActive);
	Serializer::SaveBool(m_hierarchyVisibility);

	Serializer::SaveInt(m_components.size());
	for (map<string, IComponent*>::iterator it = m_components.begin(); it != m_components.end(); ++it)
	{
		Serializer::SaveSTR(it->first); // save component's type
		it->second->Save(); // save the component
	}
}

void GameObject::Load()
{
	m_ID = Serializer::LoadSTR();
	m_name = Serializer::LoadSTR();
	m_isActive = Serializer::LoadBool();
	m_hierarchyVisibility = Serializer::LoadBool();

	int componentCount = Serializer::LoadInt();
	for (int i = 0; i < componentCount; i++)
	{
		string typeStr = Serializer::LoadSTR(); // load component's type
		LoadCompFromTypeStr(typeStr);
	}
}

template <class Type>
Type* GameObject::AddComponent()
{
	// if a component of that type already exists
	// return it and don't create a duplicate
	if (HasComponent<Type>())
		return GetComponent<Type>();

	// get component type as string
	string typeStr(typeid(Type).name());
	typeStr = typeStr.substr(typeStr.find_first_of(" \t") + 1); // remove word "class"

	// create and initialize the component
	m_components.insert(pair<string, IComponent*>(typeStr, new Type));
	m_components[typeStr]->g_gameObject = this;
	m_components[typeStr]->g_transform = GetTransform();
	m_components[typeStr]->g_d3d11Device = m_D3D11Device;
	m_components[typeStr]->g_meshPool = m_meshPool;
	m_components[typeStr]->g_scene = m_scene;
	m_components[typeStr]->g_materialPool = m_materialPool;
	m_components[typeStr]->g_texturePool = m_texturePool;
	m_components[typeStr]->g_shaderPool = m_shaderPool;
	m_components[typeStr]->g_physics = m_physics;
	m_components[typeStr]->g_scriptEngine = m_scriptEngine;
	m_components[typeStr]->Initialize();

	m_scene->MakeDirty();

	return GetComponent<Type>();
}

template <class Type>
Type* GameObject::GetComponent()
{
	for (map<string, IComponent*>::iterator it = m_components.begin(); it != m_components.end(); ++it)
	{
		IComponent* component = it->second;

		// casting failure == nullptr
		Type* typed_cmp = dynamic_cast<Type*>(component);

		if (typed_cmp != nullptr)
			return typed_cmp;
	}

	return nullptr;
}

template <class Type>
bool GameObject::HasComponent()
{
	if (GetComponent<Type>() == nullptr)
		return false;

	return true;
}

template <class Type>
void GameObject::RemoveComponent()
{
	for (map<string, IComponent*>::iterator it = m_components.begin(); it != m_components.end();)
	{
		IComponent* component = it->second;

		// casting failure == nullptr
		Type* typed_cmp = dynamic_cast<Type*>(component);

		if (typed_cmp != nullptr)
		{
			delete component;
			it = m_components.erase(it);

			m_scene->MakeDirty();

			return;
		}
		++it;
	}
}

Transform* GameObject::GetTransform()
{
	return m_transform;
}

/*------------------------------------------------------------------------------
							[HELPER]
------------------------------------------------------------------------------*/
void GameObject::LoadCompFromTypeStr(string typeStr)
{
	if (typeStr == "Transform")
		AddComponent<Transform>()->Load();

	if (typeStr == "Mesh")
		AddComponent<Mesh>()->Load();

	if (typeStr == "MeshRenderer")
		AddComponent<MeshRenderer>()->Load();

	if (typeStr == "Light")
		AddComponent<Light>()->Load();

	if (typeStr == "Camera")
		AddComponent<Camera>()->Load();

	if (typeStr == "Skybox")
		AddComponent<Skybox>()->Load();

	if (typeStr == "RigidBody")
		AddComponent<RigidBody>()->Load();

	if (typeStr == "Collider")
		AddComponent<Collider>()->Load();

	if (typeStr == "MeshCollider")
		AddComponent<MeshCollider>()->Load();

	if (typeStr == "Hinge")
		AddComponent<Hinge>()->Load();

	if (typeStr == "Script")
		AddComponent<Script>()->Load();

	if (typeStr == "LineRenderer")
		AddComponent<LineRenderer>()->Load();
}

/*------------------------------------------------------------------------------
			[Explicit template declerations and exporting]
------------------------------------------------------------------------------*/
template __declspec(dllexport) Transform* GameObject::AddComponent<Transform>();
template __declspec(dllexport) Mesh* GameObject::AddComponent<Mesh>();
template __declspec(dllexport) MeshRenderer* GameObject::AddComponent<MeshRenderer>();
template __declspec(dllexport) Light* GameObject::AddComponent<Light>();
template __declspec(dllexport) Camera* GameObject::AddComponent<Camera>();
template __declspec(dllexport) Skybox* GameObject::AddComponent<Skybox>();
template __declspec(dllexport) RigidBody* GameObject::AddComponent<RigidBody>();
template __declspec(dllexport) Collider* GameObject::AddComponent<Collider>();
template __declspec(dllexport) MeshCollider* GameObject::AddComponent<MeshCollider>();
template __declspec(dllexport) Hinge* GameObject::AddComponent<Hinge>();
template __declspec(dllexport) Script* GameObject::AddComponent<Script>();
template __declspec(dllexport) LineRenderer* GameObject::AddComponent<LineRenderer>();

template __declspec(dllexport) Transform* GameObject::GetComponent();
template __declspec(dllexport) Mesh* GameObject::GetComponent();
template __declspec(dllexport) MeshRenderer* GameObject::GetComponent();
template __declspec(dllexport) Light* GameObject::GetComponent();
template __declspec(dllexport) Camera* GameObject::GetComponent();
template __declspec(dllexport) Skybox* GameObject::GetComponent();
template __declspec(dllexport) RigidBody* GameObject::GetComponent();
template __declspec(dllexport) Collider* GameObject::GetComponent();
template __declspec(dllexport) MeshCollider* GameObject::GetComponent();
template __declspec(dllexport) Hinge* GameObject::GetComponent();
template __declspec(dllexport) Script* GameObject::GetComponent();
template __declspec(dllexport) LineRenderer* GameObject::GetComponent();

template __declspec(dllexport) bool GameObject::HasComponent<Transform>();
template __declspec(dllexport) bool GameObject::HasComponent<Mesh>();
template __declspec(dllexport) bool GameObject::HasComponent<MeshRenderer>();
template __declspec(dllexport) bool GameObject::HasComponent<Light>();
template __declspec(dllexport) bool GameObject::HasComponent<Camera>();
template __declspec(dllexport) bool GameObject::HasComponent<Skybox>();
template __declspec(dllexport) bool GameObject::HasComponent<RigidBody>();
template __declspec(dllexport) bool GameObject::HasComponent<Collider>();
template __declspec(dllexport) bool GameObject::HasComponent<MeshCollider>();
template __declspec(dllexport) bool GameObject::HasComponent<Hinge>();
template __declspec(dllexport) bool GameObject::HasComponent<Script>();
template __declspec(dllexport) bool GameObject::HasComponent<LineRenderer>();

template __declspec(dllexport) void GameObject::RemoveComponent<Transform>();
template __declspec(dllexport) void GameObject::RemoveComponent<Mesh>();
template __declspec(dllexport) void GameObject::RemoveComponent<MeshRenderer>();
template __declspec(dllexport) void GameObject::RemoveComponent<Light>();
template __declspec(dllexport) void GameObject::RemoveComponent<Camera>();
template __declspec(dllexport) void GameObject::RemoveComponent<Skybox>();
template __declspec(dllexport) void GameObject::RemoveComponent<RigidBody>();
template __declspec(dllexport) void GameObject::RemoveComponent<Collider>();
template __declspec(dllexport) void GameObject::RemoveComponent<MeshCollider>();
template __declspec(dllexport) void GameObject::RemoveComponent<Hinge>();
template __declspec(dllexport) void GameObject::RemoveComponent<Script>();
template __declspec(dllexport) void GameObject::RemoveComponent<LineRenderer>();
