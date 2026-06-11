/*!
@file BcStaticDraw.cpp
@brief ベイシックスタティック描画コンポーネント　実体
*/

#include "stdafx.h"

namespace shooting {

	IMPLEMENT_DX12SHADER(BcVSPNTBonePL, App::GetShadersDir() + L"BcVSPNTBonePL.cso")
	IMPLEMENT_DX12SHADER(BcVSPNTBonePLShadow, App::GetShadersDir() + L"BcVSPNTBonePLShadow.cso")

	//	IMPLEMENT_DX12SHADER(BcVSPNTBonePL, App::GetShadersDir() + L"BcVSPNTStaticPL.cso")
	//	IMPLEMENT_DX12SHADER(BcVSPNTBonePLShadow, App::GetShadersDir() + L"BcVSPNTStaticPLShadow.cso")




	BcPNTBoneDraw::BcPNTBoneDraw(const std::shared_ptr<GameObject>& gameObjectPtr) :
	Component(gameObjectPtr),
	m_OwnShadowActive(false),
	m_FogEnabled(true),
	m_FogStart(-25.0f),
	m_FogEnd(-40.0f),
	m_FogColor(0.8f, 0.8f, 0.8f, 1.0f),
	m_FogVector(0.0, 0.0, 1.0f)
	{
	}

