/*!
@file Character.cpp
@brief 配置オブジェクト 実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	//--------------------------------------------------------------------------------------
	// ボックスオブジェクト
	//--------------------------------------------------------------------------------------
	FixedBox::FixedBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage)
	{
		m_transParam = param;
	}
	FixedBox::~FixedBox() {}

	void FixedBox::OnCreate()
	{
		ID3D12GraphicsCommandList* pCommandList = BaseScene::Get()->m_pTgtCommandList;
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetFixed(true);
		//タグをつける
		AddTag(L"FixedBox");
		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"SKY_TX");
		ptrDraw->SetOwnShadowActive(true);
	}

	//--------------------------------------------------------------------------------------
	// 四角のオブジェクト
	//--------------------------------------------------------------------------------------
	WallBox::WallBox(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage),
		m_totalTime(0.0)
	{
		m_transParam = param;
	}
	WallBox::~WallBox() {}

	void WallBox::OnCreate()
	{
		//OBB衝突j判定を付ける
		auto ptrColl = AddComponent<CollisionObb>();
		//重力をつける
		auto ptrGra = AddComponent<Gravity>();

		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CUBE");
		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CUBE");
		ptrDraw->AddBaseTexture(L"WALL_TX");
		ptrDraw->SetOwnShadowActive(true);
	}

	void WallBox::OnUpdate(double elapsedTime)
	{
		//Transformコンポーネントを取り出す
		auto ptrTrans = GetComponent<Transform>();
		auto& param = ptrTrans->GetTransParam();

		m_totalTime += elapsedTime;
		if (m_totalTime >= XM_2PI)
		{
			m_totalTime = 0.0;
		}
		param.position.x = (float)sin(m_totalTime) * 2.0f;
	}


	//--------------------------------------------------------------------------------------
	//	追いかける配置オブジェクト
	//--------------------------------------------------------------------------------------
	//構築と破棄
	SeekObject::SeekObject(const std::shared_ptr<Stage>& StagePtr, const Vec3& startPos) :
		GameObject(StagePtr),
		m_StartPos(startPos),
		m_StateChangeSize(5.0f),
		m_Force(0),
		m_Velocity(0)
	{
		m_transParam.position = startPos;
	}
	SeekObject::~SeekObject() {}

	//初期化
	void SeekObject::OnCreate()
	{
		auto ptrTransform = GetComponent<Transform>();
		ptrTransform->SetPosition(m_StartPos);
		ptrTransform->SetScale(0.01f, 0.01f, 0.01f);
		ptrTransform->SetRotation(0.0f, 0.0f, 0.0f);

		//オブジェクトのグループを得る
		auto group = GetStage()->GetSharedObjectGroup(L"SeekGroup");
		group->IntoGroup(GetThis<SeekObject>());
		
		// コリジョン
		auto ptrColl = AddComponent<CollisionCapsule>();
		ptrColl->SetDebugDraw(false);
		const float radius = 0.2f;
		const float segmentHeight = 0.3f;
		ptrColl->SetMakedRadius(radius);
		ptrColl->SetMakedHeight(segmentHeight);

		auto ptrGra = AddComponent<Gravity>();
		//分離行動をつける
		auto PtrSep = GetBehavior<SeparationSteering>();
		PtrSep->SetGameObjectGroup(group);
		
		// 描画
		auto ptrDraw = AddComponent<BcPNTBoneDraw>();
		ptrDraw->SetFogEnabled(true);
		ptrDraw->AddBaseMesh(L"ENEMY_MODEL_SKINNED");
		ptrDraw->AddBaseTexture(L"CHARACTER_TEXTURE_SKINNED");
		const float modelDown = -(segmentHeight * 0.5f + radius);
		ptrDraw->SetModelOffset(Vec3(0.0f, modelDown, 0.0f));

		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"ENEMY_MODEL_SKINNED");
		ptrShadow->SetModelOffset(Vec3(0.0f, modelDown, 0.0f));

		// アニメーション
		ptrDraw->SetAnimationIndex(22);
		//透明処理をする
		SetAlphaActive(false);
		AddTag(L"Enemy");

		// ダメージエフェクト
		auto damageEffect = AddComponent<DamageEffect>();

		// HP設定
		auto hp = AddComponent<Health>();
		hp->SetMaxHP(20);
		hp->SetHP(20);

		hp->m_OnDamaged = [self = GetThis<SeekObject>()](const DamageInfo& info)
		{
			auto effect = self->GetComponent<DamageEffect>();
			if (effect)
			{
				effect->StartEffect(0.5f);
			}
		};

		hp->m_OnDeath = [self = GetThis<SeekObject>()](const DamageInfo& info)
		{
			self->GetStage()->RemoveGameObject(self);
		};

		// ステートマシン
		m_StateMachine.reset(new StateMachine<SeekObject>(GetThis<SeekObject>()));
		m_StateMachine->ChangeState(SeekFarState::Instance());

		m_SteeringUpdateTimer = (double)((reinterpret_cast<std::uintptr_t>(this) & 3)) * 0.0125;
	}


	//操作
	void SeekObject::OnUpdate(double elapsedTime)
	{
		auto ptrDraw = GetComponent<BcPNTBoneDraw>();

		m_totalTime += elapsedTime;

		ptrDraw->UpdateAnimation(m_totalTime);

		m_SteeringUpdateTimer -= elapsedTime;

		// 操舵計算は20Hzだけ
		if (m_SteeringUpdateTimer <= 0.0)
		{
			m_SteeringUpdateTimer += m_SteeringUpdateInterval;

			m_Force = Vec3(0);
			m_StateMachine->Update(); // この中で SetForce / ApplyForce される
		}
		else
		{
			// 前回の force を使って移動だけ継続
			ApplyForce();
		}

		// 向き更新は velocity ベース
		if (bsmUtil::lengthSqr(m_Velocity) > 1e-6f)
		{
			auto ptrUtil = GetBehavior<UtilBehavior>();
			ptrUtil->RotToHead(m_Velocity, 0.35f);
		}

		m_IsGround = false;
	}

	void SeekObject::OnCollisionEnter(const CollisionPair& pair)
	{
		CheckGroundCollision(pair);
	}

	void SeekObject::OnCollisionExecute(const CollisionPair& pair)
	{
		CheckGroundCollision(pair);
	}

	void SeekObject::CheckGroundCollision(const CollisionPair& pair)
	{
		if (pair.m_SrcHitNormal.y > 0.7f)
		{
			m_IsGround = true;

			auto grav = GetComponent<Gravity>(false);
			if (grav)
			{
				auto gravVel = grav->GetGravityVelocity();
				if (gravVel.y < 0.0f)
				{
					grav->SetGravityVelocity(Vec3(gravVel.x, 0.0f, gravVel.z));
				}
			}
		}
	}

	Vec3 SeekObject::GetTargetPos()const
	{
		auto ptrTarget = GetStage()->GetSharedGameObject(L"Player");
		return ptrTarget->GetComponent<Transform>()->GetWorldPosition();
	}


	void SeekObject::ApplyForce()
	{
		float elapsedTime = (float)Scene::GetElapsedTime();
		m_Velocity += m_Force * elapsedTime;
		m_Velocity.y = 0.0f;
		auto ptrTrans = GetComponent<Transform>();
		auto pos = ptrTrans->GetPosition();
		pos += m_Velocity * elapsedTime;
		ptrTrans->SetPosition(pos);
	}



	//--------------------------------------------------------------------------------------
	//	プレイヤーから遠いときの移動
	//--------------------------------------------------------------------------------------
	std::shared_ptr<SeekFarState> SeekFarState::Instance()
	{
		static std::shared_ptr<SeekFarState> instance(new SeekFarState);
		return instance;
	}
	void SeekFarState::Enter(const std::shared_ptr<SeekObject>& Obj)
	{
	}
	void SeekFarState::Execute(const std::shared_ptr<SeekObject>& Obj)
	{
		auto ptrSeek = Obj->GetBehavior<SeekSteering>();
		auto ptrSep = Obj->GetBehavior<SeparationSteering>();
		auto force = Obj->GetForce();
		force = ptrSeek->Execute(force, Obj->GetVelocity(), Obj->GetTargetPos());
		force += ptrSep->Execute(force);
		Obj->SetForce(force);
		Obj->ApplyForce();
		float f = bsm::bsmUtil::length(Obj->GetComponent<Transform>()->GetPosition() - Obj->GetTargetPos());
		if (f < Obj->GetStateChangeSize())
		{
			//Obj->GetStateMachine()->ChangeState(SeekNearState::Instance());
		}
	}

	void SeekFarState::Exit(const std::shared_ptr<SeekObject>& Obj)
	{
	}

	//--------------------------------------------------------------------------------------
	//	プレイヤーから近いときの移動
	//--------------------------------------------------------------------------------------
	std::shared_ptr<SeekNearState> SeekNearState::Instance()
	{
		static std::shared_ptr<SeekNearState> instance(new SeekNearState);
		return instance;
	}
	void SeekNearState::Enter(const std::shared_ptr<SeekObject>& Obj)
	{
	}
	void SeekNearState::Execute(const std::shared_ptr<SeekObject>& Obj)
	{
		auto ptrArrive = Obj->GetBehavior<ArriveSteering>();
		auto ptrSep = Obj->GetBehavior<SeparationSteering>();
		auto force = Obj->GetForce();
		force = ptrArrive->Execute(force, Obj->GetVelocity(), Obj->GetTargetPos());
		force += ptrSep->Execute(force);
		Obj->SetForce(force);
		Obj->ApplyForce();
		float f = bsm::bsmUtil::length(Obj->GetComponent<Transform>()->GetPosition() - Obj->GetTargetPos());
		if (f >= Obj->GetStateChangeSize())
		{
			Obj->GetStateMachine()->ChangeState(SeekFarState::Instance());
		}
	}
	void SeekNearState::Exit(const std::shared_ptr<SeekObject>& Obj)
	{
	}
}
