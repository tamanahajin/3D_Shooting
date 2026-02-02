#pragma once
#include <array>
#include <vector>
#include <wrl.h>
#include "stdafx.h"

namespace shooting {

	/// <summary>
	/// キーボードのステータス
	/// now : 現在のフレームの状態
	/// pre : 直前のフレームの状態
	/// pressed : 押された瞬間
	/// released : 離された瞬間
	/// </summary>
	struct KEYBOARD_STATE
	{
		std::array<BYTE, 256> now{}; // 現在のフレームの状態
		std::array<BYTE, 256> pre{}; // 直前のフレームの状態
		std::array<BYTE, 256> pressed{}; // 押された瞬間
		std::array<BYTE, 256> released{}; // 離された瞬間
		// floatW Wのfloat変換
		float fNowW;
		bool connected = true; // 接続状態
	};

	class InputDevice
	{
		KEYBOARD_STATE keyboardState;
		static constexpr BYTE kDownMask = 0x80; // キーが押されているかのマスク

	public:
		InputDevice()
		{
			keyboardState.now.fill(0);
			keyboardState.pre.fill(0);
			keyboardState.pressed.fill(0);
			keyboardState.released.fill(0);
		}

		~InputDevice() {}

		void Update()
		{
			ResetKeyboardState();
		}

		void ResetKeyboardState()
		{
			keyboardState.pre = keyboardState.now;

			// GetKeyboardState はフォーカス時のみメッセージ処理のたびに状態を返すので、
			// 失敗時には GetAsyncKeyState でフォールバックします。
			if (!::GetKeyboardState(keyboardState.now.data()))
			{
				for (int vk = 0; vk < 256; ++vk)
				{
					const SHORT s = ::GetAsyncKeyState(vk);
					keyboardState.now[vk] = (s & 0x8000) ? kDownMask : 0;
				}
			}

			for (int vk = 0; vk < 256; ++vk)
			{
				const bool nowDown = (keyboardState.now[vk] & kDownMask) != 0;
				const bool lastDown = (keyboardState.pre[vk] & kDownMask) != 0;

				keyboardState.pressed[vk] = (nowDown && !lastDown) ? kDownMask : 0;
				keyboardState.released[vk] = (!nowDown && lastDown) ? kDownMask : 0;
			}
		}

		const KEYBOARD_STATE& GetKeyboardState() const
		{
			return keyboardState;
		}

		// 補助関数 : キーの状態を手軽にチェックするための関数
		bool KeyDown(int vk) const
		{
			return (keyboardState.now[vk & 0xFF] & kDownMask) != 0;
		}
		bool KeyPressed(int vk) const
		{
			return (keyboardState.pressed[vk & 0xFF] & kDownMask) != 0;
		}
		bool KeyReleased(int vk) const
		{
			return (keyboardState.released[vk & 0xFF] & kDownMask) != 0;
		}
	};
}