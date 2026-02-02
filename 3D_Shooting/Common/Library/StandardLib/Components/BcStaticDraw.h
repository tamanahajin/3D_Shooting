#pragma once
#include "stdafx.h"

namespace shooting {

	DECLARE_DX12SHADER(BcVSPNTStaticPL)
	DECLARE_DX12SHADER(BcPSPNTPL)
	DECLARE_DX12SHADER(BcVSPNTStaticPLShadow)
	DECLARE_DX12SHADER(BcPSPNTPLShadow)

	/// <summary>
	/// スタティックなメッシュを描画するコンポーネント
	/// ポジション、法線、テクスチャを含む頂点フォーマット
	/// </summary>
	class BcPNTStaticDraw : public Component
	{
	protected:
		BasicConstant m_ConstantBuffer;
		size_t m_ConstantBufferIndex;
		// 自分自身に影を描画するかどうか
		bool m_OwnShadowActive;
		bool m_FogEnabled = true;
		float m_FogStart = -10.0f;
		float m_FogEnd = -40.0f;
		XMFLOAT4 m_FogColor;
		XMFLOAT3 m_FogVector;

	public:
		bool IsOwnShadowActive()const
		{
			return m_OwnShadowActive;
		}
		void SetOwnShadowActive(bool b)
		{
			m_OwnShadowActive = b;
		}
		bool IsSetFogEnabled()const
		{
			return m_FogEnabled;
		}
		void SetFogEnabled(bool b)
		{
			m_FogEnabled = b;
		}



		BcPNTStaticDraw(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~BcPNTStaticDraw() {}
		virtual void OnUpdateConstantBuffers()override;
		virtual void OnCommitConstantBuffers()override;
		virtual void OnCreate()override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)override;
	};
}