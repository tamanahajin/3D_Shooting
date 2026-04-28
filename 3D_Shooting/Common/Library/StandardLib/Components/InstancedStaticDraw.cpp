#include "stdafx.h"
#include "Project.h"

namespace shooting {

	IMPLEMENT_DX12SHADER(InstancedVSPNTStaticPL, App::GetShadersDir() + L"InstancedVSPNTStaticPL.cso")
		IMPLEMENT_DX12SHADER(InstancedPSPNTPL, App::GetShadersDir() + L"InstancedPSPNTPL.cso")

	InstancedStaticDraw::InstancedStaticDraw(const std::shared_ptr<GameObject>& gameObjectPtr) :
	Component(gameObjectPtr)
	{
	}

	void InstancedStaticDraw::OnCreate()
	{
		//auto pCommandList = BaseScene::Get()->m_pTgtCommandList;
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();

		for (size_t i = 0; i < BaseDevice::FrameCount; i++)
		{
			m_ConstantBufferIndex =
				frameResources[i]->AddBaseConstantBufferSet<BasicConstant>(
				pBaseDevice->GetD3D12Device());
		}

		{
			ComPtr<ID3D12PipelineState> pipeline =
				PipelineStatePool::GetPipelineState(L"InstancedPNTStatic");

			if (!pipeline)
			{
				CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
				rasterizerStateDesc.CullMode = D3D12_CULL_MODE_BACK;

				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				ZeroMemory(&psoDesc, sizeof(psoDesc));

				psoDesc.InputLayout =
				{
					VertexPositionNormalTextureMatrix::GetVertexElement(),
					VertexPositionNormalTextureMatrix::GetNumElements()
				};

				psoDesc.pRootSignature =
					RootSignaturePool::GetRootSignature(L"BaseCrossDefault").Get();

				psoDesc.VS =
				{
					reinterpret_cast<UINT8*>(
						InstancedVSPNTStaticPL::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
					InstancedVSPNTStaticPL::GetPtr()->GetShaderComPtr()->GetBufferSize()
				};

				psoDesc.PS =
				{
					reinterpret_cast<UINT8*>(
						InstancedPSPNTPL::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
					InstancedPSPNTPL::GetPtr()->GetShaderComPtr()->GetBufferSize()
				};

				psoDesc.RasterizerState = rasterizerStateDesc;
				psoDesc.BlendState = BlendState::GetOpaqueBlend();
				psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.SampleDesc.Count = 1;

				ThrowIfFailed(
					App::GetID3D12Device()->CreateGraphicsPipelineState(
					&psoDesc,
					IID_PPV_ARGS(&pipeline)));

				NAME_D3D12_OBJECT(pipeline);
				PipelineStatePool::AddPipelineState(L"InstancedPNTStatic", pipeline);
			}
		}
	}

	void InstancedStaticDraw::BuildInstanceBuffer()
	{
		m_InstanceData.clear();
		m_InstanceBuffer.Reset();
		m_InstanceBufferView = {};

		if (m_InstanceWorlds.empty())
		{
			return;
		}

		m_InstanceData.reserve(m_InstanceWorlds.size());

		for (const auto& world : m_InstanceWorlds)
		{
			StaticInstanceData d{};
			XMStoreFloat4x4(&d.matrix, (XMMATRIX)world);
			m_InstanceData.push_back(d);
		}

		const UINT bufferSize =
			static_cast<UINT>(sizeof(StaticInstanceData) * m_InstanceData.size());
		auto device = App::GetD3D12Device();

		// UPLOAD heap 1本だけで作る
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_InstanceBuffer)));

