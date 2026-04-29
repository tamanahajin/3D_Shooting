#include "stdafx.h"
#include "Project.h"

namespace shooting {

	IMPLEMENT_DX12SHADER(SpVSPCStatic, App::GetShadersDir() + L"SpVSPCStatic.cso")
	IMPLEMENT_DX12SHADER(SpPSPCStatic, App::GetShadersDir() + L"SpPSPCStatic.cso")

	using namespace SceneEnums;

	Scene::Scene(UINT frameCount, PrimDevice* pPrimDevice) :
		BaseScene(frameCount, pPrimDevice)
	{
	}

	Scene::~Scene()
	{
	}

	void Scene::SetMouseCursorVisible(bool visible)
	{
		if (m_CursorVisible == visible)
		{
			return;
		}

		m_CursorVisible = visible;

		if (visible)
		{
			while (::ShowCursor(TRUE) < 0) {}
		}
		else
		{
			while (::ShowCursor(FALSE) >= 0) {}
		}
	}

	void Scene::CreateAssetResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
	{
		// テクスチャ
		auto texFile = App::GetRelativeAssetsDir() + L"wall.jpg";
		auto texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"WALL_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"sky.jpg";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"SKY_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"trace.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"TRACE_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"trace3.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"TRACE3_TX", texture);

		texFile = App::GetRelativeAssetsDir() + L"Textures/particle_fire.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"EXPLOSION_FIRE_TX", texture);

		// ここで先にキャラ用テクスチャを登録して保持させる
		texFile = App::GetRelativeAssetsDir() + L"Model/Textures/colormap.png";
		texture = BaseTexture::CreateTextureFlomFile(pCommandList, texFile);
		RegisterTexture(L"CHARACTER_TEXTURE_SKINNED", texture);

		// 床
		{
			auto floorParts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				L"Model/floor.fbx"
			);

			std::vector<std::shared_ptr<BaseMesh>> floorMeshes;
			for (size_t i = 0; i < floorParts.size(); ++i)
			{
				floorMeshes.push_back(floorParts[i].mesh);
				RegisterMaterial(
					L"FLOOR_MAT_" + std::to_wstring(i),
					floorParts[i].material
				);
			}
			RegisterModelMesh(L"FLOOR_MODEL", floorMeshes);
		}

		// 岩付きの床
		{
			auto floorDetailParts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				L"Model/floor-detail.fbx"
			);

