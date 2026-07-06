#pragma once
#include "stdafx.h"
#include <unordered_set>

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

	struct UIButtonBehavior
	{
		/// 無効時はホバー、クリック、効果音をすべて処理しない。
		bool enabled = true;
		/// カーソルがボタンへ入った瞬間の効果音を再生する。
		bool playHoverSound = true;
		/// マウスでクリックしたときの決定音を再生する。
		bool playClickSound = true;
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
			D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::White),
			float fontSize = 0.0f);

		void AddImage(
			const std::wstring& path,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size,
			float opacity = 1.0f);

		UIButtonResult AddImageButton(
			const std::wstring& path,
			const std::wstring& buttonId,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size,
			float opacity = 0.82f,
			float hoverOpacity = 1.0f,
			const UIButtonBehavior& behavior = UIButtonBehavior());

		void AddProgressBar(
			const std::wstring& label,
			float value,
			float maxValue,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size,
			D2D1_COLOR_F fillColor = D2D1::ColorF(0.85f, 0.15f, 0.15f, 0.95f));

		void AddSlider(
			const std::wstring& label,
			float value,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size);

		void AddFullscreenOverlay(const D2D1_COLOR_F& color);
		void AddFullscreenBackgroundOverlay(const D2D1_COLOR_F& color);

		UIButtonResult AddButton(
			const std::wstring& text,
			UIAnchor anchor,
			const UIPointF& offset,
			const UISizeF& size,
			const D2D1_COLOR_F& baseColor = D2D1::ColorF(0.15f, 0.25f, 0.45f, 0.95f),
			const D2D1_COLOR_F& hoverColor = D2D1::ColorF(0.25f, 0.40f, 0.75f, 0.95f),
			const D2D1_COLOR_F& textColor = D2D1::ColorF(D2D1::ColorF::White),
			const UIButtonBehavior& behavior = UIButtonBehavior());

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
			float fontSize = 0.0f;
		};

		struct ProgressBarCommand
		{
			std::wstring label;
			float value;
			float maxValue;
			UIAnchor anchor;
			UIPointF offset;
			UISizeF size;
			D2D1_COLOR_F fillColor;
		};

		struct SliderCommand
		{
			std::wstring label;
			float value;
			UIAnchor anchor;
			UIPointF offset;
			UISizeF size;
		};

		struct ImageCommand
		{
			std::wstring path;
			UIAnchor anchor;
			UIPointF offset;
			UISizeF size;
			float opacity = 1.0f;
		};

		struct OverlayCommand
		{
			D2D1_COLOR_F color;
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
		static std::wstring MakeButtonHoverId(
			const std::wstring& prefix,
			const std::wstring& label,
			const D2D1_RECT_F& rect);
		UIButtonResult EvaluateButtonInteraction(
			const std::wstring& buttonId,
			const D2D1_RECT_F& rect,
			const UIButtonBehavior& behavior);
		void UpdateButtonHoverSound(
			const std::wstring& buttonId,
			bool hovered,
			bool playHoverSound);

		std::vector<TextCommand> m_texts;
		std::vector<ProgressBarCommand> m_bars;
		std::vector<SliderCommand> m_sliders;
		std::vector<ImageCommand> m_images;
		std::vector<ButtonCommand> m_buttons;
		std::vector<OverlayCommand> m_backgroundOverlays;
		std::vector<OverlayCommand> m_overlays;
		std::unordered_set<std::wstring> m_previousHoveredButtonIds;
		std::unordered_set<std::wstring> m_currentHoveredButtonIds;
	};

}
