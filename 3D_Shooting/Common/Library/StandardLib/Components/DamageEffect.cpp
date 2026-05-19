/*!
@file DamageEffectComponent.cpp
@brief ダメージエフェクトコンポーネント 実体（モデル全体を赤くする）
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	IMPLEMENT_DX12SHADER(VSDamageEffect, App::GetShadersDir() + L"VSDamageEffect.cso")
	IMPLEMENT_DX12SHADER(VSDamageEffectSkinning, App::GetShadersDir() + L"VSDamageEffectSkinning.cso")
	IMPLEMENT_DX12SHADER(PSDamageEffect, App::GetShadersDir() + L"PSDamageEffect.cso")

	DamageEffect::DamageEffect(const std::shared_ptr<GameObject>& gameObjectPtr)
	: Component(gameObjectPtr)
	{
	}

	DamageEffect::~DamageEffect() = default;

	void DamageEffect::OnCreate()
	{
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();

		for (size_t i = 0; i < BaseDevice::FrameCount; i++)
		{
			m_ConstantBufferIndex =
				frameResources[i]->AddBaseConstantBufferSet<DamageEffectConstantBuffer>(
				pBaseDevice->GetD3D12Device()
				);
		}

		EnsureDamageEffectPipelineState();
	}

	void DamageEffect::OnUpdate(double elapsedTime)
	{
		if (!m_IsEffectActive)
		{
			return;
		}

		m_EffectTimer -= elapsedTime;
		if (m_EffectTimer <= 0.0)
		{
			StopEffect();
		}
	}

	void DamageEffect::StartEffect(float duration)
	{
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
		auto rootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);

		CD3DX12_RASTERIZER_DESC rasterizerState(D3D12_DEFAULT);
		rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerState.DepthBias = -100;
		rasterizerState.DepthBiasClamp = 0.0f;
		rasterizerState.SlopeScaledDepthBias = -1.0f;

		CD3DX12_DEPTH_STENCIL_DESC depthStencil(D3D12_DEFAULT);
		depthStencil.DepthEnable = TRUE;
		depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		// -----------------------------
		// static 用
		// -----------------------------
		{
			ComPtr<ID3D12PipelineState> pso =
				PipelineStatePool::GetPipelineState(kDamageEffectStaticPSOKey);

			if (!pso)
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				ZeroMemory(&psoDesc, sizeof(psoDesc));

				psoDesc.InputLayout =
				{
					VertexPositionNormalTexture::GetVertexElement(),
					VertexPositionNormalTexture::GetNumElements()
				};

				psoDesc.pRootSignature = rootSignature.Get();
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
				psoDesc.BlendState = BlendState::GetAlphaBlendEx();
				psoDesc.DepthStencilState = depthStencil;
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.SampleDesc.Count = 1;

				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
				NAME_D3D12_OBJECT(pso);
				PipelineStatePool::AddPipelineState(kDamageEffectStaticPSOKey, pso);
			}
		}

		// -----------------------------
		// skinning 用
		// -----------------------------
		{
			ComPtr<ID3D12PipelineState> pso =
				PipelineStatePool::GetPipelineState(kDamageEffectSkinningPSOKey);

			if (!pso)
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				ZeroMemory(&psoDesc, sizeof(psoDesc));

				psoDesc.InputLayout =
				{
					VertexPositionNormalTextureSkinning::GetVertexElement(),
					VertexPositionNormalTextureSkinning::GetNumElements()
				};

				psoDesc.pRootSignature = rootSignature.Get();
				psoDesc.VS =
				{
					reinterpret_cast<UINT8*>(VSDamageEffectSkinning::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
					VSDamageEffectSkinning::GetPtr()->GetShaderComPtr()->GetBufferSize()
				};
				psoDesc.PS =
				{
					reinterpret_cast<UINT8*>(PSDamageEffect::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
					PSDamageEffect::GetPtr()->GetShaderComPtr()->GetBufferSize()
				};

				psoDesc.RasterizerState = rasterizerState;
				psoDesc.BlendState = BlendState::GetAlphaBlendEx();
				psoDesc.DepthStencilState = depthStencil;
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.SampleDesc.Count = 1;

				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
				NAME_D3D12_OBJECT(pso);
				PipelineStatePool::AddPipelineState(kDamageEffectSkinningPSOKey, pso);
			}
		}
	}

	void DamageEffect::UpdateConstantBuffer()
	{
		m_ConstantBuffer = {};
		m_UseSkinning = false;

		auto gameObject = GetGameObject();
		auto transform = gameObject->GetComponent<Transform>();
		if (!transform)
		{
			return;
		}

		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		if (!scene)
		{
			return;
		}

		auto stage = std::dynamic_pointer_cast<Stage>(scene->GetActiveStage(true));
		if (!stage)
		{
			return;
		}

		// -----------------------------------------
		// world 行列
		// enemy は BcPNTBoneDraw 側で model offset を足して描いているので、
		// ここでも同じ offset を反映する
		// -----------------------------------------
		auto boneDraw = gameObject->GetComponent<BcPNTBoneDraw>(false);

		auto& param = transform->GetTransParam();
		Vec3 drawPos = param.position;
		if (boneDraw)
		{
			drawPos += boneDraw->GetModelOffset();
		}

		XMMATRIX world = XMMatrixAffineTransformation(
			param.scale,
			param.rotateOrigin,
			param.quaternion,
			drawPos
		);
		XMStoreFloat4x4(&m_ConstantBuffer.World, XMMatrixTranspose(world));

		// -----------------------------------------
		// view-proj
		// -----------------------------------------
		auto camera = stage->GetCamera();
		if (camera)
		{
			XMMATRIX view = (XMMATRIX)((Mat4x4)camera->GetViewMatrix());
			XMMATRIX proj = (XMMATRIX)((Mat4x4)camera->GetProjMatrix());
			XMMATRIX viewProj = view * proj;
			XMStoreFloat4x4(&m_ConstantBuffer.ViewProj, XMMatrixTranspose(viewProj));
		}

		// -----------------------------------------
		// damage 値
		// -----------------------------------------
		float denom = (m_EffectDuration <= 0.0f) ? 0.001f : m_EffectDuration;
		float damageValue = static_cast<float>(m_EffectTimer / denom);
		damageValue = bsmUtil::Max(0.0f, std::min(1.0f, damageValue));

		m_ConstantBuffer.OutlineWidth = m_OutlineWidth;
		m_ConstantBuffer.Damage = damageValue;
		m_ConstantBuffer.Pad = XMFLOAT2(0, 0);

		// -----------------------------------------
		// bone 行列
		// -----------------------------------------
		if (boneDraw)
		{
			const auto& bones = boneDraw->GetBoneTransforms();
			if (!bones.empty())
			{
				m_UseSkinning = true;

				UINT cb_count = 0;
				for (size_t i = 0; i < bones.size() && (cb_count + 2) < (3 * MAX_BONES); ++i)
				{
					const Mat4x4& mat = bones[i];
					m_ConstantBuffer.Bones[cb_count] = ((XMMATRIX)mat).r[0];
					m_ConstantBuffer.Bones[cb_count + 1] = ((XMMATRIX)mat).r[1];
					m_ConstantBuffer.Bones[cb_count + 2] = ((XMMATRIX)mat).r[2];
					cb_count += 3;
				}
			}
		}
	}

	void DamageEffect::OnDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		if (!m_IsEffectActive)
		{
			return;
		}

		auto gameObject = GetGameObject();
		auto boneDraw = gameObject->GetComponent<BcPNTBoneDraw>(false);
		auto staticDraw = gameObject->GetComponent<BcPNTStaticDraw>(false);

		if (!boneDraw && !staticDraw)
		{
			return;
		}

		auto pBaseScene = BaseScene::Get();
		auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();

		UpdateConstantBuffer();

		memcpy(
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			&m_ConstantBuffer,
			sizeof(m_ConstantBuffer)
		);

		const wchar_t* psoKey = m_UseSkinning
			? kDamageEffectSkinningPSOKey
			: kDamageEffectStaticPSOKey;

		ComPtr<ID3D12PipelineState> damageEffectPSO =
			PipelineStatePool::GetPipelineState(psoKey, true);
		pCommandList->SetPipelineState(damageEffectPSO.Get());

		ComPtr<ID3D12RootSignature> rs =
			RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);
		pCommandList->SetGraphicsRootSignature(rs.Get());

		pCommandList->SetGraphicsRootConstantBufferView(
			pBaseScene->GetGpuSlotID(L"b0"),
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex]
			.m_baseConstantBuffer->GetGPUVirtualAddress()
		);

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// -----------------------------------------
		// skinning モデル
		// -----------------------------------------
		if (boneDraw)
		{
			auto mesh = boneDraw->GetBaseMesh(0);
			if (!mesh)
			{
				return;
			}

			pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
			pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
			pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);
			return;
		}

		// -----------------------------------------
		// static モデル
		// -----------------------------------------
		if (staticDraw)
		{
			const size_t meshCount = staticDraw->GetBaseModelMeshCount();

			if (meshCount > 0)
			{
				for (size_t i = 0; i < meshCount; ++i)
				{
					auto mesh = staticDraw->GetBaseModelMesh(i);
					if (!mesh)
					{
						continue;
					}

					pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
					pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
					pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);
				}
			}
			else
			{
				auto mesh = staticDraw->GetBaseMesh(0);
				if (!mesh)
				{
					return;
				}

				pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
				pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
				pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);
			}
		}
	}

} // namespace shooting