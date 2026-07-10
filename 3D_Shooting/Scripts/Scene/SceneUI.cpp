/*!
@file SceneUI.cpp
@brief オプションUI、タイトルUI、HUDなど
*/
#include "stdafx.h"
#include "Project.h"
#include "Scripts/Scene/SceneConstants.h"

namespace shooting {

	using namespace scene_detail;

	float Scene::UpdateOptionSliderValue(int sliderIndex, const D2D1_RECT_F& rect, float currentValue)
	{
		const auto& input = App::GetInputDevice();
		const auto& mouse = input.GetMouseState();

		const float trackLeft = rect.left + kOptionSliderTrackOffset;
		const float trackRight = rect.right;
		const D2D1_RECT_F hitRect = D2D1::RectF(
			trackLeft - 18.0f,
			rect.top - 8.0f,
			trackRight + 18.0f,
			rect.bottom + 8.0f);

		if (input.MousePressed(VK_LBUTTON) && IsMouseInRect(hitRect))
		{
			m_optionDraggingSlider = sliderIndex;
		}

		float value = currentValue;
		if (m_optionDraggingSlider == sliderIndex && input.MouseDown(VK_LBUTTON))
		{
			const float width = trackRight - trackLeft;
			if (width > 0.0f)
			{
				value = bsmUtil::Clamp((static_cast<float>(mouse.now.x) - trackLeft) / width, 0.0f, 1.0f);
			}
		}

		if (m_optionDraggingSlider == sliderIndex && input.MouseReleased(VK_LBUTTON))
		{
			m_optionDraggingSlider = kOptionSliderNone;
		}

		return value;
	}

	void Scene::UpdateTitleInput()
	{
		if (m_optionOpen)
		{
			UpdateOptionInput();
			return;
		}

		if (m_screenTransition.IsInputBlocked())
		{
			return;
		}

		const auto& input = App::GetInputDevice();

		if (input.KeyPressed(VK_UP) || input.KeyPressed('W'))
		{
			SetTitleMenuIndex(0, true);
		}
		if (input.KeyPressed(VK_DOWN) || input.KeyPressed('S'))
		{
			SetTitleMenuIndex(1, true);
		}

		if (input.KeyPressed(VK_RETURN) || input.KeyPressed(VK_SPACE))
		{
			ConfirmTitleMenuSelection();
		}

		if (input.KeyPressed(VK_ESCAPE))
		{
			OpenOptionMenu();
		}
	}

	void Scene::DrawOptionButton()
	{
		UIButtonBehavior behavior;
		behavior.enabled = !m_screenTransition.IsInputBlocked();
		// 開閉時は決定音とキャンセル音を使い分けるため、ボタン共通の決定音は鳴らさない。
		behavior.playClickSound = false;

		const auto optionButton = m_uiManager.AddImageButton(
			App::GetRelativeAssetsDir() + kOptionIconPath,
			L"OptionButton",
			UIAnchor::TopRight,
			{ -kOptionIconMargin, kOptionIconMargin },
			{ kOptionIconSize, kOptionIconSize },
			0.82f,
			1.0f,
			behavior);

		if (optionButton.clicked)
		{
			if (m_optionOpen)
			{
				CloseOptionMenu();
			}
			else
			{
				OpenOptionMenu();
			}
		}
	}

	void Scene::DrawOptionMenu(UILayer& uiLayer)
	{
		const float screenW = uiLayer.GetWidth();
		const float screenH = uiLayer.GetHeight();
		const float sliderWidth = bsmUtil::Max(240.0f, bsmUtil::Min(420.0f, screenW - 80.0f));
		const UISizeF sliderSize = { sliderWidth, kOptionSliderHeight };

		auto makeSliderRect = [&](float yOffset)
		{
			const float left = (screenW - sliderWidth) * 0.5f;
			const float top = (screenH - kOptionSliderHeight) * 0.5f + yOffset;
			return D2D1::RectF(left, top, left + sliderWidth, top + kOptionSliderHeight);
		};

		auto& audio = GameAudio::Instance();
		const D2D1_RECT_F bgmRect = makeSliderRect(-46.0f);
		const D2D1_RECT_F seRect = makeSliderRect(18.0f);
		const float bgmVolume = UpdateOptionSliderValue(kOptionSliderBgm, bgmRect, audio.GetBgmVolume());
		const float seVolume = UpdateOptionSliderValue(kOptionSliderSe, seRect, audio.GetSeVolume());

		if (std::fabs(bgmVolume - audio.GetBgmVolume()) > 0.001f)
		{
			audio.SetBgmVolume(bgmVolume);
		}
		if (std::fabs(seVolume - audio.GetSeVolume()) > 0.001f)
		{
			audio.SetSeVolume(seVolume);
		}

		m_uiManager.AddFullscreenBackgroundOverlay(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.48f));

