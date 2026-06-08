#pragma once
#include "stdafx.h"

namespace shooting {

	DECLARE_DX12SHADER(InstancedVSPNTStaticPL)
	DECLARE_DX12SHADER(InstancedVSPNTBonePL)
	DECLARE_DX12SHADER(InstancedPSPNTPL)
	DECLARE_DX12SHADER(InstancedVSShadowmap)

	struct StaticInstanceData
	{
		XMFLOAT4X4 matrix;
	};

	class InstancedStaticDraw : public Component
	{
	private:
		BasicConstant m_ConstantBuffer{};
		size_t m_ConstantBufferIndex = 0;
		std::vector<BasicConstant> m_MaterialConstantBuffers;
		std::vector<size_t> m_MaterialConstantBufferIndices;

		bool m_OwnShadowActive = false;
		bool m_CastShadowActive = false;
		bool m_UseBaseColorOverride = false;
		bool m_UseMaterialTexture = true;
		bool m_LightingEnabled = true;
		bool m_FogEnabled = true;
		float m_FogStart = -25.0f;
		float m_FogEnd = -40.0f;
		XMFLOAT4 m_FogColor = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
		Col4 m_BaseColorOverride = Col4(1.0f);

		std::wstring m_MeshKey;
		std::wstring m_MaterialPrefix;
		// 空でない場合、shadow pass だけ通常モデルではなくこの軽量メッシュを使う。
		std::wstring m_ShadowMeshKey;

		std::vector<Mat4x4> m_InstanceWorlds;
		std::vector<StaticInstanceData> m_InstanceData;

		std::vector<Mat4x4> m_ShadowInstanceWorlds;
		std::vector<StaticInstanceData> m_ShadowInstanceData;

		ComPtr<ID3D12Resource> m_InstanceBuffer;
		UINT m_InstanceBufferCapacityBytes = 0;
		D3D12_VERTEX_BUFFER_VIEW m_InstanceBufferView{};

		ComPtr<ID3D12Resource> m_ShadowInstanceBuffer;
		UINT m_ShadowInstanceBufferCapacityBytes = 0;
		D3D12_VERTEX_BUFFER_VIEW m_ShadowInstanceBufferView{};

		void BuildStaticInstanceBuffer(
			const std::vector<Mat4x4>& worlds,
			std::vector<StaticInstanceData>& instanceData,
			ComPtr<ID3D12Resource>& instanceBuffer,
			UINT& instanceBufferCapacityBytes,
			D3D12_VERTEX_BUFFER_VIEW& instanceBufferView);
		void EnsureMaterialConstantBuffers(size_t materialCount);

