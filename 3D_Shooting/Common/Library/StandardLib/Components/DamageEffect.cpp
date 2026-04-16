/*!
@file DamageEffectComponent.cpp
@brief ダメージエフェクトコンポーネント 実体（モデル全体を赤くする）
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	// シェーダ定義（あなたの他コンポーネントに合わせて GetShadersDir を使用）
	IMPLEMENT_DX12SHADER(VSDamageEffect, App::GetShadersDir() + L"VSDamageEffect.cso")
	IMPLEMENT_DX12SHADER(PSDamageEffect, App::GetShadersDir() + L"PSDamageEffect.cso")

	DamageEffect::DamageEffect(const std::shared_ptr<GameObject>& gameObjectPtr)
		: Component(gameObjectPtr)
	{
		// m_ConstantBuffer は {} でゼロ初期化済み
	}

	DamageEffect::~DamageEffect() = default;

	void DamageEffect::OnCreate()
	{
		//==========================================================
		// 1) 定数バッファ領域を FrameResource に確保
		//==========================================================
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();

		for (size_t i = 0; i < BaseDevice::FrameCount; i++)
		{
			// AddBaseConstantBufferSet<T>() は各FrameResourceに同じスロット番号で確保される想定
			m_ConstantBufferIndex =
				frameResources[i]->AddBaseConstantBufferSet<DamageEffectConstantBuffer>(pBaseDevice->GetD3D12Device());
		}

		//==========================================================
		// 2) ダメージエフェクト用PSOを作成（未作成なら）
		//==========================================================
		EnsureDamageEffectPipelineState();
	}

	void DamageEffect::OnUpdate(double elapsedTime)
	{
		if (!m_IsEffectActive)
		{
			return;
		}

		m_EffectTimer -= elapsedTime;

		// 時間切れ
		if (m_EffectTimer <= 0.0)
		{
			StopEffect();
		}
	}

	void DamageEffect::StartEffect(float duration)
	{
		OutputDebugString(L"[DMG] StartEffect called\n");
		// 0以下のdurationは事故りやすいので最低値を入れる
		if (duration <= 0.0f)
		{
			duration = 0.001f;
		}

		m_EffectDuration = duration;
		m_EffectTimer = duration;
		m_IsEffectActive = true;
	}

	void DamageEffect::StopEffect()
	{
		m_IsEffectActive = false;
		m_EffectTimer = 0.0;
	}

	void DamageEffect::EnsureDamageEffectPipelineState()
	{
		// すでに作られていれば何もしない
		ComPtr<ID3D12PipelineState> damageEffectPSO = PipelineStatePool::GetPipelineState(kDamageEffectPSOKey);
		if (damageEffectPSO)
		{
			return;
		}

		//==========================================================
		// ダメージエフェクトPSO（モデル全体を赤くする）を作る
		//  - 入力レイアウトは PNT（Position/Normal/Tex）に合わせる
		//  - RootSignature は BaseCrossDefault を使う（あなたの3D描画と同じ）
		//  - Cull = BACK（通常の裏面カリング）
		//  - DepthWrite = OFF：深度を書かない → Zを汚さない
		//  - DepthBias：Zファイティングを防ぐため、少し手前に描画
		//  - AlphaBlend = ON：赤色を半透明で重ねる
		//==========================================================
		CD3DX12_RASTERIZER_DESC rasterizerState(D3D12_DEFAULT);
		rasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 裏面カリングなし
		rasterizerState.DepthBias = 0;               // 負の値でカメラ側に近づける（Zファイティング対策）
		rasterizerState.DepthBiasClamp = 0.0f;
		rasterizerState.SlopeScaledDepthBias = 0.0f;


		CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
		depthStencil.DepthEnable = FALSE;
		depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度は書かない
		depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		ZeroMemory(&psoDesc, sizeof(psoDesc));

		// 入力レイアウト：あなたの静的メッシュ描画（BcPNTStaticDraw等）と揃える
		psoDesc.InputLayout =
		{
			VertexPositionNormalTexture::GetVertexElement(),
			VertexPositionNormalTexture::GetNumElements()
		};

		// RootSignature：BaseSceneで作ってPoolに登録済み
		psoDesc.pRootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true).Get();

		// シェーダ
		psoDesc.VS =
		{
			reinterpret_cast<UINT8*>(VSDamageEffect::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
			VSDamageEffect::GetPtr()->GetShaderComPtr()->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<UINT8*>(PSDamageEffect::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
			PSDamageEffect::GetPtr()->GetShaderComPtr()->GetBufferSize()
		};

		psoDesc.RasterizerState = rasterizerState;

		// アルファブレンドを有効にしてモデル全体に赤色を重ねる
		psoDesc.BlendState = BlendState::GetAlphaBlendEx();

		psoDesc.DepthStencilState = depthStencil;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&damageEffectPSO)));
		NAME_D3D12_OBJECT(damageEffectPSO);

		PipelineStatePool::AddPipelineState(kDamageEffectPSOKey, damageEffectPSO);
	}

	void DamageEffect::UpdateConstantBuffer()
	{
		//==========================================================
		// 行列とダメージ値（0..1）を作る
		//==========================================================
		auto transform = GetGameObject()->GetComponent<Transform>();
		if (!transform)
		{
			return;
		}

		// Transformのワールド行列（Mat4x4 -> XMMATRIX にキャストできる作りになっている前提）
		XMMATRIX world = (XMMATRIX)transform->GetWorldMatrix();
		XMStoreFloat4x4(&m_ConstantBuffer.World, XMMatrixTranspose(world));
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto stage = std::dynamic_pointer_cast<Stage>(scene->GetActiveStage(true));
		if (!stage)
		{
			return;
		}

		// カメラから ViewProj を作る
		auto camera = stage->GetCamera();
		if (camera)
		{
			XMMATRIX view = (XMMATRIX)((Mat4x4)camera->GetViewMatrix());
			XMMATRIX proj = (XMMATRIX)((Mat4x4)camera->GetProjMatrix());
			XMMATRIX viewProj = view * proj;

			XMStoreFloat4x4(&m_ConstantBuffer.ViewProj, XMMatrixTranspose(viewProj));
		}

		// damage = 1 → 0 に向かって減る（Start直後は最大）
		float denom = (m_EffectDuration <= 0.0f) ? 0.001f : m_EffectDuration;
		float damageValue = static_cast<float>(m_EffectTimer / denom);
		damageValue = bsmUtil::Max(0.0f, std::min(1.0f, damageValue));

		m_ConstantBuffer.Damage = damageValue;
		m_ConstantBuffer.OutlineWidth = m_OutlineWidth; // 未使用だが互換性のため設定

		// Pad は使わないが念のため
		m_ConstantBuffer.Pad = XMFLOAT2(0, 0);
	}

	void DamageEffect::OnDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		if (!m_IsEffectActive)
		{
			return;
		}

		OutputDebugString(L"[DMG] OnDraw called\n");

		auto pBaseScene = BaseScene::Get();
		auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();

		// 1) CB更新
		UpdateConstantBuffer();

		// 2) GPUへCBを書き込む（Upload領域に memcpy）
		//    ※BcPNTStaticDraw::OnCommitConstantBuffers() と同じ方式
		memcpy(
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			&m_ConstantBuffer,
			sizeof(m_ConstantBuffer)
		);

		// 3) PSO / RootSignature 設定
		ComPtr<ID3D12PipelineState> damageEffectPSO = PipelineStatePool::GetPipelineState(kDamageEffectPSOKey, true);
		pCommandList->SetPipelineState(damageEffectPSO.Get());

		ComPtr<ID3D12RootSignature> rs = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);
		pCommandList->SetGraphicsRootSignature(rs.Get());

		// 4) b0 (root CBV) をセット
		pCommandList->SetGraphicsRootConstantBufferView(
			pBaseScene->GetGpuSlotID(L"b0"),
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex]
			.m_baseConstantBuffer->GetGPUVirtualAddress()
		);

		// 5) 同じメッシュをもう一度描く（これがダメージエフェクト）
		//
		// 注意：ここでは「BcPNTStaticDraw を持っている」ことを前提にしています。
		//       もし SpPNTStaticDraw を使っている場合は、メッシュ参照方法を合わせてください。
		auto drawComp = GetGameObject()->GetComponent<BcPNTStaticDraw>(false);
		if (!drawComp)
		{
			OutputDebugString(L"[DMG] drawComp null\n");
			return;
		}

		//auto mesh = drawComp->GetBaseMesh(0);
		//if (!mesh)
		//{
		//	return;
		//}

		// IA設定（BcPNTStaticDraw と同じ）
		//pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
		//pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
		//pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const size_t meshCount = drawComp->GetBaseModelMeshCount();
		wchar_t buf[128];
		swprintf_s(buf, L"[DMG] meshCount = %zu\n", meshCount);
		OutputDebugString(buf);
		for (size_t i = 0; i < meshCount; ++i)
		{
			auto mesh = drawComp->GetBaseModelMesh(i);
			if (!mesh)
			{
				continue;
			}

			pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
			pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
			pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);
		}
	}

} // namespace shooting
