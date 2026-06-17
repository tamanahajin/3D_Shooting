#include "stdafx.h"
#include "Project.h"

namespace shooting {

	void UIManager::BeginFrame()
	{
		// 前フレームのホバー状態を残し、ボタンに入った瞬間だけ効果音を鳴らせるようにする。
		m_previousHoveredButtonIds.swap(m_currentHoveredButtonIds);
		m_currentHoveredButtonIds.clear();

		m_texts.clear();
		m_bars.clear();
		m_sliders.clear();
		m_images.clear();
		m_buttons.clear();
		m_backgroundOverlays.clear();
		m_overlays.clear();
	}

	void UIManager::AddText(
		const std::wstring& text,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size,
		UITextAlign align,
		D2D1_COLOR_F color,
		float fontSize)
	{
		TextCommand cmd;
		cmd.text = text;
		cmd.anchor = anchor;
		cmd.offset = offset;
		cmd.size = size;
		cmd.align = align;
		cmd.color = color;
		cmd.fontSize = fontSize;
		m_texts.push_back(cmd);
	}

	void UIManager::AddImage(
		const std::wstring& path,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size,
		float opacity)
	{
		ImageCommand cmd;
		cmd.path = path;
		cmd.anchor = anchor;
		cmd.offset = offset;
		cmd.size = size;
		cmd.opacity = opacity;
		m_images.push_back(cmd);
	}

	UIButtonResult UIManager::AddImageButton(
		const std::wstring& path,
		const std::wstring& buttonId,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size,
		float opacity,
		float hoverOpacity,
		const UIButtonBehavior& behavior)
	{
		auto device = BaseDevice::GetBaseDevice();
		const float screenW = static_cast<float>(device->GetWidth());
		const float screenH = static_cast<float>(device->GetHeight());
		const D2D1_RECT_F rect = ResolveRect(screenW, screenH, anchor, offset, size);
		const auto result = EvaluateButtonInteraction(
			MakeButtonHoverId(L"ImageButton", buttonId, rect),
			rect,
			behavior);

		AddImage(path, anchor, offset, size, result.hovered ? hoverOpacity : opacity);
		return result;
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

	void UIManager::AddSlider(
		const std::wstring& label,
		float value,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size)
	{
		SliderCommand cmd;
		cmd.label = label;
		cmd.value = bsmUtil::Clamp(value, 0.0f, 1.0f);
		cmd.anchor = anchor;
		cmd.offset = offset;
		cmd.size = size;
		m_sliders.push_back(cmd);
	}

	void UIManager::AddFullscreenOverlay(const D2D1_COLOR_F& color)
	{
		OverlayCommand cmd;
		cmd.color = color;
		m_overlays.push_back(cmd);
	}

	void UIManager::AddFullscreenBackgroundOverlay(const D2D1_COLOR_F& color)
	{
		OverlayCommand cmd;
		cmd.color = color;
		m_backgroundOverlays.push_back(cmd);
	}

	bool UIManager::IsPointInRect(float x, float y, const D2D1_RECT_F& rect)
	{
		return x >= rect.left &&
			x <= rect.right &&
			y >= rect.top &&
			y <= rect.bottom;
	}

	std::wstring UIManager::MakeButtonHoverId(
		const std::wstring& prefix,
		const std::wstring& label,
		const D2D1_RECT_F& rect)
	{
		// 同じ文字のボタンが複数あっても区別できるよう、表示位置を識別子に含める。
		return prefix + L":" + label +
			L":" + std::to_wstring(rect.left) +
			L":" + std::to_wstring(rect.top) +
			L":" + std::to_wstring(rect.right) +
			L":" + std::to_wstring(rect.bottom);
	}

	UIButtonResult UIManager::EvaluateButtonInteraction(
		const std::wstring& buttonId,
		const D2D1_RECT_F& rect,
		const UIButtonBehavior& behavior)
	{
		UIButtonResult result;
		result.rect = rect;
		if (!behavior.enabled)
		{
			return result;
		}

		const auto& input = App::GetInputDevice();
		const auto& mouse = input.GetMouseState();
		result.hovered = IsPointInRect(
			static_cast<float>(mouse.now.x),
			static_cast<float>(mouse.now.y),
			rect);
		result.clicked = result.hovered && input.MousePressed(VK_LBUTTON);

		UpdateButtonHoverSound(buttonId, result.hovered, behavior.playHoverSound);
		if (result.clicked && behavior.playClickSound)
		{
			GameAudio::Instance().PlaySound(GameSoundId::Decide);
		}

		return result;
	}

	void UIManager::UpdateButtonHoverSound(
		const std::wstring& buttonId,
		bool hovered,
		bool playHoverSound)
	{
		if (!hovered || buttonId.empty())
		{
			return;
		}

		m_currentHoveredButtonIds.insert(buttonId);
		if (playHoverSound &&
			m_previousHoveredButtonIds.find(buttonId) == m_previousHoveredButtonIds.end())
		{
			GameAudio::Instance().PlaySound(GameSoundId::CursorMove);
		}
	}

	UIButtonResult UIManager::AddButton(
		const std::wstring& text,
		UIAnchor anchor,
		const UIPointF& offset,
		const UISizeF& size,
		const D2D1_COLOR_F& baseColor,
		const D2D1_COLOR_F& hoverColor,
		const D2D1_COLOR_F& textColor,
		const UIButtonBehavior& behavior)
	{
		auto device = BaseDevice::GetBaseDevice();
		const float screenW = static_cast<float>(device->GetWidth());
		const float screenH = static_cast<float>(device->GetHeight());

		const D2D1_RECT_F rect = ResolveRect(screenW, screenH, anchor, offset, size);
		const auto result = EvaluateButtonInteraction(
			MakeButtonHoverId(L"TextButton", text, rect),
			rect,
			behavior);

		ButtonCommand cmd;
		cmd.text = text;
		cmd.baseColor = baseColor;
		cmd.hoverColor = hoverColor;
		cmd.textColor = textColor;
		cmd.anchor = anchor;
		cmd.offset = offset;
		cmd.size = size;
		cmd.hovered = result.hovered;
		m_buttons.push_back(cmd);

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

		for (const auto& overlay : m_backgroundOverlays)
		{
			layer.AddOverlayBlock(
				D2D1::RectF(0.0f, 0.0f, screenW, screenH),
				overlay.color,
				false);
		}

		for (const auto& bar : m_bars)
		{
			layer.AddProgressBar(
				ResolveRect(screenW, screenH, bar.anchor, bar.offset, bar.size),
				bar.value,
				bar.maxValue,
				bar.label);
		}

		for (const auto& slider : m_sliders)
		{
			layer.AddSliderBlock(
				ResolveRect(screenW, screenH, slider.anchor, slider.offset, slider.size),
				slider.value,
				slider.label);
		}

		for (const auto& image : m_images)
		{
			layer.AddImageBlock(
				image.path,
				ResolveRect(screenW, screenH, image.anchor, image.offset, image.size),
				image.opacity);
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
				alignment,
				text.color,
				text.fontSize);
		}

		for (const auto& overlay : m_overlays)
		{
			layer.AddOverlayBlock(
				D2D1::RectF(0.0f, 0.0f, screenW, screenH),
				overlay.color);
		}
	}

}