	public:
		explicit InstancedStaticDraw(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~InstancedStaticDraw() {}

		void SetMeshKey(const std::wstring& key) { m_MeshKey = key; }
		void SetMaterialPrefix(const std::wstring& prefix) { m_MaterialPrefix = prefix; }
		void SetShadowMeshKey(const std::wstring& key) { m_ShadowMeshKey = key; }
		void ClearShadowMeshKey() { m_ShadowMeshKey.clear(); }
		void SetInstanceWorlds(const std::vector<Mat4x4>& worlds) { m_InstanceWorlds = worlds; }
		void SetShadowInstanceWorlds(const std::vector<Mat4x4>& worlds) { m_ShadowInstanceWorlds = worlds; }
		void SetBaseColorOverride(const Col4& color) { m_UseBaseColorOverride = true; m_BaseColorOverride = color; }
		void ClearBaseColorOverride() { m_UseBaseColorOverride = false; m_BaseColorOverride = Col4(1.0f); }
		void SetUseMaterialTexture(bool b) { m_UseMaterialTexture = b; }
		void SetLightingEnabled(bool b) { m_LightingEnabled = b; }
		bool IsSetFogEnabled() const { return m_FogEnabled; }
		void SetFogEnabled(bool b) { m_FogEnabled = b; }

		void BuildInstanceBuffer();
		void BuildShadowInstanceBuffer();

		bool IsOwnShadowActive() const { return m_OwnShadowActive; }
		void SetOwnShadowActive(bool b) { m_OwnShadowActive = b; }
		bool IsCastShadowActive() const { return m_CastShadowActive; }
		void SetCastShadowActive(bool b) { m_CastShadowActive = b; }

		virtual void OnCreate() override;
		virtual void OnUpdateConstantBuffers() override;
		virtual void OnCommitConstantBuffers() override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList) override;
		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList) override;
	};

	struct SkinnedInstanceSource
	{
		Mat4x4 world;
		unsigned int animationIndex = 0;
		float animationTime = 0.0f;
		float damage = 0.0f;
	};

	struct SkinnedInstanceData
	{
		XMFLOAT4X4 matrix;
		XMFLOAT4 params;
	};

	class InstancedSkinnedDraw : public Component
	{
	private:
		BasicConstant m_ConstantBuffer{};
		size_t m_ConstantBufferIndex = 0;

		bool m_OwnShadowActive = false;
		bool m_CastShadowActive = false;

		std::wstring m_MeshKey;
		std::wstring m_TextureKey;

		std::vector<SkinnedInstanceSource> m_InstanceSources;
		std::vector<SkinnedInstanceData> m_InstanceData;
		std::vector<XMFLOAT4> m_BoneRows;
		std::vector<Mat4x4> m_WorkBones;
		std::map<unsigned long long, UINT> m_BonePoseStartByKey;
		std::map<unsigned long long, std::vector<XMFLOAT4>> m_BonePoseRowsCache;

		ComPtr<ID3D12Resource> m_InstanceBuffer;
		ComPtr<ID3D12Resource> m_BoneBuffer;
		UINT m_InstanceBufferCapacityBytes = 0;
		UINT m_BoneBufferCapacityBytes = 0;
		UINT m_BoneSrvIndex = UINT_MAX;
		void* m_MappedInstanceBuffer = nullptr;
		void* m_MappedBoneBuffer = nullptr;
		D3D12_VERTEX_BUFFER_VIEW m_InstanceBufferView{};

		float m_AnimationSampleFps = 20.0f;

		UINT EnsureBonePose(
			const std::shared_ptr<BaseAssimp>& assimp,
			unsigned int animationIndex,
			float animationTime);
		void EnsureInstanceBuffer(UINT bufferSize);
		void EnsureBoneBuffer(UINT bufferSize);
		void ReleaseMappedBuffers();
		float GetQuantizedAnimationTime(
			const std::shared_ptr<BaseAssimp>& assimp,
			unsigned int animationIndex,
			float animationTime,
			unsigned int& frameIndex) const;

	public:
		explicit InstancedSkinnedDraw(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~InstancedSkinnedDraw();

		void SetMeshKey(const std::wstring& key) { if (m_MeshKey != key) { m_BonePoseRowsCache.clear(); } m_MeshKey = key; }
		void SetTextureKey(const std::wstring& key) { m_TextureKey = key; }
		void SetInstances(const std::vector<SkinnedInstanceSource>& instances) { m_InstanceSources = instances; }
		void SetAnimationSampleFps(float fps) { m_AnimationSampleFps = (fps > 1.0f) ? fps : 1.0f; m_BonePoseRowsCache.clear(); }

		void BuildInstanceBuffer();

		bool IsOwnShadowActive() const { return m_OwnShadowActive; }
		void SetOwnShadowActive(bool b) { m_OwnShadowActive = b; }
		bool IsCastShadowActive() const { return m_CastShadowActive; }
		void SetCastShadowActive(bool b) { m_CastShadowActive = b; }

		virtual void OnCreate() override;
		virtual void OnUpdateConstantBuffers() override;
		virtual void OnCommitConstantBuffers() override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList) override;
		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList) override;
	};
}

