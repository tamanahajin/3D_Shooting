//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once
#include "stdafx.h"

namespace shooting {

	class UILayer;
	class BaseScene;
	class Scene;

	/// <summary>
	/// デバイスの基本クラスを表します。
	/// DirectX12などのグラフィックスAPIに依存する具体的なデバイス実装の基底クラスとして機能
	/// </summary>
	class BaseDevice : public PrimDevice
	{
	public:
		BaseDevice(UINT width, UINT height, std::wstring name);
		~BaseDevice();

		static const UINT FrameCount = 3;

		/// <summary>
		/// スワップチェーンを取得
		/// </summary>
		/// <returns></returns>
		virtual IDXGISwapChain* GetSwapchain() { return m_swapChain.Get(); }
		virtual ID3D12Device* GetD3D12Device() { return m_device.Get(); }
		virtual ComPtr<ID3D12Device> GetID3D12Device() { return m_device; }
		/// <summary>
		/// シーン（親クラス）を取得
		/// </summary>
		/// <returns></returns>
		static std::unique_ptr<BaseScene>& GetScene()
		{
			return s_app->m_scene;
		}
		static BaseDevice* GetBaseDevice()
		{
			return s_app;
		}
		/// <summary>
		/// 1秒に1回更新される安定したFPS値を取得
		/// </summary>
		/// <returns></returns>
		float GetStableFps() const
		{
			return m_fps;
		}
		/// <summary>
		/// 1秒に1回更新される安定した経過時間を取得
		/// </summary>
		/// <returns></returns>
		double GetStableElapsedTime() const
		{
			return m_elapsedTime;
		}
	protected:
		virtual void OnInit();
		virtual void OnKeyDown(UINT8 key);
		virtual void OnKeyUp(UINT8 key);
		virtual void OnSizeChanged(UINT width, UINT height, bool minimized);
		virtual void OnUpdate();
		virtual void OnRender();
		virtual void SetToBefore();
		virtual void OnDestroy();

	private:
		// GPU adapter management.
		struct DxgiAdapterInfo
		{
			DXGI_ADAPTER_DESC1 desc;
			bool supportsDx12FL11;
		};
		DXGI_GPU_PREFERENCE m_activeGpuPreference;
		std::map<DXGI_GPU_PREFERENCE, std::wstring> m_gpuPreferenceToName;
		UINT m_activeAdapter;
		LUID m_activeAdapterLuid;
		std::vector<DxgiAdapterInfo> m_gpuAdapterDescs;
		bool m_manualAdapterSelection;
		HANDLE m_adapterChangeEvent;
		// アダプタ登録時に返ってくるID
		DWORD m_adapterChangeRegistrationCookie;

		// D3D objects.
		ComPtr<ID3D12Device> m_device;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
#ifdef USE_DXGI_1_6
		ComPtr<IDXGISwapChain4> m_swapChain;
		ComPtr<IDXGIFactory6>   m_dxgiFactory;
#else
		ComPtr<IDXGISwapChain3> m_swapChain;
		ComPtr<IDXGIFactory2>   m_dxgiFactory;
#endif
		UINT m_dxgiFactoryFlags;
		ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
		ComPtr<ID3D12Fence> m_fence;

		// BaseScene rendering resources
		std::unique_ptr<BaseScene> m_scene;

		// UILayer
		std::unique_ptr<UILayer> m_uiLayer;
		bool m_bCtrlKeyIsPressed;
		float m_fps;
		double m_elapsedTime;

		StepTimer m_timer;

		// Frame synchronization objects
		UINT   m_frameIndex;
		HANDLE m_fenceEvent;
		UINT64 m_fenceValues[FrameCount];

		// Window state
		bool m_windowVisible;
		bool m_windowedMode;

		// Singleton object so that worker threads can share members.
		static BaseDevice* s_app;

		void LoadPipeline();
		void LoadAssets();
		/// <summary>
		/// 画面サイズに依存するリソースを用意し直す
		/// </summary>
		void LoadSizeDependentResources();
		/// <summary>
		/// 画面サイズに依存するリソースを解放
		/// </summary>
		void ReleaseSizeDependentResources();
		void UpdateUI();
		/// <summary>
		/// Direct3D/DXGIリソースを再作成
		/// </summary>
		void RecreateD3Dresources();
		/// <summary>
		/// Direct3D/DXGIリソースを解放
		/// </summary>
		void ReleaseD3DObjects();
		void EnumerateGPUadapters();
		void GetGPUAdapter(UINT adapterIndex, IDXGIAdapter1** ppAdapter);
		bool QueryForAdapterEnumerationChanges();
		HRESULT ValidateActiveAdapter();
		bool RetrieveAdapterIndex(UINT* adapterIndex, LUID prevActiveAdapterLuid);
		void SelectAdapter(UINT index);
		void SelectGPUPreference(UINT index);
		void CalculateFrameStats();
		void WaitForGpu(ID3D12CommandQueue* pCommandQueue);
		void MoveToNextFrame();
	};
}