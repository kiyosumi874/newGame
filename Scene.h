#pragma once
#include <memory>
class Input;
class SceneController;
class Dx12Wrapper;
///�V�[���Ǘ��̂��߂̊��N���X
///(�������z�N���X)
class Scene
{
protected:
	SceneController* m_controller;
	Input* m_input;
	Dx12Wrapper* m_dx12;
public:
	Scene(SceneController& controller, Input& input, Dx12Wrapper& dx12) : m_controller(&controller) , m_input(&input) , m_dx12(&dx12) {}
	virtual ~Scene() {}

	///�V�[���̍X�V���s��
	virtual void Update() = 0;

	///�`�悵�����`��
	virtual void Draw() = 0;
};