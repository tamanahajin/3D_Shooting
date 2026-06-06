#include "stdafx.h"
#include "ImGuiLayer.h"
#include "DebugSettings.h"

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace shooting {

	namespace
	{
		constexpr UINT kImGuiSrvDescriptorCount = 64;
		constexpr const char* kJapaneseFontPath = "C:\\Windows\\Fonts\\meiryo.ttc";
	}

	ImGuiLayer::ImGuiLayer(UINT frameCount, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue, DXGI_FORMAT rtvFormat) :
		m_frameCount(frameCount)
	{
		if (!pDevice || !pCommandQueue)
		{
			throw HrException(DXGI_ERROR_DEVICE_REMOVED);
		}

		CreateDescriptorHeap(pDevice);
		CreateCommandObjects(pDevice, frameCount);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		ImGui::StyleColorsDark();

		if (!io.Fonts->AddFontFromFileTTF(kJapaneseFontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese()))
		{
			io.Fonts->AddFontDefault();
		}

		ImGui_ImplWin32_Init(App::GetHwnd());

		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device = pDevice;
		initInfo.CommandQueue = pCommandQueue;
		initInfo.NumFramesInFlight = static_cast<int>(frameCount);
		initInfo.RTVFormat = rtvFormat;
		initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
		initInfo.UserData = this;
		initInfo.SrvDescriptorHeap = m_srvDescriptorHeap.Get();
		initInfo.SrvDescriptorAllocFn = &ImGuiLayer::AllocateSrvDescriptorCallback;
		initInfo.SrvDescriptorFreeFn = &ImGuiLayer::FreeSrvDescriptorCallback;

		if (!ImGui_ImplDX12_Init(&initInfo))
		{
			throw BaseException(
				L"Failed to initialize ImGui DX12 backend.",
				L"ImGui_ImplDX12_Init()",
				L"ImGuiLayer::ImGuiLayer()"
			);
		}

		m_initialized = true;
	}

	ImGuiLayer::~ImGuiLayer()
	{
		Shutdown();
	}

	void ImGuiLayer::CreateDescriptorHeap(ID3D12Device* pDevice)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = kImGuiSrvDescriptorCount;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));
		m_srvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_srvDescriptorUsed.assign(kImGuiSrvDescriptorCount, false);
	}

	void ImGuiLayer::CreateCommandObjects(ID3D12Device* pDevice, UINT frameCount)
	{
		m_commandAllocators.resize(frameCount);
		for (UINT i = 0; i < frameCount; ++i)
		{
			ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
		}

		ThrowIfFailed(pDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_commandAllocators[0].Get(),
			nullptr,
			IID_PPV_ARGS(&m_commandList)));
		ThrowIfFailed(m_commandList->Close());
	}

	ImGuiLayer::DescriptorSlot ImGuiLayer::AllocateSrvDescriptor()
	{
		for (UINT i = 0; i < static_cast<UINT>(m_srvDescriptorUsed.size()); ++i)
		{
			if (m_srvDescriptorUsed[i])
			{
				continue;
			}

			m_srvDescriptorUsed[i] = true;

			DescriptorSlot slot;
			slot.cpu = m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			slot.gpu = m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			slot.cpu.ptr += static_cast<SIZE_T>(i) * m_srvDescriptorSize;
			slot.gpu.ptr += static_cast<UINT64>(i) * m_srvDescriptorSize;
			return slot;
		}

		throw BaseException(
			L"ImGui SRV descriptors are exhausted.",
			L"kImGuiSrvDescriptorCount",
			L"ImGuiLayer::AllocateSrvDescriptor()"
		);
	}

	void ImGuiLayer::FreeSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
	{
		const auto heapStart = m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		if (cpuHandle.ptr < heapStart.ptr || m_srvDescriptorSize == 0)
		{
			return;
		}

		const SIZE_T offset = cpuHandle.ptr - heapStart.ptr;
		if ((offset % m_srvDescriptorSize) != 0)
		{
			return;
		}

		const UINT index = static_cast<UINT>(offset / m_srvDescriptorSize);
		if (index < m_srvDescriptorUsed.size())
		{
			m_srvDescriptorUsed[index] = false;
		}
	}

	void ImGuiLayer::AllocateSrvDescriptorCallback(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
	{
		auto* layer = static_cast<ImGuiLayer*>(info->UserData);
		auto slot = layer->AllocateSrvDescriptor();
		*outCpuHandle = slot.cpu;
		*outGpuHandle = slot.gpu;
	}

	void ImGuiLayer::FreeSrvDescriptorCallback(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE)
	{
		auto* layer = static_cast<ImGuiLayer*>(info->UserData);
		layer->FreeSrvDescriptor(cpuHandle);
	}

	void ImGuiLayer::BeginFrame(float fps, double elapsedTime)
	{
		if (!m_initialized)
		{
			return;
		}

		m_fps = fps;
		m_elapsedTime = elapsedTime;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (ImGui::IsKeyPressed(ImGuiKey_F1))
		{
			m_showDebugWindow = !m_showDebugWindow;
		}

		DrawDebugWindow();
	}

	void ImGuiLayer::DrawDebugWindow()
	{
		if (!m_showDebugWindow)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(360.0f, 300.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Debug", &m_showDebugWindow))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("FPS: %.1f", m_fps);
		ImGui::Text("Frame Time: %.3f ms", m_elapsedTime * 1000.0);
		ImGui::Separator();
		ImGui::TextUnformatted("F1: Toggle debug window");
		ImGui::Separator();

		auto& debug = GameDebugSettingsStore::Get();
		ImGui::Checkbox("Player Invincible", &debug.playerInvincible);
		ImGui::Checkbox("Override Enemy Count", &debug.overrideEnemyCount);
		ImGui::BeginDisabled(!debug.overrideEnemyCount);
		ImGui::InputInt("Enemy Count", &debug.enemyCountOverride);
		ImGui::EndDisabled();
		ImGui::SliderFloat("Damage Multiplier", &debug.playerDamageMultiplier, 0.0f, 10.0f, "%.2f");
		ImGui::InputInt("Start Wave", &debug.startWave);
		ImGui::SliderFloat("Enemy Speed Multiplier", &debug.enemySpeedMultiplier, 0.1f, 5.0f, "%.2f");
		ImGui::Checkbox("Show Collision", &debug.showCollision);

		if (debug.enemyCountOverride < 0)
		{
			debug.enemyCountOverride = 0;
		}
		if (debug.playerDamageMultiplier < 0.0f)
		{
			debug.playerDamageMultiplier = 0.0f;
		}
		if (debug.playerDamageMultiplier > 10.0f)
		{
			debug.playerDamageMultiplier = 10.0f;
		}
		if (debug.startWave < 1)
		{
			debug.startWave = 1;
		}
		if (debug.enemySpeedMultiplier < 0.1f)
		{
			debug.enemySpeedMultiplier = 0.1f;
		}
		if (debug.enemySpeedMultiplier > 5.0f)
		{
			debug.enemySpeedMultiplier = 5.0f;
		}

		ImGui::End();
	}

	void ImGuiLayer::Render(
		UINT frameIndex,
		ID3D12CommandQueue* pCommandQueue,
		ID3D12Resource* pRenderTarget,
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
		bool backbufferStartsInPresent,
		bool setBackbufferReadyForPresent)
	{
		if (!m_initialized || !pCommandQueue || !pRenderTarget || frameIndex >= m_commandAllocators.size())
		{
			return;
		}

		ImGui::Render();

		auto commandAllocator = m_commandAllocators[frameIndex].Get();
		ThrowIfFailed(commandAllocator->Reset());
		ThrowIfFailed(m_commandList->Reset(commandAllocator, nullptr));

		if (backbufferStartsInPresent)
		{
			m_commandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					pRenderTarget,
					D3D12_RESOURCE_STATE_PRESENT,
					D3D12_RESOURCE_STATE_RENDER_TARGET));
		}

		m_commandList->OMSetRenderTargets(1, &renderTargetView, FALSE, nullptr);

		ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvDescriptorHeap.Get() };
		m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

		if (setBackbufferReadyForPresent)
		{
			m_commandList->ResourceBarrier(
				1,
				&CD3DX12_RESOURCE_BARRIER::Transition(
					pRenderTarget,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PRESENT));
		}

		ThrowIfFailed(m_commandList->Close());

		ID3D12CommandList* commandLists[] = { m_commandList.Get() };
		pCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	}

	void ImGuiLayer::Shutdown()
	{
		if (!m_initialized)
		{
			m_commandList.Reset();
			m_commandAllocators.clear();
			m_srvDescriptorHeap.Reset();
			m_srvDescriptorUsed.clear();
			return;
		}

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		m_initialized = false;
		m_commandList.Reset();
		m_commandAllocators.clear();
		m_srvDescriptorHeap.Reset();
		m_srvDescriptorUsed.clear();
	}

	void ImGuiLayer::HandleWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui::GetCurrentContext() == nullptr)
		{
			return;
		}

		ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
	}

	bool ImGuiLayer::WantsMouseCapture()
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
	}

	bool ImGuiLayer::WantsKeyboardCapture()
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
	}
}
