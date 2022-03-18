#pragma once
#include <memory>
class Input;
class SceneController;
class Dx12Wrapper;
///シーン管理のための基底クラス
///(純粋仮想クラス)
class Scene
{
protected:
	SceneController* m_controller;
	Input* m_input;
	Dx12Wrapper* m_dx12;
public:
	Scene(SceneController& controller, Input& input, Dx12Wrapper& dx12) : m_controller(&controller) , m_input(&input) , m_dx12(&dx12) {}
	virtual ~Scene() {}

	///シーンの更新を行う
	virtual void Update() = 0;

	///描画しもぁ～す
	virtual void Draw() = 0;
};