		m_uiManager.AddSlider(
			L"BGM",
			audio.GetBgmVolume(),
			UIAnchor::Center,
			{ 0.0f, -46.0f },
			sliderSize);

		m_uiManager.AddSlider(
			L"SE",
			audio.GetSeVolume(),
			UIAnchor::Center,
			{ 0.0f, 18.0f },
			sliderSize);

		UIButtonBehavior exitButtonBehavior;
		exitButtonBehavior.enabled = !m_screenTransition.IsInputBlocked();
		auto exitButton = m_uiManager.AddButton(
			L"EXIT",
			UIAnchor::Center,
			{ 0.0f, 104.0f },
			{ 180.0f, 52.0f },
			D2D1::ColorF(0.08f, 0.09f, 0.11f, 0.92f),
			D2D1::ColorF(0.18f, 0.20f, 0.24f, 0.96f),
			D2D1::ColorF(D2D1::ColorF::White),
			exitButtonBehavior);

		if (exitButton.clicked)
		{
			RequestExitGame();
		}
	}

	void Scene::RenderUIWithTransition(UILayer& uiLayer)
	{
		if (m_screenTransition.GetAlpha() > 0.0f)
		{
			m_uiManager.AddFullscreenOverlay(m_screenTransition.GetOverlayColor());
		}

		m_uiManager.Render(uiLayer);
	}

	void Scene::UpdateInitialControlGuide(double elapsedTime)
	{
		if (m_initialControlGuideSecondsRemaining <= 0.0)
		{
			return;
		}

		m_initialControlGuideSecondsRemaining = bsmUtil::Max(
			0.0,
			m_initialControlGuideSecondsRemaining - bsmUtil::Max(0.0, elapsedTime));
	}

	void Scene::DrawInitialControlGuide()
	{
		if (m_initialControlGuideSecondsRemaining <= 0.0)
		{
			return;
		}

		const float alpha = static_cast<float>(bsmUtil::Clamp(
			m_initialControlGuideSecondsRemaining / kInitialControlGuideFadeSeconds,
			0.0,
			1.0));
		const std::wstring guideText =
			L"WASD 移動    Space ジャンプ    マウス 照準    左クリック 攻撃";

		// HPゲージより少し上に出し、戦闘画面を隠しすぎない短い一行だけにする。
		m_uiManager.AddText(
			guideText,
			UIAnchor::BottomCenter,
			{ 2.0f, -103.0f },
			{ 760.0f, 34.0f },
			UITextAlign::Center,
			D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.70f * alpha),
			22.0f);
		m_uiManager.AddText(
			guideText,
			UIAnchor::BottomCenter,
			{ 0.0f, -105.0f },
			{ 760.0f, 34.0f },
			UITextAlign::Center,
			D2D1::ColorF(1.0f, 0.92f, 0.50f, alpha),
			22.0f);
	}

	void Scene::UpdateUI(std::unique_ptr<UILayer>& uiLayer)
	{
		if (!uiLayer)
		{
			return;
		}

		m_uiManager.BeginFrame();

		if (m_gameState == GameState::Title)
		{
			if (m_optionOpen)
			{
				DrawOptionMenu(*uiLayer);
				DrawOptionButton();
				uiLayer->SetCrosshairEnabled(false);
				RenderUIWithTransition(*uiLayer);
				return;
			}

			const bool inputBlocked = m_screenTransition.IsInputBlocked();
			const float titleBob = std::sin(static_cast<float>(m_titleTime) * 1.8f) * 10.0f;
			const D2D1_COLOR_F selectedColor = D2D1::ColorF(1.0f, 0.86f, 0.12f, 1.0f);
			const D2D1_COLOR_F normalColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.92f);
			const D2D1_COLOR_F buttonBaseColor = D2D1::ColorF(0.04f, 0.05f, 0.06f, 0.74f);
			const D2D1_COLOR_F buttonHoverColor = D2D1::ColorF(0.14f, 0.16f, 0.18f, 0.88f);
			const float screenW = uiLayer->GetWidth();
			const UISizeF menuSize = { 260.0f, 38.0f };
			const float logoWidth = bsmUtil::Min(780.0f, screenW * 0.74f);
			const float logoHeight = logoWidth * (173.0f / 1365.0f);
			UIButtonBehavior titleButtonBehavior;
			titleButtonBehavior.enabled = !inputBlocked;
			// キーボード決定と同じ経路で音を鳴らすため、マウスクリック時の自動再生は無効にする。
			titleButtonBehavior.playClickSound = false;

			m_uiManager.AddImage(
				App::GetRelativeAssetsDir() + L"Textures/TitleLogo.png",
				UIAnchor::Center,
				{ 0.0f, -220.0f + titleBob },
				{ logoWidth, logoHeight });

			auto startButton = m_uiManager.AddButton(
				L"START",
				UIAnchor::Center,
				{ 0.0f, 110.0f },
				menuSize,
				buttonBaseColor,
				buttonHoverColor,
				m_titleMenuIndex == 0 ? selectedColor : normalColor,
				titleButtonBehavior);

			auto exitButton = m_uiManager.AddButton(
				L"EXIT",
				UIAnchor::Center,
				{ 0.0f, 158.0f },
				menuSize,
				buttonBaseColor,
				buttonHoverColor,
				m_titleMenuIndex == 1 ? selectedColor : normalColor,
				titleButtonBehavior);

			if (startButton.hovered)
			{
				SetTitleMenuIndex(0, false);
			}
			else if (exitButton.hovered)
			{
				SetTitleMenuIndex(1, false);
			}

			if (startButton.clicked)
			{
				SetTitleMenuIndex(0, false);
				ConfirmTitleMenuSelection();
			}
			if (exitButton.clicked)
			{
				SetTitleMenuIndex(1, false);
				ConfirmTitleMenuSelection();
			}

			DrawOptionButton();
			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}

		if (m_gameState == GameState::Result)
		{
			const D2D1_COLOR_F white = D2D1::ColorF(D2D1::ColorF::White);
			const D2D1_COLOR_F red = D2D1::ColorF(0.92f, 0.12f, 0.10f, 1.0f);
			const D2D1_COLOR_F yellow = D2D1::ColorF(1.0f, 0.82f, 0.12f, 1.0f);

			// ゲームオーバー時の3D画面を残し、UIだけを読みやすく暗くする。
			m_uiManager.AddFullscreenBackgroundOverlay(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.68f));

			m_uiManager.AddText(
				L"GAME OVER",
				UIAnchor::Center,
				{ 0.0f, -230.0f },
				{ 720.0f, 72.0f },
				UITextAlign::Center,
				red,
				56.0f);

			m_uiManager.AddText(
				L"生存時間",
				UIAnchor::Center,
				{ 0.0f, -145.0f },
				{ 360.0f, 32.0f },
				UITextAlign::Center,
				white,
				22.0f);

			const int survivalSeconds = static_cast<int>(m_lastSurvivalTime);
			wchar_t survivalText[32];
			swprintf_s(
				survivalText,
				L"%02d:%02d",
				survivalSeconds / 60,
				survivalSeconds % 60);
			m_uiManager.AddText(
				survivalText,
				UIAnchor::Center,
				{ 0.0f, -108.0f },
				{ 360.0f, 46.0f },
				UITextAlign::Center,
				yellow,
				38.0f);

			auto addResultRow = [&](const std::wstring& label, const std::wstring& value, float y)
			{
				m_uiManager.AddText(
					label,
					UIAnchor::Center,
					{ -150.0f, y },
					{ 250.0f, 38.0f },
					UITextAlign::Right,
					white,
					22.0f);
				m_uiManager.AddText(
					value,
					UIAnchor::Center,
					{ 100.0f, y - 2.0f },
					{ 220.0f, 42.0f },
					UITextAlign::Left,
					yellow,
					28.0f);
			};

			addResultRow(L"総撃破数", std::to_wstring(m_lastDefeatedEnemyCount), -48.0f);
			addResultRow(L"到達ウェーブ", std::to_wstring(m_lastReachedWave), -10.0f);
			addResultRow(L"プレイヤーレベル", std::to_wstring(m_lastPlayerLevel), 28.0f);
			addResultRow(L"与えた総ダメージ", std::to_wstring(m_lastTotalDamageDealt), 66.0f);

			m_uiManager.AddText(
				L"BEST EXPLOSION",
				UIAnchor::Center,
				{ 0.0f, 118.0f },
				{ 460.0f, 36.0f },
				UITextAlign::Center,
				white,
				26.0f);

			wchar_t bestExplosionText[64];
			swprintf_s(
				bestExplosionText,
				L"1 BOMB / %d KILLS",
				m_lastBestExplosionKills);
			m_uiManager.AddText(
				bestExplosionText,
				UIAnchor::Center,
				{ 0.0f, 153.0f },
				{ 500.0f, 48.0f },
				UITextAlign::Center,
				yellow,
				34.0f);

			UIButtonBehavior titleButtonBehavior;
			titleButtonBehavior.enabled = !m_screenTransition.IsInputBlocked();
			auto titleButton = m_uiManager.AddButton(
				L"TITLE",
				UIAnchor::Center,
				{ 0.0f, 220.0f },
				{ 240.0f, 58.0f },
				D2D1::ColorF(0.35f, 0.12f, 0.12f, 0.95f),
				D2D1::ColorF(0.65f, 0.20f, 0.20f, 0.95f),
				white,
				titleButtonBehavior);

			if (titleButton.clicked)
			{
				RequestStartTitle();
			}

			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (!gameStage)
		{
			if (m_screenTransition.GetAlpha() > 0.0f)
			{
				RenderUIWithTransition(*uiLayer);
			}
			else
			{
				uiLayer->ClearDrawCommands();
			}
			return;
		}

