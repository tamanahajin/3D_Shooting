/*!
@file EnemyController.h
@brief 敵の配列管理、移動、戦闘、描画データ生成をまとめるコントローラ
*/

#pragma once
#include "stdafx.h"
#include "EnemyStatus.h"
#include <map>
#include <memory>
#include <vector>

namespace shooting {

	struct DamageInfo;
	class GameStage;
	class EnemyCollisionProxy;

	/*!
	@brief 敵の実行時状態を配列でまとめて管理するコントローラ

	大量の敵を GameObject として個別更新せず、位置・速度・HP・アニメーションを配列で更新する。
	CollisionManager へ参加する必要がある衝突だけは EnemyCollisionProxy に分離する。
	*/
	class EnemyController : public GameObject
	{
	private:
		enum class LandingDeathState
		{
			None,
			WaitingForAirborne,
			WaitingForLanding
		};

		struct EnemyState
		{
			Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 previousPosition = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 velocity = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 force = Vec3(0.0f, 0.0f, 0.0f);
			Vec3 gravityVelocity = Vec3(0.0f, 0.0f, 0.0f);

			// 爆弾などの吹っ飛び用。通常の追跡移動とは別に制御する。
			Vec3 knockbackVelocity = Vec3(0.0f, 0.0f, 0.0f);
			double knockbackControlTimer = 0.0;
			double knockbackLaunchTimer = 0.0;

			// rotationは移動方向の向きとして使うため、爆風演出は別クォータニオンに分けて保持する。
			Quat knockbackSpinRotation = Quat();
			Vec3 knockbackSpinAxis = Vec3(0.0f, 1.0f, 0.0f);
			float knockbackSpinSpeed = 0.0f;
			double knockbackSpinTimer = 0.0;

			Quat rotation = Quat();
			double steeringTimer = 0.0;
			double steeringInterval = 0.05;

			double damageFlashTimer = 0.0;
			double damageFlashDuration = 0.2;

			// 被弾時の押され演出。実座標は動かさず、描画用の行列だけに反映する。
			double hitPushTimer = 0.0;
			double hitPushDuration = 0.10;
			float hitPushDistance = 0.16f;
			float hitPushLeanAngle = 0.16f;
			Vec3 hitPushDirection = Vec3(1.0f, 0.0f, 0.0f);

			double animationTime = 0.0;
			AnimState animationState = AnimState::Idle;
			bool animationFinished = false;

			// active=falseになると更新・描画対象から外れ、次の敵生成時に同じスロットを再利用する。
			bool active = true;
			bool isGround = false;
			bool isDead = false;
			bool deathAnimFinished = false;

			// 致死ダメージ後の離陸待ちと着地待ちを区別し、その場で死亡することを防ぐ。
			LandingDeathState landingDeathState = LandingDeathState::None;
			double delayedDeathMinTimer = 0.0;

			EnemyStatus status;
			int hp = 20;
			int maxHp = 20;

			// CollisionManagerに参加するための軽量GameObject。描画やAI本体は持たない。
			std::weak_ptr<EnemyCollisionProxy> proxy;
		};

		std::weak_ptr<GameStage> m_gameStage;
		// 敵本体の状態配列。EnemyCollisionProxyやEnemyInstancedRendererはこの配列を参照する。
		std::vector<EnemyState> m_enemies;
		// 使用を終えた敵状態のインデックス。新しい敵はここからスロットを取得して再利用する。
		std::vector<size_t> m_freeEnemyIndices;
		// 死亡済み敵のコリジョンプロキシを再利用するためのプール。
		std::vector<std::shared_ptr<EnemyCollisionProxy>> m_collisionProxyPool;
		// 各敵の分離力を一時保存する。全敵位置を使うため、OnUpdate冒頭でまとめて計算する。
		std::vector<Vec3> m_separationForces;
		// 敵同士の分離計算を軽くするための空間グリッド。
		std::map<long long, std::vector<size_t>> m_cellMap;
		float m_cellSize = 2.0f;
		float m_separationRange = 2.0f;
		float m_moveSpeedMultiplier = 1.0f;

