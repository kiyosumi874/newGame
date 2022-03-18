#pragma once
#include "Scene.h"
#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Print.h"
#include <memory>

class Input;
class Dx12Wrapper;
class TitleScene final : public Scene
{
private:
	int m_wait;
	std::unique_ptr<PMDRenderer> m_pmdRenderer = nullptr;
	std::unique_ptr<PMDActor> m_pmdActor = nullptr;
	std::unique_ptr<Print> m_print = nullptr;

	void (TitleScene::* m_updater)();
	void FadeinUpdate();
	void NormalUpdate();
	void FadeoutUpdate();

public:
	TitleScene(SceneController& controller, Input& input, Dx12Wrapper& dx12);
	~TitleScene();
	void Update() override;
	void Draw() override;
};