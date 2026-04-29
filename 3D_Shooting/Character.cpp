/*!
@file Character.cpp
@brief 配置オブジェクト 実体
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	SkyDome::SkyDome(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	SkyDome::~SkyDome() {}

	void SkyDome::OnCreate()
	{
		SetShadowActive(false);
		SetAlphaActive(false);

		auto ptrDraw = AddComponent<SkyDomeDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_SPHERE");
		ptrDraw->AddBaseTexture(L"SKY_TX");
		ptrDraw->SetRadius(450.0f);

		AddTag(L"Sky");
	}

	//--------------------------------------------------------------------------------------
	// フロアオブジェクト
	//--------------------------------------------------------------------------------------
	Floor::Floor(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix)
	{
		m_transParam = param;
	}
	Floor::~Floor() {}

	void Floor::OnCreate()
	{
		AddTag(L"Floor");

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();

		const auto& meshes = BaseScene::Get()->GetModelMesh(m_MeshKey);
		ptrDraw->AddBaseModelMesh(meshes);

		for (size_t i = 0; i < meshes.size(); ++i)
		{
			ptrDraw->AddBaseMaterial(
				m_MaterialPrefix + std::to_wstring(i)
			);
		}

		ptrDraw->SetOwnShadowActive(false);
	}

	FloorInstancedRenderer::FloorInstancedRenderer(
		const std::shared_ptr<Stage>& stage,
		const std::wstring& meshKey,
		const std::wstring& materialPrefix,
		const std::vector<Mat4x4>& instanceWorlds) :
		GameObject(stage),
		m_MeshKey(meshKey),
		m_MaterialPrefix(materialPrefix),
		m_InstanceWorlds(instanceWorlds)
	{
	}

	FloorInstancedRenderer::~FloorInstancedRenderer() {}

	void FloorInstancedRenderer::OnCreate()
	{
		auto ptrDraw = AddComponent<InstancedStaticDraw>();

		ptrDraw->SetMeshKey(m_MeshKey);
		ptrDraw->SetMaterialPrefix(m_MaterialPrefix);
		ptrDraw->SetInstanceWorlds(m_InstanceWorlds);
		ptrDraw->SetBaseColorOverride(Col4(0.627f, 0.659f, 0.788f, 1.0f));
		ptrDraw->SetUseMaterialTexture(false);
		ptrDraw->SetLightingEnabled(true);
		ptrDraw->SetOwnShadowActive(false);
		ptrDraw->BuildInstanceBuffer();

		AddTag(L"Floor");
	}

	EnemyInstancedRenderer::EnemyInstancedRenderer(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyInstancedRenderer::~EnemyInstancedRenderer() {}

	void EnemyInstancedRenderer::OnCreate()
	{
		m_Draw = AddComponent<InstancedSkinnedDraw>();
		m_Draw->SetMeshKey(L"ENEMY_MODEL_SKINNED");
		m_Draw->SetTextureKey(L"CHARACTER_TEXTURE_SKINNED");
		m_Draw->SetOwnShadowActive(false);

		AddTag(L"EnemyRenderer");
	}

	void EnemyInstancedRenderer::OnUpdate2(double elapsedTime)
	{
		if (!m_Draw)
		{
			return;
		}

		std::vector<std::shared_ptr<GameObject>> enemies;
		GetStage()->GetUsedTagObjectVec(L"Enemy", enemies);

		m_InstanceSources.clear();
		m_InstanceSources.reserve(enemies.size());

		for (const auto& enemy : enemies)
		{
			if (!enemy || !enemy->IsDrawActive())
			{
				continue;
			}

			auto transform = enemy->GetComponent<Transform>(false);
			if (!transform)
			{
				continue;
			}

			auto& param = transform->GetTransParam();
			Mat4x4 world;
			world.affineTransformation(
				param.scale,
				param.rotateOrigin,
				param.quaternion,
				param.position + m_ModelOffset);

			SkinnedInstanceSource source{};
			source.world = world;

			auto anim = enemy->GetBehavior<AnimationStateBehavior>();
			if (anim)
			{
				source.animationIndex = static_cast<unsigned int>(anim->GetCurrentState());
				source.animationTime = static_cast<float>(anim->GetPlaybackTimeSeconds());
			}

			auto seek = std::dynamic_pointer_cast<SeekObject>(enemy);
			if (seek)
			{
				source.damage = seek->GetDamageFlashValue();
			}

			m_InstanceSources.push_back(source);
		}

		m_Draw->SetInstances(m_InstanceSources);
		m_Draw->BuildInstanceBuffer();
	}

	FloorCollision::FloorCollision(
		const std::shared_ptr<Stage>& stage,
		const TransParam& param,
		const Vec3& collisionSize) :
		GameObject(stage),
		m_CollisionSize(collisionSize)
	{
		m_transParam = param;
	}

	FloorCollision::~FloorCollision() {}

	void FloorCollision::OnCreate()
	{
		auto ptrColl = AddComponent<CollisionObb>();
		ptrColl->SetDebugDraw(false);
		ptrColl->SetMakedSize(
			m_CollisionSize.x,
			m_CollisionSize.y,
			m_CollisionSize.z);
		ptrColl->SetFixed(true);

		AddTag(L"Floor");
	}

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
			self->StartDamageFlash(0.2);
		};

		hp->m_OnDeath = [self = GetThis<SeekObject>()](const DamageInfo& info)
		{
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


	//操作
	void SeekObject::OnUpdate(double elapsedTime)
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
		// 0.7f は約45度（cos(45°) ≈ 0.707）
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
