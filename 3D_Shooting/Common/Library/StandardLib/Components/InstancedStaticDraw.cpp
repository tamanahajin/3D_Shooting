#include "stdafx.h"
#include "Common/Library/BasicLib/BenchmarkRecorder.h"
#include "Project.h"

namespace shooting {

	IMPLEMENT_DX12SHADER(InstancedVSPNTStaticPL, App::GetShadersDir() + L"InstancedVSPNTStaticPL.cso")
	IMPLEMENT_DX12SHADER(InstancedVSPNTBonePL, App::GetShadersDir() + L"InstancedVSPNTBonePL.cso")
		IMPLEMENT_DX12SHADER(InstancedPSPNTPL, App::GetShadersDir() + L"InstancedPSPNTPL.cso")
	IMPLEMENT_DX12SHADER(InstancedVSShadowmap, App::GetShadersDir() + L"InstancedVSShadowmap.cso")

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
		{
			ComPtr<ID3D12PipelineState> pipeline =
				PipelineStatePool::GetPipelineState(L"InstancedPNTShadowMap");

			if (!pipeline)
			{
				auto rootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault", true);

				CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
				depthStencilDesc.DepthEnable = TRUE;
				depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
				depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
				depthStencilDesc.StencilEnable = FALSE;

				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				ZeroMemory(&psoDesc, sizeof(psoDesc));
				psoDesc.InputLayout =
				{
					VertexPositionNormalTextureMatrix::GetVertexElement(),
					VertexPositionNormalTextureMatrix::GetNumElements()
				};
				psoDesc.pRootSignature = rootSignature.Get();
				psoDesc.VS =
				{
					reinterpret_cast<UINT8*>(
						InstancedVSShadowmap::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
					InstancedVSShadowmap::GetPtr()->GetShaderComPtr()->GetBufferSize()
				};
				psoDesc.PS = { CD3DX12_SHADER_BYTECODE(0, 0) };
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				psoDesc.DepthStencilState = depthStencilDesc;
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 0;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				psoDesc.SampleDesc.Count = 1;

				ThrowIfFailed(
					App::GetID3D12Device()->CreateGraphicsPipelineState(
						&psoDesc,
						IID_PPV_ARGS(&pipeline)));

				NAME_D3D12_OBJECT(pipeline);
				PipelineStatePool::AddPipelineState(L"InstancedPNTShadowMap", pipeline);
			}
		}
	}

	void InstancedStaticDraw::BuildStaticInstanceBuffer(
		const std::vector<Mat4x4>& worlds,
		std::vector<StaticInstanceData>& instanceData,
		ComPtr<ID3D12Resource>& instanceBuffer,
		UINT& instanceBufferCapacityBytes,
		D3D12_VERTEX_BUFFER_VIEW& instanceBufferView)
	{
		instanceData.clear();
		instanceBufferView = {};

		if (worlds.empty())
		{
			return;
		}

		instanceData.reserve(worlds.size());

		for (const auto& world : worlds)
		{
			StaticInstanceData d{};
			XMStoreFloat4x4(&d.matrix, (XMMATRIX)world);
			instanceData.push_back(d);
		}

		const UINT bufferSize =
			static_cast<UINT>(sizeof(StaticInstanceData) * instanceData.size());
		auto device = App::GetD3D12Device();

		if (!instanceBuffer || instanceBufferCapacityBytes < bufferSize)
		{
			instanceBuffer.Reset();
			instanceBufferCapacityBytes = 0;

			ThrowIfFailed(device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&instanceBuffer)));

