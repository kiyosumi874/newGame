#pragma once
#include <memory>
class Input;
class SceneController;
///シーン管理のための基底クラス
///(純粋仮想クラス)
class Scene
{
protected:
	SceneController* m_controller;
	Input* m_input;
public:
	Scene(SceneController& controller, Input& input) : m_controller(&controller) , m_input(&input) {}
	virtual ~Scene() {}

	///シーンの更新を行う
	virtual void Update() = 0;

	///描画しもぁ～す
	virtual void Draw() = 0;
};