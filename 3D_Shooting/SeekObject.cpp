/*!
@file Character.cpp
@brief 配置オブジェクト 実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {


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

	void SeekObject::StartDamageFlash(double duration)
	{
		if (duration <= 0.0)
		{
			duration = 0.001;
		}

		m_DamageFlashDuration = duration;
		m_DamageFlashTimer = duration;
	}

	float SeekObject::GetDamageFlashValue() const
	{
		if (m_DamageFlashDuration <= 0.0 || m_DamageFlashTimer <= 0.0)
		{
			return 0.0f;
		}

		double value = m_DamageFlashTimer / m_DamageFlashDuration;
		if (value < 0.0)
		{
			value = 0.0;
		}
		else if (value > 1.0)
		{
			value = 1.0;
		}

		return static_cast<float>(value);
	}

	Vec3 SeekObject::GetDamageNumberPosition()
	{
		auto transform = GetComponent<Transform>(false);
		Vec3 position = transform ? transform->GetPosition() : m_transParam.position;
		position.y += 0.35f;
		return position;
	}

	void SeekObject::ShowDamageNumber(const DamageInfo& info)
	{
		if (info.m_Damage <= 0)
		{
			return;
		}

		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (gameStage)
		{
			gameStage->SpawnDamageNumber(GetDamageNumberPosition(), info.m_Damage);
		}
	}

	//初期化
	void SeekObject::OnCreate()
	{
		auto ptrTransform = GetComponent<Transform>();
		ptrTransform->SetPosition(m_StartPos);
		ptrTransform->SetScale(0.01f, 0.01f, 0.01f);
		ptrTransform->SetRotation(0.0f, 0.0f, 0.0f);
		SetBatchUpdateManaged(true);

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
		
		// 描画は EnemyInstancedRenderer でまとめて行う

		// アニメーション
		auto anim = GetBehavior<AnimationStateBehavior>();
		anim->SetFallbackMeshKey(L"ENEMY_MODEL_SKINNED");
		anim->ChangeAnimation(AnimState::Idle);
		//透明処理をする
		SetAlphaActive(false);
		AddTag(L"Enemy");

		// HP設定
		auto hp = AddComponent<Health>();
		hp->SetMaxHP(20);
		hp->SetHP(20);

		hp->m_OnDamaged = [self = GetThis<SeekObject>()](const DamageInfo& info)
		{
			self->ShowDamageNumber(info);
			self->StartDamageFlash(0.2);
		};

		hp->m_OnDeath = [self = GetThis<SeekObject>()](const DamageInfo& info)
		{
			self->ShowDamageNumber(info);
			self->StartDamageFlash(0.2);
			self->m_IsDead = true;
			self->m_DeathAnimFinished = false;

			auto anim = self->GetBehavior<AnimationStateBehavior>();
			anim->ChangeAnimation(AnimState::Dead);
		};

		// ステートマシン
		m_StateMachine.reset(new StateMachine<SeekObject>(GetThis<SeekObject>()));
		m_StateMachine->ChangeState(SeekFarState::Instance());

		m_SteeringUpdateTimer = (double)((reinterpret_cast<std::uintptr_t>(this) & 3)) * 0.0125;
	}


	void SeekObject::ApplyGravity(double elapsedTime)
	{
		auto grav = GetComponent<Gravity>(false);
		auto transform = GetComponent<Transform>(false);
		if (!grav || !transform)
		{
			return;
		}

		const float dt = static_cast<float>(elapsedTime);
		auto gravityVelocity = grav->GetGravityVelocity();
		gravityVelocity += grav->GetGravity() * dt;
		grav->SetGravityVelocity(gravityVelocity);

		auto position = transform->GetPosition();
		position += gravityVelocity * dt;
		transform->SetPosition(position);
	}

	void SeekObject::RotateToVelocity(float lerpFact)
	{
		if (lerpFact <= 0.0f || bsmUtil::lengthSqr(m_Velocity) <= 1e-6f)
		{
			return;
		}

		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		Vec3 direction = m_Velocity;
		direction.y = 0.0f;
		if (bsmUtil::lengthSqr(direction) <= 1e-6f)
		{
			return;
		}

		direction.normalize();
		const float angle = atan2(direction.x, direction.z);
		Quat target;
		target.rotationRollPitchYawFromVector(Vec3(0.0f, angle, 0.0f));
		target.normalize();

		Quat current = transform->GetQuaternion();
		if (lerpFact >= 1.0f)
		{
			current = target;
		}
		else
		{
			current = XMQuaternionSlerp(current, target, lerpFact);
		}
		transform->SetQuaternion(current);
	}

	void SeekObject::UpdateBatched(double elapsedTime, const Vec3& targetPosition, const Vec3& separationForce)
	{
		if (m_DamageFlashTimer > 0.0)
		{
			m_DamageFlashTimer -= elapsedTime;
			if (m_DamageFlashTimer < 0.0)
			{
				m_DamageFlashTimer = 0.0;
			}
		}

		auto anim = GetBehavior<AnimationStateBehavior>();
		if (m_IsDead)
		{
			if (anim->GetCurrentState() != AnimState::Dead)
			{
				anim->ChangeAnimation(AnimState::Dead);
			}

			ApplyGravity(elapsedTime);
			anim->OnUpdate(elapsedTime);

			if (anim->IsFinished())
			{
				m_DeathAnimFinished = true;
				SetDrawActive(false);
				SetUpdateActive(false);
				RemoveTag(L"Enemy");
				GetStage()->RemoveGameObject(GetThis<SeekObject>());
			}

			return;
		}

		if (!m_IsGround)
		{
			anim->ChangeAnimation(AnimState::Fall);
		}
		else
		{
			anim->ChangeAnimation(AnimState::Sprint);
		}

		m_SteeringUpdateTimer -= elapsedTime;
		if (m_SteeringUpdateTimer <= 0.0)
		{
			m_SteeringUpdateTimer += m_SteeringUpdateInterval;
			if (m_SteeringUpdateTimer < 0.0)
			{
				m_SteeringUpdateTimer = 0.0;
			}

			auto transform = GetComponent<Transform>(false);
			Vec3 position = transform ? transform->GetPosition() : m_StartPos;
			Vec3 toTarget = targetPosition - position;
			toTarget.y = 0.0f;

			Vec3 seekForce(0.0f, 0.0f, 0.0f);
			if (bsmUtil::lengthSqr(toTarget) > 1e-6f)
			{
				toTarget.normalize();
				seekForce = toTarget * 5.0f - m_Velocity;
			}

			Vec3 separation = separationForce;
			separation.y = 0.0f;
			m_Force = Vec3(0.0f, 0.0f, 0.0f);
			Steering::AccumulateForce(m_Force, seekForce, 20.0f);
			Steering::AccumulateForce(m_Force, separation, 20.0f);
		}

		ApplyForce(elapsedTime);
		ApplyGravity(elapsedTime);
		anim->OnUpdate(elapsedTime);
		RotateToVelocity(0.35f);

		m_IsGround = false;
	}

	//操作
	void SeekObject::OnUpdate(double elapsedTime)
	{
		if (IsBatchUpdateManaged())
		{
			return;
		}
		if (m_DamageFlashTimer > 0.0)
		{
			m_DamageFlashTimer -= elapsedTime;
			if (m_DamageFlashTimer < 0.0)
			{
				m_DamageFlashTimer = 0.0;
			}
		}

		auto anim = GetBehavior<AnimationStateBehavior>();
		if (m_IsDead)
		{
			if (anim->GetCurrentState() != AnimState::Dead)
			{
				anim->ChangeAnimation(AnimState::Dead);
			}
			else if (anim->IsFinished())
			{
				m_DeathAnimFinished = true;
				SetDrawActive(false);
				SetUpdateActive(false);
				RemoveTag(L"Enemy");
				GetStage()->RemoveGameObject(GetThis<SeekObject>());
			}

			return;
		}

		if (!m_IsGround)
		{
			anim->ChangeAnimation(AnimState::Fall);
		}
		else
		{
			anim->ChangeAnimation(AnimState::Sprint);
		}

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

		// 地面判定をリセット
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
		// 衝突法線のY成分をチェック（上向きの法線 = 地面との衝突）
		// 0.7f は約45度（cos(45°) ? 0.707）
		// これより大きい = より水平に近い面 = 地面とみなす
		if (pair.m_SrcHitNormal.y > 0.7f)
		{
			m_IsGround = true;

			// 重力速度をリセット（地面に着地）
			auto grav = GetComponent<Gravity>();
			auto gravVel = grav->GetGravityVelocity();

			// 下向きの速度の場合のみリセット（着地時）
			if (gravVel.y < 0.0f)
			{
				grav->SetGravityVelocity(Vec3(gravVel.x, 0.0f, gravVel.z));
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
		ApplyForce(Scene::GetElapsedTime());
	}

	void SeekObject::ApplyForce(double elapsedTime)
	{
		const float dt = static_cast<float>(elapsedTime);
		m_Velocity += m_Force * dt;
		m_Velocity.y = 0.0f;
		auto ptrTrans = GetComponent<Transform>();
		auto pos = ptrTrans->GetPosition();
		pos += m_Velocity * dt;
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
