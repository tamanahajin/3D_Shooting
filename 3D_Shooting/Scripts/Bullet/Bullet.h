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
		float  m_speed = 0.0f;

		// 寿命（秒）
		double m_lifeTime = 0.0;
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
		float  m_speed = 0.0f;

		// 爆発までの時間
		double m_defaultFuseTime = 0.0;
		double m_fuseTime = m_defaultFuseTime;

		Vec3  m_velocity = Vec3(0, 0, 0);
		Vec3  m_gravity = Vec3(0, -9.8f, 0);
		Vec3  m_targetPos = Vec3(0, 0, 0);
		Vec3  m_targetNormal = Vec3(0, 1, 0);
		bool  m_hasTarget = false;
		bool  m_hasTargetSurface = false;
		std::weak_ptr<GameObject> m_impactIgnoredObject;
		float m_arcHeight = 0.0f;
		Vec3  m_startPos = Vec3(0, 0, 0);   // p0（発射時位置）
		Vec3  m_v0 = Vec3(0, 0, 0);   // v0（発射時初速）
		float m_flyTime = 0.0f;          // 発射からの経過 t
		float m_totalT = 0.0f;          // 目標到達に必要な T
		bool  m_useBallistic = false;     // ターゲット弾道を使うか
		bool  m_useGeneratedGroundImpact = true;
		float m_arcHeightPerDistXZ = 0.0f;

		BombState m_state = BombState::Flying;
		double m_explosionDuration = 0.0; // 爆風が有効な時間
		double m_explosionTimer = 0.0;
		ExplosionResolver m_explosionResolver;

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
			m_impactIgnoredObject.reset();
		}

		void SetAimFromPreview(
			const Vec3& target,
			const WeaponTuning& t,
			const Vec3& targetNormal,
			bool hasTargetSurface,
			const std::shared_ptr<GameObject>& ignoredObject = nullptr)
		{
			m_targetPos = target;
			m_targetNormal = targetNormal;
			m_hasTarget = true;
			m_hasTargetSurface = hasTargetSurface;
			m_impactIgnoredObject = ignoredObject;

			m_speed = t.bombSpeed;
			m_defaultFuseTime = t.bombFuseTime;
			m_explosionDuration = t.explosionDuration;
			m_arcHeight = t.arcHeightBase;
			m_gravity = t.gravity;
			m_arcHeightPerDistXZ = t.arcHeightPerDistXZ;
			m_explosionResolver.SetExplosionDamage(t.explosionDamage);

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