	void BcPNTBoneDraw::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();
		//ベイシックコンスタントバッファ
		for (size_t i = 0; i < BaseDevice::FrameCount; i++)
		{
			m_ConstantBufferIndex =
				frameResources[i]->AddBaseConstantBufferSet<BasicConstant>(pBaseDevice->GetD3D12Device());
		}
		// シーンパイプラインステート
		{

			ComPtr<ID3D12PipelineState> defaultPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBone");
			ComPtr<ID3D12PipelineState> defaultShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBoneShadow");
			ComPtr<ID3D12PipelineState> alphaPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBonecAlpha");
			ComPtr<ID3D12PipelineState> alphaShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBoneAlphaShadow");


			CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
			//カリング
			rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			ZeroMemory(&psoDesc, sizeof(psoDesc));

			psoDesc.InputLayout = { VertexPositionNormalTextureSkinning::GetVertexElement(), VertexPositionNormalTextureSkinning::GetNumElements() };
			//			psoDesc.InputLayout = { VertexPositionNormalTexture::GetVertexElement(), VertexPositionNormalTexture::GetNumElements() };
			psoDesc.pRootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault").Get();
			psoDesc.VS =
			{
				reinterpret_cast<UINT8*>(BcVSPNTBonePL::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				BcVSPNTBonePL::GetPtr()->GetShaderComPtr()->GetBufferSize()
			};
			psoDesc.PS =
			{
				reinterpret_cast<UINT8*>(BcPSPNTPL::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				BcPSPNTPL::GetPtr()->GetShaderComPtr()->GetBufferSize()
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
			//デフォルト影無し
			if (!defaultPipelineState)
			{
				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&defaultPipelineState)));
				NAME_D3D12_OBJECT(defaultPipelineState);
				PipelineStatePool::AddPipelineState(L"BcPNTBone", defaultPipelineState);
			}
			//アルファ影なし
			psoDesc.BlendState = BlendState::GetAlphaBlendEx();
			if (!alphaPipelineState)
			{
				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&alphaPipelineState)));
				NAME_D3D12_OBJECT(alphaPipelineState);
				PipelineStatePool::AddPipelineState(L"BcPNTBoneAlpha", alphaPipelineState);
			}
			//デフォルト影あり
			psoDesc.BlendState = BlendState::GetOpaqueBlend();
			psoDesc.VS =
			{
				reinterpret_cast<UINT8*>(BcVSPNTBonePLShadow::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				BcVSPNTBonePLShadow::GetPtr()->GetShaderComPtr()->GetBufferSize()
			};
			psoDesc.PS =
			{
				reinterpret_cast<UINT8*>(BcPSPNTPLShadow::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				BcPSPNTPLShadow::GetPtr()->GetShaderComPtr()->GetBufferSize()
			};
			if (!defaultShadowPipelineState)
			{
				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&defaultShadowPipelineState)));
				NAME_D3D12_OBJECT(defaultShadowPipelineState);
				PipelineStatePool::AddPipelineState(L"BcPNTBoneShadow", defaultShadowPipelineState);
			}
			psoDesc.BlendState = BlendState::GetAlphaBlendEx();
			//アルファ影あり
			if (!alphaShadowPipelineState)
			{
				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&alphaShadowPipelineState)));
				NAME_D3D12_OBJECT(alphaShadowPipelineState);
				PipelineStatePool::AddPipelineState(L"BcPNTBoneAlphaShadow", alphaShadowPipelineState);
			}

		}
	}

	bool BcPNTBoneDraw::UpdateAnimation(double animeTime)
	{
		auto mesh = GetBaseMesh(0);
		if (!mesh)
		{
			return false;
		}

		auto ptrBaseAssimp = mesh->GetBaseAssimp();
		if (!ptrBaseAssimp)
		{
			return false;
		}

		m_BoneTransforms.clear();
		m_NodeGlobalTransforms.clear();
		ptrBaseAssimp->GetBoneTransforms(
			(float)animeTime,
			m_BoneTransforms,
			m_CurrentAnimationIndex
		);
		m_NodeGlobalTransforms = ptrBaseAssimp->GetNodeGlobalTransforms();
		return true;
	}

	void BcPNTBoneDraw::OnUpdateConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto stage = std::dynamic_pointer_cast<Stage>(scene->GetActiveStage(true));
		auto& frameResources = scene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();
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

			auto ptrTrans = gameObject->GetComponent<Transform>();
			auto& param = ptrTrans->GetTransParam();

			{
				m_ConstantBuffer = {};
				m_ConstantBuffer.activeFlg.y = GetDrawTexture(0) ? 1 : 0;
				m_ConstantBuffer.activeFlg.x = 3;

				Vec3 drawPos = param.position + m_ModelOffset;

				auto world = XMMatrixAffineTransformation(
					param.scale,
					param.rotateOrigin,
					param.quaternion,
					drawPos
				);

				auto view = (XMMATRIX)((Mat4x4)myCamera->GetViewMatrix());
				auto proj = (XMMATRIX)((Mat4x4)myCamera->GetProjMatrix());
				auto worldView = world * view;
				m_ConstantBuffer.worldViewProj =
					Mat4x4(XMMatrixTranspose(XMMatrixMultiply(worldView, proj)));

				if (m_FogEnabled && BaseScene::Get()->IsFogEnabled())
				{
					auto start = m_FogStart;
					auto end = m_FogEnd;
					if (start == end)
					{
						static const XMVECTORF32 fullyFogged = { 0, 0, 0, 1 };
						m_ConstantBuffer.fogVector = Vec4(fullyFogged);
					}
					else
					{
						XMMATRIX worldViewTrans = worldView;
						XMVECTOR worldViewZ = XMVectorMergeXY(
							XMVectorMergeZW(worldViewTrans.r[0], worldViewTrans.r[2]),
							XMVectorMergeZW(worldViewTrans.r[1], worldViewTrans.r[3])
						);
						XMVECTOR wOffset = XMVectorSwizzle<1, 2, 3, 0>(XMLoadFloat(&start));
						m_ConstantBuffer.fogVector = Vec4((worldViewZ + wOffset) / (start - end));
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
					Vec3(lightEye), Vec3(lightAt), Vec3(0, 1.0f, 0)
				);
				XMMATRIX LightProj = XMMatrixOrthographicLH(
					ShadowMap::GetViewWidth(),
					ShadowMap::GetViewHeight(),
					ShadowMap::GetLightNear(),
					ShadowMap::GetLightFar()
				);

				m_ConstantBuffer.lightView = Mat4x4(XMMatrixTranspose(LightView));
				m_ConstantBuffer.lightProjection = Mat4x4(XMMatrixTranspose(LightProj));

				size_t BoneSz = m_BoneTransforms.size();
				if (BoneSz > 0)
				{
					UINT cb_count = 0;
					for (size_t b = 0; b < BoneSz; b++)
					{
						bsm::Mat4x4 mat = m_BoneTransforms[b];
						m_ConstantBuffer.Bones[cb_count] = ((XMMATRIX)mat).r[0];
						m_ConstantBuffer.Bones[cb_count + 1] = ((XMMATRIX)mat).r[1];
						m_ConstantBuffer.Bones[cb_count + 2] = ((XMMATRIX)mat).r[2];
						cb_count += 3;
					}
				}
			}
		}
	}

	void BcPNTBoneDraw::OnCommitConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto pCurrentFrameResource = scene->GetCurrentFrameResource();
		//シーン
		memcpy(pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			   &m_ConstantBuffer, sizeof(m_ConstantBuffer));
	}

	void BcPNTBoneDraw::OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		auto pBaseScene = BaseScene::Get();
		auto& frameResources = pBaseScene->GetFrameResources();
		auto pCurrentFrameResource = pBaseScene->GetCurrentFrameResource();
		auto pBaseDevice = BaseDevice::GetBaseDevice();
		auto& viewport = pBaseScene->GetViewport();
		auto& scissorRect = pBaseScene->GetScissorRect();
		auto depthDsvs = pBaseScene->GetDepthDsvs();
		auto depthGPUDsvs = pBaseScene->GetDepthSrvGpuHandles();

		auto CbvSrvUavDescriptorHeap = pBaseScene->GetCbvSrvUavDescriptorHeap();
		auto mesh = GetBaseMesh(0);
		auto texture = GetBaseTexture(0);
		if (!texture)
		{
			int a = 0;
		}
		if (mesh)
		{
			ComPtr<ID3D12PipelineState> defaultPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBone", true);
			ComPtr<ID3D12PipelineState> defaultShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBoneShadow", true);
			ComPtr<ID3D12PipelineState> alphaPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBoneAlpha", true);
			ComPtr<ID3D12PipelineState> alphaShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTBoneAlphaShadow", true);
			//null rv
			CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvGpuNullHandle(CbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

			// set PipelineState and GetGpuSlotID(L"t0")
			if (GetGameObject()->IsAlphaActive())
			{
				if (IsOwnShadowActive())
				{
					pCommandList->SetPipelineState(alphaShadowPipelineState.Get());
					pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"t0"), depthGPUDsvs[SceneEnums::DepthGenPass::Shadow]);        // Set the shadow texture as an SRV.
				}
				else
				{
					pCommandList->SetPipelineState(alphaPipelineState.Get());
					pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"t0"), cbvSrvGpuNullHandle);        // Set the shadow texture as an SRV.
				}
			}
			else
			{
				if (IsOwnShadowActive())
				{
					pCommandList->SetPipelineState(defaultShadowPipelineState.Get());
					pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"t0"), depthGPUDsvs[SceneEnums::DepthGenPass::Shadow]);        // Set the shadow texture as an SRV.
				}
				else
				{
					pCommandList->SetPipelineState(defaultPipelineState.Get());
					pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"t0"), cbvSrvGpuNullHandle);        // Set the shadow texture as an SRV.
				}

			}
			//Sampler
			UINT index = pBaseScene->GetSamplerIndex(L"LinearClamp");
			if (index == UINT_MAX)
			{
				throw BaseException(
					L"LinearClampサンプラーが特定できません。",
					L"Scene::ScenePass()"
				);
			}
			CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle(
				pBaseScene->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
				index,
				pBaseScene->GetSamplerDescriptorHandleIncrementSize()
			);
			pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"s0"), samplerHandle);

			index = pBaseScene->GetSamplerIndex(L"ComparisonLinear");
			if (index == UINT_MAX)
			{
				throw BaseException(
					L"ComparisonLinearサンプラーが特定できません。",
					L"Scene::ScenePass()"
				);
			}
			CD3DX12_GPU_DESCRIPTOR_HANDLE samplerHandle2(
				pBaseScene->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
				index,
				pBaseScene->GetSamplerDescriptorHandleIncrementSize()
			);
			pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"s1"), samplerHandle2);
			//シェーダリソース（テクスチャ）のハンドルの設定
			if (texture)
			{
				CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(
					pBaseScene->GetCbvSrvUavDescriptorHeap()->GetGPUDescriptorHandleForHeapStart(),
					texture->GetSrvIndex(),
					pBaseScene->GetCbvSrvUavDescriptorHandleIncrementSize()
				);
				pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"t1"), srvHandle);
			}
			else
			{
				CD3DX12_GPU_DESCRIPTOR_HANDLE srvNullHandle(
					pBaseScene->GetCbvSrvUavDescriptorHeap()->GetGPUDescriptorHandleForHeapStart()
				);
				pCommandList->SetGraphicsRootDescriptorTable(pBaseScene->GetGpuSlotID(L"t1"), srvNullHandle);
			}
			//Cbv
			// Set scene constant buffer.
			pCommandList->SetGraphicsRootConstantBufferView(pBaseScene->GetGpuSlotID(L"b0"),
															pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_baseConstantBuffer->GetGPUVirtualAddress());
			//Draw
			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pCommandList->IASetVertexBuffers(0, 1, &mesh->GetVertexBufferView());
			pCommandList->IASetIndexBuffer(&mesh->GetIndexBufferView());
			pCommandList->DrawIndexedInstanced(mesh->GetNumIndices(), 1, 0, 0, 0);

			// ダメージエフェクト
			if (auto dmg = GetGameObject()->GetComponent<DamageEffect>(false))
			{
				dmg->OnDraw(pCommandList);
			}
		}

	}

	std::shared_ptr<BaseTexture> BcPNTBoneDraw::GetDrawTexture(size_t index)
	{
		// 1. material 優先
		if (index < GetBaseMaterialCount())
		{
			auto material = GetBaseMaterial(index);
			if (material)
			{
				auto tex = material->GetBaseColorTexture();
				if (tex)
				{
					return tex;
				}
			}
		}

		// 2. 従来の texture
		if (index < GetBaseTextureCount())
		{
			auto tex = GetBaseTexture(index);
			if (tex)
			{
				return tex;
			}
		}

		// 3. 最後の保険: 0番 texture
		if (GetBaseTextureCount() > 0)
		{
			return GetBaseTexture(0);
		}

		return nullptr;
	}

	float BcPNTBoneDraw::GetCurrentAnimationDurationSeconds()
	{
		auto self = m_gameObject.lock();
		if (!self)
		{
			return 0.0f;
		}

		auto mesh = GetBaseMesh(0);
		if (!mesh)
		{
			return 0.0f;
		}

		auto assimp = mesh->GetBaseAssimp();
		if (!assimp)
		{
			return 0.0f;
		}

		return assimp->GetAnimationDurationSeconds(m_CurrentAnimationIndex);
	}

	int BcPNTBoneDraw::GetAnimationCount()
	{
		auto mesh = GetBaseMesh(0);
		if (!mesh)
		{
			return 0;
		}

		auto assimp = mesh->GetBaseAssimp();
		if (!assimp)
		{
			return 0;
		}

		return assimp->GetAnimationCount();
	}

	std::wstring BcPNTBoneDraw::GetAnimationName(int index)
	{
		auto mesh = GetBaseMesh(0);
		if (!mesh)
		{
			return L"";
		}

		auto assimp = mesh->GetBaseAssimp();
		if (!assimp)
		{
			return L"";
		}

		return assimp->GetAnimationName(index);
	}
}