#if defined(_DEBUG)
		if (m_stageEditor.IsActive())
		{
			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}
#endif

		auto device = BaseDevice::GetBaseDevice();
		auto player = gameStage->GetSharedGameObjectEx<Player>(L"Player", false);
		auto hp = player ? player->GetComponent<Health>() : nullptr;

		if (m_optionOpen)
		{
			DrawOptionMenu(*uiLayer);
			DrawOptionButton();
			uiLayer->SetCrosshairEnabled(false);
			RenderUIWithTransition(*uiLayer);
			return;
		}

		// 左上：デバッグ表示
		{
			const auto& debug = GameDebugSettingsStore::Get();
			wchar_t buff[256] = {};

			// 表示項目を個別に無効化できるよう、改行を含む文字列を組み立て分ける。
			if (debug.showFps && debug.showElapsedTime)
			{
				swprintf_s(
					buff,
					L"FPS: %.1f\nElapsed Time: %.6f",
					device->GetStableFps(),
					device->GetStableElapsedTime());
			}
			else if (debug.showFps)
			{
				swprintf_s(buff, L"FPS: %.1f", device->GetStableFps());
			}
			else if (debug.showElapsedTime)
			{
				swprintf_s(buff, L"Elapsed Time: %.6f", device->GetStableElapsedTime());
			}

			if (buff[0] != L'\0')
			{
				m_uiManager.AddText(
					buff,
					UIAnchor::TopLeft,
					{ 20.0f, 20.0f },
					{ 300.0f, 70.0f },
					UITextAlign::Left);
			}
		}

		// 右上：ベンチマーク開始/終了通知
		{
			auto& benchmark = BenchmarkRecorder::Instance();
			if (benchmark.HasNotification())
			{
				m_uiManager.AddText(
					benchmark.GetNotificationText(),
					UIAnchor::TopRight,
					{ -20.0f, 20.0f },
					{ 360.0f, 36.0f },
					UITextAlign::Right,
					D2D1::ColorF(1.0f, 0.88f, 0.18f, 1.0f),
					22.0f);
			}
		}

		// 下中央：HPゲージ
		if (hp)
		{
			wchar_t hpLabel[128];
			swprintf_s(hpLabel, L"HP  %d / %d", hp->GetHP(), hp->GetMaxHP());

			m_uiManager.AddProgressBar(
				hpLabel,
				static_cast<float>(hp->GetHP()),
				static_cast<float>(hp->GetMaxHP()),
				UIAnchor::BottomCenter,
				{ 0.0f, -48.0f },
				{ 320.0f, 28.0f });
		}

		if (player)
		{
			m_uiManager.AddProgressBar(
				L"",
				static_cast<float>(player->GetExperience()),
				static_cast<float>(player->GetRequiredExperience()),
				UIAnchor::TopCenter,
				{ 0.0f, 24.0f },
				{ 420.0f, 18.0f },
				D2D1::ColorF(0.12f, 0.52f, 1.0f, 0.95f));

			const std::wstring levelText = L"LV " + std::to_wstring(player->GetLevel());
			m_uiManager.AddText(
				levelText,
				UIAnchor::TopCenter,
				{ 0.0f, 45.0f },
				{ 180.0f, 28.0f },
				UITextAlign::Center,
				D2D1::ColorF(1.0f, 0.92f, 0.50f, 1.0f),
				22.0f);

			// 左下：現在所持している爆弾数
			const std::wstring bombCountText =
				L"x " + std::to_wstring(player->GetBombAmmo());

			m_uiManager.AddImage(
				App::GetRelativeAssetsDir() + kBombHudIconPath,
				UIAnchor::BottomLeft,
				{ kBombHudMargin, -kBombHudMargin },
				{ kBombHudIconSize, kBombHudIconSize });

			// 数字へ影を付け、明暗の異なるステージ背景でも読み取れるようにする。
			m_uiManager.AddText(
				bombCountText,
				UIAnchor::BottomLeft,
				{ kBombHudMargin + kBombHudIconSize + 10.0f, -kBombHudMargin + 2.0f },
				{ 100.0f, kBombHudIconSize },
				UITextAlign::Left,
				D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.85f),
				30.0f);
			m_uiManager.AddText(
				bombCountText,
				UIAnchor::BottomLeft,
				{ kBombHudMargin + kBombHudIconSize + 8.0f, -kBombHudMargin },
				{ 100.0f, kBombHudIconSize },
				UITextAlign::Left,
				D2D1::ColorF(1.0f, 0.88f, 0.18f, 1.0f),
				30.0f);
		}

		DrawInitialControlGuide();

		// ダメージ数
		if (auto camera = gameStage->GetCamera())
		{
			const float screenW = uiLayer->GetWidth();
			const float screenH = uiLayer->GetHeight();
			auto view = (XMMATRIX)((Mat4x4)camera->GetViewMatrix());
			auto proj = (XMMATRIX)((Mat4x4)camera->GetProjMatrix());
			auto world = XMMatrixIdentity();

			for (const auto& damageNumber : gameStage->GetDamageNumbers())
			{
				auto projected = XMVector3Project(
					(XMVECTOR)damageNumber.position,
					0.0f,
					0.0f,
					screenW,
					screenH,
					0.0f,
					1.0f,
					proj,
					view,
					world);

				XMFLOAT3 screenPos;
				XMStoreFloat3(&screenPos, projected);
				if (screenPos.z < 0.0f || screenPos.z > 1.0f)
				{
					continue;
				}
				if (screenPos.x < -100.0f || screenPos.x > screenW + 100.0f ||
					screenPos.y < -60.0f || screenPos.y > screenH + 60.0f)
				{
					continue;
				}

				const float alpha = damageNumber.GetAlpha();
				const float width = 96.0f;
				const float height = 34.0f;
				const float left = screenPos.x - width * 0.5f;
				const float top = screenPos.y - height * 0.5f;

				m_uiManager.AddText(
					damageNumber.text,
					UIAnchor::TopLeft,
					{ left + 2.0f, top + 2.0f },
					{ width, height },
					UITextAlign::Center,
					D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.55f * alpha));

				m_uiManager.AddText(
					damageNumber.text,
					UIAnchor::TopLeft,
					{ left, top },
					{ width, height },
					UITextAlign::Center,
					D2D1::ColorF(1.0f, 0.82f, 0.16f, alpha));
			}
		}
		DrawOptionButton();
		uiLayer->SetCrosshairEnabled(true);
		RenderUIWithTransition(*uiLayer);
	}

}
