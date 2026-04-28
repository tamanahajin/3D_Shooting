#pragma once
#include "stdafx.h"

namespace shooting {

	DECLARE_DX12SHADER(InstancedVSPNTStaticPL)
	DECLARE_DX12SHADER(InstancedPSPNTPL)

	struct StaticInstanceData
	{
		XMFLOAT4X4 matrix;
	};

	class InstancedStaticDraw : public Component
	{
	private:
		BasicConstant m_ConstantBuffer{};
		size_t m_ConstantBufferIndex = 0;

		bool m_OwnShadowActive = false;

		std::wstring m_MeshKey;
		std::wstring m_MaterialPrefix;

		std::vector<Mat4x4> m_InstanceWorlds;
		std::vector<StaticInstanceData> m_InstanceData;

		ComPtr<ID3D12Resource> m_InstanceBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_InstanceBufferView{};

	public:
		explicit InstancedStaticDraw(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~InstancedStaticDraw() {}

		void SetMeshKey(const std::wstring& key) { m_MeshKey = key; }
		void SetMaterialPrefix(const std::wstring& prefix) { m_MaterialPrefix = prefix; }
		void SetInstanceWorlds(const std::vector<Mat4x4>& worlds) { m_InstanceWorlds = worlds; }

		void BuildInstanceBuffer();

		bool IsOwnShadowActive() const { return m_OwnShadowActive; }
		void SetOwnShadowActive(bool b) { m_OwnShadowActive = b; }

		virtual void OnCreate() override;
		virtual void OnUpdateConstantBuffers() override;
		virtual void OnCommitConstantBuffers() override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList) override;
	};
}