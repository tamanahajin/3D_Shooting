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

	class UIManager
	{
	public:
		void BeginFrame();

		void AddText(
			const std::wstring& text,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size,
			UITextAlign align = UITextAlign::Left);

		void AddProgressBar(
			const std::wstring& label,
			float value,
			float maxValue,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size);

		void Render(UILayer& layer) const;

	private:
		struct TextCommand
		{
			std::wstring text;
			UIAnchor anchor;
			UIPointF offset;
			UISizeF size;
			UITextAlign align;
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

		std::vector<TextCommand> m_texts;
		std::vector<ProgressBarCommand> m_bars;
	};

}