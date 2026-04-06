#include "stdafx.h"
#include "UILayer.h"

namespace shooting {

	UILayer::UILayer(UINT frameCount, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue) :
		m_width(0.0f),
		m_height(0.0f)
	{
		m_wrappedRenderTargets.resize(frameCount);
		m_d2dRenderTargets.resize(frameCount);
		Initialize(pDevice, pCommandQueue);
	}

	void UILayer::Initialize(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue)
	{
		UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		ComPtr<ID3D11Device> d3d11Device;

		ThrowIfFailed(D3D11On12CreateDevice(
			pDevice,
			d3d11DeviceFlags,
			nullptr,
			0,
			reinterpret_cast<IUnknown**>(&pCommandQueue),
			1,
			0,
			&d3d11Device,
			&m_d3d11DeviceContext,
			nullptr));

		ThrowIfFailed(d3d11Device.As(&m_d3d11On12Device));

		D2D1_DEVICE_CONTEXT_OPTIONS deviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;
		ThrowIfFailed(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &m_d2dFactory));

		ComPtr<IDXGIDevice> dxgiDevice;
		ThrowIfFailed(m_d3d11On12Device.As(&dxgiDevice));
		ThrowIfFailed(m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice));
		ThrowIfFailed(m_d2dDevice->CreateDeviceContext(deviceOptions, &m_d2dDeviceContext));

		ThrowIfFailed(DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			&m_dwriteFactory));
	}

	void UILayer::UpdateLabels(const std::wstring& uiText)
	{
		ClearDrawCommands();
		AddTextBlock(uiText, D2D1::RectF(20.0f, 20.0f, m_width - 20.0f, m_height - 20.0f), DWRITE_TEXT_ALIGNMENT_LEADING);
	}

	void UILayer::ClearDrawCommands()
	{
		m_textBlocks.clear();
		m_progressBars.clear();
	}

	IDWriteTextFormat* UILayer::ResolveTextFormat(DWRITE_TEXT_ALIGNMENT align) const
	{
		switch (align)
		{
		case DWRITE_TEXT_ALIGNMENT_CENTER:
			return m_textFormatCenter.Get();
		case DWRITE_TEXT_ALIGNMENT_TRAILING:
			return m_textFormatRight.Get();
		case DWRITE_TEXT_ALIGNMENT_LEADING:
		default:
			return m_textFormatLeft.Get();
		}
	}

	void UILayer::AddTextBlock(
		const std::wstring& text,
		const D2D1_RECT_F& rect,
		DWRITE_TEXT_ALIGNMENT align)
	{
		TextBlock block;
		block.text = text;
		block.layout = rect;
		block.pFormat = ResolveTextFormat(align);
		m_textBlocks.push_back(block);
	}

	void UILayer::AddProgressBar(
		const D2D1_RECT_F& rect,
		float value,
		float maxValue,
		const std::wstring& label)
	{
		ProgressBarBlock block;
		block.layout = rect;
		block.value = value;
		block.maxValue = (maxValue > 0.0f) ? maxValue : 1.0f;
		block.label = label;
		m_progressBars.push_back(block);
	}

	void UILayer::Render(UINT frameIndex)
	{
		ID3D11Resource* ppResources[] = { m_wrappedRenderTargets[frameIndex].Get() };

		m_d2dDeviceContext->SetTarget(m_d2dRenderTargets[frameIndex].Get());
		m_d3d11On12Device->AcquireWrappedResources(ppResources, _countof(ppResources));

		m_d2dDeviceContext->BeginDraw();

		// バーを先に描画
		for (const auto& bar : m_progressBars)
		{
			float ratio = bar.value / bar.maxValue;
			if (ratio < 0.0f) ratio = 0.0f;
			if (ratio > 1.0f) ratio = 1.0f;

			const auto& r = bar.layout;
			const float width = r.right - r.left;

			D2D1_RECT_F fillRect = r;
			fillRect.right = fillRect.left + width * ratio;

			// 背景
			m_textBrush->SetColor(D2D1::ColorF(0.12f, 0.12f, 0.12f, 0.80f));
			m_d2dDeviceContext->FillRectangle(r, m_textBrush.Get());

			// HP本体
			m_textBrush->SetColor(D2D1::ColorF(0.85f, 0.15f, 0.15f, 0.95f));
			m_d2dDeviceContext->FillRectangle(fillRect, m_textBrush.Get());

			// 枠
			m_textBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
			m_d2dDeviceContext->DrawRectangle(r, m_textBrush.Get(), 2.0f);

			// ラベル
			if (!bar.label.empty())
			{
				m_d2dDeviceContext->DrawText(
					bar.label.c_str(),
					static_cast<UINT>(bar.label.length()),
					m_textFormatCenter.Get(),
					r,
					m_textBrush.Get());
			}
		}

		// テキスト
		m_textBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
		for (const auto& textBlock : m_textBlocks)
		{
			m_d2dDeviceContext->DrawText(
				textBlock.text.c_str(),
				static_cast<UINT>(textBlock.text.length()),
				textBlock.pFormat,
				textBlock.layout,
				m_textBrush.Get());
		}

		// クロスヘア
		if (m_crosshair.enabled)
		{
			const float cx = std::floor(m_width * 0.5f) + 0.5f;
			const float cy = std::floor(m_height * 0.5f) + 0.5f;

			const float g = m_crosshair.gap;
			const float L = m_crosshair.len;
			const float t = m_crosshair.thickness;

			m_d2dDeviceContext->DrawLine(
				D2D1::Point2F(cx, cy - g - L),
				D2D1::Point2F(cx, cy - g),
				m_textBrush.Get(), t);

			m_d2dDeviceContext->DrawLine(
				D2D1::Point2F(cx, cy + g),
				D2D1::Point2F(cx, cy + g + L),
				m_textBrush.Get(), t);

			m_d2dDeviceContext->DrawLine(
				D2D1::Point2F(cx - g - L, cy),
				D2D1::Point2F(cx - g, cy),
				m_textBrush.Get(), t);

			m_d2dDeviceContext->DrawLine(
				D2D1::Point2F(cx + g, cy),
				D2D1::Point2F(cx + g + L, cy),
				m_textBrush.Get(), t);
		}

		m_d2dDeviceContext->EndDraw();

		m_d3d11On12Device->ReleaseWrappedResources(ppResources, _countof(ppResources));
		m_d3d11DeviceContext->Flush();
	}

	void UILayer::ReleaseResources()
	{
		for (UINT i = 0; i < FrameCount(); i++)
		{
			ID3D11Resource* ppResources[] = { m_wrappedRenderTargets[i].Get() };
			m_d3d11On12Device->ReleaseWrappedResources(ppResources, _countof(ppResources));
		}

		m_d2dDeviceContext->SetTarget(nullptr);
		m_d3d11DeviceContext->Flush();

		for (UINT i = 0; i < FrameCount(); i++)
		{
			m_d2dRenderTargets[i].Reset();
			m_wrappedRenderTargets[i].Reset();
		}

		m_textBlocks.clear();
		m_progressBars.clear();

		m_textBrush.Reset();
		m_textFormatLeft.Reset();
		m_textFormatCenter.Reset();
		m_textFormatRight.Reset();
		m_d2dDeviceContext.Reset();
		m_dwriteFactory.Reset();
		m_d2dDevice.Reset();
		m_d2dFactory.Reset();
		m_d3d11DeviceContext.Reset();
		m_d3d11On12Device.Reset();
	}

	void UILayer::Resize(ComPtr<ID3D12Resource>* ppRenderTargets, UINT width, UINT height)
	{
		m_width = static_cast<float>(width);
		m_height = static_cast<float>(height);

		D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

		for (UINT i = 0; i < FrameCount(); i++)
		{
			D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };

			ThrowIfFailed(m_d3d11On12Device->CreateWrappedResource(
				ppRenderTargets[i].Get(),
				&d3d11Flags,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT,
				IID_PPV_ARGS(&m_wrappedRenderTargets[i])));

			ComPtr<IDXGISurface> surface;
			ThrowIfFailed(m_wrappedRenderTargets[i].As(&surface));

			ThrowIfFailed(m_d2dDeviceContext->CreateBitmapFromDxgiSurface(
				surface.Get(),
				&bitmapProperties,
				&m_d2dRenderTargets[i]));
		}

		ThrowIfFailed(m_d2dDeviceContext->CreateSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::White),
			&m_textBrush));

		const float fontSize = bsmUtil::Max(18.0f, m_height / 34.0f);

		auto CreateFormat = [&](Microsoft::WRL::ComPtr<IDWriteTextFormat>& outFormat, DWRITE_TEXT_ALIGNMENT align)
			{
				ThrowIfFailed(m_dwriteFactory->CreateTextFormat(
					L"Arial",
					nullptr,
					DWRITE_FONT_WEIGHT_NORMAL,
					DWRITE_FONT_STYLE_NORMAL,
					DWRITE_FONT_STRETCH_NORMAL,
					fontSize,
					L"ja-jp",
					&outFormat));

				ThrowIfFailed(outFormat->SetTextAlignment(align));
				ThrowIfFailed(outFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
			};

		CreateFormat(m_textFormatLeft, DWRITE_TEXT_ALIGNMENT_LEADING);
		CreateFormat(m_textFormatCenter, DWRITE_TEXT_ALIGNMENT_CENTER);
		CreateFormat(m_textFormatRight, DWRITE_TEXT_ALIGNMENT_TRAILING);

		m_d2dDeviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		m_d2dDeviceContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
	}

}