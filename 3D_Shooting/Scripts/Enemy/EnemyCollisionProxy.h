/*!
@file EnemyCollisionProxy.h
@brief 敵配列と CollisionManager をつなぐ軽量コリジョンプロキシ
*/

#pragma once
#include "stdafx.h"
#include "EnemyStatus.h"
#include <memory>

namespace shooting {

	struct DamageInfo;
	class EnemyController;

	/*!
	@brief 敵1体分の衝突イベントだけを受け持つ軽量 GameObject

	描画・AI・HP は EnemyController の配列に置き、このクラスは CollisionManager に参加するための
	Transform と Collision だけを持つ。
	*/
	class EnemyCollisionProxy : public GameObject
	{
	private:
		std::weak_ptr<EnemyController> m_controller;
		size_t m_enemyIndex = 0;
		Vec3 m_startPosition;
		Vec3 m_modelScale = Vec3(0.01f, 0.01f, 0.01f);
		float m_collisionRadius = 0.2f;
		float m_collisionHeight = 0.3f;
		bool m_inUse = true;

		void HandleCollision(const CollisionPair& pair);

	public:
		EnemyCollisionProxy(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyController>& controller,
			size_t enemyIndex,
			const Vec3& startPosition,
			const EnemyStatus& status);
		virtual ~EnemyCollisionProxy();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief プールから取り出したプロキシを新しい敵へ割り当て直す

		GameObject と CollisionCapsule は作り直さず、参照先と Transform だけを差し替える。
		*/
		void ResetForEnemy(
			const std::shared_ptr<EnemyController>& controller,
			size_t enemyIndex,
			const Vec3& startPosition,
			const EnemyStatus& status);
		/*!
		@brief ステージに残したまま、衝突判定と更新対象から外す
		*/
		void DeactivateForPool();
		bool IsInUse() const { return m_inUse; }
		virtual void OnCollisionEnter(const CollisionPair& pair) override;
		virtual void OnCollisionExecute(const CollisionPair& pair) override;

		size_t GetEnemyIndex() const { return m_enemyIndex; }
		/*!
		@brief 対応する敵配列要素へダメージ処理を転送する
		*/
		bool ApplyDamage(const DamageInfo& info);
		/*!
		@brief 対応する敵配列要素へノックバックを転送する
		*/
		void AddKnockback(const Vec3& velocity);
		bool IsAlive() const;
	};

}
