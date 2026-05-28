/*!
@file Character.h
@brief 配置オブジェクト
*/


#pragma once
#include "stdafx.h"

namespace shooting {

	struct DamageInfo;
	class EnemyBatchController;
	class GameStage;
	class EnemyCollisionProxy;
	class SeekObject;
	class InstancedStaticDraw;

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
	};
	//--------------------------------------------------------------------------------------
	// フロアオブジェクト（見た目専用）
	//--------------------------------------------------------------------------------------
	class Floor : public GameObject {
	private:
		std::wstring m_MeshKey;
		std::wstring m_MaterialPrefix;

	public:
		Floor(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param,
			const std::wstring& meshKey = L"FLOOR_MODEL",
			const std::wstring& materialPrefix = L"FLOOR_MAT_");

		virtual ~Floor();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}

		const std::wstring& GetMeshKey() const { return m_MeshKey; }
		const std::wstring& GetMaterialPrefix() const { return m_MaterialPrefix; }
	};

	class SkyDome : public GameObject
	{
	public:
		explicit SkyDome(const std::shared_ptr<Stage>& stage);
		virtual ~SkyDome();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
	};

	class FloorInstancedRenderer : public GameObject
	{
	private:
		std::wstring m_MeshKey;
		std::wstring m_MaterialPrefix;
		std::vector<Mat4x4> m_InstanceWorlds;

	public:
		FloorInstancedRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::wstring& meshKey,
			const std::wstring& materialPrefix,
			const std::vector<Mat4x4>& instanceWorlds);

		virtual ~FloorInstancedRenderer();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
	};
	class StageObjectInstancedRenderer : public GameObject
	{
	private:
		std::wstring m_MeshKey;
		std::wstring m_MaterialPrefix;
		std::vector<Mat4x4> m_InstanceWorlds;
		// 通常描画用とは別に、shadow pass で描く範囲内インスタンスだけを保持する。
		std::vector<Mat4x4> m_ShadowInstanceWorlds;
		// shadow 用インスタンス buffer を更新するため、作成した描画コンポーネントを保持する。
		std::shared_ptr<InstancedStaticDraw> m_Draw;
		// カメラ注視点が大きく動いたときだけ shadow 用配列を再構築するための状態。
		Vec3 m_LastShadowCullAt;
		bool m_ShadowCullInitialized = false;

		// shadow map 範囲内のインスタンスだけに絞り、描画コンポーネントへ反映する。
		void RefreshShadowInstances(bool force);

	public:
		StageObjectInstancedRenderer(
			const std::shared_ptr<Stage>& stage,
			const std::wstring& meshKey,
			const std::wstring& materialPrefix,
			const std::vector<Mat4x4>& instanceWorlds);

		virtual ~StageObjectInstancedRenderer();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;
	};


	class StageCollisionBox : public GameObject
	{
	private:
		Vec3 m_CollisionSize;

	public:
		StageCollisionBox(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param,
			const Vec3& collisionSize);

		virtual ~StageCollisionBox();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
	};

	class StageCollisionCapsule : public GameObject
	{
	private:
		float m_Radius;
		float m_Height;

	public:
		StageCollisionCapsule(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param,
			float radius,
			float height);

		virtual ~StageCollisionCapsule();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
	};


	class SlopeCollisionDebugBox : public GameObject
	{
	public:
		SlopeCollisionDebugBox(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param);

		virtual ~SlopeCollisionDebugBox();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
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
	//--------------------------------------------------------------------------------------
	// フロアコリジョンオブジェクト（当たり判定専用）
	//--------------------------------------------------------------------------------------
	class FloorCollision : public GameObject {
	private:
		Vec3 m_CollisionSize;

	public:
		FloorCollision(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param,
			const Vec3& collisionSize);

		virtual ~FloorCollision();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}
	};

	//--------------------------------------------------------------------------------------
	// ボックスオブジェクト
	//--------------------------------------------------------------------------------------
	class FixedBox : public GameObject {
	public:
		FixedBox(const std::shared_ptr<Stage>& stage, const TransParam& param);
		virtual ~FixedBox();
		//構築時
		virtual void OnCreate()override;
		//更新時
		virtual void OnUpdate(double elapsedTime)override {}
	};

	//--------------------------------------------------------------------------------------
	// 四角のオブジェクト
	//--------------------------------------------------------------------------------------
	class WallBox : public  GameObject {
		double m_totalTime;
	protected:
	public:
		WallBox(const std::shared_ptr<Stage>& stage, const TransParam& param);
		virtual ~WallBox();
		virtual void OnCreate();
		virtual void OnUpdate(double elapsedTime);
	};

	//--------------------------------------------------------------------------------------
	//	追いかける配置オブジェクト
	//--------------------------------------------------------------------------------------
	class SeekObject : public GameObject {
		std::shared_ptr<BaseMesh> m_baseMesh;
		double m_totalTime;

		//ステートマシーン
		std::unique_ptr< StateMachine<SeekObject> >  m_StateMachine;
		Vec3 m_StartPos;
		float m_StateChangeSize;
		//フォース
		Vec3 m_Force;
		//速度
		Vec3 m_Velocity;
		double m_SteeringUpdateTimer = 0.0;
		double m_SteeringUpdateInterval = 0.05;
		bool m_IsGround = false;
		bool m_IsDead = false;
		bool m_DeathAnimFinished = false;
		double m_DamageFlashTimer = 0.0;
		double m_DamageFlashDuration = 0.2;

		void CheckGroundCollision(const CollisionPair& pair);
		void ShowDamageNumber(const DamageInfo& info);
		Vec3 GetDamageNumberPosition();
	public:
		//構築と破棄
		SeekObject(const std::shared_ptr<Stage>& StagePtr, const Vec3& startPos);
		virtual ~SeekObject();
		//初期化
		virtual void OnCreate() override;
		//アクセサ
		const std::unique_ptr<StateMachine<SeekObject>>& GetStateMachine()
		{
			return m_StateMachine;
		}
		float GetStateChangeSize() const
		{
			return m_StateChangeSize;
		}
		const Vec3& GetForce()const
		{
			return m_Force;
		}
		void SetForce(const Vec3& f)
		{
			m_Force = f;
		}
		void AddForce(const Vec3& f)
		{
			m_Force += f;
		}
		const Vec3& GetVelocity()const
		{
			return m_Velocity;
		}
		void SetVelocity(const Vec3& v)
		{
			m_Velocity = v;
		}
		void ApplyForce();
		void ApplyForce(double elapsedTime);
		void ApplyGravity(double elapsedTime);
		void RotateToVelocity(float lerpFact);
		void UpdateBatched(double elapsedTime, const Vec3& targetPosition, const Vec3& separationForce);
		Vec3 GetTargetPos()const;
		void StartDamageFlash(double duration = 0.2);
		float GetDamageFlashValue() const;
		//操作
		virtual void OnUpdate(double elapsedTime) override;
		virtual void OnCollisionEnter(const CollisionPair& pair) override;
		virtual void OnCollisionExecute(const CollisionPair& pair) override;
	};

	//--------------------------------------------------------------------------------------
	//	class SeekFarState : public ObjState<SeekObject>;
	//	用途: プレイヤーから遠いときの移動
	//--------------------------------------------------------------------------------------
	class SeekFarState : public ObjState<SeekObject>
	{
		SeekFarState() {}
	public:
		static std::shared_ptr<SeekFarState> Instance();
		virtual void Enter(const std::shared_ptr<SeekObject>& Obj)override;
		virtual void Execute(const std::shared_ptr<SeekObject>& Obj)override;
		virtual void Exit(const std::shared_ptr<SeekObject>& Obj)override;
	};

	//--------------------------------------------------------------------------------------
	//	class SeekNearState : public ObjState<SeekObject>;
	//	用途: プレイヤーから近いときの移動
	//--------------------------------------------------------------------------------------
	class SeekNearState : public ObjState<SeekObject>
	{
		SeekNearState() {}
	public:
		static std::shared_ptr<SeekNearState> Instance();
		virtual void Enter(const std::shared_ptr<SeekObject>& Obj)override;
		virtual void Execute(const std::shared_ptr<SeekObject>& Obj)override;
		virtual void Exit(const std::shared_ptr<SeekObject>& Obj)override;
	};

	
}

