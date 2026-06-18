#pragma once
#include "stdafx.h"

namespace shooting {

	class CollisionManager;

	class MainCamera : public PerspecCamera {
	private:
		// StageがCameraを所有するため、逆方向は弱参照にして循環所有を防ぐ。
		std::weak_ptr<Stage> m_stage;
		// 目標となるオブジェクト
		std::weak_ptr<GameObject> m_targetObject;
		// 目標を追いかける際の補間値
		float m_toTargetLerp;
		// 目標から視点を調整する位置ベクトル
		Vec3 m_targetToAt;
		float m_radY;
		float m_radXZ;
		// カメラの上下スピード
		float m_cameraUpDownSpeed;
		// カメラを下げる下限角度
		float m_cameraUnderRot;
		// 腕（EyeとAtの距離）の長さの設定
		float m_armLen;
		float m_maxArm;
		float m_minArm;
		// 回転スピード
		float m_rotSpeed;
		// 左右スティック変更のモード
		bool m_lrBaseMode;
		// 上下スティック変更のモード
		bool m_udBaseMode;
		bool  m_mouseLook = true;
		bool  m_cursorLocked = false;
		int   m_showCursorCount = 0;
		POINT m_saveCursorPos{};
		// rad / pixel（好みで調整） 1px動かした時に何ラジアン回るか
		float m_mouseSens = 0.0015f;
		// m_cameraUnderRot と同等
		float m_pitchMin = -XM_PI * (80.0f / 180.0f); // -80deg
		float m_pitchMax = XM_PI * (80.0f / 180.0f); // +80deg
		POINT m_prevMouse{};
		bool  m_hasPrevMouse = false;
		float m_cameraColRadius = 0.30f;     // 太さ（0.2～0.4くらいで調整）
		float m_cameraColMargin = 0.05f;     // 壁から少し離す
		float m_pushInRate = 0.40f;          // 壁に当たった時の追従（大きいほど速い）
		float m_returnRate = 0.15f;          // 壁が無い時の戻り（小さいほどゆっくり）
		float m_armLenCurrent = 5.0f;      // 実際に使う距離（壁で縮む）
		float m_cameraColProbeStartOffset = 0.85f; // 注視点直近の接触でカメラが不要にズームしないよう、少し離れた位置から判定を始める
		float m_camRadius = 0.45f;         // カメラの太さ（SphereCast半径）
		float m_camSkin = 0.08f;           // めり込み防止の余白
		float m_pushInRatio = 0.35f;       // 壁に当たった時（寄る）割合
		float m_pullOutRatio = 0.15f;      // 壁から離れる時（戻る）割合
		bool m_spawnIntroViewActive = false;
		Vec3 m_spawnIntroEye = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 m_spawnIntroAt = Vec3(0.0f, 0.0f, 0.0f);
		float m_shakeIntensity = 0.0f;
		float m_shakeDuration = 0.0f;
		float m_shakeTimeRemaining = 0.0f;
		float m_shakeElapsedTime = 0.0f;
		Vec3 m_lastShakeOffset = Vec3(0.0f, 0.0f, 0.0f);

		std::weak_ptr<CollisionManager> m_collisionManager;
		Vec3 UpdateCameraShake(float elapsedTime);
	public:
		MainCamera(const std::shared_ptr<Stage>& stage);
		//MainCamera();
		MainCamera(float armLen);
		virtual ~MainCamera();

		//--------------------------------------------------------------------------------------
		/*!
		@brief カメラの位置を設定する
		@param[in]	eye	カメラ位置
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		virtual void SetEye(const Vec3& eye)override;
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
		@param[in]	obj	カメラの目標オブジェクト
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		void SetTargetObject(const std::shared_ptr<GameObject>& obj);
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

		void SetSpawnIntroView(bool active, const Vec3& eye, const Vec3& at);
		void FinishSpawnIntroViewAndResumeFollow();

		/*!
		@brief ワールド上の衝撃位置からカメラシェイクを開始する
		@param[in] worldPosition 衝撃が発生した位置
		@param[in] intensity 衝撃位置に最も近い場合の最大移動量
		@param[in] duration シェイクを継続する秒数
		@param[in] maxDistance シェイクが届く最大距離

		複数の爆発が続いた場合は強さを加算するが、過剰に揺れないよう内部で上限を設ける。
		*/
		void RequestCameraShake(
			const Vec3& worldPosition,
			float intensity,
			float duration,
			float maxDistance);

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
		@param[in]	at	視点位置
		@return	なし
		*/
		//--------------------------------------------------------------------------------------
		virtual void SetAt(const Vec3& at)override;
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
