#pragma once
#include "Scene.h"
#include <memory>

class Input;
class TitleScene final : public Scene
{
private:
	int m_wait;


	void (TitleScene::* m_updater)();
	void FadeinUpdate();
	void NormalUpdate();
	void FadeoutUpdate();

public:
	TitleScene(SceneController& controller, Input& input);
	~TitleScene();
	void Update() override;
	void Draw() override;
};