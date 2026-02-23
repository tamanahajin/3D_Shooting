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
	public:
		explicit IBullet(const std::shared_ptr<Stage>& stage)
			: GameObject(stage)
		{
		}
		virtual ~IBullet() = default;

		/// <summary>
		/// 「プールから使用中か？」の判定。
		/// 弾が寿命切れ/命中などで終了したら false にする。
		/// </summary>
		virtual bool IsActive() const noexcept = 0;

		/// <summary>
		/// アクティブ状態の設定。
		/// Pool側が回収時に false を入れたり、Spawn時に true を入れたりする。
		/// </summary>
		virtual void SetActive(bool active) noexcept = 0;

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