#include "SceneController.h"
#include "TitleScene.h"
#include "Input.h"
using namespace std;

SceneController::SceneController(Input& input, Dx12Wrapper& dx12)
{
	m_scene.emplace_front(make_unique<TitleScene>(*this, input, dx12));
}

SceneController::~SceneController()
{
	m_scene.clear();
}

bool SceneController::SceneUpdate()
{
	if (m_scene.empty())
	{
		return false;
	}
	m_scene.front()->Update();
	auto rit = m_scene.rbegin();
	for (; rit != m_scene.rend(); ++rit)
	{
		(*rit)->Draw();
	}
	return true;
}

void SceneController::ChangeScene(std::unique_ptr<Scene> scene)
{
	m_scene.pop_front();
	m_scene.emplace_front(move(scene));
}

bool SceneController::IsMultipleScene()
{
	return m_scene.size() >= 2;
}

void SceneController::PushScene(std::unique_ptr<Scene> scene)
{
	m_scene.emplace_front(move(scene));
}

void SceneController::PopScene()
{
	m_scene.erase(m_scene.begin());
}
