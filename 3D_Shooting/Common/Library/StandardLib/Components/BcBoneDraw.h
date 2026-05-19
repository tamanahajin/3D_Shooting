/*!
@file BcBoneDraw.h
@brief ベイシック描画コンポーネント
*/


#pragma once
#include "stdafx.h"

namespace shooting {


	DECLARE_DX12SHADER(BcVSPNTBonePL)
	DECLARE_DX12SHADER(BcVSPNTBonePLShadow)


	//--------------------------------------------------------------------------------------
	///	BcPNTBoneDrawコンポーネント
	//--------------------------------------------------------------------------------------
	class BcPNTBoneDraw : public Component {
	protected:
		BasicConstant m_ConstantBuffer;
		size_t m_ConstantBufferIndex;
		//自分自身に影を描画するかどうか
		bool m_OwnShadowActive;
		//フォグが有効かどうか
		bool m_FogEnabled = true;
		//フォグの開始位置
		float m_FogStart = -10.0f;
		//フォグの終了位置
		float m_FogEnd = -40.0f;
		//フォグ色
		XMFLOAT4 m_FogColor;
		//フォグベクトル
		XMFLOAT3 m_FogVector;

		std::vector<Mat4x4>  m_BoneTransforms;
		std::map<std::string, Mat4x4> m_NodeGlobalTransforms;
		std::vector<std::shared_ptr<BaseMaterial>> m_BaseMaterialVec;
		unsigned int m_CurrentAnimationIndex = 0;
		Vec3 m_ModelOffset = Vec3(0.0f, 0.0f, 0.0f);

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

		std::shared_ptr<BaseTexture> GetDrawTexture(size_t index);

		void AddBaseMaterial(const std::wstring& key)
		{
			m_BaseMaterialVec.push_back(BaseScene::Get()->GetMaterial(key));
		}

		void AddBaseMaterial(const std::shared_ptr<BaseMaterial>& material)
		{
			m_BaseMaterialVec.push_back(material);
		}

		std::shared_ptr<BaseMaterial> GetBaseMaterial(size_t index) const
		{
			if (index >= m_BaseMaterialVec.size())
			{
				return nullptr;
			}
			return m_BaseMaterialVec[index];
		}

		size_t GetBaseMaterialCount() const
		{
			return m_BaseMaterialVec.size();
		}

		void SetAnimationIndex(unsigned int index)
		{
			m_CurrentAnimationIndex = index;
		}

		unsigned int GetAnimationIndex() const
		{
			return m_CurrentAnimationIndex;
		}

		float GetCurrentAnimationDurationSeconds();

		int GetAnimationCount();
		std::wstring GetAnimationName(int index);

		void SetModelOffset(const Vec3& offset)
		{
			m_ModelOffset = offset;
		}

		const Vec3& GetModelOffset() const
		{
			return m_ModelOffset;
		}

		const std::vector<Mat4x4>& GetBoneTransforms() const
		{
			return m_BoneTransforms;
		}

		bool TryGetNodeGlobalTransform(const std::string& nodeName, Mat4x4& outTransform) const
		{
			auto it = m_NodeGlobalTransforms.find(nodeName);
			if (it == m_NodeGlobalTransforms.end())
			{
				return false;
			}

			outTransform = it->second;
			return true;
		}

		BcPNTBoneDraw(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~BcPNTBoneDraw() {}
		virtual void OnUpdateConstantBuffers()override;
		virtual void OnCommitConstantBuffers()override;
		virtual void OnCreate()override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)override;

		virtual bool UpdateAnimation(double animeTime);

	};

}
