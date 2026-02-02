#include "stdafx.h"

namespace shooting {

	IMPLEMENT_DX12SHADER(BcVSPNTStaticPL, App::GetShadersDir() + L"BcVSPNTStaticPL.cso")
	IMPLEMENT_DX12SHADER(BcPSPNTPL, App::GetShadersDir() + L"BcPSPNTPL.cso")

	IMPLEMENT_DX12SHADER(BcVSPNTStaticPLShadow, App::GetShadersDir() + L"BcVSPNTStaticPLShadow.cso")
	IMPLEMENT_DX12SHADER(BcPSPNTPLShadow, App::GetShadersDir() + L"BcPSPNTPLShadow.cso")

	BcPNTStaticDraw::BcPNTStaticDraw(const std::shared_ptr<GameObject>& gameObjectPtr) :
		Component(gameObjectPtr),
		m_OwnShadowActive(false),
		m_FogEnabled(true),
		m_FogStart(-25.0f),
		m_FogEnd(-40.0f),
		m_FogColor(0.8f, 0.8f, 0.8f, 1.0f),
		m_FogVector(0.0, 0.0, 1.0f)
	{
	}

	void BcPNTStaticDraw::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		auto pBaseScene = BaseScene::Get();
		const auto& frameResource = pBaseScene->GetFrameResources();
		auto pBaseDevice = BaseDevice::GetBaseDevice();
		// コンスタントバッファ
		for (size_t i = 0; i < BaseDevice::FrameCount; i++)
		{
			m_ConstantBufferIndex = frameResource[i]->AddBaseConstantBufferSet<BasicConstant>(pBaseDevice->GetD3D12Device());
		}
		// パイプラインステート
		{

			ComPtr<ID3D12PipelineState> defaultPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTStatic");
			ComPtr<ID3D12PipelineState> defaultShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTStaticShadow");
			ComPtr<ID3D12PipelineState> alphaPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTStaticAlpha");
			ComPtr<ID3D12PipelineState> alphaShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTStaticAlphaShadow");


			CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
			//カリング
			rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			ZeroMemory(&psoDesc, sizeof(psoDesc));
			psoDesc.InputLayout = { VertexPositionNormalTexture::GetVertexElement(), VertexPositionNormalTexture::GetNumElements() };
			psoDesc.pRootSignature = RootSignaturePool::GetRootSignature(L"BaseCrossDefault").Get();
			psoDesc.VS =
			{
				reinterpret_cast<UINT8*>(BcVSPNTStaticPL::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				BcVSPNTStaticPL::GetPtr()->GetShaderComPtr()->GetBufferSize()
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
				PipelineStatePool::AddPipelineState(L"BcPNTStatic", defaultPipelineState);
			}
			//アルファ影なし
			psoDesc.BlendState = BlendState::GetAlphaBlendEx();
			if (!alphaPipelineState)
			{
				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&alphaPipelineState)));
				NAME_D3D12_OBJECT(alphaPipelineState);
				PipelineStatePool::AddPipelineState(L"BcPNTStaticAlpha", alphaPipelineState);
			}
			//デフォルト影あり
			psoDesc.BlendState = BlendState::GetOpaqueBlend();
			psoDesc.VS =
			{
				reinterpret_cast<UINT8*>(BcVSPNTStaticPLShadow::GetPtr()->GetShaderComPtr()->GetBufferPointer()),
				BcVSPNTStaticPLShadow::GetPtr()->GetShaderComPtr()->GetBufferSize()
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
				PipelineStatePool::AddPipelineState(L"BcPNTStaticShadow", defaultShadowPipelineState);
			}
			psoDesc.BlendState = BlendState::GetAlphaBlendEx();
			//アルファ影あり
			if (!alphaShadowPipelineState)
			{
				ThrowIfFailed(App::GetID3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&alphaShadowPipelineState)));
				NAME_D3D12_OBJECT(alphaShadowPipelineState);
				PipelineStatePool::AddPipelineState(L"BcPNTStaticAlphaShadow", alphaShadowPipelineState);
			}

		}
	}

	void BcPNTStaticDraw::OnUpdateConstantBuffers()
	{
		// シーンとステージの取得
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto stage = std::dynamic_pointer_cast<Stage>(scene->GetActiveStage(true));
		
		// カメラとライトの宣言
		std::shared_ptr<PerspecCamera> mainCamera;
		std::shared_ptr<LightSet> myLightSet;
		
		// ステージが存在しない場合は処理を中断
		if (!stage)
		{
			return;
		}
		
		auto gameObject = m_gameObject.lock();
		if (gameObject)
		{
			// カメラとライトセットを取得
			mainCamera = std::dynamic_pointer_cast<PerspecCamera>(gameObject->GetCamera());
			myLightSet = gameObject->GetLightSet();

			// トランスフォームコンポーネントとそのパラメータを取得
			auto ptrTrans = gameObject->GetComponent<Transform>();
			auto& param = ptrTrans->GetTransParam();
			
			// シーンのコンスタントバッファの設定
			{
				// コンスタントバッファの初期化
				m_ConstantBuffer = {};
				
				// テクスチャの有無をフラグに設定
				if (GetBaseTexture(0))
				{
					m_ConstantBuffer.activeFlg.y = 1; // テクスチャあり
				}
				else
				{
					m_ConstantBuffer.activeFlg.y = 0; // テクスチャなし
				}
				m_ConstantBuffer.activeFlg.x = 3; // 描画モード設定

				// ワールド行列の計算（スケール、回転、位置から変換行列を作成）
				auto world = XMMatrixAffineTransformation(
					param.scale,        // スケール
					param.rotateOrigin, // 回転の原点
					param.quaternion,   // 回転（クォータニオン）
					param.position      // 位置
				);
				
				// ビュー行列とプロジェクション行列の取得
				auto view = (XMMATRIX)((Mat4x4)mainCamera->GetViewMatrix());
				auto proj = (XMMATRIX)((Mat4x4)mainCamera->GetProjMatrix());
				
				// ワールドビュー行列の計算
				auto worldView = world * view;
				
				// ワールドビュープロジェクション行列を転置してシェーダー用に設定
				m_ConstantBuffer.worldViewProj = Mat4x4(XMMatrixTranspose(XMMatrixMultiply(worldView, proj)));
				
				// フォグ（霧）の設定
				if (m_FogEnabled)
				{
					auto start = m_FogStart; // フォグ開始距離
					auto end = m_FogEnd;     // フォグ終了距離
					
					// 開始と終了が同じ場合の特殊処理
					if (start == end)
					{
						// 開始と終了が同じ場合、すべてが100％フォグされるようにします
						static const XMVECTORF32 fullyFogged = { 0, 0, 0, 1 };
						m_ConstantBuffer.fogVector = Vec4(fullyFogged);
					}
					else
					{
						// 通常のフォグ計算
						XMMATRIX worldViewTrans = worldView;
						
						// ワールドビュー行列からZ成分を抽出（_13, _23, _33, _43の要素）
						XMVECTOR worldViewZ = XMVectorMergeXY(XMVectorMergeZW(worldViewTrans.r[0], worldViewTrans.r[2]),
															  XMVectorMergeZW(worldViewTrans.r[1], worldViewTrans.r[3]));
						
						// フォグのオフセット計算
						XMVECTOR wOffset = XMVectorSwizzle<1, 2, 3, 0>(XMLoadFloat(&start));
						
						// フォグベクトルの計算（距離に応じたフォグの強さを計算）
						m_ConstantBuffer.fogVector = Vec4((worldViewZ + wOffset) / (start - end));
					}
					
					// フォグの色を設定
					m_ConstantBuffer.fogColor = (Col4)m_FogColor;
				}
				else
				{
					// フォグが無効の場合、ベクトルと色をゼロに設定
					m_ConstantBuffer.fogVector = Vec4(g_XMZero);
					m_ConstantBuffer.fogColor = Vec4(g_XMZero);
				}
				
				// ライトの設定（複数のライトをループ処理）
				for (int i = 0; i < myLightSet->GetNumLights(); i++)
				{
					// 各ライトの方向、拡散色、スペキュラ色を設定
					m_ConstantBuffer.lightDirection[i] = (Vec4)myLightSet->GetLight(i).m_directional;
					m_ConstantBuffer.lightDiffuseColor[i] = (Vec4)myLightSet->GetLight(i).m_diffuseColor;
					m_ConstantBuffer.lightSpecularColor[i] = (Vec4)myLightSet->GetLight(i).m_specularColor;
				}
				
				// ワールド行列を設定し、転置する（シェーダーで使用するため）
				m_ConstantBuffer.world = Mat4x4(world);
				m_ConstantBuffer.world.transpose();

				// ワールド行列の逆行列を計算（法線変換用）
				XMMATRIX worldInverse = XMMatrixInverse(nullptr, world);
				m_ConstantBuffer.worldInverseTranspose[0] = Vec4(worldInverse.r[0]);
				m_ConstantBuffer.worldInverseTranspose[1] = Vec4(worldInverse.r[1]);
				m_ConstantBuffer.worldInverseTranspose[2] = Vec4(worldInverse.r[2]);

				// ビュー行列の逆行列を計算してカメラ位置を取得
				XMMATRIX viewInverse = XMMatrixInverse(nullptr, view);
				m_ConstantBuffer.eyePosition = Vec4(viewInverse.r[3]); // カメラの位置

				// マテリアル色の設定
				Col4 diffuse = Col4(1.0f);                              // 拡散反射色（白）
				Col4 alphaVector = (Col4)XMVectorReplicate(1.0f);       // アルファ値（不透明）
				Col4 emissiveColor = Col4(0.0f);                        // 自己発光色（なし）
				Col4 ambientLightColor = (Col4)myLightSet->GetAmbient(); // 環境光の色

				// エミッシブ色の計算（自己発光 + 環境光の効果）
				m_ConstantBuffer.emissiveColor = (emissiveColor + (ambientLightColor * diffuse)) * alphaVector;
				
				// スペキュラ色とパワーの設定（現在は無効）
				m_ConstantBuffer.specularColorAndPower = Col4(0, 0, 0, 1);

				// 拡散反射色の設定（xyz = diffuse * alpha, w = alpha）
				m_ConstantBuffer.diffuseColor = Col4(XMVectorSelect(alphaVector, diffuse * alphaVector, g_XMSelect1110));

				// シャドウマップ用のライト設定
				auto mainLight = myLightSet->GetMainBaseLight();
				Vec3 calcLightDir = Vec3(mainLight.m_directional) * Vec3(-1.0f); // ライトの逆方向を計算

				// ライトの注視点（カメラの注視点と同じ）
				Vec3 lightAt(mainCamera->GetAt());

				// ライトの位置を計算
				Vec3 lightEye(calcLightDir);
				lightEye *= Vec3(ShadowMap::GetLightHeight()); // ライトの高さを適用
				lightEye += lightAt;                           // 注視点からのオフセット

				// ライト位置を4次元ベクトルとして設定
				Vec4 LightEye4 = Vec4(lightEye, 1.0f);
				m_ConstantBuffer.lightPos = LightEye4;
				
				// カメラ位置を4次元ベクトルとして設定
				Vec4 eyePos4 = Vec4((Vec3)mainCamera->GetEye(), 1.0f);
				m_ConstantBuffer.eyePos = eyePos4;
				
				// ライトのビュー行列とプロジェクション行列の宣言
				XMMATRIX LightView, LightProj;
				
				// ライトのビュー行列を計算（ライトから見たシーン）
				LightView = XMMatrixLookAtLH(
					Vec3(lightEye),     // ライトの位置
					Vec3(lightAt),      // ライトの注視点
					Vec3(0, 1.0f, 0)    // 上方向ベクトル
				);
				
				// ライトの正射影プロジェクション行列を計算（シャドウマップ用）
				LightProj = XMMatrixOrthographicLH(
					ShadowMap::GetViewWidth(),   // ビューの幅
					ShadowMap::GetViewHeight(),  // ビューの高さ
					ShadowMap::GetLightNear(),   // ニアクリップ
					ShadowMap::GetLightFar()     // ファークリップ
				);
				
				// ライトのビュー行列とプロジェクション行列を転置して設定
				m_ConstantBuffer.lightView = Mat4x4(XMMatrixTranspose(LightView));
				m_ConstantBuffer.lightProjection = Mat4x4(XMMatrixTranspose(LightProj));
			}
		}
	}

	void BcPNTStaticDraw::OnCommitConstantBuffers()
	{
		auto scene = dynamic_cast<Scene*>(BaseScene::Get());
		auto pCurrentFrameResource = scene->GetCurrentFrameResource();
		//シーン
		memcpy(pCurrentFrameResource->m_baseConstantBufferSetVec[m_ConstantBufferIndex].m_pBaseConstantBufferWO,
			   &m_ConstantBuffer, sizeof(m_ConstantBuffer));
	}

	void BcPNTStaticDraw::OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)
	{
		//		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
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
				= PipelineStatePool::GetPipelineState(L"BcPNTStatic", true);
			ComPtr<ID3D12PipelineState> defaultShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTStaticShadow", true);
			ComPtr<ID3D12PipelineState> alphaPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTStaticAlpha", true);
			ComPtr<ID3D12PipelineState> alphaShadowPipelineState
				= PipelineStatePool::GetPipelineState(L"BcPNTStaticAlphaShadow", true);
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
		}

	}
}