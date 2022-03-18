#include "TitleScene.h"
#include "Input.h"
#include "SceneController.h"
#include <memory>

void TitleScene::FadeinUpdate()
{
	if (--m_wait == 0)
	{
		m_updater = &TitleScene::NormalUpdate;
	}
}

void TitleScene::NormalUpdate()
{
	if (m_input->IsDown(m_input->BUTTON_ID_UP))
	{
		m_updater = &TitleScene::FadeoutUpdate;
		m_wait = 90;
	}
}

void TitleScene::FadeoutUpdate()
{
	if (--m_wait == 0)
	{
		// 仮でタイトルシーン
		m_controller->ChangeScene(std::make_unique<TitleScene>(*m_controller, *m_input));
	}
}

TitleScene::TitleScene(SceneController& controller, Input& input)
	: Scene(controller, input)
{
	m_wait = 60;
	m_updater = &TitleScene::FadeinUpdate;
}

TitleScene::~TitleScene()
{
}

void TitleScene::Update()
{
	(this->*m_updater)();
}

void TitleScene::Draw()
{
}
