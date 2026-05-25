#pragma once
#include "stdafx.h"

namespace shooting {

	class CollisionManager;

	class MainCamera : public PerspecCamera {
	private:
		std::shared_ptr<Stage> m_Stage;
		// 目標となるオブジェクト
		std::weak_ptr<GameObject> m_TargetObject;
		// 目標を追いかける際の補間値
		float m_ToTargetLerp;
		// 目標から視点を調整する位置ベクトル
		Vec3 m_TargetToAt;
		float m_RadY;
		float m_RadXZ;
		// カメラの上下スピード
		float m_CameraUpDownSpeed;
		// カメラを下げる下限角度
		float m_CameraUnderRot;
		// 腕（EyeとAtの距離）の長さの設定
		float m_ArmLen;
		float m_MaxArm;
		float m_MinArm;
		// ズームスピード
		float m_ZoomSpeed;
		// 回転スピード
		float m_RotSpeed;
		// 左右スティック変更のモード
		bool m_LRBaseMode;
		// 上下スティック変更のモード
		bool m_UDBaseMode;
		bool  m_MouseLook = true;
		bool  m_CursorLocked = false;
		int   m_ShowCursorCount = 0;
		POINT m_SaveCursorPos{};
		// rad / pixel（好みで調整） 1px動かした時に何ラジアン回るか
		float m_MouseSens = 0.0015f;
		// アーム長の増減量
		float m_WheelSens = 0.6f;
		// m_CameraUnderRot と同等
		float m_PitchMin = -XM_PI * (80.0f / 180.0f); // -80deg
		float m_PitchMax = XM_PI * (80.0f / 180.0f); // +80deg
		POINT m_PrevMouse{};
		bool  m_HasPrevMouse = false;
		float m_CameraColRadius = 0.30f;     // 太さ（0.2～0.4くらいで調整）
		float m_CameraColMargin = 0.05f;     // 壁から少し離す
		float m_PushInRate = 0.40f;          // 壁に当たった時の追従（大きいほど速い）
		float m_ReturnRate = 0.15f;          // 壁が無い時の戻り（小さいほどゆっくり）
		float m_ArmLenCurrent = 5.0f;      // 実際に使う距離（壁で縮む）
		float m_CameraColProbeStartOffset = 0.85f; // 注視点直近の接触でカメラが不要にズームしないよう、少し離れた位置から判定を始める
		float m_CamRadius = 0.45f;         // カメラの太さ（SphereCast半径）
		float m_CamSkin = 0.08f;           // めり込み防止の余白
		float m_PushInRatio = 0.35f;       // 壁に当たった時（寄る）割合
		float m_PullOutRatio = 0.15f;      // 壁から離れる時（戻る）割合

		std::weak_ptr<CollisionManager> m_CollisionManager;
	public:
		MainCamera(const std::shared_ptr<Stage>& stage);
		//MainCamera();
		MainCamera(float ArmLen);
		virtual ~MainCamera();

