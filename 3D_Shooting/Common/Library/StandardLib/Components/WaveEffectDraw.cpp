#include "stdafx.h"
#include "WaveEffectDraw.h"

namespace shooting {

	DECLARE_DX12SHADER(VSWaveEffectDraw)
	DECLARE_DX12SHADER(PSWaveEffectDraw)

	IMPLEMENT_DX12SHADER(VSWaveEffectDraw, App::GetShadersDir() + L"VSWaveEffectDraw.cso")
	IMPLEMENT_DX12SHADER(PSWaveEffectDraw, App::GetShadersDir() + L"PSWaveEffectDraw.cso")

	WaveEffectDraw::WaveEffectDraw(const std::shared_ptr<GameObject>& gameObjectPtr)
		: Component(gameObjectPtr)
	{
	}

	void WaveEffectDraw::SetColor(const Col4& color)
	{
		m_Color = color;
	}

	void WaveEffectDraw::SetWaveTime(float time)
	{
		m_WaveTime = time;
	}

	void WaveEffectDraw::SetWave(float amplitude, float frequency, float speed)
	{
		m_WaveAmplitude = amplitude;
		m_WaveFrequency = frequency;
		m_WaveSpeed = speed;
	}

	void WaveEffectDraw::SetWaveDirection(const Vec2& direction)
	{
		m_WaveDirection = direction;
	}

	void WaveEffectDraw::SetEdgeMask(float edgeStart, float edgeEnd)
	{
		m_WaveEdgeStart = edgeStart;
		m_WaveEdgeEnd = edgeEnd;
	}

	void WaveEffectDraw::SetShakeAxis(const Vec3& axis)
	{
		m_WaveShakeAxis = axis;
	}

	void WaveEffectDraw::OnCreate()
	{
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();

		for (size_t i = 0; i < BaseDevice::FrameCount; ++i)
		{
			m_ConstantBufferIndex =
				frameResources[i]->AddBaseConstantBufferSet<WaveEffectDrawConstant>(pBaseDevice->GetD3D12Device());
		}

		auto pipelineState = PipelineStatePool::GetPipelineState(L"WaveEffectDraw");
		if (pipelineState)
		{
			return;
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { VertexPositionNormalTexture::GetVertexElement(), VertexPositionNormalTexture::GetNumElements() };
		psoDesc.pRootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault").Get();
		psoDesc.VS =
		{
			reinterpret_cast<UINT8*>(VSWaveEffectDraw::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
			VSWaveEffectDraw::GetPtr()->GetShaderComPtr()->GetBufferSize()
		};
		psoDesc.PS =
		{
			reinterpret_cast<UINT8*>(PSWaveEffectDraw::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
			PSWaveEffectDraw::GetPtr()->GetShaderComPtr()->GetBufferSize()
		};

		CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.RasterizerState = rasterizerStateDesc;
		psoDesc.BlendState = BlendState::GetAlphaBlendEx();

		CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
		// 半透明エフェクトは深度テストだけ行い、深度値は書き込まない。
		// 書き込むと、透明部分が後続描画を隠してしまう可能性がある。
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState = depthStencilDesc;

		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
		NAME_D3D12_OBJECT(pipelineState);
		PipelineStatePool::AddPipelineState(L"WaveEffectDraw", pipelineState);
	}

	void WaveEffectDraw::OnUpdateConstantBuffers()
	{
		auto gameObject = m_gameObject.lock();
		if (!gameObject)
		{
			return;
		}

		auto transform = gameObject->GetComponent<Transform>(false);
		auto camera = std::dynamic_pointer_cast<PerspecCamera>(gameObject->GetCamera());
		if (!transform || !camera)
		{
			return;
		}

		Mat4x4 world = transform->GetWorldMatrix();
		Mat4x4 view = camera->GetViewMatrix();
		Mat4x4 projection = camera->GetProjMatrix();
		const auto worldView = XMMatrixMultiply((XMMATRIX)world, (XMMATRIX)view);

		m_ConstantBuffer = {};
		m_ConstantBuffer.worldViewProjection =
			Mat4x4(XMMatrixTranspose(XMMatrixMultiply(worldView, (XMMATRIX)projection)));
		m_ConstantBuffer.color = m_Color;

		// b0だけで完結させることで、既存RootSignatureのb1用descriptor tableを使わずに済ませる。
		// シェーダー側のWaveEffectは関数として分離しているため、描画クラスだけを差し替えても使い回せる。
		m_ConstantBuffer.waveTimeAmplitudeFrequencySpeed =
			Vec4(m_WaveTime, m_WaveAmplitude, m_WaveFrequency, m_WaveSpeed);
		m_ConstantBuffer.waveDirectionEdgeStartEnd =
			Vec4(m_WaveDirection.x, m_WaveDirection.y, m_WaveEdgeStart, m_WaveEdgeEnd);
		m_ConstantBuffer.waveShakeAxis =
			Vec4(m_WaveShakeAxis.x, m_WaveShakeAxis.y, m_WaveShakeAxis.z, 0.0f);
	}

	void WaveEffectDraw::OnCommitConstantBuffers()
	{
		auto pCurrentFrameResource = BaseScene::Get()->GetCurrentFrameResource();
		memcpy(
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			&m_ConstantBuffer,
			sizeof(m_ConstantBuffer));
	}

	void WaveEffectDraw::OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		if (GetBaseModelMeshCount() == 0)
		{
			return;
		}

		auto mesh = GetBaseMesh(0);
		if (!mesh)
		{
			return;
		}

		auto pBaseScene = BaseScene::Get();
		auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();
		auto pipelineState = PipelineStatePool::GetPipelineState(L"WaveEffectDraw", true);

		pCommandList->SetPipelineState(pipelineState.Get());
		pCommandList->SetGraphicsRootConstantBufferView(
			pBaseScene->GetGpuSlotID(L"b0"),
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex]
			.m_baseConstantBuffer->GetGPUVirtualAddress());

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
		pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
		pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);
	}
}
