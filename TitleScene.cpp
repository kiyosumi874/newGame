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
	if (m_input->IsDown(m_input->BUTTON_ID_1))
	{
		m_updater = &TitleScene::FadeoutUpdate;
		m_wait = 90;
	}
	
}

void TitleScene::FadeoutUpdate()
{
	if (--m_wait == 0)
	{
		// ���Ń^�C�g���V�[��
		m_controller->ChangeScene(std::make_unique<TitleScene>(*m_controller, *m_input, *m_dx12));
	}
}

TitleScene::TitleScene(SceneController& controller, Input& input, Dx12Wrapper& dx12)
	: Scene(controller, input, dx12)
{
	m_wait = 60;
	m_updater = &TitleScene::FadeinUpdate;

	m_print.reset(new Print(*m_dx12));
	m_pmdRenderer.reset(new PMDRenderer(*m_dx12));
	m_pmdActor.reset(new PMDActor("model/�����~�N.pmd", *m_pmdRenderer, *m_dx12));
	m_pmdActor->LoadVMDFile("motion/motion.vmd");
	m_pmdActor->PlayAnimation();
}

TitleScene::~TitleScene()
{
}

void TitleScene::Update()
{
	m_input->Update();
	(this->*m_updater)();
	if (m_input->IsDown(m_input->BUTTON_ID_2))
	{
		m_controller->PopScene();
	}
}

void TitleScene::Draw()
{
	//�S�̂̕`�揀��
	m_dx12->BeginDraw();
	// PMD�p�̕`��p�C�v���C���ɍ��킹��
	m_dx12->CommandList()->SetPipelineState(m_pmdRenderer->GetPipelineState());
	// ���[�g�V�O�l�`����PMD�p�ɍ��킹��
	m_dx12->CommandList()->SetGraphicsRootSignature(m_pmdRenderer->GetRootSignature());

	m_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_dx12->SetScene();

	m_pmdActor->Update();
	m_pmdActor->Draw();

	m_print->Draw();

	m_dx12->EndDraw();
	//�t���b�v
	m_dx12->Swapchain()->Present(1, 0);
	GraphicsMemory::Get().Commit(m_dx12->CmdQue());
}
