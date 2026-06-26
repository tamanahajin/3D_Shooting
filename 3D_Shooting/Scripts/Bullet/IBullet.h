#pragma once
#include "stdafx.h"

namespace shooting {

	/// <summary>
	/// 弾オブジェクトの共通インターフェイス
	///
	/// BulletPool が弾を再利用するために必要な最小APIだけを定義する。
	/// これにより BulletPool は DefaultBullet 等の具体型に依存しない。
	/// </summary>
	class IBullet : public GameObject
	{
	private:
		bool m_isActive = false;

	public:
		explicit IBullet(const std::shared_ptr<Stage>& stage)
			: GameObject(stage)
		{
		}
		virtual ~IBullet() = default;

		/// <summary>
		/// プールから貸し出されている弾かを返す。
		/// </summary>
		bool IsActive() const noexcept
		{
			return m_isActive;
		}

		/// <summary>
		/// 弾の使用状態とGameObject側の有効状態をまとめて切り替える。
		/// </summary>
		void SetActive(bool active) noexcept
		{
			m_isActive = active;

			// 個別に変更すると待機中の弾が描画・衝突対象へ残るため、ここで同期する。
			SetUpdateActive(active);
			SetDrawActive(active);
			SetShadowActive(active);
		}

		/// <summary>
		/// プールから再使用するときに、内部状態を初期化する。
		/// 例：寿命タイマー、爆発フラグ、ヒット履歴、演出状態など
		/// </summary>
		virtual void ResetForSpawn() noexcept = 0;

		/// <summary>
		/// （任意）回収時に呼ばれるフック。
		/// 例：スケールを戻す、VFXを止める等
		/// 使わない弾は何もしなくてOK。
		/// </summary>
		virtual void OnReturnToPool() noexcept {}
	};

}
