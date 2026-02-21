/*!
@file DamageEffectComponent.cpp
@brief ダメージエフェクトコンポーネント 実体（外周アウトライン）
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
		// 2) アウトライン用PSOを作成（未作成なら）
		//==========================================================
		EnsureOutlinePipelineState();
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

	void DamageEffect::EnsureOutlinePipelineState()
	{
		// すでに作られていれば何もしない
		ComPtr<ID3D12PipelineState> outlinePSO = PipelineStatePool::GetPipelineState(kOutlinePSOKey);
		if (outlinePSO)
		{
			return;
		}

		//==========================================================
		// アウトラインPSO（外周輪郭）を作る
		//  - 入力レイアウトは PNT（Position/Normal/Tex）に合わせる
		//  - RootSignature は BaseCrossDefault を使う（あなたの3D描画と同じ）
		//  - Cull = FRONT：表面を捨てて裏面だけ描く → 外周が残りやすい
		//  - DepthWrite = OFF：深度を書かない → Zを汚さない
		//==========================================================
		CD3DX12_RASTERIZER_DESC rasterizerState(D3D12_DEFAULT);
		rasterizerState.CullMode = D3D12_CULL_MODE_FRONT; // ★ここが肝

		CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
		depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // ★深度は書かない

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

		// PSDamageEffect は alpha = damage を返すので、ここはアルファブレンドにしておくとフェードが自然
		// もっと強い輪郭にしたいなら GetOpaqueBlend() + PSのalphaを1にする、でもOK。
		psoDesc.BlendState = BlendState::GetAlphaBlendEx();

		psoDesc.DepthStencilState = depthStencil;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outlinePSO)));
		NAME_D3D12_OBJECT(outlinePSO);

		PipelineStatePool::AddPipelineState(kOutlinePSOKey, outlinePSO);
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
		m_ConstantBuffer.OutlineWidth = m_OutlineWidth;

		// Pad は使わないが念のため
		m_ConstantBuffer.Pad = XMFLOAT2(0, 0);

		//==========================================================
		// 非一様スケールがある場合の注意：
		//   法線変換は「逆転置」が理想です。
		//   ただし、現状のVSDamageEffect.hlslは gWorld で法線を変換しているので、
		//   非一様スケールが強いと輪郭が歪む可能性があります。
		//   → 本格対応したい場合は HLSL側に normalMatrix を追加するのが定石。
		//==========================================================
	}

	void DamageEffect::OnDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		if (!m_IsEffectActive)
		{
			return;
		}

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
		ComPtr<ID3D12PipelineState> outlinePSO = PipelineStatePool::GetPipelineState(kOutlinePSOKey, true);
		pCommandList->SetPipelineState(outlinePSO.Get());

		ComPtr<ID3D12RootSignature> rs = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);
		pCommandList->SetGraphicsRootSignature(rs.Get());

		// 4) b0 (root CBV) をセット
		pCommandList->SetGraphicsRootConstantBufferView(
			pBaseScene->GetGpuSlotID(L"b0"),
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex]
			.m_baseConstantBuffer->GetGPUVirtualAddress()
		);

		// 5) 同じメッシュをもう一度描く（これがアウトライン）
		//
		// 注意：ここでは「BcPNTStaticDraw を持っている」ことを前提にしています。
		//       もし SpPNTStaticDraw を使っている場合は、メッシュ参照方法を合わせてください。
		auto drawComp = GetGameObject()->GetComponent<BcPNTStaticDraw>(false);
		if (!drawComp)
		{
			return;
		}

		auto mesh = drawComp->GetBaseMesh(0);
		if (!mesh)
		{
			return;
		}

		// IA設定（BcPNTStaticDraw と同じ）
		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
		pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
		pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);
	}

} // namespace shooting
