#include "stdafx.h"
#include "Project.h"

namespace shooting {

	void UIManager::BeginFrame()
	{
		m_texts.clear();
		m_bars.clear();
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