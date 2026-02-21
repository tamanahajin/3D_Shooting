#pragma once
#include "stdafx.h"

namespace shooting {
	
	class DefaultBullet : public IBullet {
	private:
		float m_Speed;
		bool m_IsActive;
		//寿命
		double m_LifeTime;
		//経過時間
		double m_ElapsedTime;
	public:
		DefaultBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~DefaultBullet() {}

		bool IsActive() const noexcept;
		void SetActive(bool active) noexcept;

		//構築時処理
		virtual void OnCreate()override;
		//更新時処理
		virtual void OnUpdate(double elapsedTime);
		//衝突開始時処理
		virtual void OnCollisionEnter(const CollisionPair& pair)override;
		//衝突継続時処理
		virtual void OnCollisionExecute(const CollisionPair& pair)override;

		void ResetLife() noexcept
		{
			m_ElapsedTime = 0.0;
			SetActive(true);
		}
	};

	/// <summary>
	/// ゲーム内「爆弾弾」：外周当たり/着弾/時間で爆発し、範囲ダメージを与える
	///
	/// 重要ポイント：
	/// - BulletPool は DefaultBullet を dynamic_cast して IsActive()/ResetLife() を見る設計
	///   → BombBullet を DefaultBullet 派生にすると、そのままプール運用できる
	/// </summary>
	class BombBullet : public DefaultBullet
	{
	private:
		// 飛翔パラメータ
		float  m_Speed = 10.0f;

		// 信管（秒）：0になると爆発へ
		double m_FuseTime = 1.0;

		// 爆発状態
		bool   m_Exploding = false;
		double m_ExplosionDuration = 0.08; // 爆風が有効な時間（短く）
		double m_ExplosionTimer = 0.0;

		// 爆風の大きさ（TransformのScaleを大きくして範囲表現する想定）
		float  m_ExplosionScale = 3.0f;

		// 範囲ダメージ
		int    m_ExplosionDamage = 12;

		// 同じ相手に多重ヒットしないための記録（爆発中だけ使う）
		std::unordered_set<const GameObject*> m_HitOnce;

	public:
		BombBullet(const std::shared_ptr<Stage>& stagePtr, const TransParam& param);
		virtual ~BombBullet() {}

		// コンポーネント生成
		void OnCreate() override;

		// 独自更新（Flying / Exploding）
		void OnUpdate(double elapsedTime) override;

		// 着弾したら即爆発
		void OnCollisionEnter(const CollisionPair& pair) override;

		// 必要なら継続衝突でも処理したい場合に使用
		void OnCollisionExecute(const CollisionPair& pair) override;

	private:
		// 爆発開始
		void StartExplosion(const std::shared_ptr<GameObject>& firstHit);

		// 爆発中に当たった対象へダメージ
		void TryApplyExplosionDamage(const std::shared_ptr<GameObject>& target);
	};
}