		long long MakeCellKey(int x, int z) const;
		/*!
		@brief 敵同士の分離計算用に近傍検索グリッドを作る

		敵数が増えたときに全組み合わせ比較にならないよう、現在位置からセルへ振り分ける。
		*/
		void BuildSpatialGrid();
		/*!
		@brief 近くの敵から離れるための水平分離力を計算する
		*/
		Vec3 CalculateSeparation(size_t index) const;
		void SyncProxyTransform(size_t index);
		/*!
		@brief 空きプロキシを優先して取得し、なければ新規作成する

		Wave 開始時に CollisionCapsule 生成が集中しないよう、死亡済みプロキシを再利用する。
		*/
		std::shared_ptr<EnemyCollisionProxy> AcquireCollisionProxy(size_t index, const Vec3& startPosition, const EnemyStatus& status);
		void ReleaseCollisionProxy(const std::shared_ptr<EnemyCollisionProxy>& proxy);
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
		bool IsKnockbackActive(const EnemyState& enemy) const;
		void RotateToVelocity(EnemyState& enemy, float lerpFact);
		/*!
		@brief CSV地形・坂・高台・中央床に対して敵の接地位置を解決する

		敵は通常の物理 GameObject ではないため、バッチ側の状態を地形解決用構造体へ詰め替えて処理する。
		*/
		bool ResolveGeneratedGround(EnemyState& enemy, double elapsedTime);

	public:
		explicit EnemyController(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyController();
		virtual void OnCreate() override;
		/*!
		@brief 敵配列の移動、重力、地形追従、死亡更新をまとめて進める
		*/
		virtual void OnUpdate(double elapsedTime) override;
		/*!
		@brief CollisionManager が補正したプロキシ位置を配列側へ戻す
		*/
		virtual void OnUpdate2(double elapsedTime) override;

		size_t AddEnemy(const Vec3& startPosition);
		/*!
		@brief 敵状態スロットとコリジョンプロキシを再利用しながら敵を追加する

		配列要素を詰めるとプロキシが参照するインデックスがずれるため、空きスロットとして再利用する。
		*/
		size_t AddEnemy(const Vec3& startPosition, const EnemyStatus& status);
		/*!
		@brief 敵コリジョンプロキシを事前生成してプールへ入れる

		敵生成フレームの負荷を避けたい場面で、ステージ開始時などに先に呼び出す。
		*/
		void PrewarmCollisionProxyPool(int count);
		void SetMoveSpeedMultiplier(float multiplier);
		float GetMoveSpeedMultiplier() const { return m_moveSpeedMultiplier; }
		/*!
		@brief 敵へダメージを適用し、即死または遅延死亡の成立を返す

		爆弾で吹っ飛ばす場合は死亡を着地後へ遅らせるため、戻り値は「撃破確定」の意味を含む。
		*/
		bool ApplyDamage(size_t index, const DamageInfo& info);
		/*!
		@brief 爆風などで敵へノックバック速度を与える
		*/
		void AddKnockback(size_t index, const Vec3& velocity);
		/*!
		@brief 爆風で吹っ飛ぶ敵に描画用のランダム回転を開始する

		実座標とコリジョンには反映せず、描画行列だけへ合成する。
		*/
		void AddRandomRotation(size_t index);
		/*!
		@brief プロキシ側で検出した接地衝突を敵状態へ反映する
		*/
		void NotifyGroundCollision(size_t index, const CollisionPair& pair);
		bool IsEnemyAlive(size_t index) const;
		/*!
		@brief 指定した敵がプレイヤーへ接触ダメージを与えられるか返す
		*/
		bool CanDamagePlayer(size_t index) const;
		/*! @brief 指定した敵がプレイヤーへ接触時に与えるダメージを返す */
		int GetContactDamage(size_t index) const;
		int GetAliveEnemyCount() const;
		int GetTotalEnemyCount() const;
		/*!
		@brief 敵配列からスキンメッシュ描画用インスタンス配列を作成する

		被弾押されや爆風回転など、実座標に影響させない演出もここで描画行列へ反映する。
		*/
		void FillInstanceSources(std::vector<SkinnedInstanceSource>& outSources, const Vec3& modelOffset) const;
	};

}
