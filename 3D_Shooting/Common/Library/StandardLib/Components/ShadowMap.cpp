/*!
@file ShadowmapComp.cpp
@brief シャドウマップコンポーネント　実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	float ShadowMap::m_lightHeight(200.0f);
	float ShadowMap::m_lightNear(0.1f);
	float ShadowMap::m_lightFar(220.0f);
	float ShadowMap::m_viewWidth(32.0f);
	float ShadowMap::m_viewHeight(32.0f);
	float ShadowMap::m_posAdjustment(0.1f);


	//--------------------------------------------------------------------------------------
	///	ShadowMap
	//--------------------------------------------------------------------------------------
	IMPLEMENT_DX12SHADER(PNTShadowMap, App::GetShadersDir() + L"VSShadowmap.cso")

		ShadowMap::ShadowMap(const std::shared_ptr<GameObject>& gameObjectPtr) :
		Component(gameObjectPtr)
	{
	}

	void ShadowMap::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();
		//シャドウマップのコンスタントバッファ
		for (size_t i = 0; i < BaseDevice::FrameCount; i++)
		{
			m_shadowConstantBufferIndex =
				frameResources[i]->AddBaseConstantBufferSet<ShadowConstantBuffer>(pBaseDevice->GetD3D12Device());
		}
		// シャドウマップパイプラインステート
		{
			ComPtr<ID3D12PipelineState> PNTShadowMapPipelineState
				= PipelineStatePool::GetPipelineState(L"PNTShadowMap");
			auto rootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);

			// シャドウマップ用
			CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
			depthStencilDesc.DepthEnable = TRUE;
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			depthStencilDesc.StencilEnable = FALSE;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout = { VertexPositionNormalTexture::GetVertexElement(), VertexPositionNormalTexture::GetNumElements() };;
			psoDesc.pRootSignature = rootSignature.Get();
			psoDesc.VS =
			{
				reinterpret_cast<UINT8*>(PNTShadowMap::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				PNTShadowMap::GetPtr()->GetShaderComPtr()->GetBufferSize()

			};
			psoDesc.PS =
			{
				CD3DX12_SHADER_BYTECODE(0, 0)
			};
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState = depthStencilDesc;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 0;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
			psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleDesc.Count = 1;
			if (!PNTShadowMapPipelineState)
			{
				ThrowIfFailed(pBaseDevice->GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PNTShadowMapPipelineState)));
				NAME_D3D12_OBJECT(PNTShadowMapPipelineState);
				PipelineStatePool::AddPipelineState(L"PNTShadowMap", PNTShadowMapPipelineState);
			}
		}

	}

	void ShadowMap::OnUpdateConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto stage = std::dynamic_pointer_cast<Stage>(scene->GetActiveStage(true));
		auto& viewport = scene->GetViewport();
		std::shared_ptr<PerspecCamera> myCamera;
		std::shared_ptr<LightSet> myLightSet;
		if (!stage)
		{
			return;
		}

		auto gameObject = m_gameObject.lock();
		if (gameObject)
		{

			myCamera = std::dynamic_pointer_cast<PerspecCamera>(gameObject->GetCamera());
			myLightSet = gameObject->GetLightSet();

			//Transformコンポーネントを取り出す
			auto ptrTrans = gameObject->GetComponent<Transform>();
			auto& param = ptrTrans->GetTransParam();

			//シャドウマップのコンスタントバッファ
			{
				m_shadowConstantBuffer = {};
				Mat4x4 world;
				auto lights = myLightSet;
				auto light = lights->GetMainBaseLight();
				//位置の取得
				auto pos = ptrTrans->GetWorldMatrix().transInMatrix();
				Vec3 posSpan = light.m_directional;
				posSpan *= m_posAdjustment;
				pos += posSpan;
				//ワールド行列の決定
				world.affineTransformation(
					ptrTrans->GetScale(),			//スケーリング
					ptrTrans->GetRotateOrigin(),		//回転の中心（重心）
					ptrTrans->GetQuaternion(),				//回転角度
					pos				//位置
				);
				Vec3 lightDir = -light.m_directional;
				auto camera = myCamera;
				Vec3 lightAt = camera->GetAt();
				Vec3 lightEye = lightAt + (lightDir * m_lightHeight);

				auto width = viewport.Width;
				auto height = viewport.Height;

				Mat4x4 lightView = XMMatrixLookAtLH(lightEye, lightAt, Vec3(0, 1.0f, 0));
				Mat4x4 lightProj = XMMatrixOrthographicLH(m_viewWidth, m_viewHeight, m_lightNear, m_lightFar);

				m_shadowConstantBuffer.world = world.transpose();
				m_shadowConstantBuffer.view = lightView.transpose();
				m_shadowConstantBuffer.projection = lightProj.transpose();
			}
		}
	}

	void ShadowMap::OnCommitConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto pCurrentFrameResource = scene->GetCurrentFrameResource();
		//シャドウマップ
		memcpy(pCurrentFrameResource->m_baseConstantBufferSetVec[m_shadowConstantBufferIndex].m_pBaseConstantBufferWO,
			   &m_shadowConstantBuffer, sizeof(m_shadowConstantBuffer));
	}

	void ShadowMap::OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		//		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		auto mesh = GetBaseMesh(0);
		if (mesh)
		{
			auto pBaseScene = BaseScene::Get();
			auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();
			//Cbv
			// Set shadow constant buffer.
			pCommandList->SetGraphicsRootConstantBufferView(pBaseScene->GetGpuSlotID(L"b0"),
															pCurrentFrameResource->m_baseConstantBufferSetVec[m_shadowConstantBufferIndex].m_baseConstantBuffer->GetGPUVirtualAddress());
			// Draw
			pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
			pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
			pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);
		}
	}

}
