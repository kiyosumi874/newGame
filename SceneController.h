#pragma once
#include<deque>
#include<memory>
class Scene;
class Input;
///シーン管理クラス
class SceneController
{
private:
	std::deque<std::unique_ptr<Scene>> m_scene;

public:
	SceneController(Input& input);
	~SceneController();

	void SceneUpdate();
	void ChangeScene(std::unique_ptr<Scene>);
	bool IsMultipleScene();
	void PushScene(std::unique_ptr<Scene>);
	void PopScene();
};