#pragma once
#include "stdafx.h"

namespace shooting {

	struct CrosshairDesc
	{
		bool  enabled = false;
		float gap = 6.0f;
		float len = 10.0f;
		float thickness = 1.0f;
	};

	class UILayer
	{
	public:
		UILayer(UINT frameCount, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);

		void UpdateLabels(const std::wstring& uiText);

		void ClearDrawCommands();
		void AddTextBlock(
			const std::wstring& text,
			const D2D1_RECT_F& rect,
			DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING,
			D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::White),
			float fontSize = 0.0f);

		void AddImageBlock(
			const std::wstring& path,
			const D2D1_RECT_F& rect,
			float opacity = 1.0f);

		void AddProgressBar(
			const D2D1_RECT_F& rect,
			float value,
			float maxValue,
			const std::wstring& label = L"");

		void AddButtonBlock(
			const D2D1_RECT_F& rect,
			const std::wstring& text,
			const D2D1_COLOR_F& baseColor,
			const D2D1_COLOR_F& hoverColor,
			const D2D1_COLOR_F& textColor,
			bool hovered);

		void Render(UINT frameIndex);
		void ReleaseResources();
		void Resize(Microsoft::WRL::ComPtr<ID3D12Resource>* ppRenderTargets, UINT width, UINT height);

		float GetWidth() const { return m_width; }
		float GetHeight() const { return m_height; }

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
		IDWriteTextFormat* ResolveTextFormat(DWRITE_TEXT_ALIGNMENT align) const;
		Microsoft::WRL::ComPtr<IDWriteTextFormat> CreateTextFormat(float fontSize, DWRITE_TEXT_ALIGNMENT align) const;
		Microsoft::WRL::ComPtr<ID2D1Bitmap1> GetOrLoadBitmap(const std::wstring& path);
		Microsoft::WRL::ComPtr<ID2D1Bitmap1> LoadBitmapFromFile(const std::wstring& path) const;

		float m_width;
		float m_height;

		struct TextBlock
		{
			std::wstring text;
			D2D1_RECT_F layout;
			D2D1_COLOR_F color;
			IDWriteTextFormat* pFormat = nullptr;
			Microsoft::WRL::ComPtr<IDWriteTextFormat> customFormat;
		};

		struct ProgressBarBlock
		{
			D2D1_RECT_F layout;
			float value = 0.0f;
			float maxValue = 1.0f;
			std::wstring label;
		};

		struct ImageBlock
		{
			std::wstring path;
			D2D1_RECT_F layout;
			float opacity = 1.0f;
			Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
		};

		struct ButtonBlock
		{
			D2D1_RECT_F layout;
			std::wstring text;
			D2D1_COLOR_F baseColor;
			D2D1_COLOR_F hoverColor;
			D2D1_COLOR_F textColor;
			bool hovered = false;
		};

		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12Device;
		Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
		Microsoft::WRL::ComPtr<IWICImagingFactory2> m_wicFactory;
		Microsoft::WRL::ComPtr<ID2D1Factory3> m_d2dFactory;
		Microsoft::WRL::ComPtr<ID2D1Device2> m_d2dDevice;
		Microsoft::WRL::ComPtr<ID2D1DeviceContext2> m_d2dDeviceContext;
		std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> m_wrappedRenderTargets;
		std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap1>> m_d2dRenderTargets;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;

		Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatLeft;
		Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatCenter;
		Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormatRight;
		float m_baseFontSize = 18.0f;

		std::vector<TextBlock> m_textBlocks;
		std::vector<ProgressBarBlock> m_progressBars;
		std::vector<ImageBlock> m_imageBlocks;
		std::vector<ButtonBlock> m_buttons;
		std::map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap1>> m_bitmapCache;

		CrosshairDesc m_crosshair;
	};

}
