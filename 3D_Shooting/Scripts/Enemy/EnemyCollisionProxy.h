/*!
@file EnemyCollisionProxy.h
@brief 敵1体分の軽量コリジョンプロキシ
*/

#pragma once
#include "stdafx.h"
#include "EnemyStatus.h"
#include <memory>

namespace shooting {

	struct DamageInfo;
	class EnemyBatchController;

	/*!
	@brief 敵1体分の軽量コリジョンプロキシ

	敵の描画・AI・HPは EnemyBatchController の配列で管理し、
	このクラスは CollisionManager に参加するための Transform と Collision だけを持つ。
	*/
	class EnemyCollisionProxy : public GameObject
	{
	private:
		std::weak_ptr<EnemyBatchController> m_controller;
		size_t m_enemyIndex = 0;
		Vec3 m_startPosition;
		Vec3 m_modelScale = Vec3(0.01f, 0.01f, 0.01f);
		float m_collisionRadius = 0.2f;
		float m_collisionHeight = 0.3f;
		bool m_inUse = true;

		/*!
		@brief 衝突相手を判定し、接地・弾・爆弾の処理をコントローラへ転送する
		@param pair CollisionManager から渡された衝突情報
		*/
		void HandleCollision(const CollisionPair& pair);

	public:
		/*!
		@brief 敵プロキシを生成する
		@param stage 所属するステージ
		@param controller 本体状態を持つ敵バッチコントローラ
		@param enemyIndex m_enemies 内の対象インデックス
		@param startPosition 生成位置
		@param status 当たり判定サイズとモデルスケールを含む敵設定
		*/
		EnemyCollisionProxy(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyBatchController>& controller,
			size_t enemyIndex,
			const Vec3& startPosition,
			const EnemyStatus& status);
		virtual ~EnemyCollisionProxy();
		/*!
		@brief Transform と Capsule Collision を作成する
		*/
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief プールから取り出したプロキシを新しい敵に割り当てる
		@param controller 本体状態を持つ敵バッチコントローラ
		@param enemyIndex m_enemies 内の対象インデックス
		@param startPosition 初期位置
		@param status 当たり判定サイズとモデルスケールを含む敵設定

		GameObject と CollisionCapsule は作り直さず、参照先と Transform だけを差し替える。
		*/
		void ResetForEnemy(
			const std::shared_ptr<EnemyBatchController>& controller,
			size_t enemyIndex,
			const Vec3& startPosition,
			const EnemyStatus& status);
		/*!
		@brief 使用中プロキシをプールへ戻せる状態にする
		*/
		void DeactivateForPool();
		/*!
		@brief 現在敵に割り当てられているかを取得する
		@return 使用中なら true
		*/
		bool IsInUse() const { return m_inUse; }
		/*!
		@brief 衝突開始時の処理を共通ハンドラへ渡す
		@param pair 衝突情報
		*/
		virtual void OnCollisionEnter(const CollisionPair& pair) override;
		/*!
		@brief 衝突継続時の処理を共通ハンドラへ渡す
		@param pair 衝突情報
		*/
		virtual void OnCollisionExecute(const CollisionPair& pair) override;

		/*!
		@brief 対応する敵配列インデックスを取得する
		@return m_enemies 内のインデックス
		*/
		size_t GetEnemyIndex() const { return m_enemyIndex; }
		/*!
		@brief このプロキシに対応する敵へダメージを適用する
		@param info ダメージ量、攻撃者、吹っ飛び死亡遅延などの情報
		@return このダメージで即死亡、または着地後の死亡が確定した場合は true
		*/
		bool ApplyDamage(const DamageInfo& info);
		/*!
		@brief このプロキシに対応する敵へノックバック速度を与える
		@param velocity 爆風などで与える速度
		*/
		void AddKnockback(const Vec3& velocity);
		/*!
		@brief 対応する敵が現在生存しているかを返す
		@return 生存中なら true
		*/
		bool IsAlive() const;
	};

}
