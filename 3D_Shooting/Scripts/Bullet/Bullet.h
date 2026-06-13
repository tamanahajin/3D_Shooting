#pragma once
#include "stdafx.h"
#include <unordered_set>
#include <DirectXMath.h>

namespace shooting {

	//============================================================
	// DefaultBullet（通常弾）
	//============================================================
	class DefaultBullet : public IBullet
	{
	private:
		float  m_Speed = 15.0f;
		bool   m_IsActive = false;

		// 寿命（秒）
		double m_LifeTime = 5.0;
		// 経過時間（秒）
		double m_ElapsedTime = 0.0;

	public:
		DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~DefaultBullet() = default;

		// ----- IBullet -----
		bool IsActive() const noexcept override;
		void SetActive(bool active) noexcept override;

		/// <summary>
		/// プールから再利用される直前に呼ばれる。
		/// 寿命だけをリセットして Active にする。
		/// </summary>
		virtual void ResetForSpawn() noexcept override
		{
			m_ElapsedTime = 0.0;
			SetActive(true);
		}

		// ----- GameObject -----
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;
		virtual void OnCollisionEnter(const CollisionPair& pair) override;
		virtual void OnCollisionExecute(const CollisionPair& pair) override;
	};


	//============================================================
	// BombBullet（ボム弾：範囲攻撃）
	//
	// 方式：
	//  1) 飛翔中：信管（Fuse）を減らす / 何かに当たったら爆発開始
	//  2) 爆発中：短時間だけ「爆風判定」を出して範囲ダメージ
	//
	// 注意：
	//  爆風の範囲はここでは「TransformのScaleを一気に大きくする」ことで表現。
	//  もし CollisionSphere が Scale に追従しない設計なら、CollisionSphere の半径APIに置き換える。
	//============================================================
	class BombBullet : public DefaultBullet
	{
	private:
		// 飛翔速度（DefaultBulletの速度とは別扱いにしている）
		float  m_Speed = 10.0f;

		// 爆発までの時間
		// 定数
		const double FUSE_TIME = 3.0f;
		double m_FuseTime = FUSE_TIME;

		Vec3  m_Velocity = Vec3(0, 0, 0);
		Vec3  m_Gravity = Vec3(0, -9.8f, 0);
		Vec3  m_TargetPos = Vec3(0, 0, 0);
		Vec3  m_TargetNormal = Vec3(0, 1, 0);
		bool  m_HasTarget = false;
		bool  m_HasTargetSurface = false;
		float m_ArcHeight = 1.5f;
		Vec3  m_StartPos = Vec3(0, 0, 0);   // p0（発射時位置）
		Vec3  m_V0 = Vec3(0, 0, 0);   // v0（発射時初速）
		float m_FlyTime = 0.0f;          // 発射からの経過 t
		float m_TotalT = 0.0f;          // 目標到達に必要な T
		bool  m_UseBallistic = false;     // ターゲット弾道を使うか
		bool  m_UseGeneratedGroundImpact = true;
		float m_ArcHeightPerDistXZ = 0.0f;

		// 爆発状態
		bool   m_Exploding = false;
		double m_ExplosionDuration = 0.08; // 爆風が有効な時間
		double m_ExplosionTimer = 0.0;

		// 爆風のスケール（Transform.Scale）
		float  m_ExplosionScale = 3.0f;

		// 範囲ダメージ
		int    m_ExplosionDamage = 10;

		// 爆発中の多重ヒット防止
		std::unordered_set<const GameObject*> m_HitOnce;
		// この爆弾1個で死亡が確定した敵数。BEST EXPLOSION の更新に使う。
		int m_ExplosionKillCount = 0;

		bool SolveBallistic_ApexHeight(
			const Vec3& p0, const Vec3& p1,
			const Vec3& gravity, float arcHeight,
			Vec3& outV0, float& outT
		) const;
		bool TryGetStageGroundHeight(const Vec3& position, float& outHeight) const noexcept;
		Vec3 SnapTargetToStageGround(const Vec3& target) const noexcept;
		bool TryResolveTerrainImpact(
			const Vec3& previousPosition,
			const Vec3& currentPosition,
			Vec3& outImpactPosition) const noexcept;
	public:
		BombBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~BombBullet() = default;

		// ----- IBullet -----
		void ResetForSpawn() noexcept override;

		void OnReturnToPool() noexcept override
		{
			if (auto trans = GetComponent<Transform>())
			{
				trans->SetScale(Vec3(0.1f, 0.1f, 0.1f));
			}
		}

		// ----- Bom -----
		void SetTarget(const Vec3& target)
		{
			m_TargetPos = target;
			m_TargetNormal = Vec3(0, 1, 0);
			m_HasTarget = true;
			m_HasTargetSurface = false;
		}

		void SetAimFromPreview(const Vec3& target, const BombTuning& t, const Vec3& targetNormal, bool hasTargetSurface)
		{
			m_TargetPos = target;
			m_TargetNormal = targetNormal;
			m_HasTarget = true;
			m_HasTargetSurface = hasTargetSurface;

			m_ArcHeight = t.arcHeightBase;
			m_Gravity = t.gravity;
			m_ArcHeightPerDistXZ = t.arcHeightPerDistXZ;

			// CollisionSphere の半径が「scale * 0.5」仕様なので、
			// radius を合わせたいなら scale = radius * 2 にするのが基本。
			m_ExplosionScale = t.explosionRadius * 2.0f;
		}

		// ----- GameObject -----
		void OnCreate() override;
		void OnUpdate(double elapsedTime) override;
		void OnCollisionEnter(const CollisionPair& pair) override;
		void OnCollisionExecute(const CollisionPair& pair) override;

	private:
		void StartExplosion(const std::shared_ptr<GameObject>& firstHit);
		void TryApplyExplosionDamage(const std::shared_ptr<GameObject>& target);
	};

}
