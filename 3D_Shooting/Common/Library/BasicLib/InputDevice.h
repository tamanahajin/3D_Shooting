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

	struct MOUSE_STATE
	{
		POINT now{ 0,0 };
		POINT pre{ 0,0 };
		POINT delta{ 0,0 };

		std::array<BYTE, 3> btnNow{};      // 0:L 1:R 2:M
		std::array<BYTE, 3> btnPre{};
		std::array<BYTE, 3> btnPressed{};
		std::array<BYTE, 3> btnReleased{};

		int wheelDeltaFrame = 0; // そのフレーム分（120単位が多い）
	};

	class InputDevice
	{
	private:
		KEYBOARD_STATE keyboardState;
		static constexpr BYTE kDownMask = 0x80; // キーが押されているかのマスク

		HWND m_hwnd = nullptr;
		MOUSE_STATE m_mouse;
		int m_wheelAccum = 0; // WM_MOUSEWHEEL で積む（次のUpdateでframeに反映）
	public:
		InputDevice()
		{
			keyboardState.now.fill(0);
			keyboardState.pre.fill(0);
			keyboardState.pressed.fill(0);
			keyboardState.released.fill(0);
		}

		~InputDevice() {}

		void SetHwnd(HWND hwnd) { m_hwnd = hwnd; }

		void AttachWindow(HWND hwnd)
		{
			m_hwnd = hwnd;

			// 初回delta暴れ防止：now/pre を現在位置で揃える
			POINT p{};
			if (::GetCursorPos(&p))
			{
				::ScreenToClient(m_hwnd, &p);
				m_mouse.now = p;
				m_mouse.pre = p;
				m_mouse.delta = { 0,0 };
			}
		}

		// client座標で指定して、カーソルをそこへワープ + now/pre/delta を同期
		void WarpCursorToClientPos(const POINT& clientPos)
		{
			if (m_hwnd)
			{
				POINT s = clientPos;
				::ClientToScreen(m_hwnd, &s);
				::SetCursorPos(s.x, s.y);
			}
			else
			{
				// hwnd未設定なら clientPos を screen とみなす（保険）
				::SetCursorPos(clientPos.x, clientPos.y);
			}

			// ★重要：ワープしたので InputDevice の状態も合わせる
			m_mouse.now = clientPos;
			m_mouse.pre = clientPos;
			m_mouse.delta = POINT{ 0,0 };
		}

		void Update()
		{
			ResetKeyboardState();
			ResetMouseState();
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

		void ResetMouseState()
		{
			// ホイール：このフレーム分を確定してから、蓄積をクリア
			m_mouse.wheelDeltaFrame = m_wheelAccum;
			m_wheelAccum = 0;

			// 位置
			m_mouse.pre = m_mouse.now;

			POINT p;
			if (::GetCursorPos(&p))
			{
				if (m_hwnd)
				{
					::ScreenToClient(m_hwnd, &p);
				}
				m_mouse.now = p;
			}

			m_mouse.delta.x = m_mouse.now.x - m_mouse.pre.x;
			m_mouse.delta.y = m_mouse.now.y - m_mouse.pre.y;

			// ボタン
			m_mouse.btnPre = m_mouse.btnNow;

			auto poll = [&](int vk)->BYTE {
				SHORT s = ::GetAsyncKeyState(vk);
				return (s & 0x8000) ? kDownMask : 0;
				};

			m_mouse.btnNow[0] = poll(VK_LBUTTON);
			m_mouse.btnNow[1] = poll(VK_RBUTTON);
			m_mouse.btnNow[2] = poll(VK_MBUTTON);

			for (int i = 0; i < 3; ++i)
			{
				bool nowDown = (m_mouse.btnNow[i] & kDownMask) != 0;
				bool lastDown = (m_mouse.btnPre[i] & kDownMask) != 0;

				m_mouse.btnPressed[i] = (nowDown && !lastDown) ? kDownMask : 0;
				m_mouse.btnReleased[i] = (!nowDown && lastDown) ? kDownMask : 0;
			}
		}

		// WM_MOUSEWHEELから呼ぶ
		void AddWheelDelta(int delta) { m_wheelAccum += delta; }

		const MOUSE_STATE& GetMouseState() const { return m_mouse; }

		POINT GetMouseDelta() const { return m_mouse.delta; }

		int GetMouseWheelDelta() const { return m_mouse.wheelDeltaFrame; }

		bool MouseDown(int vk) const
		{
			// vk は VK_LBUTTON / VK_RBUTTON / VK_MBUTTON
			int idx = (vk == VK_LBUTTON) ? 0 : (vk == VK_RBUTTON) ? 1 : 2;
			return (m_mouse.btnNow[idx] & kDownMask) != 0;
		}
		bool MousePressed(int vk) const
		{
			int idx = (vk == VK_LBUTTON) ? 0 : (vk == VK_RBUTTON) ? 1 : 2;
			return (m_mouse.btnPressed[idx] & kDownMask) != 0;
		}
		bool MouseReleased(int vk) const
		{
			int idx = (vk == VK_LBUTTON) ? 0 : (vk == VK_RBUTTON) ? 1 : 2;
			return (m_mouse.btnReleased[idx] & kDownMask) != 0;
		}
	};
}