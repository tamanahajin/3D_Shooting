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

		//ステージ作成
		ResetActiveStage<GameStage>(pDevice);
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

		auto gameStage = std::dynamic_pointer_cast<GameStage>(m_activeStage);
		if (!gameStage)
		{
			uiLayer->ClearDrawCommands();
			return;
		}

		auto device = BaseDevice::GetBaseDevice();
		auto player = gameStage->GetSharedGameObjectEx<Player>(L"Player", false);
		auto hp = player ? player->GetComponent<Health>() : nullptr;

		m_uiManager.BeginFrame();

		// 右上：撃破数
		{
			wchar_t buff[128];
			swprintf_s(
				buff,
				L"Kills  %d / %d",
				gameStage->GetDefeatedEnemyCount(),
				gameStage->GetTotalEnemyCount());

			m_uiManager.AddText(
				buff,
				UIAnchor::TopRight,
				{ -20.0f, 20.0f },
				{ 260.0f, 40.0f },
				UITextAlign::Right);
		}

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
		if (m_activeStage)
		{
			m_activeStage->UpdateStage();
			UpdateConstantBuffers();
			CommitConstantBuffers();
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