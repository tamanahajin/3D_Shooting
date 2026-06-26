#pragma once
#include "stdafx.h"
#include "ExplosionResolver.h"
#include "BallisticTrajectory.h"
#include <DirectXMath.h>

namespace shooting {

	//============================================================
	// DefaultBullet（通常弾）
	//============================================================
	class DefaultBullet : public IBullet
	{
	private:
		float  m_speed = 15.0f;

		// 寿命（秒）
		double m_lifeTime = 5.0;
		// 経過時間（秒）
		double m_elapsedTime = 0.0;

	public:
		DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~DefaultBullet() = default;

		// ----- IBullet -----
		/// <summary>
		/// プールから再利用される直前に呼ばれる。
		/// 寿命をリセットする。
		/// </summary>
		virtual void ResetForSpawn() noexcept override
		{
			m_elapsedTime = 0.0;
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
	class BombBullet : public IBullet
	{
	private:
		enum class BombState
		{
			Flying,
			Exploding
		};

		// 飛翔速度
		float  m_speed = 10.0f;

		// 爆発までの時間
		const double m_defaultFuseTime = 3.0;
		double m_fuseTime = m_defaultFuseTime;

		Vec3  m_velocity = Vec3(0, 0, 0);
		Vec3  m_gravity = Vec3(0, -9.8f, 0);
		Vec3  m_targetPos = Vec3(0, 0, 0);
		Vec3  m_targetNormal = Vec3(0, 1, 0);
		bool  m_hasTarget = false;
		bool  m_hasTargetSurface = false;
		float m_arcHeight = 1.5f;
		Vec3  m_startPos = Vec3(0, 0, 0);   // p0（発射時位置）
		Vec3  m_v0 = Vec3(0, 0, 0);   // v0（発射時初速）
		float m_flyTime = 0.0f;          // 発射からの経過 t
		float m_totalT = 0.0f;          // 目標到達に必要な T
		bool  m_useBallistic = false;     // ターゲット弾道を使うか
		bool  m_useGeneratedGroundImpact = true;
		float m_arcHeightPerDistXZ = 0.0f;

		BombState m_state = BombState::Flying;
		double m_explosionDuration = 0.08; // 爆風が有効な時間
		double m_explosionTimer = 0.0;
		ExplosionResolver m_explosionResolver;

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
			if (auto trans = GetComponent<Transform>(false))
			{
				trans->SetScale(Vec3(0.1f, 0.1f, 0.1f));
			}
		}

		// ----- Bom -----
		void SetTarget(const Vec3& target)
		{
			m_targetPos = target;
			m_targetNormal = Vec3(0, 1, 0);
			m_hasTarget = true;
			m_hasTargetSurface = false;
		}

		void SetAimFromPreview(const Vec3& target, const BombTuning& t, const Vec3& targetNormal, bool hasTargetSurface)
		{
			m_targetPos = target;
			m_targetNormal = targetNormal;
			m_hasTarget = true;
			m_hasTargetSurface = hasTargetSurface;

			m_arcHeight = t.arcHeightBase;
			m_gravity = t.gravity;
			m_arcHeightPerDistXZ = t.arcHeightPerDistXZ;

			// CollisionSphere の半径が「scale * 0.5」仕様なので、
			// radius を合わせたいなら scale = radius * 2 にするのが基本。
			m_explosionResolver.SetExplosionScale(t.explosionRadius * 2.0f);
		}

		// ----- GameObject -----
		void OnCreate() override;
		void OnUpdate(double elapsedTime) override;
		void OnCollisionEnter(const CollisionPair& pair) override;
		void OnCollisionExecute(const CollisionPair& pair) override;

	private:
		void StartExplosion(const std::shared_ptr<GameObject>& firstHit);
	};

}