			std::vector<std::shared_ptr<BaseMesh>> floorDetailMeshes;
			for (size_t i = 0; i < floorDetailParts.size(); ++i)
			{
				floorDetailMeshes.push_back(floorDetailParts[i].mesh);
				RegisterMaterial(
					L"FLOOR_DETAIL_MAT_" + std::to_wstring(i),
					floorDetailParts[i].material
				);
			}
			RegisterModelMesh(L"FLOOR_DETAIL_MODEL", floorDetailMeshes);
		}

		// Enemy skinned instancing mesh.
		auto enemySkinnedMesh = BaseMesh::CreateMergedBoneModelMesh(
			pCommandList,
			App::GetRelativeAssetsDir(),
			L"Model/character-orc.fbx"
		);
		RegisterMesh(L"ENEMY_MODEL_SKINNED", enemySkinnedMesh);

		// プレイヤー
		auto skinnedMesh = BaseMesh::CreateMergedBoneModelMesh(
			pCommandList,
			App::GetRelativeAssetsDir(),
			L"Model/character-human.fbx"
		);
		RegisterMesh(L"PLAYER_MODEL_SKINNED", skinnedMesh);
	}

	void Scene::StartGame()
	{
		m_LastScore = 0;
		m_GameState = GameState::Playing;

		SetMouseCursorVisible(false);

		ResetActiveStage<GameStage>(App::GetD3D12Device());
	}

	void Scene::UpdateConstantBuffers()
	{
		if (m_activeStage)
		{
			m_activeStage->OnUpdateConstantBuffers();
		}
	}

	void Scene::CommitConstantBuffers()
	{
		if (m_activeStage)
		{
			m_activeStage->OnCommitConstantBuffers();
		}
	}


	void Scene::UpdateUI(std::unique_ptr<UILayer>& uiLayer)
	{
		if (!uiLayer)
		{
			return;
		}

		m_uiManager.BeginFrame();

		if (m_GameState == GameState::Title)
		{
			m_uiManager.AddText(
				L"3D SHOOTING",
				UIAnchor::Center,
				{ 0.0f, -160.0f },
				{ 600.0f, 80.0f },
				UITextAlign::Center);

			auto startButton = m_uiManager.AddButton(
				L"START",
				UIAnchor::Center,
				{ 0.0f, 0.0f },
				{ 260.0f, 64.0f },
				D2D1::ColorF(0.10f, 0.35f, 0.20f, 0.95f),
				D2D1::ColorF(0.20f, 0.60f, 0.35f, 0.95f),
				D2D1::ColorF(D2D1::ColorF::White));

			if (startButton.clicked)
			{
				StartGame();
			}

			uiLayer->SetCrosshairEnabled(false);
			m_uiManager.Render(*uiLayer);
			return;
		}

		if (m_GameState == GameState::Result)
		{
			wchar_t buff[256];
			swprintf_s(
				buff,
				L"GAME OVER\n\nScore: %d",
				m_LastScore);

			m_uiManager.AddText(
				buff,
				UIAnchor::Center,
				{ 0.0f, -170.0f },
				{ 600.0f, 160.0f },
				UITextAlign::Center);

			auto titleButton = m_uiManager.AddButton(
				L"BACK TO TITLE",
				UIAnchor::Center,
				{ 0.0f, 80.0f },
				{ 320.0f, 64.0f },
				D2D1::ColorF(0.35f, 0.12f, 0.12f, 0.95f),
				D2D1::ColorF(0.65f, 0.20f, 0.20f, 0.95f),
				D2D1::ColorF(D2D1::ColorF::White));

			if (titleButton.clicked)
			{
				if (m_activeStage)
				{
					m_activeStage->OnDestroy();
					m_activeStage = nullptr;
				}

				m_GameState = GameState::Title;
				SetMouseCursorVisible(true);
			}

			uiLayer->SetCrosshairEnabled(false);
			m_uiManager.Render(*uiLayer);
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (!gameStage)
		{
			uiLayer->ClearDrawCommands();
			return;
		}

		auto device = BaseDevice::GetBaseDevice();
		auto player = gameStage->GetSharedGameObjectEx<Player>(L"Player", false);
		auto hp = player ? player->GetComponent<Health>() : nullptr;

		// 左上：デバッグ表示
		{
			wchar_t buff[256];
			swprintf_s(
				buff,
				L"FPS: %.1f\nFrame: %.6f",
				device->GetStableFps(),
				device->GetStableElapsedTime());

			m_uiManager.AddText(
				buff,
				UIAnchor::TopLeft,
				{ 20.0f, 20.0f },
				{ 260.0f, 70.0f },
				UITextAlign::Left);
		}

		// 右上：撃破数
		//{
		//	wchar_t buff[128];
		//	swprintf_s(
		//		buff,
		//		L"Kills  %d / %d",
		//		gameStage->GetDefeatedEnemyCount(),
		//		gameStage->GetTotalEnemyCount());

		//	m_uiManager.AddText(
		//		buff,
		//		UIAnchor::TopRight,
		//		{ -20.0f, 20.0f },
		//		{ 260.0f, 40.0f },
		//		UITextAlign::Right);
		//}

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

		uiLayer->SetCrosshairEnabled(true);
		m_uiManager.Render(*uiLayer);
	}

	void Scene::Update(double elapsedTime)
	{
		s_elapsedTime = elapsedTime;

		if (m_GameState == GameState::Playing)
		{
			if (m_activeStage)
			{
				m_activeStage->UpdateStage();

				auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
				auto player = gameStage ? gameStage->GetSharedGameObjectEx<Player>(L"Player", false) : nullptr;

				if (player && player->IsDeathAnimationFinished())
				{
					m_LastScore = gameStage->GetDefeatedEnemyCount();
					m_GameState = GameState::Result;

					SetMouseCursorVisible(true);
				}

				UpdateConstantBuffers();
				CommitConstantBuffers();
			}
			return;
		}

		if (m_GameState == GameState::Result)
		{
			if (m_activeStage)
			{
				UpdateConstantBuffers();
				CommitConstantBuffers();
			}
			return;
		}
	}

	void Scene::ShadowPass(ID3D12GraphicsCommandList* pCommandList)
	{
		// Set necessary state.
		auto rootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);
		pCommandList->SetGraphicsRootSignature(rootSignature.Get());
		//PipelineState
		auto shadowPipeline = PipelineStatePool::GetPipelineState(L"PNTShadowMap", false);
		if (shadowPipeline)
		{
			pCommandList->SetPipelineState(shadowPipeline.Get());
		}
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->RSSetViewports(1, &m_viewport);
		pCommandList->RSSetScissorRects(1, &m_scissorRect);
		// No render target needed for the shadow pass.
		pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &m_depthDsvs[DepthGenPass::Shadow]);

		if (m_activeStage)
		{
			m_pTgtCommandList = pCommandList;
			m_activeStage->OnShadowDraw(pCommandList);
		}
	}

	void Scene::ScenePass(ID3D12GraphicsCommandList* pCommandList)
	{
		auto& viewport = GetViewport();
		auto& scissorRect = GetScissorRect();
		auto depthDsvs = GetDepthDsvs();
		// set RootSignature
		auto rootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);
		pCommandList->SetGraphicsRootSignature(rootSignature.Get());
		// set Viewports & ScissorRects
		pCommandList->RSSetViewports(1, &viewport);
		pCommandList->RSSetScissorRects(1, &scissorRect);
		// set RenderTargets
		pCommandList->OMSetRenderTargets(1, &GetCurrentBackBufferRtvCpuHandle(), FALSE, &depthDsvs[DepthGenPass::Scene]);
		if (m_activeStage)
		{
			m_pTgtCommandList = pCommandList;
			m_activeStage->OnSceneDraw(pCommandList);
		}
	}
}