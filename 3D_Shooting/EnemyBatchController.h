/*!
@file EnemyBatchController.h
@brief 敵バッチ管理
*/

#pragma once
#include "stdafx.h"
#include <map>
#include <memory>
#include <vector>

namespace shooting {

	struct DamageInfo;
	class GameStage;
	class EnemyBatchController;
	class EnemyCollisionProxy;

	struct EnemyStatus
	{
		int maxHp = 3;
		float moveSpeed = 5.0f;
		Vec3 modelScale = Vec3(0.01f, 0.01f, 0.01f);
		float collisionRadius = 0.2f;
		float collisionHeight = 0.3f;
		double steeringInterval = 0.05;
		double damageFlashDuration = 0.2;
		float damageNumberOffsetY = 0.35f;
		double hitPushDuration = 0.30;
		float hitPushDistance = 0.16f;
		float hitPushLeanAngle = 0.36f;
	};

	class EnemyInstancedRenderer : public GameObject
	{
	private:
		std::shared_ptr<InstancedSkinnedDraw> m_Draw;
		std::vector<SkinnedInstanceSource> m_InstanceSources;
		Vec3 m_ModelOffset = Vec3(0.0f, -0.35f, 0.0f);

	public:
		explicit EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyInstancedRenderer();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		virtual void OnUpdate2(double elapsedTime) override;
	};

	class EnemyCollisionProxy : public GameObject
	{
	private:
		std::weak_ptr<EnemyBatchController> m_Controller;
		size_t m_EnemyIndex = 0;
		Vec3 m_StartPosition;
		Vec3 m_ModelScale = Vec3(0.01f, 0.01f, 0.01f);
		float m_CollisionRadius = 0.2f;
		float m_CollisionHeight = 0.3f;

		void HandleCollision(const CollisionPair& pair);

	public:
		EnemyCollisionProxy(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyBatchController>& controller,
			size_t enemyIndex,
			const Vec3& startPosition,
			const EnemyStatus& status);
		virtual ~EnemyCollisionProxy();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		virtual void OnCollisionEnter(const CollisionPair& pair) override;
		virtual void OnCollisionExecute(const CollisionPair& pair) override;

		size_t GetEnemyIndex() const { return m_EnemyIndex; }
		bool ApplyDamage(const DamageInfo& info);
		void AddKnockback(const Vec3& velocity);
		bool IsAlive() const;
	};

	class EnemyBatchController : public GameObject
	{
	private:
		// 1体の敵に必要な実行時状態。
		// 描画、移動、HP、アニメーションを配列で持つことで、大量の敵をまとめて効率よく更新する。
		struct EnemyState
		{
			// 移動・地形解決用。previousPositionは坂や高台との接触解決で使う。
			Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 previousPosition = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 velocity = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 force = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 gravityVelocity = Vec3(0.0f, 0.0f, 0.0f);

			// 爆弾などの吹っ飛び用。通常の追跡移動とは別に制御する。
			Vec3 knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
			double knockbackControlTimer = 0.0;

			// 描画用の向きと、一定間隔で追跡力を再計算するためのタイマー。
			Quat rotation = Quat();
			double steeringTimer = 0.0;
			double steeringInterval = 0.05;

			// 被弾時の赤フラッシュ。値は描画インスタンスへ渡す。
			double damageFlashTimer = 0.0;
			double damageFlashDuration = 0.2;

			// 被弾時の押され演出。実座標は動かさず、描画用の行列だけに反映する。
			double hitPushTimer = 0.0;
			double hitPushDuration = 0.10;
			float hitPushDistance = 0.16f;
			float hitPushLeanAngle = 0.16f;
			Vec3 hitPushDirection = Vec3(1.0f, 0.0f, 0.0f);

			// スキンアニメーション用。死亡などの単発アニメーションはanimationFinishedで止める。
			double animationTime = 0.0;
			AnimState animationState = AnimState::Idle;
			bool animationFinished = false;

			// active=falseになると配列には残すが、更新・描画対象から外す。
			bool active = true;
			bool isGround = false;
			bool isDead = false;
			bool deathAnimFinished = false;

			// 爆弾で致死ダメージを受けた場合、吹っ飛びが見えるよう着地まで死亡を遅らせる。
			bool delayDeathUntilLanding = false;
			bool delayedDeathWasAirborne = false;
			double delayedDeathMinTimer = 0.0;

			// 種類ごとの調整値と現在HP。
			EnemyStatus status;
			int hp = 20;
			int maxHp = 20;

			// CollisionManagerに参加するための軽量GameObject。描画やAI本体は持たない。
			std::weak_ptr<EnemyCollisionProxy> proxy;
		};

		// 敵本体の状態配列。EnemyCollisionProxyやEnemyInstancedRendererはこの配列を参照する。
		std::vector<EnemyState> m_Enemies;
		// 各敵の分離力を一時保存する。全敵位置を使うため、OnUpdate冒頭でまとめて計算する。
		std::vector<Vec3> m_SeparationForces;
		// 敵同士の分離計算を軽くするための空間グリッド。
		std::map<long long, std::vector<size_t>> m_CellMap;
		float m_CellSize = 2.0f;
		float m_SeparationRange = 2.0f;
		float m_MoveSpeedMultiplier = 1.0f;

		long long MakeCellKey(int x, int z) const;
		void BuildSpatialGrid();
		Vec3 CalculateSeparation(size_t index) const;
		void SyncProxyTransform(size_t index);
		void RemoveEnemyProxy(size_t index);
		void ChangeAnimation(EnemyState& enemy, AnimState state, bool forceRestart = false);
		void UpdateAnimation(EnemyState& enemy, double elapsedTime);
		double GetAnimationDurationSeconds(AnimState state) const;
		double GetHoldTimeSeconds(double duration) const;
		bool IsOneShotState(AnimState state) const;
		bool IsHoldLastFrameState(AnimState state) const;
		float GetDamageFlashValue(const EnemyState& enemy) const;
		void ShowDamageNumber(size_t index, const DamageInfo& info);
		void StartDamageFlash(EnemyState& enemy, double duration = 0.2);
		void StartHitPush(EnemyState& enemy, const DamageInfo& info);
		void KillEnemy(EnemyState& enemy);
		void KillByFall(EnemyState& enemy);
		void RotateToVelocity(EnemyState& enemy, float lerpFact);
		bool ResolveGeneratedGround(const GameStage& gameStage, EnemyState& enemy, double elapsedTime);

	public:
		explicit EnemyBatchController(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyBatchController();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;
		virtual void OnUpdate2(double elapsedTime) override;

		size_t AddEnemy(const Vec3& startPosition);
		size_t AddEnemy(const Vec3& startPosition, const EnemyStatus& status);
		void SetMoveSpeedMultiplier(float multiplier);
		float GetMoveSpeedMultiplier() const { return m_MoveSpeedMultiplier; }
		bool ApplyDamage(size_t index, const DamageInfo& info);
		void AddKnockback(size_t index, const Vec3& velocity);
		void NotifyGroundCollision(size_t index, const CollisionPair& pair);
		bool IsEnemyAlive(size_t index) const;
		int GetAliveEnemyCount() const;
		int GetTotalEnemyCount() const;
		void FillInstanceSources(std::vector<SkinnedInstanceSource>& outSources, const Vec3& modelOffset) const;
	};

}