			instanceBufferCapacityBytes = bufferSize;
		}

		void* mappedPtr = nullptr;
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(instanceBuffer->Map(0, &readRange, &mappedPtr));
		memcpy(mappedPtr, instanceData.data(), bufferSize);
		instanceBuffer->Unmap(0, nullptr);

		instanceBufferView.BufferLocation = instanceBuffer->GetGPUVirtualAddress();
		instanceBufferView.StrideInBytes = sizeof(StaticInstanceData);
		instanceBufferView.SizeInBytes = bufferSize;
	}

	void InstancedStaticDraw::BuildInstanceBuffer()
	{
		BuildStaticInstanceBuffer(
			m_InstanceWorlds,
			m_InstanceData,
			m_InstanceBuffer,
			m_InstanceBufferCapacityBytes,
			m_InstanceBufferView);
	}

	void InstancedStaticDraw::BuildShadowInstanceBuffer()
	{
		BuildStaticInstanceBuffer(
			m_ShadowInstanceWorlds,
			m_ShadowInstanceData,
			m_ShadowInstanceBuffer,
			m_ShadowInstanceBufferCapacityBytes,
			m_ShadowInstanceBufferView);
	}

	void InstancedStaticDraw::EnsureMaterialConstantBuffers(size_t materialCount)
	{
		if (m_MaterialConstantBufferIndices.size() == materialCount)
		{
			return;
		}

		m_MaterialConstantBufferIndices.clear();
		m_MaterialConstantBuffers.clear();

		if (materialCount == 0)
		{
			return;
		}

		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto& frameResources = scene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();

		m_MaterialConstantBuffers.resize(materialCount);
		for (size_t materialIndex = 0; materialIndex < materialCount; ++materialIndex)
		{
			size_t constantBufferIndex = 0;
			for (size_t frameIndex = 0; frameIndex < BaseDevice::FrameCount; ++frameIndex)
			{
				constantBufferIndex =
					frameResources[frameIndex]->AddBaseConstantBufferSet<BasicConstant>(
						pBaseDevice->GetD3D12Device());
			}
			m_MaterialConstantBufferIndices.push_back(constantBufferIndex);
		}
	}

	void InstancedStaticDraw::OnUpdateConstantBuffers()
	{
		auto myCamera = GetGameObject()->GetCamera();
		auto myLightSet = GetGameObject()->GetLightSet();
		if (!myCamera || !myLightSet)
		{
			return;
		}

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);
		EnsureMaterialConstantBuffers(meshes.size());

		BasicConstant baseConstant{};
		baseConstant.activeFlg.x = m_LightingEnabled ? 3 : 0;
		baseConstant.activeFlg.z = m_OwnShadowActive ? 1 : 0;

		// インスタンス側で world を持つので、ここは identity
		auto world = XMMatrixIdentity();
		auto view = (XMMATRIX)((Mat4x4)myCamera->GetViewMatrix());
		auto proj = (XMMATRIX)((Mat4x4)myCamera->GetProjMatrix());
		auto worldView = world * view;

		baseConstant.worldViewProj =
			Mat4x4(XMMatrixTranspose(XMMatrixMultiply(worldView, proj)));

		if (m_FogEnabled && BaseScene::Get()->IsFogEnabled())
		{
			const float start = m_FogStart;
			const float end = m_FogEnd;
			if (start == end)
			{
				// 開始距離と終了距離が同じ場合は計算が不安定になるため、全体をフォグ済みとして扱う。
				static const XMVECTORF32 fullyFogged = { 0, 0, 0, 1 };
				baseConstant.fogVector = Vec4(fullyFogged);
			}
			else
			{
				// インスタンス描画では各頂点がシェーダ内で world 座標へ変換される。
				// そのため world は identity のまま view のZ成分だけを使い、worldPosとの内積でフォグ量を出す。
				XMVECTOR viewZ = XMVectorMergeXY(
					XMVectorMergeZW(view.r[0], view.r[2]),
					XMVectorMergeZW(view.r[1], view.r[3]));
				XMVECTOR wOffset = XMVectorSwizzle<1, 2, 3, 0>(XMLoadFloat(&start));
				baseConstant.fogVector = Vec4((viewZ + wOffset) / (start - end));
			}
			baseConstant.fogColor = (Col4)m_FogColor;
		}
		else
		{
			baseConstant.fogVector = Vec4(g_XMZero);
			baseConstant.fogColor = Vec4(g_XMZero);
		}

		for (int i = 0; i < myLightSet->GetNumLights(); i++)
		{
			baseConstant.lightDirection[i] = (Vec4)myLightSet->GetLight(i).m_directional;
			baseConstant.lightDiffuseColor[i] = (Vec4)myLightSet->GetLight(i).m_diffuseColor;
			baseConstant.lightSpecularColor[i] = (Vec4)myLightSet->GetLight(i).m_specularColor;
		}

		baseConstant.world = Mat4x4(world);
		baseConstant.world.transpose();

		XMMATRIX worldInverse = XMMatrixInverse(nullptr, world);
		baseConstant.worldInverseTranspose[0] = Vec4(worldInverse.r[0]);
		baseConstant.worldInverseTranspose[1] = Vec4(worldInverse.r[1]);
		baseConstant.worldInverseTranspose[2] = Vec4(worldInverse.r[2]);

		XMMATRIX viewInverse = XMMatrixInverse(nullptr, view);
		baseConstant.eyePosition = Vec4(viewInverse.r[3]);

		auto mainLight = myLightSet->GetMainBaseLight();
		Vec3 calcLightDir = Vec3(mainLight.m_directional) * Vec3(-1.0f);

		Vec3 lightAt(myCamera->GetAt());
		Vec3 lightEye(calcLightDir);

		lightEye *= Vec3(ShadowMap::GetLightHeight());
		lightEye += lightAt;

		baseConstant.lightPos = Vec4(lightEye, 1.0f);
		baseConstant.eyePos = Vec4((Vec3)myCamera->GetEye(), 1.0f);

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

		baseConstant.lightView = Mat4x4(XMMatrixTranspose(LightView));
		baseConstant.lightProjection = Mat4x4(XMMatrixTranspose(LightProj));

		for (size_t materialIndex = 0; materialIndex < meshes.size(); ++materialIndex)
		{
			auto constant = baseConstant;

			std::shared_ptr<BaseMaterial> material;
			try
			{
				material = BaseScene::Get()->GetMaterial(m_MaterialPrefix + std::to_wstring(materialIndex));
			}
			catch (...)
			{
				material = nullptr;
			}

			const bool hasTexture =
				m_UseMaterialTexture && material && material->GetBaseColorTexture();
			constant.activeFlg.y = hasTexture ? 1 : 0;

			Col4 diffuse = m_UseBaseColorOverride
				? m_BaseColorOverride
				: (material ? material->GetBaseColor() : Col4(1.0f));

			Col4 alphaVector = (Col4)XMVectorReplicate(diffuse.w);
			Col4 emissiveColor = Col4(0.0f);
			Col4 ambientLightColor = (Col4)myLightSet->GetAmbient();

			constant.emissiveColor =
				(emissiveColor + (ambientLightColor * diffuse)) * alphaVector;
			constant.specularColorAndPower = Col4(0, 0, 0, 1);
			constant.diffuseColor =
				Col4(XMVectorSelect(alphaVector, diffuse * alphaVector, g_XMSelect1110));

			if (materialIndex < m_MaterialConstantBuffers.size())
			{
				m_MaterialConstantBuffers[materialIndex] = constant;
			}
		}

		m_ConstantBuffer = m_MaterialConstantBuffers.empty()
			? baseConstant
			: m_MaterialConstantBuffers.front();
	}
	void InstancedStaticDraw::OnCommitConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto pCurrentFrameResource = scene->GetCurrentFrameResource();

		memcpy(
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			&m_ConstantBuffer,
			sizeof(m_ConstantBuffer));

		const size_t materialCount = std::min(
			m_MaterialConstantBuffers.size(),
			m_MaterialConstantBufferIndices.size());
		for (size_t i = 0; i < materialCount; ++i)
		{
			const size_t constantBufferIndex = m_MaterialConstantBufferIndices[i];
			memcpy(
				pCurrentFrameResource->m_baseConstantBufferSetVec[constantBufferIndex].m_pBaseConstantBufferWO,
				&m_MaterialConstantBuffers[i],
				sizeof(BasicConstant));
		}
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

		if (IsOwnShadowActive())
		{
			auto depthGPUDsvs = pBaseScene->GetDepthSrvGpuHandles();
			pCommandList->SetGraphicsRootDescriptorTable(
				pBaseScene->GetGpuSlotID(L"t0"),
				depthGPUDsvs[SceneEnums::DepthGenPass::Shadow]
			);
		}
		else
		{
			pCommandList->SetGraphicsRootDescriptorTable(
				pBaseScene->GetGpuSlotID(L"t0"),
				cbvSrvGpuNullHandle
			);
		}

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

			const size_t materialConstantIndex =
				(i < m_MaterialConstantBufferIndices.size())
				? m_MaterialConstantBufferIndices[i]
				: m_ConstantBufferIndex;
			pCommandList->SetGraphicsRootConstantBufferView(
				pBaseScene->GetGpuSlotID(L"b0"),
				pCurrentFrameResource->m_baseConstantBufferSetVec[materialConstantIndex]
				.m_baseConstantBuffer->GetGPUVirtualAddress());

			auto materialKey = m_MaterialPrefix + std::to_wstring(i);
			std::shared_ptr<BaseMaterial> material;
			try
			{
				material = BaseScene::Get()->GetMaterial(materialKey);
			}
			catch (...)
			{
				material = nullptr;
			}
			auto texture = (m_UseMaterialTexture && material) ? material->GetBaseColorTexture() : nullptr;
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
			BenchmarkRecorder::Instance().CountDrawCall();
			pCommandList->DrawIndexedInstanced(
				mesh->GetNumIndices(),
				static_cast<UINT>(m_InstanceData.size()),
				0, 0, 0);
		}
	}

	void InstancedStaticDraw::OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		if (!m_CastShadowActive || m_ShadowInstanceData.empty() || !m_ShadowInstanceBuffer)
		{
			return;
		}

		auto pBaseScene = BaseScene::Get();
		auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();
		auto pipeline = PipelineStatePool::GetPipelineState(L"InstancedPNTShadowMap", true);
		pCommandList->SetPipelineState(pipeline.Get());

		pCommandList->SetGraphicsRootConstantBufferView(
			pBaseScene->GetGpuSlotID(L"b0"),
			pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex]
			.m_baseConstantBuffer->GetGPUVirtualAddress());

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		std::vector<std::shared_ptr<BaseMesh>> shadowProxyMeshes;
		const std::vector<std::shared_ptr<BaseMesh>>* meshes = nullptr;
		if (!m_ShadowMeshKey.empty())
		{
			// 高台や坂など重いモデルは、shadow pass だけ軽量 proxy mesh で描く。
			shadowProxyMeshes.push_back(BaseScene::Get()->GetMesh(m_ShadowMeshKey));
			meshes = &shadowProxyMeshes;
		}
		else
		{
			meshes = &BaseScene::Get()->GetModelMesh(m_MeshKey);
		}

		for (const auto& mesh : *meshes)
		{
			if (!mesh)
			{
				continue;
			}

			D3D12_VERTEX_BUFFER_VIEW views[2] =
			{
				mesh->GetVertexBufferView(),
				m_ShadowInstanceBufferView
			};

			pCommandList->IASetVertexBuffers(0, 2, views);
			pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
			BenchmarkRecorder::Instance().CountDrawCall();
			pCommandList->DrawIndexedInstanced(
				mesh->GetNumIndices(),
				static_cast<UINT>(m_ShadowInstanceData.size()),
				0, 0, 0);
		}
	}

	InstancedSkinnedDraw::InstancedSkinnedDraw(const std::shared_ptr<GameObject>& gameObjectPtr) :
		Component(gameObjectPtr)
	{
	}

	InstancedSkinnedDraw::~InstancedSkinnedDraw()
	{
		ReleaseMappedBuffers();
	}

	void InstancedSkinnedDraw::OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		(void)pCommandList;
	}

	namespace {
		UINT GetUploadBufferCapacity(UINT requiredSize)
		{
			UINT capacity = 256;
			while (capacity < requiredSize)
			{
				capacity *= 2;
			}
			return capacity;
		}

		unsigned long long MakeBonePoseKey(unsigned int animationIndex, unsigned int frameIndex)
		{
			return (static_cast<unsigned long long>(animationIndex) << 32) |
				static_cast<unsigned long long>(frameIndex);
		}
	}

	void InstancedSkinnedDraw::ReleaseMappedBuffers()
	{
		if (m_InstanceBuffer && m_MappedInstanceBuffer)
		{
			m_InstanceBuffer->Unmap(0, nullptr);
			m_MappedInstanceBuffer = nullptr;
		}

		if (m_BoneBuffer && m_MappedBoneBuffer)
		{
			m_BoneBuffer->Unmap(0, nullptr);
			m_MappedBoneBuffer = nullptr;
		}
	}

	float InstancedSkinnedDraw::GetQuantizedAnimationTime(
		const std::shared_ptr<BaseAssimp>& assimp,
		unsigned int animationIndex,
		float animationTime,
		unsigned int& frameIndex) const
	{
		frameIndex = 0;

		if (!assimp)
		{
			return 0.0f;
		}

		if (animationTime < 0.0f || !std::isfinite(animationTime))
		{
			animationTime = 0.0f;
		}

		const float duration = assimp->GetAnimationDurationSeconds(animationIndex);
		if (duration <= 0.0f)
		{
			return 0.0f;
		}

		const float loopEpsilon = (0.001f > duration * 0.0001f) ? 0.001f : duration * 0.0001f;
		const float effectiveDuration = ((duration - loopEpsilon) > loopEpsilon) ? (duration - loopEpsilon) : loopEpsilon;
		float localTime = fmodf(animationTime, effectiveDuration);
		if (localTime < 0.0f)
		{
			localTime += effectiveDuration;
		}

		const float sampleFps = (m_AnimationSampleFps > 1.0f) ? m_AnimationSampleFps : 1.0f;
		frameIndex = static_cast<unsigned int>(floorf(localTime * sampleFps));
		return static_cast<float>(frameIndex) / sampleFps;
	}

	UINT InstancedSkinnedDraw::EnsureBonePose(
		const std::shared_ptr<BaseAssimp>& assimp,
		unsigned int animationIndex,
		float animationTime)
	{
		unsigned int frameIndex = 0;
		const float sampleTime = GetQuantizedAnimationTime(
			assimp,
			animationIndex,
			animationTime,
			frameIndex);

		const unsigned long long poseKey = MakeBonePoseKey(animationIndex, frameIndex);
		auto currentFramePose = m_BonePoseStartByKey.find(poseKey);
		if (currentFramePose != m_BonePoseStartByKey.end())
		{
			return currentFramePose->second;
		}

		auto cachedRows = m_BonePoseRowsCache.find(poseKey);
		if (cachedRows == m_BonePoseRowsCache.end())
		{
			m_WorkBones.clear();
			assimp->GetBoneTransforms(
				sampleTime,
				m_WorkBones,
				animationIndex);

			if (m_WorkBones.empty())
			{
				return UINT_MAX;
			}

			std::vector<XMFLOAT4> rows;
			rows.reserve(m_WorkBones.size() * 3);
			for (const auto& bone : m_WorkBones)
			{
				XMMATRIX matrix = (XMMATRIX)bone;
				XMFLOAT4 row{};

				XMStoreFloat4(&row, matrix.r[0]);
				rows.push_back(row);
				XMStoreFloat4(&row, matrix.r[1]);
				rows.push_back(row);
				XMStoreFloat4(&row, matrix.r[2]);
				rows.push_back(row);
			}

			m_BonePoseRowsCache[poseKey] = rows;
			cachedRows = m_BonePoseRowsCache.find(poseKey);
		}

		if (cachedRows == m_BonePoseRowsCache.end() || cachedRows->second.empty())
		{
			return UINT_MAX;
		}

		const UINT boneStart = static_cast<UINT>(m_BoneRows.size() / 3);
		m_BonePoseStartByKey[poseKey] = boneStart;
		m_BoneRows.insert(
			m_BoneRows.end(),
			cachedRows->second.begin(),
			cachedRows->second.end());

		return boneStart;
	}
	void InstancedSkinnedDraw::EnsureInstanceBuffer(UINT bufferSize)
	{
		if (bufferSize == 0)
		{
			return;
		}

		if (m_InstanceBuffer && m_InstanceBufferCapacityBytes >= bufferSize)
		{
			return;
		}

		if (m_InstanceBuffer && m_MappedInstanceBuffer)
		{
			m_InstanceBuffer->Unmap(0, nullptr);
			m_MappedInstanceBuffer = nullptr;
		}

		m_InstanceBuffer.Reset();
		m_InstanceBufferCapacityBytes = 0;

		const UINT capacity = GetUploadBufferCapacity(bufferSize);
		auto device = App::GetD3D12Device();

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(capacity),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_InstanceBuffer)));

		m_InstanceBufferCapacityBytes = capacity;

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_InstanceBuffer->Map(0, &readRange, &m_MappedInstanceBuffer));
	}

	void InstancedSkinnedDraw::EnsureBoneBuffer(UINT bufferSize)
	{
		if (bufferSize == 0)
		{
			return;
		}

		if (m_BoneBuffer && m_BoneBufferCapacityBytes >= bufferSize)
		{
			return;
		}

		if (m_BoneBuffer && m_MappedBoneBuffer)
		{
			m_BoneBuffer->Unmap(0, nullptr);
			m_MappedBoneBuffer = nullptr;
		}

		m_BoneBuffer.Reset();
		m_BoneBufferCapacityBytes = 0;

		const UINT capacity = GetUploadBufferCapacity(bufferSize);
		auto device = App::GetD3D12Device();

		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(capacity),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_BoneBuffer)));

		m_BoneBufferCapacityBytes = capacity;

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

		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_BoneBuffer->Map(0, &readRange, &m_MappedBoneBuffer));
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
		m_BonePoseStartByKey.clear();
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

			const UINT boneStart = EnsureBonePose(
				assimp,
				animationIndex,
				source.animationTime);
			if (boneStart == UINT_MAX)
			{
				continue;
			}

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
		}

		if (m_InstanceData.empty() || m_BoneRows.empty())
		{
			return;
		}

		const UINT instanceBufferSize =
			static_cast<UINT>(sizeof(SkinnedInstanceData) * m_InstanceData.size());
		EnsureInstanceBuffer(instanceBufferSize);
		memcpy(m_MappedInstanceBuffer, m_InstanceData.data(), instanceBufferSize);

		m_InstanceBufferView.BufferLocation = m_InstanceBuffer->GetGPUVirtualAddress();
		m_InstanceBufferView.StrideInBytes = sizeof(SkinnedInstanceData);
		m_InstanceBufferView.SizeInBytes = instanceBufferSize;

		const UINT boneBufferSize =
			static_cast<UINT>(sizeof(XMFLOAT4) * m_BoneRows.size());
		EnsureBoneBuffer(boneBufferSize);
		memcpy(m_MappedBoneBuffer, m_BoneRows.data(), boneBufferSize);
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

		if (m_FogEnabled && BaseScene::Get()->IsFogEnabled())
		{
			const float start = m_FogStart;
			const float end = m_FogEnd;
			if (start == end)
			{
				// 開始距離と終了距離が同じ場合はゼロ除算を避け、全体をフォグ済みとして扱う。
				static const XMVECTORF32 fullyFogged = { 0, 0, 0, 1 };
				m_ConstantBuffer.fogVector = Vec4(fullyFogged);
			}
			else
			{
				// 敵の頂点はシェーダー内でインスタンスごとのワールド座標へ変換される。
				// view のZ成分を定数化し、ピクセルシェーダーでワールド座標との内積からフォグ量を求める。
				XMVECTOR viewZ = XMVectorMergeXY(
					XMVectorMergeZW(view.r[0], view.r[2]),
					XMVectorMergeZW(view.r[1], view.r[3]));
				XMVECTOR wOffset = XMVectorSwizzle<1, 2, 3, 0>(XMLoadFloat(&start));
				m_ConstantBuffer.fogVector = Vec4((viewZ + wOffset) / (start - end));
			}
			m_ConstantBuffer.fogColor = (Col4)m_FogColor;
		}
		else
		{
			m_ConstantBuffer.fogVector = Vec4(g_XMZero);
			m_ConstantBuffer.fogColor = Vec4(g_XMZero);
		}

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
		BenchmarkRecorder::Instance().CountDrawCall();
		pCommandList->DrawIndexedInstanced(
			mesh->GetNumIndices(),
			static_cast<UINT>(m_InstanceData.size()),
			0, 0, 0);
	}
}

