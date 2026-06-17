/*!
@file Character.h
@brief 配置オブジェクト
*/


#pragma once
#include "stdafx.h"

namespace shooting {

	struct DamageInfo;
	class SeekObject;
	class InstancedStaticDraw;
	//--------------------------------------------------------------------------------------
	// フロアオブジェクト（見た目専用）
	//--------------------------------------------------------------------------------------
	class Floor : public GameObject {
	private:
		std::wstring m_meshKey;
		std::wstring m_materialPrefix;

	public:
		Floor(
			const std::shared_ptr<Stage>& stage,
			const TransParam& param,
			const std::wstring& meshKey = L"FLOOR_MODEL",
			const std::wstring& materialPrefix = L"FLOOR_MAT_");

		virtual ~Floor();
		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override {}

		const std::wstring& GetMeshKey() const { return m_meshKey; }
		const std::wstring& GetMaterialPrefix() const { return m_materialPrefix; }
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
		std::wstring m_meshKey;
		std::wstring m_materialPrefix;
		std::vector<Mat4x4> m_instanceWorlds;

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
		std::wstring m_meshKey;
		std::wstring m_materialPrefix;
		std::vector<Mat4x4> m_instanceWorlds;
		// 通常描画用とは別に、shadow pass で描く範囲内インスタンスだけを保持する。
		std::vector<Mat4x4> m_shadowInstanceWorlds;
		// shadow 用インスタンス buffer を更新するため、作成した描画コンポーネントを保持する。
		std::shared_ptr<InstancedStaticDraw> m_draw;
		// カメラ注視点が大きく動いたときだけ shadow 用配列を再構築するための状態。
		Vec3 m_lastShadowCullAt;
		bool m_shadowCullInitialized = false;

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
		Vec3 m_collisionSize;

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
		float m_radius;
		float m_height;

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


	//--------------------------------------------------------------------------------------
	// フロアコリジョンオブジェクト（当たり判定専用）
	//--------------------------------------------------------------------------------------
	class FloorCollision : public GameObject {
	private:
		Vec3 m_collisionSize;

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
		std::unique_ptr< StateMachine<SeekObject> >  m_stateMachine;
		Vec3 m_startPos;
		float m_stateChangeSize;
		//フォース
		Vec3 m_force;
		//速度
		Vec3 m_velocity;
		double m_steeringUpdateTimer = 0.0;
		double m_steeringUpdateInterval = 0.05;
		bool m_isGround = false;
		bool m_isDead = false;
		bool m_deathAnimFinished = false;
		double m_damageFlashTimer = 0.0;
		double m_damageFlashDuration = 0.2;

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
			return m_stateMachine;
		}
		float GetStateChangeSize() const
		{
			return m_stateChangeSize;
		}
		const Vec3& GetForce()const
		{
			return m_force;
		}
		void SetForce(const Vec3& f)
		{
			m_force = f;
		}
		void AddForce(const Vec3& f)
		{
			m_force += f;
		}
		const Vec3& GetVelocity()const
		{
			return m_velocity;
		}
		void SetVelocity(const Vec3& v)
		{
			m_velocity = v;
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

