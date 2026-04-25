#include "stdafx.h"
#include "Project.h"

namespace shooting {

	void UIManager::BeginFrame()
	{
		m_texts.clear();
		m_bars.clear();
		m_buttons.clear();
	}

	void UIManager::AddText(
		const std::wstring& text,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size,
		UITextAlign align)
	{
		TextCommand cmd;
		cmd.text = text;
		cmd.anchor = anchor;
		cmd.offset = offset;
		cmd.size = size;
		cmd.align = align;
		m_texts.push_back(cmd);
	}

	void UIManager::AddProgressBar(
		const std::wstring& label,
		float value,
		float maxValue,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size)
	{
		ProgressBarCommand cmd;
		cmd.label = label;
		cmd.value = value;
		cmd.maxValue = maxValue;
		cmd.anchor = anchor;
		cmd.offset = offset;
		cmd.size = size;
		m_bars.push_back(cmd);
	}

	bool UIManager::IsPointInRect(float x, float y, const D2D1_RECT_F& rect)
	{
		return x >= rect.left &&
			x <= rect.right &&
			y >= rect.top &&
			y <= rect.bottom;
	}

	UIButtonResult UIManager::AddButton(
		const std::wstring& text,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size,
		const D2D1_COLOR_F& baseColor,
		const D2D1_COLOR_F& hoverColor,
		const D2D1_COLOR_F& textColor)
	{
		auto device = BaseDevice::GetBaseDevice();
		const float screenW = static_cast<float>(device->GetWidth());
		const float screenH = static_cast<float>(device->GetHeight());

		D2D1_RECT_F rect = ResolveRect(screenW, screenH, anchor, offset, size);

		const auto& mouse = App::GetInputDevice().GetMouseState();
		const bool hovered = IsPointInRect(
			static_cast<float>(mouse.now.x),
			static_cast<float>(mouse.now.y),
			rect);

		const bool clicked = hovered && App::GetInputDevice().MousePressed(VK_LBUTTON);

		ButtonCommand cmd;
		cmd.text = text;
		cmd.baseColor = baseColor;
		cmd.hoverColor = hoverColor;
		cmd.textColor = textColor;
		cmd.anchor = anchor;
		cmd.offset = offset;
		cmd.size = size;
		cmd.hovered = hovered;
		m_buttons.push_back(cmd);

		UIButtonResult result;
		result.hovered = hovered;
		result.clicked = clicked;
		result.rect = rect;
		return result;
	}

	D2D1_RECT_F UIManager::ResolveRect(
		float screenW,
		float screenH,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size)
	{
		float left = 0.0f;
		float top = 0.0f;

		switch (anchor)
		{
		case UIAnchor::TopLeft:
			left = offset.x;
			top = offset.y;
			break;
		case UIAnchor::TopCenter:
			left = (screenW - size.w) * 0.5f + offset.x;
			top = offset.y;
			break;
		case UIAnchor::TopRight:
			left = (screenW - size.w) + offset.x;
			top = offset.y;
			break;
		case UIAnchor::CenterLeft:
			left = offset.x;
			top = (screenH - size.h) * 0.5f + offset.y;
			break;
		case UIAnchor::Center:
			left = (screenW - size.w) * 0.5f + offset.x;
			top = (screenH - size.h) * 0.5f + offset.y;
			break;
		case UIAnchor::CenterRight:
			left = (screenW - size.w) + offset.x;
			top = (screenH - size.h) * 0.5f + offset.y;
			break;
		case UIAnchor::BottomLeft:
			left = offset.x;
			top = (screenH - size.h) + offset.y;
			break;
		case UIAnchor::BottomCenter:
			left = (screenW - size.w) * 0.5f + offset.x;
			top = (screenH - size.h) + offset.y;
			break;
		case UIAnchor::BottomRight:
			left = (screenW - size.w) + offset.x;
			top = (screenH - size.h) + offset.y;
			break;
		default:
			break;
		}

		return D2D1::RectF(left, top, left + size.w, top + size.h);
	}

	void UIManager::Render(UILayer& layer) const
	{
		layer.ClearDrawCommands();

		const float screenW = layer.GetWidth();
		const float screenH = layer.GetHeight();

		for (const auto& bar : m_bars)
		{
			layer.AddProgressBar(
				ResolveRect(screenW, screenH, bar.anchor, bar.offset, bar.size),
				bar.value,
				bar.maxValue,
				bar.label);
		}

		for (const auto& button : m_buttons)
		{
			D2D1_RECT_F rect = ResolveRect(
				screenW,
				screenH,
				button.anchor,
				button.offset,
				button.size);

			std::wstring text = button.text;
			if (button.hovered)
			{
				text = L"> " + text + L" <";
			}

			layer.AddButtonBlock(
				rect,
				button.text,
				button.baseColor,
				button.hoverColor,
				button.textColor,
				button.hovered);
		}

		for (const auto& text : m_texts)
		{
			DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
			switch (text.align)
			{
			case UITextAlign::Left:
				alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
				break;
			case UITextAlign::Center:
				alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
				break;
			case UITextAlign::Right:
				alignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
				break;
			}

			layer.AddTextBlock(
				text.text,
				ResolveRect(screenW, screenH, text.anchor, text.offset, text.size),
				alignment);
		}
	}

}