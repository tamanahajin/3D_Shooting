#pragma once
#include "stdafx.h"

struct ImGui_ImplDX12_InitInfo;

namespace shooting {

	class ImGuiLayer
	{
	public:
		ImGuiLayer(UINT frameCount, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue, DXGI_FORMAT rtvFormat);
		~ImGuiLayer();

		void BeginFrame(float fps, double elapsedTime);
		void Render(
			UINT frameIndex,
			ID3D12CommandQueue* pCommandQueue,
			ID3D12Resource* pRenderTarget,
			D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
			bool backbufferStartsInPresent,
			bool setBackbufferReadyForPresent);
		void Shutdown();

		static void HandleWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		static bool WantsMouseCapture();
		static bool WantsKeyboardCapture();

	private:
		struct DescriptorSlot
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
			D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
		};

		void CreateDescriptorHeap(ID3D12Device* pDevice);
		void CreateCommandObjects(ID3D12Device* pDevice, UINT frameCount);
		DescriptorSlot AllocateSrvDescriptor();
		void FreeSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);
		void DrawDebugWindow();

		static void AllocateSrvDescriptorCallback(
			ImGui_ImplDX12_InitInfo* info,
			D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
			D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle);
		static void FreeSrvDescriptorCallback(
			ImGui_ImplDX12_InitInfo* info,
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

		bool m_initialized = false;
		bool m_showDebugWindow = true;
		bool m_showDemoWindow = false;
		float m_fps = 0.0f;
		double m_elapsedTime = 0.0;

		UINT m_frameCount = 0;
		UINT m_srvDescriptorSize = 0;
		std::vector<bool> m_srvDescriptorUsed;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap;
		std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
	};
}
