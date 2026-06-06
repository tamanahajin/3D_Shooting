#pragma once
#include "stdafx.h"
#include "UIManager.h"
#include "Project.h"

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
		SimpleConstant m_ConstantBuffer;
		size_t m_ConstantBufferIndex;
		std::shared_ptr<BaseMesh> m_mesh;
		double m_totalTime;
		TransParam m_param;
		std::shared_ptr<Camera> m_camera;
		std::shared_ptr<LightSet> m_lightSet;
		UIManager m_uiManager;
		bool m_CursorVisible = true;

		GameState m_GameState = GameState::Title;
		int m_LastScore = 0;
		int m_TitleMenuIndex = 0;
		int m_TitleHoveredMenuIndex = -1;
		double m_TitleTime = 0.0;
		ScreenTransition m_ScreenTransition;

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
		void PlayButtonDecideSound();
		void ConfirmTitleMenuSelection();
		void SetTitleMenuIndex(int index, bool playCursorMoveSound);
		void UpdateTitleInput();
		void RenderUIWithTransition(UILayer& uiLayer);
		void SetMouseCursorVisible(bool visible);

		virtual void CreateAssetResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) override;
		virtual void Update(double elapsedTime) override;
		virtual void UpdateConstantBuffers() override;
		virtual void CommitConstantBuffers() override;
		virtual void UpdateUI(std::unique_ptr<UILayer>& uiLayer) override;
		virtual void ShadowPass(ID3D12GraphicsCommandList* pCommandList) override;
		virtual void ScenePass(ID3D12GraphicsCommandList* pCommandList) override;
	};

}
