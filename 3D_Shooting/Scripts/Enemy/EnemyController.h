/*!
@file EnemyController.h
@brief 敵バッチ管理
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

	大量の敵を GameObject として個別更新せず、位置・速度・HP・アニメーションを
	配列でまとめて更新する。衝突だけは EnemyCollisionProxy に分離する。
	*/
	class EnemyController : public GameObject
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

			// 爆風で吹っ飛んでいる間だけ描画に足すランダム回転。実座標と当たり判定には反映しない。
			// rotationは移動方向の向きとして使うため、爆風演出は別クォータニオンに分けて保持する。
			Quat knockbackSpinRotation = Quat();
			// 回転軸。爆風を受けた瞬間にランダム決定し、回転中は固定する。
			Vec3 knockbackSpinAxis = Vec3(0.0f, 1.0f, 0.0f);
			// 1秒あたりの回転量。符号もランダムにして、敵ごとに回転方向を変える。
			float knockbackSpinSpeed = 0.0f;
			// 回転を進める残り時間。0になった後は、接地して演出が終わるまで最後の姿勢を保持する。
			double knockbackSpinTimer = 0.0;

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

			// active=falseになると更新・描画対象から外れ、次の敵生成時に同じスロットを再利用する。
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

		// GameStageがこのコントローラを所有する
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

		/*!
		@brief 空間グリッドのセル座標を一意なキーへ変換する
		@param x セルX座標
		@param z セルZ座標
		@return m_cellMap で使う64bitキー
		*/
		long long MakeCellKey(int x, int z) const;
		/*!
		@brief 現在の敵位置から近傍検索用の空間グリッドを再構築する
		*/
		void BuildSpatialGrid();
		/*!
		@brief 指定した敵に働く分離力を計算する
		@param index 対象敵のインデックス
		@return 近い敵から離れるための水平ベクトル
		*/
		Vec3 CalculateSeparation(size_t index) const;
		/*!
		@brief 配列側の敵状態をプロキシTransformへ同期する
		@param index 対象敵のインデックス
		*/
		void SyncProxyTransform(size_t index);
		/*!
		@brief 空きプロキシを取得し、なければ新規作成する
		@param index 割り当てる敵のインデックス
		@param startPosition 生成位置
		@param status 敵設定
		@return 使用可能な EnemyCollisionProxy
		*/
		std::shared_ptr<EnemyCollisionProxy> AcquireCollisionProxy(size_t index, const Vec3& startPosition, const EnemyStatus& status);
		/*!
		@brief 使用済みプロキシをプールへ戻す
		@param proxy 戻すプロキシ
		*/
		void ReleaseCollisionProxy(const std::shared_ptr<EnemyCollisionProxy>& proxy);
		/*!
		@brief 死亡アニメーションが終わった敵のプロキシを削除対象にする
		@param index 対象敵のインデックス
		*/
		void RemoveEnemyProxy(size_t index);
		/*!
		@brief 敵のアニメーション状態を切り替える
		@param enemy 更新対象の敵状態
		@param state 切り替え先のアニメーション
		@param forceRestart 同じ状態でも先頭から再生し直す場合は true
		*/
		void ChangeAnimation(EnemyState& enemy, AnimState state, bool forceRestart = false);
		/*!
		@brief 敵のアニメーション時間を進める
		@param enemy 更新対象の敵状態
		@param elapsedTime 経過時間
		*/
		void UpdateAnimation(EnemyState& enemy, double elapsedTime);
		/*!
		@brief 指定アニメーションの再生時間を取得する
		@param state 対象アニメーション
		@return 秒単位の再生時間。取得できない場合は0
		*/
		double GetAnimationDurationSeconds(AnimState state) const;
		/*!
		@brief 最終フレーム手前で止めるための保持時刻を返す
		@param duration アニメーション全体の長さ
		@return 停止に使う秒数
		*/
		double GetHoldTimeSeconds(double duration) const;
		/*!
		@brief 指定アニメーションが単発再生かを判定する
		@param state 対象アニメーション
		@return 単発再生なら true
		*/
		bool IsOneShotState(AnimState state) const;
		/*!
		@brief 指定アニメーションを最終フレーム付近で保持するかを判定する
		@param state 対象アニメーション
		@return 最終フレーム付近で止めるなら true
		*/
		bool IsHoldLastFrameState(AnimState state) const;
		/*!
		@brief 被弾フラッシュの現在値を取得する
		@param enemy 対象敵の状態
		@return 描画シェーダへ渡す0.0から1.0の値
		*/
		float GetDamageFlashValue(const EnemyState& enemy) const;
		/*!
		@brief 敵の位置にダメージ数値を表示する
		@param index 対象敵のインデックス
		@param info ダメージ情報
		*/
		void ShowDamageNumber(size_t index, const DamageInfo& info);
		/*!
		@brief 被弾フラッシュを開始する
		@param enemy 対象敵の状態
		@param duration フラッシュ時間
		*/
		void StartDamageFlash(EnemyState& enemy, double duration = 0.2);
		/*!
		@brief 通常被弾時の描画用押され演出を開始する
		@param enemy 対象敵の状態
		@param info ダメージ情報
		*/
		void StartHitPush(EnemyState& enemy, const DamageInfo& info);
		/*!
		@brief 敵を死亡状態へ切り替える
		@param enemy 対象敵の状態
		*/
		void KillEnemy(EnemyState& enemy);
		/*!
		@brief 落下死ラインを超えた敵を死亡させる
		@param enemy 対象敵の状態
		*/
		void KillByFall(EnemyState& enemy);
		/*!
		@brief 敵の描画向きを速度方向へ補間する
		@param enemy 対象敵の状態
		@param lerpFact 回転補間率
		*/
		void RotateToVelocity(EnemyState& enemy, float lerpFact);
		/*!
		@brief CSV地形と中央床に対して敵の接地位置を解決する
		@param enemy 対象敵の状態
		@param elapsedTime 経過時間
		@return 接地できた場合は true
		*/
		bool ResolveGeneratedGround(EnemyState& enemy, double elapsedTime);

	public:
		/*!
		@brief 敵バッチコントローラを生成する
		@param stage 所属するステージ
		*/
		explicit EnemyController(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyController();
		/*!
		@brief 共有オブジェクト登録と初期状態を設定する
		*/
		virtual void OnCreate() override;
		/*!
		@brief 敵配列の移動、重力、地形追従、死亡更新を行う
		@param elapsedTime 経過時間
		*/
		virtual void OnUpdate(double elapsedTime) override;
		/*!
		@brief CollisionManager の結果を配列側へ戻す
		@param elapsedTime 経過時間。この処理では使用しない
		*/
		virtual void OnUpdate2(double elapsedTime) override;

		/*!
		@brief 既定設定で敵を1体追加する
		@param startPosition 生成位置
		@return 追加された敵のインデックス
		*/
		size_t AddEnemy(const Vec3& startPosition);
		/*!
		@brief 指定設定で敵を1体追加する
		@param startPosition 生成位置
		@param status 敵の調整値
		@return 割り当てられた敵スロットのインデックス

		死亡済みの空きスロットがある場合はその EnemyState を初期化して再利用し、
		空きがない場合だけ敵状態配列を拡張する。
		*/
		size_t AddEnemy(const Vec3& startPosition, const EnemyStatus& status);
		/*!
		@brief 敵コリジョンプロキシを事前生成してプールへ入れる
		@param count 確保しておきたいプロキシ数

		Wave開始時の AddGameObject<EnemyCollisionProxy> 集中を避けるため、
		ステージ開始時など負荷を逃がしやすいタイミングで呼ぶ。
		*/
		void PrewarmCollisionProxyPool(int count);
		/*!
		@brief 全敵に適用する移動速度倍率を設定する
		@param multiplier 速度倍率。0.1未満は0.1に丸める
		*/
		void SetMoveSpeedMultiplier(float multiplier);
		/*!
		@brief 現在の移動速度倍率を取得する
		@return 移動速度倍率
		*/
		float GetMoveSpeedMultiplier() const { return m_moveSpeedMultiplier; }
		/*!
		@brief 指定敵へダメージを適用する
		@param index 対象敵のインデックス
		@param info ダメージ情報
		@return このダメージで即死亡、または着地後の死亡が確定した場合は true
		*/
		bool ApplyDamage(size_t index, const DamageInfo& info);
		/*!
		@brief 指定敵へノックバック速度を与える
		@param index 対象敵のインデックス
		@param velocity 爆風などで与える速度
		*/
		void AddKnockback(size_t index, const Vec3& velocity);
		/*!
		@brief 指定敵へ爆風用のランダム回転演出を開始する
		@param index 対象敵のインデックス

		実座標とコリジョンには影響させず、敵描画レンダラーの描画行列にだけ反映する。
		*/
		void AddRandomRotation(size_t index);
		/*!
		@brief プロキシ側の接地衝突を敵状態へ反映する
		@param index 対象敵のインデックス
		@param pair 衝突情報
		*/
		void NotifyGroundCollision(size_t index, const CollisionPair& pair);
		/*!
		@brief 指定敵が生存しているかを判定する
		@param index 対象敵のインデックス
		@return 生存中なら true
		*/
		bool IsEnemyAlive(size_t index) const;
		/*!
		@brief 生存している敵の数を取得する
		@return 生存敵数
		*/
		int GetAliveEnemyCount() const;
		/*!
		@brief 現在確保されている敵状態スロット数を取得する
		@return 再利用待ちの空きスロットを含む敵状態配列の要素数
		*/
		int GetTotalEnemyCount() const;
		/*!
		@brief 描画用のスキンインスタンス配列を作成する
		@param outSources 出力先のインスタンス配列
		@param modelOffset モデル原点補正
		*/
		void FillInstanceSources(std::vector<SkinnedInstanceSource>& outSources, const Vec3& modelOffset) const;
	};

}
