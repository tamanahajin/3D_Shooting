/*!
@file SceneState.cpp
@brief タイトル開始、ゲーム開始、ステージエディタ切替、オプション開閉など状態遷移系
*/
#include "stdafx.h"
#include "Project.h"
#include "Scripts/Scene/SceneConstants.h"

namespace shooting {

	using namespace scene_detail;

	void Scene::StartTitle()
	{
		if (m_stageEditor.IsActive())
		{
			auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
			if (gameStage)
			{
				m_stageEditor.Exit(*gameStage);
			}
		}
		m_stageEditorReloadRequested = false;
		SetFogEnabled(true);

		auto& benchmark = BenchmarkRecorder::Instance();
		if (benchmark.IsRunning())
		{
			benchmark.Stop(true);
		}
		benchmark.ClearNotification();

		m_optionOpen = false;
		m_waitingForOptionMouseRelease = false;
		m_optionDraggingSlider = kOptionSliderNone;
		m_gameState = GameState::Title;
		m_titleMenuIndex = 0;
		m_titleTime = 0.0;

		SetMouseCursorVisible(true);

		ResetActiveStage<TitleStage>(App::GetD3D12Device());
	}

	void Scene::StartGame()
	{
		m_stageEditorReloadRequested = false;
		SetFogEnabled(true);

		// ベンチマークはインゲーム中だけ扱う。前ステートの通知は新しいプレイへ持ち越さない。
		BenchmarkRecorder::Instance().ClearNotification();

		m_optionOpen = false;
		m_waitingForOptionMouseRelease = false;
		m_optionDraggingSlider = kOptionSliderNone;
		m_lastSurvivalTime = 0.0;
		m_lastDefeatedEnemyCount = 0;
		m_lastReachedWave = 0;
		m_lastTotalDamageDealt = 0;
		m_lastBestExplosionKills = 0;
		m_gameState = GameState::Playing;
		if (!m_hasShownInitialControlGuide)
		{
			m_hasShownInitialControlGuide = true;
			m_initialControlGuideSecondsRemaining = kInitialControlGuideDurationSeconds;
		}
		else
		{
			m_initialControlGuideSecondsRemaining = 0.0;
		}

		SetMouseCursorVisible(false);
		// インゲームBGMはGameStage側でプレイヤー登場演出が終わった後に開始する。
		GameAudio::Instance().StopBgm();

		ResetActiveStage<GameStage>(App::GetD3D12Device());
	}

	void Scene::EnterStageEditor()
	{
#if defined(_DEBUG)
		if (m_gameState != GameState::Playing ||
			m_optionOpen ||
			m_screenTransition.IsInputBlocked())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (!gameStage || !m_stageEditor.Enter(*gameStage))
		{
			return;
		}

		auto& benchmark = BenchmarkRecorder::Instance();
		if (benchmark.IsRunning())
		{
			// エディタ中はゲーム更新を止めるため、計測中なら編集開始前までで確定する。
			benchmark.Stop(true);
		}

		m_stageEditorReloadRequested = false;
		SetFogEnabled(false);
		SetMouseCursorVisible(true);
#endif
	}

	void Scene::ExitStageEditor()
	{
#if defined(_DEBUG)
		if (!m_stageEditor.IsActive())
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (gameStage)
		{
			m_stageEditor.Exit(*gameStage);
		}

		const auto& input = App::GetInputDevice();
		// エディタを閉じたクリックが射撃や爆弾入力へ流れないよう、ボタンの解放を待つ。
		m_waitingForOptionMouseRelease =
			input.MouseDown(VK_LBUTTON) ||
			input.MouseDown(VK_RBUTTON) ||
			input.MouseDown(VK_MBUTTON);
		m_stageEditorReloadRequested = false;
		SetFogEnabled(true);
		SetMouseCursorVisible(false);
#endif
	}

	void Scene::ReloadStageForEditor()
	{
#if defined(_DEBUG)
		if (!m_stageEditor.IsActive())
		{
			m_stageEditorReloadRequested = false;
			return;
		}

		// 描画中にステージを破棄しないよう、ImGuiからの要求を次のUpdateで処理する。
		auto gameStage = ResetActiveStage<GameStage>(App::GetD3D12Device());
		m_stageEditor.OnStageReloaded(*gameStage);
		m_stageEditorReloadRequested = false;
#endif
	}

	void Scene::RequestStartGame()
	{
		if (m_gameState != GameState::Title || m_screenTransition.IsInputBlocked())
		{
			return;
		}

		m_screenTransition.Start(
			kSceneTransitionFadeOutSeconds,
			kSceneTransitionFadeInSeconds,
			[this]()
			{
				StartGame();
			});
	}

	void Scene::RequestStartTitle()
	{
		if (m_screenTransition.IsInputBlocked())
		{
			return;
		}

		m_screenTransition.Start(
			kSceneTransitionFadeOutSeconds,
			kSceneTransitionFadeInSeconds,
			[this]()
			{
				StartTitle();
			});
	}

	void Scene::RequestExitGame()
	{
		if (m_screenTransition.IsInputBlocked())
		{
			return;
		}

		// EXIT直後に終了すると決定音が聞こえる前にアプリが閉じるため、フェードアウト後に終了する。
		m_screenTransition.Start(
			kSceneTransitionFadeOutSeconds,
			kSceneTransitionFadeInSeconds,
			[]()
			{
				::PostQuitMessage(0);
			});
	}

	void Scene::ConfirmTitleMenuSelection()
	{
		GameAudio::Instance().PlaySound(GameSoundId::Decide);

		if (m_titleMenuIndex == 0)
		{
			RequestStartGame();
		}
		else
		{
			RequestExitGame();
		}
	}

	void Scene::SetTitleMenuIndex(int index, bool playCursorMoveSound)
	{
		index = bsmUtil::Clamp(index, 0, 1);
		if (m_titleMenuIndex == index)
		{
			return;
		}

		m_titleMenuIndex = index;
		if (playCursorMoveSound)
		{
			GameAudio::Instance().PlaySound(GameSoundId::CursorMove);
		}
	}

	void Scene::OpenOptionMenu()
	{
		if (m_optionOpen || m_screenTransition.IsInputBlocked())
		{
			return;
		}

		m_optionOpen = true;
		m_optionDraggingSlider = kOptionSliderNone;
		GameAudio::Instance().PlaySound(GameSoundId::Decide);

		if (m_gameState == GameState::Playing)
		{
			SetMouseCursorVisible(true);
		}
	}

	void Scene::CloseOptionMenu()
	{
		if (!m_optionOpen)
		{
			return;
		}

		m_optionOpen = false;
		m_optionDraggingSlider = kOptionSliderNone;
		GameAudio::Instance().PlaySound(GameSoundId::Cancel);

		if (m_gameState == GameState::Playing)
		{
			const auto& input = App::GetInputDevice();
			// UIを閉じたクリックが射撃入力へ流れないよう、押されているボタンの解放を待つ。
			m_waitingForOptionMouseRelease =
				input.MouseDown(VK_LBUTTON) ||
				input.MouseDown(VK_RBUTTON) ||
				input.MouseDown(VK_MBUTTON);
			SetMouseCursorVisible(false);
		}
	}

	void Scene::UpdateOptionInput()
	{
		if (!m_optionOpen || m_screenTransition.IsInputBlocked())
		{
			return;
		}

		if (App::GetInputDevice().KeyPressed(VK_ESCAPE))
		{
			CloseOptionMenu();
		}
	}

}
