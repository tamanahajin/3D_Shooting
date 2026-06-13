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
	class EnemyIndividualDrawProxy;
	class BcPNTBoneDraw;

	/*!
	@brief 敵1体分の調整値

	HP、移動速度、当たり判定、被弾演出など、敵種別ごとに変えたい値をまとめる。
	EnemyBatchController::EnemyState はこの設定値をコピーして実行時状態と一緒に保持する。
	*/
	struct EnemyStatus
	{
		int maxHp = 3;
		float moveSpeed = 5.0f;
		Vec3 modelScale = Vec3(0.01f, 0.01f, 0.01f);
		float collisionRadius = 0.2f;
		float collisionHeight = 0.3f;
		float groundFootOffset = 0.35f;
		double steeringInterval = 0.05;
		double damageFlashDuration = 0.2;
		float damageNumberOffsetY = 0.35f;
		double hitPushDuration = 0.30;
		float hitPushDistance = 0.16f;
		float hitPushLeanAngle = 0.36f;
	};

	/*!
	@brief 敵バッチをまとめて描画するインスタンシング描画オブジェクト

	EnemyBatchController から描画用インスタンス配列を受け取り、
	InstancedSkinnedDraw へ渡す。敵1体ごとの GameObject 描画は行わない。
	*/
	class EnemyInstancedRenderer : public GameObject
	{
	private:
		std::weak_ptr<EnemyBatchController> m_Controller;
		std::shared_ptr<InstancedSkinnedDraw> m_Draw;
		std::vector<SkinnedInstanceSource> m_InstanceSources;
		Vec3 m_ModelOffset = Vec3(0.0f, -0.35f, 0.0f);
		bool m_RenderingEnabled = true;

	public:
		/*!
		@brief 敵描画オブジェクトを生成する
		@param stage 所属するステージ
		*/
		explicit EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage);
		/*!
		@brief 敵描画オブジェクトを生成し、参照する敵バッチコントローラを保持する
		@param stage 所属するステージ
		@param controller 描画元になる敵バッチコントローラ
		*/
		EnemyInstancedRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyBatchController>& controller);
		virtual ~EnemyInstancedRenderer();
		/*!
		@brief 描画コンポーネントと敵描画タグを初期化する
		*/
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief EnemyBatchController の状態を描画インスタンスへ反映する
		@param elapsedTime 経過時間。この処理では使用しない
		*/
		virtual void OnUpdate2(double elapsedTime) override;
		/*!
		@brief インスタンシング描画の有効状態を切り替える
		@param enabled 有効にする場合は true
		*/
		void SetRenderingEnabled(bool enabled);
		/*!
		@brief 現在インスタンシング描画が有効かを取得する
		@return 有効なら true
		*/
		bool IsRenderingEnabled() const { return m_RenderingEnabled; }
	};

	/*!
	@brief 敵1体を通常のスキンメッシュ描画で表示する軽量プロキシ

	動画比較用に、EnemyBatchController の配列更新はそのまま使い、
	描画だけを BcPNTBoneDraw の1体ずつの DrawIndexedInstanced(1) に置き換える。
	*/
	class EnemyIndividualDrawProxy : public GameObject
	{
	private:
		std::shared_ptr<BcPNTBoneDraw> m_Draw;

	public:
		/*!
		@brief 通常描画プロキシを生成する
		@param stage 所属するステージ
		*/
		explicit EnemyIndividualDrawProxy(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyIndividualDrawProxy();
		/*!
		@brief BcPNTBoneDraw を作成し、敵モデルとテクスチャを設定する
		*/
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief インスタンス描画用データを通常描画用 Transform とアニメーションへ反映する
		@param source EnemyBatchController が作成した描画用データ
		*/
		void ApplyInstanceSource(const SkinnedInstanceSource& source);
		/*!
		@brief プロキシの描画・更新を切り替える
		@param enabled 有効にする場合は true
		*/
		void SetRenderingEnabled(bool enabled);
	};

	/*!
	@brief 敵を1体ずつ通常描画するレンダラー

	EnemyBatchController の描画用データを、敵数分の EnemyIndividualDrawProxy へ同期する。
	インスタンシングを使わない比較動画用の描画経路。
	*/
	class EnemyIndividualRenderer : public GameObject
	{
	private:
		std::weak_ptr<EnemyBatchController> m_Controller;
		std::vector<std::shared_ptr<EnemyIndividualDrawProxy>> m_DrawProxies;
		std::vector<SkinnedInstanceSource> m_InstanceSources;
		Vec3 m_ModelOffset = Vec3(0.0f, -0.35f, 0.0f);
		bool m_RenderingEnabled = false;

		/*!
		@brief 描画に必要なプロキシ数を揃える
		@param requiredCount 必要なプロキシ数
		*/
		void ResizeDrawProxies(size_t requiredCount);
		/*!
		@brief 生成済みプロキシをステージから外す
		*/
		void ClearDrawProxies();

	public:
		/*!
		@brief 通常描画レンダラーを生成する
		@param stage 所属するステージ
		*/
		explicit EnemyIndividualRenderer(const std::shared_ptr<Stage>& stage);
		/*!
		@brief 通常描画レンダラーを生成し、参照する敵バッチコントローラを保持する
		@param stage 所属するステージ
		@param controller 描画元になる敵バッチコントローラ
		*/
		EnemyIndividualRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<EnemyBatchController>& controller);
		virtual ~EnemyIndividualRenderer();
		/*!
		@brief レンダラー本体のタグと初期無効状態を設定する
		*/
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
		/*!
		@brief EnemyBatchController の状態を通常描画プロキシへ反映する
		@param elapsedTime 経過時間。この処理では使用しない
		*/
		virtual void OnUpdate2(double elapsedTime) override;
		/*!
		@brief 通常描画経路の有効状態を切り替える
		@param enabled 有効にする場合は true
		*/
		void SetRenderingEnabled(bool enabled);
		/*!
		@brief 現在通常描画経路が有効かを取得する
		@return 有効なら true
		*/
		bool IsRenderingEnabled() const { return m_RenderingEnabled; }
	};

	/*!
	@brief 敵1体分の軽量コリジョンプロキシ

	敵の描画・AI・HPは EnemyBatchController の配列で管理し、
	このクラスは CollisionManager に参加するための Transform と Collision だけを持つ。
	*/
	class EnemyCollisionProxy : public GameObject
	{
	private:
		std::weak_ptr<EnemyBatchController> m_Controller;
		size_t m_EnemyIndex = 0;
		Vec3 m_StartPosition;
		Vec3 m_ModelScale = Vec3(0.01f, 0.01f, 0.01f);
		float m_CollisionRadius = 0.2f;
		float m_CollisionHeight = 0.3f;
		bool m_InUse = true;

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
		@param enemyIndex m_Enemies 内の対象インデックス
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
		@param enemyIndex m_Enemies 内の対象インデックス
		@param startPosition 生成位置
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
		bool IsInUse() const { return m_InUse; }
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
		@return m_Enemies 内のインデックス
		*/
		size_t GetEnemyIndex() const { return m_EnemyIndex; }
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

	/*!
	@brief 敵の実行時状態を配列でまとめて管理するコントローラ

	大量の敵を GameObject として個別更新せず、位置・速度・HP・アニメーションを
	配列でまとめて更新する。衝突だけは EnemyCollisionProxy に分離する。
	*/
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

		// GameStageがこのコントローラを所有する
		std::weak_ptr<GameStage> m_GameStage;
		// 敵本体の状態配列。EnemyCollisionProxyやEnemyInstancedRendererはこの配列を参照する。
		std::vector<EnemyState> m_Enemies;
		// 死亡済み敵のコリジョンプロキシを再利用するためのプール。
		std::vector<std::shared_ptr<EnemyCollisionProxy>> m_CollisionProxyPool;
		// 各敵の分離力を一時保存する。全敵位置を使うため、OnUpdate冒頭でまとめて計算する。
		std::vector<Vec3> m_SeparationForces;
		// 敵同士の分離計算を軽くするための空間グリッド。
		std::map<long long, std::vector<size_t>> m_CellMap;
		float m_CellSize = 2.0f;
		float m_SeparationRange = 2.0f;
		float m_MoveSpeedMultiplier = 1.0f;

		/*!
		@brief 空間グリッドのセル座標を一意なキーへ変換する
		@param x セルX座標
		@param z セルZ座標
		@return m_CellMap で使う64bitキー
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
		explicit EnemyBatchController(const std::shared_ptr<Stage>& stage);
		virtual ~EnemyBatchController();
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
		@return 追加された敵のインデックス
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
		float GetMoveSpeedMultiplier() const { return m_MoveSpeedMultiplier; }
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
		@brief これまでに配列へ追加された敵数を取得する
		@return 敵配列の総数
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
