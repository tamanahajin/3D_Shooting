#pragma once
#include "stdafx.h"

namespace shooting {

	class UILayer;

	

	enum class UIAnchor
	{
		TopLeft,
		TopCenter,
		TopRight,
		CenterLeft,
		Center,
		CenterRight,
		BottomLeft,
		BottomCenter,
		BottomRight
	};

	enum class UITextAlign
	{
		Left,
		Center,
		Right
	};

	struct UIPointF
	{
		float x = 0.0f;
		float y = 0.0f;
	};

	struct UISizeF
	{
		float w = 0.0f;
		float h = 0.0f;
	};

	struct UIButtonResult
	{
		bool hovered = false;
		bool clicked = false;
		D2D1_RECT_F rect{};
	};

	class UIManager
	{
	public:
		void BeginFrame();

		void AddText(
			const std::wstring& text,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size,
			UITextAlign align = UITextAlign::Left,
			D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::White));

		void AddProgressBar(
			const std::wstring& label,
			float value,
			float maxValue,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size);

		UIButtonResult AddButton(
			const std::wstring& text,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size,
			const D2D1_COLOR_F& baseColor = D2D1::ColorF(0.15f, 0.25f, 0.45f, 0.95f),
			const D2D1_COLOR_F& hoverColor = D2D1::ColorF(0.25f, 0.40f, 0.75f, 0.95f),
			const D2D1_COLOR_F& textColor = D2D1::ColorF(D2D1::ColorF::White));

		void Render(UILayer& layer) const;

	private:

		struct TextCommand
		{
			std::wstring text;
			UIAnchor anchor;
			UIPointF offset;
			UISizeF size;
			UITextAlign align;
			D2D1_COLOR_F color;
		};

		struct ProgressBarCommand
		{
			std::wstring label;
			float value;
			float maxValue;
			UIAnchor anchor;
			UIPointF offset;
			UISizeF size;
		};

		static D2D1_RECT_F ResolveRect(
			float screenW,
			float screenH,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size);

		struct ButtonCommand
		{
			std::wstring text;
			UIAnchor anchor;
			UIPointF offset;
			UISizeF size;
			bool hovered = false;
			D2D1_COLOR_F baseColor;
			D2D1_COLOR_F hoverColor;
			D2D1_COLOR_F textColor;
		};

		static bool IsPointInRect(float x, float y, const D2D1_RECT_F& rect);

		std::vector<TextCommand> m_texts;
		std::vector<ProgressBarCommand> m_bars;
		std::vector<ButtonCommand> m_buttons;
	};

}