		// CPU から直接書き込む
		void* mappedPtr = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_InstanceBuffer->Map(0, &readRange, &mappedPtr));
		memcpy(mappedPtr, m_InstanceData.data(), bufferSize);
		m_InstanceBuffer->Unmap(0, nullptr);

		m_InstanceBufferView.BufferLocation = m_InstanceBuffer->GetGPUVirtualAddress();
		m_InstanceBufferView.StrideInBytes = sizeof(StaticInstanceData);
		m_InstanceBufferView.SizeInBytes = bufferSize;
	}

	void InstancedStaticDraw::OnUpdateConstantBuffers()
	{
		auto myCamera = GetGameObject()->GetCamera();
		auto myLightSet = GetGameObject()->GetLightSet();
		if (!myCamera || !myLightSet)
		{
			return;
		}

		m_ConstantBuffer = {};

		// テクスチャ有無
		bool hasTexture = false;
		const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);
		if (!meshes.empty())
		{
			auto material = BaseScene::Get()->GetMaterial(m_MaterialPrefix + std::to_wstring(0));
			if (material && material->GetBaseColorTexture())
			{
				hasTexture = true;
			}
		}

		// 通常描画と合わせる
		m_ConstantBuffer.activeFlg.x = 3;
		m_ConstantBuffer.activeFlg.y = hasTexture ? 1.0f : 0.0f;

		// インスタンス側で world を持つので、ここは identity
		auto world = XMMatrixIdentity();
		auto view = (XMMATRIX)((Mat4x4)myCamera->GetViewMatrix());
		auto proj = (XMMATRIX)((Mat4x4)myCamera->GetProjMatrix());
		auto worldView = world * view;

		m_ConstantBuffer.worldViewProj =
			Mat4x4(XMMatrixTranspose(XMMatrixMultiply(worldView, proj)));

		// Fog
		if (false)
		{
			// 必要ならあとで Fog 対応
		}
		else
		{
			m_ConstantBuffer.fogVector = Vec4(g_XMZero);
			m_ConstantBuffer.fogColor = Vec4(g_XMZero);
		}

		// Light
		for (int i = 0; i < myLightSet->GetNumLights(); i++)
		{
			m_ConstantBuffer.lightDirection[i] = (Vec4)myLightSet->GetLight(i).m_directional;
			m_ConstantBuffer.lightDiffuseColor[i] = (Vec4)myLightSet->GetLight(i).m_diffuseColor;
			m_ConstantBuffer.lightSpecularColor[i] = (Vec4)myLightSet->GetLight(i).m_specularColor;
		}

		// world
		m_ConstantBuffer.world = Mat4x4(world);
		m_ConstantBuffer.world.transpose();

		// world inverse transpose = identity
		XMMATRIX worldInverse = XMMatrixInverse(nullptr, world);
		m_ConstantBuffer.worldInverseTranspose[0] = Vec4(worldInverse.r[0]);
		m_ConstantBuffer.worldInverseTranspose[1] = Vec4(worldInverse.r[1]);
		m_ConstantBuffer.worldInverseTranspose[2] = Vec4(worldInverse.r[2]);

		// eye
		XMMATRIX viewInverse = XMMatrixInverse(nullptr, view);
		m_ConstantBuffer.eyePosition = Vec4(viewInverse.r[3]);

		// material 相当
		Col4 diffuse = Col4(1.0f);
		Col4 alphaVector = (Col4)XMVectorReplicate(1.0f);
		Col4 emissiveColor = Col4(0.0f);
		Col4 ambientLightColor = (Col4)myLightSet->GetAmbient();

		m_ConstantBuffer.emissiveColor =
			(emissiveColor + (ambientLightColor * diffuse)) * alphaVector;
		m_ConstantBuffer.specularColorAndPower = Col4(0, 0, 0, 1);
		m_ConstantBuffer.diffuseColor =
			Col4(XMVectorSelect(alphaVector, diffuse * alphaVector, g_XMSelect1110));

		auto mainLight = myLightSet->GetMainBaseLight();
		Vec3 calcLightDir = Vec3(mainLight.m_directional) * Vec3(-1.0f);

		Vec3 lightAt(myCamera->GetAt());
		Vec3 lightEye(calcLightDir);

		lightEye *= Vec3(ShadowMap::GetLightHeight());
		lightEye += lightAt;

		Vec4 LightEye4 = Vec4(lightEye, 1.0f);
		m_ConstantBuffer.lightPos = LightEye4;

		Vec4 eyePos4 = Vec4((Vec3)myCamera->GetEye(), 1.0f);
		m_ConstantBuffer.eyePos = eyePos4;

		XMMATRIX LightView, LightProj;
		LightView = XMMatrixLookAtLH(
			Vec3(lightEye),
			Vec3(lightAt),
			Vec3(0, 1.0f, 0)
		);
		LightProj = XMMatrixOrthographicLH(
			ShadowMap::GetViewWidth(),
			ShadowMap::GetViewHeight(),
			ShadowMap::GetLightNear(),
			ShadowMap::GetLightFar()
		);

		m_ConstantBuffer.lightView = Mat4x4(XMMatrixTranspose(LightView));
		m_ConstantBuffer.lightProjection = Mat4x4(XMMatrixTranspose(LightProj));
	}

	void InstancedStaticDraw::OnCommitConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto pCurrentFrameResource = scene->GetCurrentFrameResource();

		memcpy(
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			&m_ConstantBuffer,
			sizeof(m_ConstantBuffer));
	}

	void InstancedStaticDraw::OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		if (m_InstanceData.empty() || !m_InstanceBuffer)
		{
			return;
		}


		auto pBaseScene = BaseScene::Get();
		auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();
		auto CbvSrvUavDescriptorHeap = pBaseScene->GetCbvSrvUavDescriptorHeap();

		CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvGpuNullHandle(
			pBaseScene->GetCbvSrvUavDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()
		);

		pCommandList->SetGraphicsRootDescriptorTable(
			pBaseScene->GetGpuSlotID(L"t0"),
			cbvSrvGpuNullHandle
		);

		auto pipeline = PipelineStatePool::GetPipelineState(L"InstancedPNTStatic", true);
		pCommandList->SetPipelineState(pipeline.Get());

		UINT index = pBaseScene->GetSamplerIndex(L"LinearClamp");
		CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle(
			pBaseScene->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
			index,
			pBaseScene->GetSamplerDescriptorHandleIncrementSize());
		pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"s0"), samplerHandle);

		index = pBaseScene->GetSamplerIndex(L"ComparisonLinear");
		CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle2(
			pBaseScene->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
			index,
			pBaseScene->GetSamplerDescriptorHandleIncrementSize());
		pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"s1"), samplerHandle2);

		pCommandList->SetGraphicsRootConstantBufferView(
			pBaseScene->GetGpuSlotID(L"b0"),
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex]
			.m_baseConstantBuffer->GetGPUVirtualAddress());

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			auto mesh = meshes[i];
			if (!mesh)
			{
				continue;
			}

			//auto material = BaseScene::Get()->GetMaterial(m_MaterialPrefix + std::to_wstring(i));
			//auto texture = material ? material->GetBaseColorTexture() : nullptr;
			auto texture = BaseScene::Get()->GetTexture(L"CHARACTER_TEXTURE_SKINNED");

			if (texture)
			{
				CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(
					pBaseScene->GetCbvSrvUavDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
					texture->GetSrvIndex(),
					pBaseScene->GetCbvSrvUavDescriptorHandleIncrementSize());
				pCommandList->SetGraphicsRootDescriptorTable(
					pBaseScene->GetGpuSlotID(L"t1"),
					srvHandle);
			}

			D3D12_VERTEX_BUFFER_VIEW views[2] =
			{
				mesh->GetVertexBufferView(),
				m_InstanceBufferView
			};

			pCommandList->IASetVertexBuffers(0, 2, views);
			pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
			pCommandList->DrawIndexedInstanced(
				mesh->GetNumIndices(),
				static_cast<UINT>(m_InstanceData.size()),
				0, 0, 0);
		}
	}
}