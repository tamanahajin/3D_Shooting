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
@file UILayer.h
@brief UIï∂éöóÒÉNÉâÉX
@copyright WiZ Tamura Hiroki,Yamanoi Yasushi MIT License (MIT).
 MIT License URL: https://opensource.org/license/mit
*/

#pragma once

#include "stdafx.h"

namespace shooting {

	struct CrosshairDesc
	{
		bool  enabled = false;
		float gap = 6.0f;   // íÜêSÇÃãÛÇ´
		float len = 10.0f;  // ê¸ÇÃí∑Ç≥
		float thickness = 1.0f;   // ê¸ÇÃëæÇ≥
	};

	class UILayer
	{
	public:
		UILayer(UINT frameCount, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);

		void UpdateLabels(const std::wstring& uiText);
		void Render(UINT frameIndex);
		void ReleaseResources();
		void Resize(Microsoft::WRL::ComPtr<ID3D12Resource>* ppRenderTargets, UINT width, UINT height);

		void SetCrosshairEnabled(bool e) { m_crosshair.enabled = e; }
		void SetCrosshairStyle(float gap, float len, float thickness)
		{
			m_crosshair.gap = gap;
			m_crosshair.len = len;
			m_crosshair.thickness = thickness;
		}
	private:
		UINT FrameCount() { return static_cast<UINT>(m_wrappedRenderTargets.size()); }
		void Initialize(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);

		// Render target dimensions
		float m_width;
		float m_height;

		struct TextBlock
		{
			std::wstring text;
			D2D1_RECT_F layout;
			IDWriteTextFormat* pFormat;
		};

		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12Device;
		Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
		Microsoft::WRL::ComPtr<ID2D1Factory3> m_d2dFactory;
		Microsoft::WRL::ComPtr<ID2D1Device2> m_d2dDevice;
		Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dDeviceContext;
		std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> m_wrappedRenderTargets;
		std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap1>> m_d2dRenderTargets;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;
		Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
		std::vector<TextBlock> m_textBlocks;

		CrosshairDesc m_crosshair;
	};
}