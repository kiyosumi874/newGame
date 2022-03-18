#include "SceneController.h"
#include "TitleScene.h"
#include "Input.h"
using namespace std;

SceneController::SceneController(Input& input)
{
	m_scene.emplace_front(make_unique<TitleScene>(*this, input));
}

SceneController::~SceneController()
{
	m_scene.clear();
}

void SceneController::SceneUpdate()
{
	if (m_scene.empty())
	{
		return;
	}
	m_scene.front()->Update();
	auto rit = m_scene.rbegin();
	for (; rit != m_scene.rend(); ++rit)
	{
		(*rit)->Draw();
	}
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
