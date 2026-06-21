#include "stdafx.h"
#include "Project.h"

namespace shooting {


	SeekObject::SeekObject(const std::shared_ptr<Stage>& StagePtr, const Vec3& startPos) :
		GameObject(StagePtr),
		m_startPos(startPos),
		m_stateChangeSize(5.0f),
		m_force(0),
		m_velocity(0)
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

		m_damageFlashDuration = duration;
		m_damageFlashTimer = duration;
	}

	float SeekObject::GetDamageFlashValue() const
	{
		if (m_damageFlashDuration <= 0.0 || m_damageFlashTimer <= 0.0)
		{
			return 0.0f;
		}

		double value = m_damageFlashTimer / m_damageFlashDuration;
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

	void SeekObject::OnCreate()
	{
		auto ptrTransform = GetComponent<Transform>();
		ptrTransform->SetPosition(m_startPos);
		ptrTransform->SetScale(0.01f, 0.01f, 0.01f);
		ptrTransform->SetRotation(0.0f, 0.0f, 0.0f);
		SetBatchUpdateManaged(true);

		auto group = GetStage()->GetSharedObjectGroup(L"SeekGroup");
		group->IntoGroup(GetThis<SeekObject>());
		
		auto ptrColl = AddComponent<CollisionCapsule>();
		ptrColl->SetDebugDraw(false);
		const float radius = 0.2f;
		const float segmentHeight = 0.3f;
		ptrColl->SetMakedRadius(radius);
		ptrColl->SetMakedHeight(segmentHeight);

		auto ptrGra = AddComponent<Gravity>();
		auto separationSteering = GetBehavior<SeparationSteering>();
		separationSteering->SetGameObjectGroup(group);
		
		// 描画は EnemyInstancedRenderer でまとめて行う

		auto anim = GetBehavior<AnimationStateBehavior>();
		anim->SetFallbackMeshKey(L"ENEMY_MODEL_SKINNED");
		anim->ChangeAnimation(AnimState::Idle);
		SetAlphaActive(false);
		AddTag(L"Enemy");

		auto hp = AddComponent<Health>();
		hp->SetMaxHP(20);
		hp->SetHP(20);

		// Healthから所有者を強参照すると自己循環になるため、イベント側は弱参照だけを保持する。
		std::weak_ptr<SeekObject> weakSelf = GetThis<SeekObject>();
		hp->m_OnDamaged = [weakSelf](const DamageInfo& info)
		{
			auto self = weakSelf.lock();
			if (!self)
			{
				return;
			}

			self->ShowDamageNumber(info);
			self->StartDamageFlash(0.2);
		};

		hp->m_OnDeath = [weakSelf](const DamageInfo& info)
		{
			auto self = weakSelf.lock();
			if (!self)
			{
				return;
			}

			self->ShowDamageNumber(info);
			self->StartDamageFlash(0.2);
			self->m_isDead = true;
			self->m_deathAnimFinished = false;

			auto anim = self->GetBehavior<AnimationStateBehavior>();
			anim->ChangeAnimation(AnimState::Dead);
		};

		m_stateMachine.reset(new StateMachine<SeekObject>(GetThis<SeekObject>()));
		m_stateMachine->ChangeState(SeekFarState::Instance());

		m_steeringUpdateTimer = (double)((reinterpret_cast<std::uintptr_t>(this) & 3)) * 0.0125;
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
		if (lerpFact <= 0.0f || bsmUtil::lengthSqr(m_velocity) <= 1e-6f)
		{
			return;
		}

		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		Vec3 direction = m_velocity;
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
		if (m_damageFlashTimer > 0.0)
		{
			m_damageFlashTimer -= elapsedTime;
			if (m_damageFlashTimer < 0.0)
			{
				m_damageFlashTimer = 0.0;
			}
		}

		auto anim = GetBehavior<AnimationStateBehavior>();
		if (m_isDead)
		{
			if (anim->GetCurrentState() != AnimState::Dead)
			{
				anim->ChangeAnimation(AnimState::Dead);
			}

			ApplyGravity(elapsedTime);
			anim->OnUpdate(elapsedTime);

			if (anim->IsFinished())
			{
				m_deathAnimFinished = true;
				SetDrawActive(false);
				SetUpdateActive(false);
				RemoveTag(L"Enemy");
				GetStage()->RemoveGameObject(GetThis<SeekObject>());
			}

			return;
		}

		if (!m_isGround)
		{
			anim->ChangeAnimation(AnimState::Fall);
		}
		else
		{
			anim->ChangeAnimation(AnimState::Sprint);
		}

		m_steeringUpdateTimer -= elapsedTime;
		if (m_steeringUpdateTimer <= 0.0)
		{
			m_steeringUpdateTimer += m_steeringUpdateInterval;
			if (m_steeringUpdateTimer < 0.0)
			{
				m_steeringUpdateTimer = 0.0;
			}

			auto transform = GetComponent<Transform>(false);
			Vec3 position = transform ? transform->GetPosition() : m_startPos;
			Vec3 toTarget = targetPosition - position;
			toTarget.y = 0.0f;

			Vec3 seekForce(0.0f, 0.0f, 0.0f);
			if (bsmUtil::lengthSqr(toTarget) > 1e-6f)
			{
				toTarget.normalize();
				seekForce = toTarget * 5.0f - m_velocity;
			}

			Vec3 separation = separationForce;
			separation.y = 0.0f;
			m_force = Vec3(0.0f, 0.0f, 0.0f);
			Steering::AccumulateForce(m_force, seekForce, 20.0f);
			Steering::AccumulateForce(m_force, separation, 20.0f);
		}

		ApplyForce(elapsedTime);
		ApplyGravity(elapsedTime);
		anim->OnUpdate(elapsedTime);
		RotateToVelocity(0.35f);

		m_isGround = false;
	}

	void SeekObject::OnUpdate(double elapsedTime)
	{
		if (IsBatchUpdateManaged())
		{
			return;
		}
		if (m_damageFlashTimer > 0.0)
		{
			m_damageFlashTimer -= elapsedTime;
			if (m_damageFlashTimer < 0.0)
			{
				m_damageFlashTimer = 0.0;
			}
		}

		auto anim = GetBehavior<AnimationStateBehavior>();
		if (m_isDead)
		{
			if (anim->GetCurrentState() != AnimState::Dead)
			{
				anim->ChangeAnimation(AnimState::Dead);
			}
			else if (anim->IsFinished())
			{
				m_deathAnimFinished = true;
				SetDrawActive(false);
				SetUpdateActive(false);
				RemoveTag(L"Enemy");
				GetStage()->RemoveGameObject(GetThis<SeekObject>());
			}

			return;
		}

		if (!m_isGround)
		{
			anim->ChangeAnimation(AnimState::Fall);
		}
		else
		{
			anim->ChangeAnimation(AnimState::Sprint);
		}

		m_steeringUpdateTimer -= elapsedTime;

		// 操舵計算は20Hzだけ
		if (m_steeringUpdateTimer <= 0.0)
		{
			m_steeringUpdateTimer += m_steeringUpdateInterval;

			m_force = Vec3(0);
			m_stateMachine->Update();
		}
		else
		{
			ApplyForce();
		}

		if (bsmUtil::lengthSqr(m_velocity) > 1e-6f)
		{
			auto ptrUtil = GetBehavior<UtilBehavior>();
			ptrUtil->RotToHead(m_velocity, 0.35f);
		}

		m_isGround = false;
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
			m_isGround = true;

			auto grav = GetComponent<Gravity>();
			auto gravVel = grav->GetGravityVelocity();

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
		m_velocity += m_force * dt;
		m_velocity.y = 0.0f;
		auto ptrTrans = GetComponent<Transform>();
		auto pos = ptrTrans->GetPosition();
		pos += m_velocity * dt;
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
