#pragma once
#include "stdafx.h"
#include "Scripts/UI/UIManager.h"
#include "Project.h"
#include "Scripts/Stage/StageEditor.h"

namespace shooting {

	DECLARE_DX12SHADER(SpVSPCStatic)
	DECLARE_DX12SHADER(SpPSPCStatic)

	enum class GameState
	{
		Title,
		Playing,
		Result
	};

	class Scene : public BaseScene
	{
		SimpleConstant m_constantBuffer;
		size_t m_constantBufferIndex;
		std::shared_ptr<BaseMesh> m_mesh;
		double m_totalTime;
		TransParam m_param;
		std::shared_ptr<Camera> m_camera;
		std::shared_ptr<LightSet> m_lightSet;
		UIManager m_uiManager;
		bool m_cursorVisible = true;

		GameState m_gameState = GameState::Title;
		double m_lastSurvivalTime = 0.0;
		int m_lastDefeatedEnemyCount = 0;
		int m_lastReachedWave = 0;
		int m_lastPlayerLevel = 1;
		long long m_lastTotalDamageDealt = 0;
		int m_lastBestExplosionKills = 0;
		int m_titleMenuIndex = 0;
		double m_titleTime = 0.0;
		bool m_optionOpen = false;
		bool m_waitingForOptionMouseRelease = false;
		int m_optionDraggingSlider = -1;
		ScreenTransition m_screenTransition;
		StageEditor m_stageEditor;
		bool m_stageEditorReloadRequested = false;
		bool m_hasShownInitialControlGuide = false;
		double m_initialControlGuideSecondsRemaining = 0.0;

	public:
		Scene(UINT frameCount, PrimDevice* pPrimDevice);
		virtual ~Scene();

	protected:
		bool IsMouseInRect(const D2D1_RECT_F& rect) const;
		void StartGame();
		void StartTitle();
		void RequestStartGame();
		void RequestStartTitle();
		void RequestExitGame();
		void ConfirmTitleMenuSelection();
		void SetTitleMenuIndex(int index, bool playCursorMoveSound);
		void UpdateTitleInput();
		void UpdateOptionInput();
		void OpenOptionMenu();
		void CloseOptionMenu();
		void DrawOptionButton();
		void DrawOptionMenu(UILayer& uiLayer);
		float UpdateOptionSliderValue(int sliderIndex, const D2D1_RECT_F& rect, float currentValue);
		void RenderUIWithTransition(UILayer& uiLayer);
		void UpdateInitialControlGuide(double elapsedTime);
		void DrawInitialControlGuide();
		void SetMouseCursorVisible(bool visible);
		void EnterStageEditor();
		void ExitStageEditor();
		void ReloadStageForEditor();

		virtual void CreateAssetResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) override;
		virtual void Update(double elapsedTime) override;
		virtual void UpdateConstantBuffers() override;
		virtual void CommitConstantBuffers() override;
		virtual void Destroy() override;
		virtual void UpdateUI(std::unique_ptr<UILayer>& uiLayer) override;
		virtual void UpdateImGui() override;
		virtual void ShadowPass(ID3D12GraphicsCommandList* pCommandList) override;
		virtual void ScenePass(ID3D12GraphicsCommandList* pCommandList) override;
	};

}
