#include "stdafx.h"
#include "Project.h"

namespace shooting {

	IMPLEMENT_DX12SHADER(InstancedVSPNTStaticPL, App::GetShadersDir() + L"InstancedVSPNTStaticPL.cso")
	IMPLEMENT_DX12SHADER(InstancedVSPNTBonePL, App::GetShadersDir() + L"InstancedVSPNTBonePL.cso")
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
		m_InstanceBufferView.SizeInBytes = 0;

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

		if (!m_InstanceBuffer || m_InstanceBufferCapacityBytes < bufferSize)
		{
			m_InstanceBuffer.Reset();
			m_InstanceBufferCapacityBytes = 0;

			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_InstanceBuffer)));

			m_InstanceBufferCapacityBytes = bufferSize;
		}

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

		bool hasTexture = false;
		if (m_UseMaterialTexture)
		{
			const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);
			for (size_t i = 0; i < meshes.size(); ++i)
			{
				auto material = BaseScene::Get()->GetMaterial(m_MaterialPrefix + std::to_wstring(i));
				if (material && material->GetBaseColorTexture())
				{
					hasTexture = true;
					break;
				}
			}
		}

		m_ConstantBuffer.activeFlg.x = m_LightingEnabled ? 3 : 0;
		m_ConstantBuffer.activeFlg.y = hasTexture ? 1 : 0;

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
		Col4 diffuse = m_UseBaseColorOverride ? m_BaseColorOverride : Col4(1.0f);
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
			//auto texture = BaseScene::Get()->GetTexture(L"CHARACTER_TEXTURE_SKINNED");

			auto materialKey = m_MaterialPrefix + std::to_wstring(i);
			auto material = BaseScene::Get()->GetMaterial(materialKey);
			auto texture = (m_UseMaterialTexture && material) ? material->GetBaseColorTexture() : nullptr;
			//auto texture = GetBaseTexture(0);

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
			else
			{
				pCommandList->SetGraphicsRootDescriptorTable(
					pBaseScene->GetGpuSlotID(L"t1"),
					cbvSrvGpuNullHandle);
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
	InstancedSkinnedDraw::InstancedSkinnedDraw(const std::shared_ptr<GameObject>& gameObjectPtr) :
		Component(gameObjectPtr)
	{
	}

	void InstancedSkinnedDraw::OnCreate()
	{
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();

		for (size_t i = 0; i < BaseDevice::FrameCount; i++)
		{
			m_ConstantBufferIndex =
				frameResources[i]->AddBaseConstantBufferSet<BasicConstant>(
				pBaseDevice->GetD3D12Device());
		}

		if (m_BoneSrvIndex == UINT_MAX)
		{
			m_BoneSrvIndex = pBaseScene->GetSrvNextIndex();
		}

		ComPtr<ID3D12PipelineState> pipeline =
			PipelineStatePool::GetPipelineState(L"InstancedPNTBone");

		if (!pipeline)
		{
			CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
			rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;

			std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements(
				VertexPositionNormalTextureSkinning::GetVertexElement(),
				VertexPositionNormalTextureSkinning::GetVertexElement() +
				VertexPositionNormalTextureSkinning::GetNumElements());

			D3D12_INPUT_ELEMENT_DESC instanceElements[] =
			{
				{ "MATRIX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
				{ "MATRIX", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
				{ "MATRIX", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
				{ "MATRIX", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
				{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
			};

			for (const auto& element : instanceElements)
			{
				inputElements.push_back(element);
			}

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			ZeroMemory(&psoDesc, sizeof(psoDesc));

			psoDesc.InputLayout =
			{
				inputElements.data(),
				static_cast<UINT>(inputElements.size())
			};

			psoDesc.pRootSignature =
				RootSignaturePool::GetRootSignature(L"BaseCrossDefault").Get();

			psoDesc.VS =
			{
				reinterpret_cast<UINT8*>(
					InstancedVSPNTBonePL::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				InstancedVSPNTBonePL::GetPtr()->GetShaderComPtr()->GetBufferSize()
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
			PipelineStatePool::AddPipelineState(L"InstancedPNTBone", pipeline);
		}
	}

	void InstancedSkinnedDraw::BuildInstanceBuffer()
	{
		m_InstanceData.clear();
		m_BoneRows.clear();
		m_InstanceBufferView.SizeInBytes = 0;

		if (m_InstanceSources.empty())
		{
			return;
		}

		auto mesh = BaseScene::Get()->GetMesh(m_MeshKey);
		if (!mesh)
		{
			return;
		}

		auto assimp = mesh->GetBaseAssimp();
		if (!assimp)
		{
			return;
		}

		const int animationCount = assimp->GetAnimationCount();
		if (animationCount <= 0)
		{
			return;
		}

		m_InstanceData.reserve(m_InstanceSources.size());

		for (const auto& source : m_InstanceSources)
		{
			unsigned int animationIndex = source.animationIndex;
			if (animationIndex >= static_cast<unsigned int>(animationCount))
			{
				animationIndex = 0;
			}

			m_WorkBones.clear();
			assimp->GetBoneTransforms(
				source.animationTime,
				m_WorkBones,
				animationIndex);

			if (m_WorkBones.empty())
			{
				continue;
			}

			const UINT boneStart = static_cast<UINT>(m_BoneRows.size() / 3);

			SkinnedInstanceData instance{};
			XMStoreFloat4x4(&instance.matrix, (XMMATRIX)source.world);

			float damage = source.damage;
			if (damage < 0.0f)
			{
				damage = 0.0f;
			}
			else if (damage > 1.0f)
			{
				damage = 1.0f;
			}
			instance.params = XMFLOAT4(static_cast<float>(boneStart), damage, 0.0f, 0.0f);
			m_InstanceData.push_back(instance);

			for (const auto& bone : m_WorkBones)
			{
				XMMATRIX matrix = (XMMATRIX)bone;
				XMFLOAT4 row{};

				XMStoreFloat4(&row, matrix.r[0]);
				m_BoneRows.push_back(row);
				XMStoreFloat4(&row, matrix.r[1]);
				m_BoneRows.push_back(row);
				XMStoreFloat4(&row, matrix.r[2]);
				m_BoneRows.push_back(row);
			}
		}

		if (m_InstanceData.empty() || m_BoneRows.empty())
		{
			return;
		}

		auto device = App::GetD3D12Device();

		const UINT instanceBufferSize =
			static_cast<UINT>(sizeof(SkinnedInstanceData) * m_InstanceData.size());

		if (!m_InstanceBuffer || m_InstanceBufferCapacityBytes < instanceBufferSize)
		{
			m_InstanceBuffer.Reset();
			m_InstanceBufferCapacityBytes = 0;

			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(instanceBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_InstanceBuffer)));

			m_InstanceBufferCapacityBytes = instanceBufferSize;
		}

		void* mappedPtr = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_InstanceBuffer->Map(0, &readRange, &mappedPtr));
		memcpy(mappedPtr, m_InstanceData.data(), instanceBufferSize);
		m_InstanceBuffer->Unmap(0, nullptr);

		m_InstanceBufferView.BufferLocation = m_InstanceBuffer->GetGPUVirtualAddress();
		m_InstanceBufferView.StrideInBytes = sizeof(SkinnedInstanceData);
		m_InstanceBufferView.SizeInBytes = instanceBufferSize;

		const UINT boneBufferSize =
			static_cast<UINT>(sizeof(XMFLOAT4) * m_BoneRows.size());

		if (!m_BoneBuffer || m_BoneBufferCapacityBytes < boneBufferSize)
		{
			m_BoneBuffer.Reset();
			m_BoneBufferCapacityBytes = 0;

			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(boneBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_BoneBuffer)));

			m_BoneBufferCapacityBytes = boneBufferSize;

			if (m_BoneSrvIndex == UINT_MAX)
			{
				m_BoneSrvIndex = BaseScene::Get()->GetSrvNextIndex();
			}

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.NumElements = m_BoneBufferCapacityBytes / sizeof(XMFLOAT4);
			srvDesc.Buffer.StructureByteStride = sizeof(XMFLOAT4);
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuHandle(
				BaseScene::Get()->GetCbvSrvUavDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
				m_BoneSrvIndex,
				BaseScene::Get()->GetCbvSrvUavDescriptorHandleIncrementSize());
			device->CreateShaderResourceView(m_BoneBuffer.Get(), &srvDesc, srvCpuHandle);
		}

		mappedPtr = nullptr;
		ThrowIfFailed(m_BoneBuffer->Map(0, &readRange, &mappedPtr));
		memcpy(mappedPtr, m_BoneRows.data(), boneBufferSize);
		m_BoneBuffer->Unmap(0, nullptr);
	}

	void InstancedSkinnedDraw::OnUpdateConstantBuffers()
	{
		auto myCamera = GetGameObject()->GetCamera();
		auto myLightSet = GetGameObject()->GetLightSet();
		if (!myCamera || !myLightSet)
		{
			return;
		}

		m_ConstantBuffer = {};

		std::shared_ptr<BaseTexture> texture;
		if (!m_TextureKey.empty())
		{
			texture = BaseScene::Get()->GetTexture(m_TextureKey);
		}

		m_ConstantBuffer.activeFlg.x = 3;
		m_ConstantBuffer.activeFlg.y = texture ? 1 : 0;

		auto world = XMMatrixIdentity();
		auto view = (XMMATRIX)((Mat4x4)myCamera->GetViewMatrix());
		auto proj = (XMMATRIX)((Mat4x4)myCamera->GetProjMatrix());
		auto worldView = world * view;

		m_ConstantBuffer.worldViewProj =
			Mat4x4(XMMatrixTranspose(XMMatrixMultiply(worldView, proj)));

		m_ConstantBuffer.fogVector = Vec4(g_XMZero);
		m_ConstantBuffer.fogColor = Vec4(g_XMZero);

		for (int i = 0; i < myLightSet->GetNumLights(); i++)
		{
			m_ConstantBuffer.lightDirection[i] = (Vec4)myLightSet->GetLight(i).m_directional;
			m_ConstantBuffer.lightDiffuseColor[i] = (Vec4)myLightSet->GetLight(i).m_diffuseColor;
			m_ConstantBuffer.lightSpecularColor[i] = (Vec4)myLightSet->GetLight(i).m_specularColor;
		}

		m_ConstantBuffer.world = Mat4x4(world);
		m_ConstantBuffer.world.transpose();

		XMMATRIX worldInverse = XMMatrixInverse(nullptr, world);
		m_ConstantBuffer.worldInverseTranspose[0] = Vec4(worldInverse.r[0]);
		m_ConstantBuffer.worldInverseTranspose[1] = Vec4(worldInverse.r[1]);
		m_ConstantBuffer.worldInverseTranspose[2] = Vec4(worldInverse.r[2]);

		XMMATRIX viewInverse = XMMatrixInverse(nullptr, view);
		m_ConstantBuffer.eyePosition = Vec4(viewInverse.r[3]);

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

		m_ConstantBuffer.lightPos = Vec4(lightEye, 1.0f);
		m_ConstantBuffer.eyePos = Vec4((Vec3)myCamera->GetEye(), 1.0f);

		XMMATRIX LightView = XMMatrixLookAtLH(
			Vec3(lightEye),
			Vec3(lightAt),
			Vec3(0, 1.0f, 0)
		);
		XMMATRIX LightProj = XMMatrixOrthographicLH(
			ShadowMap::GetViewWidth(),
			ShadowMap::GetViewHeight(),
			ShadowMap::GetLightNear(),
			ShadowMap::GetLightFar()
		);

		m_ConstantBuffer.lightView = Mat4x4(XMMatrixTranspose(LightView));
		m_ConstantBuffer.lightProjection = Mat4x4(XMMatrixTranspose(LightProj));
	}

	void InstancedSkinnedDraw::OnCommitConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto pCurrentFrameResource = scene->GetCurrentFrameResource();

		memcpy(
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			&m_ConstantBuffer,
			sizeof(m_ConstantBuffer));
	}

	void InstancedSkinnedDraw::OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		if (m_InstanceData.empty() || !m_InstanceBuffer || !m_BoneBuffer)
		{
			return;
		}

		auto pBaseScene = BaseScene::Get();
		auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();

		CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvGpuNullHandle(
			pBaseScene->GetCbvSrvUavDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()
		);

		pCommandList->SetGraphicsRootDescriptorTable(
			pBaseScene->GetGpuSlotID(L"t0"),
			cbvSrvGpuNullHandle
		);

		auto pipeline = PipelineStatePool::GetPipelineState(L"InstancedPNTBone", true);
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

		CD3DX12_GPU_DESCRIPTOR_HANDLE boneSrvHandle(
			pBaseScene->GetCbvSrvUavDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
			m_BoneSrvIndex,
			pBaseScene->GetCbvSrvUavDescriptorHandleIncrementSize());
		pCommandList->SetGraphicsRootDescriptorTable(
			pBaseScene->GetGpuSlotID(L"t2"),
			boneSrvHandle);

		std::shared_ptr<BaseTexture> texture;
		if (!m_TextureKey.empty())
		{
			texture = pBaseScene->GetTexture(m_TextureKey);
		}

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
		else
		{
			pCommandList->SetGraphicsRootDescriptorTable(
				pBaseScene->GetGpuSlotID(L"t1"),
				cbvSrvGpuNullHandle);
		}

		auto mesh = pBaseScene->GetMesh(m_MeshKey);
		if (!mesh)
		{
			return;
		}

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