		//--------------------------------------------------------------------------------------
		/*!
		@brief カメラの位置を設定する
		@param[in]	Eye	カメラ位置
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		virtual void SetEye(const Vec3& Eye)override;
		//--------------------------------------------------------------------------------------
		/*!
		@brief カメラの位置を設定する
		@param[in]	x	x位置
		@param[in]	y	y位置
		@param[in]	z	z位置
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		virtual void SetEye(float x, float y, float z)override;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	カメラの目標オブジェクトを得る
		@return	カメラの目標
		*/
		//--------------------------------------------------------------------------------------
		std::shared_ptr<GameObject> GetTargetObject() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	カメラの目標オブジェクトを設定する
		@param[in]	Obj	カメラの目標オブジェクト
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetTargetObject(const std::shared_ptr<GameObject>& Obj);
		//--------------------------------------------------------------------------------------
		/*!
		@brief	オブジェクトを追いかける場合の補間値を得る
		@return	オブジェクトを追いかける場合の補間値
		*/
		//--------------------------------------------------------------------------------------
		float GetToTargetLerp() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	オブジェクトを追いかける場合の補間値を設定する
		@param[in]	f	オブジェクトを追いかける場合の補間値
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetToTargetLerp(float f);
		//--------------------------------------------------------------------------------------
		/*!
		@brief	EyeとAtの距離を得る
		@return	EyeとAtの距離
		*/
		//--------------------------------------------------------------------------------------
		float GetArmLengh() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	EyeとAtの距離を更新する（現在のEyeとAtから求める）
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void UpdateArmLengh();
		//--------------------------------------------------------------------------------------
		/*!
		@brief	EyeとAtの距離の最大値を得る
		@return	EyeとAtの距離の最大値
		*/
		//--------------------------------------------------------------------------------------
		float GetMaxArm() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	EyeとAtの距離の最大値を設定する
		@param[in]	f	EyeとAtの距離の最大値
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetMaxArm(float f);
		//--------------------------------------------------------------------------------------
		/*!
		@brief	EyeとAtの距離の最小値を得る
		@return	EyeとAtの距離の最小値
		*/
		//--------------------------------------------------------------------------------------
		float GetMinArm() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	EyeとAtの距離の最小値設定する
		@param[in]	f	EyeとAtの距離の最小値
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetMinArm(float f);
		//--------------------------------------------------------------------------------------
		/*!
		@brief	回転スピードを得る
		@return	回転スピード（0.0f以上）
		*/
		//--------------------------------------------------------------------------------------
		float GetRotSpeed() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	回転スピードを設定する
		@param[in]	f	回転スピード（マイナスを入力してもプラスになる）
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetRotSpeed(float f);
		//--------------------------------------------------------------------------------------
		/*!
		@brief	ターゲットからAtへの調整ベクトルを得る
		@return	ターゲットからAtへの調整ベクトル
		*/
		//--------------------------------------------------------------------------------------
		Vec3 GetTargetToAt() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	ターゲットからAtへの調整ベクトルを設定する
		@param[in]	v	ターゲットからAtへの調整ベクトルを
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetTargetToAt(const Vec3& v);
		//--------------------------------------------------------------------------------------
		/*!
		@brief	Rスティックの左右変更をBaseモードにするかどうかを得る
		@return	Baseモードならtrue（デフォルト）
		*/
		//--------------------------------------------------------------------------------------
		bool GetLRBaseMode() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	Rスティックの左右変更をBaseモードにするかどうかを得る
		@return	Baseモードならtrue（デフォルト）
		*/
		//--------------------------------------------------------------------------------------
		bool IsLRBaseMode() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	Rスティックの左右変更をBaseモードにするかどうかを設定する
		@param[in]	b	Baseモードならtrue
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetLRBaseMode(bool b);
		//--------------------------------------------------------------------------------------
		/*!
		@brief	Rスティックの上下変更をBaseモードにするかどうかを得る
		@return	Baseモードならtrue（デフォルト）
		*/
		//--------------------------------------------------------------------------------------
		bool GetUDBaseMode() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	Rスティックの上下変更をBaseモードにするかどうかを得る
		@return	Baseモードならtrue（デフォルト）
		*/
		//--------------------------------------------------------------------------------------
		bool IsUDBaseMode() const;
		//--------------------------------------------------------------------------------------
		/*!
		@brief	Rスティックの上下変更をBaseモードにするかどうかを設定する
		@param[in]	b	Baseモードならtrue
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetUDBaseMode(bool b);

		/// <summary>
		/// マウスの移動量を計算します。
		/// </summary>
		/// <param name="hwnd">マウス入力を取得するウィンドウのハンドル。</param>
		/// <param name="prev">前回のマウス位置。初回呼び出し時は更新されます。</param>
		/// <param name="hasPrev">前回の位置が有効かどうかを示すフラグ。初回呼び出し時はfalseにする必要があります。</param>
		/// <param name="dx">X軸方向のマウス移動量の出力先。</param>
		/// <param name="dy">Y軸方向のマウス移動量の出力先。</param>
		/// <returns>マウスの移動量の取得に成功した場合はtrue、失敗した場合はfalse。</returns>
		bool GetMouseDelta(HWND hwnd, POINT& prev, bool& hasPrev, float& dx, float& dy);

		/// <summary>
		/// クライアント領域の中心点を取得します。
		/// </summary>
		/// <param name="hwnd">対象となるウィンドウのハンドル。</param>
		/// <returns>クライアント領域の中心点。</returns>
		POINT GetClientCenter(HWND hwnd);

		/// <summary>
		/// クライアント領域の中心点をスクリーン座標で取得します。
		/// </summary>
		/// <param name="hwnd">対象となるウィンドウのハンドル。</param>
		/// <returns>スクリーン座標におけるクライアント領域の中心点。</returns>
		POINT GetClientCenterInScreen(HWND hwnd);

		/// <summary>
		/// ウィンドウのクライアント領域内にカーソルをクリップします。
		/// </summary>
		/// <param name="hwnd">クライアント領域を取得するウィンドウのハンドル。</param>
		/// <param name="enable">trueの場合、カーソルをクライアント領域にクリップします。falseの場合、クリップを解除します。</param>
		void ClipCursorToClient(HWND hwnd, bool enable);

		/// <summary>
		/// カーソルの表示・非表示を設定
		/// </summary>
		/// <param name="visible">カーソルを表示する場合は true、非表示にする場合は false。</param>
		/// <param name="counter">可視性の状態を追跡するカウンターへの参照。</param>
		void SetCursorVisible(bool visible, int& counter);

		void BeginMouseLook();
		void EndMouseLook();

		Vec3 ResolveCameraEyeBySweep(
			const std::shared_ptr<Stage>& stage,
			const Vec3& pivot,
			const Vec3& desiredEye,
			float radius,
			float skin,
			const std::shared_ptr<GameObject>& ignoreObj,
			bool onlyFixed = true);

		//--------------------------------------------------------------------------------------
		/*!
		@brief カメラの視点を設定する
		@param[in]	At	視点位置
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		virtual void SetAt(const Vec3& At)override;
		//--------------------------------------------------------------------------------------
		/*!
		@brief カメラの視点を設定する
		@param[in]	x	x位置
		@param[in]	y	y位置
		@param[in]	z	z位置
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		virtual void SetAt(float x, float y, float z)override;

		virtual void OnCreate()override;
		//--------------------------------------------------------------------------------------
		/*!
		@brief 更新処理
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		virtual void OnUpdate(double elapsedTime)override;

	};
}
