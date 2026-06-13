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

/*!
@file PrimDevice.h
@brief デバイス親クラス
@copyright WiZ Tamura Hiroki,Yamanoi Yasushi MIT License (MIT).
 MIT License URL: https://opensource.org/license/mit
*/
#pragma once

#include "stdafx.h"

namespace shooting {


	/// <summary>
	/// プリミティブなデバイスクラス
	/// PrimDeviceは最小限の責務を持ち、DirectX固有の複雑さをBaseDeviceに委譲することで、疎結合で再利用可能な設計を実現
	/// </summary>
	class PrimDevice
	{
	public:
		PrimDevice(UINT width, UINT height, std::wstring name);
		virtual ~PrimDevice();

		virtual void OnInit() = 0;
		virtual void OnUpdate() = 0;
		virtual void OnRender() = 0;
		virtual void SetToBefore() {}
		virtual void OnUpdateDraw();
		virtual void OnSizeChanged(UINT width, UINT height, bool minimized) = 0;
		virtual void OnDestroy() = 0;

		// Samples override the event handlers to handle specific messages.
		// 日本語： サンプルはイベントハンドラをオーバーライドして、特定のメッセージを処理します。
		virtual void OnKeyDown(UINT8 /*key*/) {}
		virtual void OnKeyUp(UINT8 /*key*/) {}
		virtual void OnWindowMoved(int /*x*/, int /*y*/) {}
		virtual void OnMouseMove(UINT /*x*/, UINT /*y*/) {}
		virtual void OnLeftButtonDown(UINT /*x*/, UINT /*y*/) {}
		virtual void OnLeftButtonUp(UINT /*x*/, UINT /*y*/) {}
		virtual void OnDisplayChanged() {}
		// アクセサ
		UINT GetWidth() const { return m_width; }
		UINT GetHeight() const { return m_height; }
		const WCHAR* GetTitle() const { return m_title.c_str(); }
		bool GetTearingSupport() const { return m_tearingSupport; }
		RECT GetWindowsBounds() const { return m_windowBounds; }
		virtual IDXGISwapChain* GetSwapchain() { return nullptr; }
		virtual ID3D12Device* GetD3D12Device() { return nullptr; }
		virtual ComPtr<ID3D12Device> GetID3D12Device() { return nullptr; }
		bool IsQuiteEscapeKey() const { return m_quiteEscapeKey; }
		void SetQuiteEscapeKey(bool b) { m_quiteEscapeKey = b; }

		void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);
		void UpdateForSizeChange(UINT clientWidth, UINT clientHeight);
		void SetWindowBounds(int left, int top, int right, int bottom);
		std::wstring GetAssetFullPath(LPCWSTR assetName);

		// フレーム確定後に参照するマウス状態
		struct MouseFrameState
		{
			int x = 0;          // 現在座標
			int y = 0;
			int deltaX = 0;     // このフレームの移動量（累積確定）
			int deltaY = 0;
			int wheelDelta = 0; // このフレームのホイール量（±120の累積）
			bool hasPos = false;
		};

		// WindowProc から呼ぶ
		virtual void OnMouseWheel(int wheelDelta);

		// ゲーム側（カメラなど）から参照する
		const MouseFrameState& GetMouseFrameState() const { return m_mouseFrame; }

	protected:
		void GetHardwareAdapter(
			_In_ IDXGIFactory1* pFactory,
			_Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
			bool requestHighPerformanceAdapter = false);

		void SetCustomWindowText(LPCWSTR text);
		void CheckTearingSupport();

		// Viewport dimensions.
		UINT m_width;
		UINT m_height;
		float m_aspectRatio;

		//ESCキーで終了させるかどうか
		bool m_quiteEscapeKey;


		// Window bounds
		RECT m_windowBounds;

		// Whether or not tearing is available for fullscreen borderless windowed mode.
		bool m_tearingSupport;

		// Adapter info.
		bool m_useWarpDevice;

		// Override to be able to start without Dx11on12 UI for PIX. PIX doesn't support 11 on 12. 
		bool m_enableUI;

		// 1フレーム分の入力を確定させる（OnUpdateDrawの先頭で呼ぶ）
		void FlushMouseInputForFrame();
	private:
		// Root assets path.
		std::wstring m_assetsPath;

		// Window title.
		std::wstring m_title;

		// ---- メッセージで届く入力をここに累積（フレーム開始時に確定→クリア） ----
		int  m_mouseAccumDX = 0;
		int  m_mouseAccumDY = 0;
		int  m_wheelAccum = 0;

		// ---- 最新の絶対座標 ----
		int  m_mouseX = 0;
		int  m_mouseY = 0;
		bool m_mouseHasPos = false;

		// ---- フレーム確定値 ----
		MouseFrameState m_mouseFrame;

		// パフォーマンス比較中は、描画負荷の差がFPSへ直接反映されるよう上限を無効にする。
		// 通常プレイ向けに60FPS制限へ戻す場合は true に変更する。
		static constexpr bool kEnableFrameRateLimit = true;

		// ---- 60FPS上限用 ----
		void LimitFrameRate();
		bool m_frameLimiterInitialized = false;
		bool m_timerPeriodRaised = false;
		LARGE_INTEGER m_frameLimiterFrequency{};
		LARGE_INTEGER m_nextFrameTime{};
	};
}
