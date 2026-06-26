/*!
@file ExplosionResolver.h
@brief 爆発時の演出、ダメージ、吹き飛ばしを解決するクラス
*/

#pragma once
#include "stdafx.h"
#include <unordered_set>

namespace shooting {

	/*!
	@brief 1回の爆発に伴う効果をまとめて適用する

	BombBulletは飛翔と状態遷移を担当し、対象ごとの多重ヒット防止や
	爆発演出、ダメージ、吹き飛ばしはこのクラスへ委譲する。
	*/
	class ExplosionResolver
	{
	private:
		float m_explosionScale = 3.0f;
		int m_explosionDamage = 10;
		std::unordered_set<const GameObject*> m_hitTargets;
		int m_killCount = 0;

	public:
		void Reset() noexcept;
		void SetExplosionScale(float scale) noexcept { m_explosionScale = scale; }
		float GetExplosionScale() const noexcept { return m_explosionScale; }
		int GetExplosionDamage() const noexcept { return m_explosionDamage; }

		/*!
		@brief 爆発開始時の演出と初回衝突対象への効果を適用する
		@param source 爆発元のゲームオブジェクト
		@param firstHit 爆発開始時点で衝突していた対象
		*/
		void StartExplosion(
			const std::shared_ptr<GameObject>& source,
			const std::shared_ptr<GameObject>& firstHit);

		/*!
		@brief 爆風へ入った対象にダメージと吹き飛ばしを適用する
		*/
		void ApplyToTarget(
			const std::shared_ptr<GameObject>& source,
			const std::shared_ptr<GameObject>& target);
	};

}
