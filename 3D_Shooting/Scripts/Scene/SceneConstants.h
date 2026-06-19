#pragma once

namespace shooting::scene_detail {

	inline constexpr float kSceneTransitionFadeOutSeconds = 0.35f;
	inline constexpr float kSceneTransitionFadeInSeconds = 0.45f;

	inline constexpr const wchar_t* kOptionIconPath = L"UI/option.png";
	inline constexpr const wchar_t* kBombHudIconPath = L"UI/bom_icon.png";

	inline constexpr int kOptionSliderNone = -1;
	inline constexpr int kOptionSliderBgm = 0;
	inline constexpr int kOptionSliderSe = 1;

	inline constexpr float kOptionIconSize = 46.0f;
	inline constexpr float kOptionIconMargin = 20.0f;
	inline constexpr float kOptionSliderHeight = 34.0f;
	inline constexpr float kOptionSliderTrackOffset = 118.0f;
	inline constexpr float kBombHudIconSize = 56.0f;
	inline constexpr float kBombHudMargin = 24.0f;

	inline constexpr double kInitialControlGuideDurationSeconds = 5.0;
	inline constexpr double kInitialControlGuideFadeSeconds = 0.8;

}
