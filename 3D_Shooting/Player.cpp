#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const Vec3 kSpawnIntroWalkDirection(0.0f, 0.0f, 1.0f);
		const float kSpawnIntroWalkDistance = 2.4f;
		const double kSpawnIntroDuration = 1.05;
		const float kSpawnIntroPortalBackOffset = 0.25f;
		const float kSpawnIntroPortalHeight = 0.85f;
		const float kSpawnIntroPortalScale = 1.15f;
		const float kSpawnIntroCameraDistance = 4.2f;
		const float kSpawnIntroCameraHeight = 1.35f;
		const float kSpawnIntroCameraLookHeight = 1.0f;

		float SmoothStep(float t)
		{
			t = bsmUtil::Clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		class PlayerSpawnPortal : public GameObject
		{
		private:
			float m_Elapsed = 0.0f;
			float m_LifeTime = 1.35f;

		public:
			PlayerSpawnPortal(const std::shared_ptr<Stage>& stage, const TransParam& param)
				: GameObject(stage)
			{
				m_transParam = param;
			}

			void OnCreate() override
			{
				AddTag(L"PlayerSpawnPortal");
				SetAlphaActive(true);
				SetShadowActive(false);

				auto draw = AddComponent<WaveEffectDraw>();
				draw->AddBaseMesh(L"PLAYER_SPAWN_PORTAL_DISC");
				draw->SetColor(Col4(0.015f, 0.018f, 0.03f, 0.72f));
				draw->SetWave(0.355f, 14.0f, 5.8f);
				draw->SetWaveDirection(Vec2(1.0f, 0.45f));
				draw->SetEdgeMask(0.12f, 1.0f);
				draw->SetShakeAxis(Vec3(0.0f, 1.0f, 0.0f));
			}

			void OnUpdate(double elapsedTime) override
			{
				m_Elapsed += static_cast<float>(elapsedTime);
				const float t = m_LifeTime > 0.0f ? bsmUtil::Clamp(m_Elapsed / m_LifeTime, 0.0f, 1.0f) : 1.0f;
				const float fadeIn = bsmUtil::Clamp(t / 0.15f, 0.0f, 1.0f);
				const float fadeOut = 1.0f - bsmUtil::Clamp((t - 0.72f) / 0.28f, 0.0f, 1.0f);
				const float alpha = 0.72f * fadeIn * fadeOut;

				if (auto draw = GetComponent<WaveEffectDraw>(false))
				{
					draw->SetColor(Col4(0.015f, 0.018f, 0.03f, alpha));
					draw->SetWaveTime(m_Elapsed);
				}

				if (m_Elapsed >= m_LifeTime)
				{
					if (auto stage = GetStage(false))
					{
						stage->RemoveGameObject(GetThis<GameObject>());
					}
				}
			}
		};
	}

	Player::Player(const std::shared_ptr<Stage>& stage, const TransParam& param) :
		GameObject(stage),
		m_Speed(6.0f),
		m_IsGround(false)
	{
		m_transParam = param;
	}

	Vec2 Player::GetInputState() const
	{
		Vec2 ret;
		//コントローラの取得
		//auto cntlVec = App::GetInputDevice().GetControlerVec();
		//ret.x = 0.0f;
		//ret.y = 0.0f;
		//WORD wButtons = 0;
		//if (cntlVec[0].bConnected)
		//{
		//	ret.x = cntlVec[0].fThumbLX;
		//	ret.y = cntlVec[0].fThumbLY;
		//}
		//return ret;

		// キーボード入力取得
		auto keyVec = GetInputKey();
		// キーボード入力があれば優先する
		if (keyVec.x != 0.0f || keyVec.y != 0.0f)
		{
			ret = keyVec;
		}
		return ret;
	}

	Vec2 Player::GetInputKey() const
	{
		Vec2 ret(0.0f, 0.0f);

		// キーボード入力取得
		const auto& input = App::GetInputDevice();

		// 左右（X）
		if (input.KeyDown('A') || input.KeyDown(VK_LEFT))  ret.x -= 1.0f;
		if (input.KeyDown('D') || input.KeyDown(VK_RIGHT)) ret.x += 1.0f;

		// 前後（Y）※このクラスでは ret.y を「前(+) / 後(-)」として使う
		if (input.KeyDown('W') || input.KeyDown(VK_UP))    ret.y += 1.0f;
		if (input.KeyDown('S') || input.KeyDown(VK_DOWN))  ret.y -= 1.0f;

		// 斜め移動が速くならないように正規化
		if (ret.length() > 1.0f)
		{
			ret.normalize();
		}
		return ret;
	}

	Vec3 Player::GetMoveVector() const
	{
		Vec3 angle(0, 0, 0);
		//入力の取得
		auto inPut = GetInputState();
		float moveX = inPut.x;
		float moveZ = inPut.y;
		if (moveX != 0 || moveZ != 0)
		{
			float moveLength = 0;	//動いた時のスピード
			auto ptrTransform = GetComponent<Transform>();
			auto ptrCamera = GetStage()->GetCamera();
			//進行方向の向きを計算
			auto front = ptrTransform->GetPosition() - ptrCamera->GetEye();
			front.y = 0;
			front.normalize();
			//進行方向向きからの角度を算出
			float frontAngle = atan2(front.z, front.x);
			//向き計算
			Vec2 moveVec(moveX, moveZ);
			float moveSize = moveVec.length();
			//向きから角度を計算
			float cntlAngle = atan2(-moveX, moveZ);
			//トータルの角度を算出
			float totalAngle = frontAngle + cntlAngle;
			//角度からベクトルを作成
			angle = Vec3(cos(totalAngle), 0, sin(totalAngle));
			//正規化する
			angle.normalize();
			//移動サイズを設定。
			angle *= moveSize;
			//Y軸は変化させない
			angle.y = 0;
		}
		return angle;
	}

	void Player::MovePlayer(float elapsedTime)
	{
		auto angle = GetMoveVector();
		if (angle.length() > 0.0f)
		{
			auto pos = GetComponent<Transform>()->GetPosition();
			pos += angle * elapsedTime * m_Speed;
			GetComponent<Transform>()->SetPosition(pos);
		}
		//回転の計算
		if (angle.length() > 0.0f)
		{
			auto anim = GetBehavior<AnimationStateBehavior>();

			if (!anim->IsPlayingAttack())
			{
				auto utilPtr = GetBehavior<UtilBehavior>();
				utilPtr->RotToHead(angle, 1.0f);
			}
		}
	}

	void Player::OnCreate()
	{
		GetStage()->SetSharedGameObject(L"Player", GetThis<Player>());

		auto ptrTransform = GetComponent<Transform>();
		//ptrTransform->SetPosition(m_StartPos);
		ptrTransform->SetScale(0.01f, 0.01f, 0.01f);
		ptrTransform->SetRotation(0.0f, 0.0f, 0.0f);

		// コリジョン
		auto ptrColl = AddComponent<CollisionCapsule>();
		ptrColl->SetDebugDraw(false);
		const float radius = 0.2f;
		const float segmentHeight = 0.3f;
		ptrColl->SetMakedRadius(radius);
		ptrColl->SetMakedHeight(segmentHeight);
		//重力をつける
		AddComponent<Gravity>();

		// 描画
		auto ptrDraw = AddComponent<BcPNTBoneDraw>();
		ptrDraw->SetFogEnabled(true);
		ptrDraw->AddBaseMesh(L"PLAYER_MODEL_SKINNED");
		ptrDraw->AddBaseTexture(L"CHARACTER_TEXTURE_SKINNED");
		const float modelDown = -(segmentHeight * 0.5f + radius);
		ptrDraw->SetModelOffset(Vec3(0.0f, modelDown, 0.0f));

		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"PLAYER_MODEL_SKINNED");
		ptrShadow->SetModelOffset(Vec3(0.0f, modelDown, 0.0f));

		// アニメーション
		auto anim = GetBehavior<AnimationStateBehavior>();
		anim->ChangeAnimation(AnimState::Idle);
		//透明処理
		SetAlphaActive(false);
		//カメラを得る
		m_MainCamera = std::dynamic_pointer_cast<MainCamera>(GetStage()->GetCamera());
		m_CollisionManager = GetStage()->GetCollisionManager();
		m_BulletManager = GetStage()->GetSharedGameObjectEx<BulletManager>(L"BulletManager", false);

		if (m_MainCamera)
		{
			//MainCameraである
			//MainCameraに注目するオブジェクト（プレイヤー）の設定
			m_MainCamera->SetTargetObject(GetThis<GameObject>());
			m_MainCamera->SetTargetToAt(Vec3(0, 1.0f, 0));
		}

		AddTag(L"Player");

		auto hp = AddComponent<Health>();
		hp->SetMaxHP(20);
		hp->SetHP(20);

		hp->m_OnDamaged = [self = GetThis<Player>()](const DamageInfo& info)
		{
			// ダメージエフェクトを開始
			auto effect = self->GetComponent<DamageEffect>();
			if (effect)
			{
				effect->StartEffect();
			}
		};

		hp->m_OnDeath = [self = GetThis<Player>()](const DamageInfo& info)
		{
			self->m_IsDead = true;
			self->m_DeathAnimFinished = false;

			auto anim = self->GetBehavior<AnimationStateBehavior>();
			anim->ChangeAnimation(AnimState::Dead);
		};

		// ダメージエフェクト
		auto damageEffect = AddComponent<DamageEffect>();

		m_BombPreview = AddComponent<BombAimPreview>();
		m_BombPreview->SetTuning(GetBombTuning());
		m_BombPreview->SetMaxRange(20.0f); // 最大到達距離を設定

		BeginSpawnIntro();
	}

	void Player::BeginSpawnIntro()
	{
		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		m_SpawnIntroActive = true;
		m_SpawnIntroTimer = 0.0;
		m_SpawnIntroEndPosition = transform->GetPosition();
		m_SpawnIntroStartPosition = m_SpawnIntroEndPosition - (kSpawnIntroWalkDirection * kSpawnIntroWalkDistance);
		m_SpawnIntroStartPosition.y = m_SpawnIntroEndPosition.y;
		transform->SetPosition(m_SpawnIntroStartPosition);

		if (auto util = GetBehavior<UtilBehavior>())
		{
			util->RotToHead(kSpawnIntroWalkDirection, 1.0f);
		}
		UpdateSpawnIntroCamera(m_SpawnIntroStartPosition);

		TransParam portalParam;
		portalParam.position = m_SpawnIntroStartPosition - (kSpawnIntroWalkDirection * kSpawnIntroPortalBackOffset)
			+ Vec3(0.0f, kSpawnIntroPortalHeight, 0.0f);
		portalParam.scale = Vec3(kSpawnIntroPortalScale, kSpawnIntroPortalScale, kSpawnIntroPortalScale);
		portalParam.quaternion.rotationRollPitchYawFromVector(Vec3(XM_PIDIV2, 0.0f, 0.0f));
		GetStage()->AddGameObject<PlayerSpawnPortal>(portalParam);

		// 演出中は歩いて出てくる位置を手動で決めるため、通常の重力更新を止める。
		if (auto gravity = GetComponent<Gravity>(false))
		{
			gravity->SetGravityVelocityZero();
			gravity->SetUpdateActive(false);
		}
	}

	bool Player::UpdateSpawnIntro(double elapsedTime)
	{
		if (!m_SpawnIntroActive)
		{
			return false;
		}

		m_SpawnIntroTimer += elapsedTime;
		const float rawT = static_cast<float>(m_SpawnIntroTimer / kSpawnIntroDuration);
		const float t = SmoothStep(rawT);

		auto transform = GetComponent<Transform>(false);
		if (transform)
		{
			const Vec3 position = m_SpawnIntroStartPosition + (m_SpawnIntroEndPosition - m_SpawnIntroStartPosition) * t;
			transform->SetPosition(position);
			UpdateSpawnIntroCamera(position);
		}

		if (auto util = GetBehavior<UtilBehavior>())
		{
			util->RotToHead(kSpawnIntroWalkDirection, 1.0f);
		}

		if (rawT < 1.0f)
		{
			return true;
		}

		m_SpawnIntroActive = false;
		m_IsGround = true;

		if (transform)
		{
			transform->SetPosition(m_SpawnIntroEndPosition);
		}

		if (auto gravity = GetComponent<Gravity>(false))
		{
			gravity->SetGravityVelocityZero();
			gravity->SetUpdateActive(true);
		}

		if (auto anim = GetBehavior<AnimationStateBehavior>())
		{
			anim->ChangeAnimation(AnimState::Idle);
		}

		if (m_MainCamera)
		{
			// 登場カメラの位置・角度を通常追従カメラへ引き継いでから解除する。
			// 単純に SetSpawnIntroView(false) だけ行うと、通常カメラが持っていた古い角度に戻ってしまう。
			m_MainCamera->FinishSpawnIntroViewAndResumeFollow();
		}

		return false;
	}

	void Player::UpdateSpawnIntroCamera(const Vec3& playerPosition)
	{
		if (!m_MainCamera)
		{
			return;
		}

		// プレイヤーは+Z方向へ歩くので、カメラも+Z側に置くとキャラの正面が見える。
		const Vec3 at = playerPosition + Vec3(0.0f, kSpawnIntroCameraLookHeight, 0.0f);
		const Vec3 eye = playerPosition
			+ (kSpawnIntroWalkDirection * kSpawnIntroCameraDistance)
			+ Vec3(0.0f, kSpawnIntroCameraHeight, 0.0f);
		m_MainCamera->SetSpawnIntroView(true, eye, at);
	}

	void Player::OnPushA()
	{
		if (m_IsGround)
		{
			auto grav = GetComponent<Gravity>();
			grav->StartJump(Vec3(0, 4.0f, 0));
			m_IsGround = false;
		}
	}

	void Player::ResolveSlopeCollision(double elapsedTime)
	{
		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (!gameStage)
		{
			return;
		}

		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		auto capsule = GetComponent<CollisionCapsule>(false);
		StageGroundResolveState groundState;
		groundState.position = transform->GetPosition();
		groundState.previousPosition = transform->GetBeforePosition();
		groundState.footOffset = capsule
			? capsule->GetMakedRadius() + (capsule->GetMakedHeight() * 0.5f)
			: 0.35f;
		groundState.wasGrounded = m_IsGround;
		groundState.elapsedTime = static_cast<float>(elapsedTime);

		auto gravity = GetComponent<Gravity>(false);
		if (gravity)
		{
			groundState.gravityVelocity = gravity->GetGravityVelocity();
		}

		bool terrainBlockedX = false;
		bool terrainBlockedZ = false;
		// 空中で坂や高台の内部へ入り込む前に、移動軸を戻してすり抜けを防ぐ。
		TrySlideAgainstGeneratedTerrainStep(*gameStage, groundState, 0.75f, terrainBlockedX, terrainBlockedZ);

		if (!TryResolveStageGround(*gameStage, groundState))
		{
			const float baseFloorHalf = 32.5f;
			const bool insideBaseFloor = fabsf(groundState.position.x) <= baseFloorHalf &&
				fabsf(groundState.position.z) <= baseFloorHalf;
			if (!insideBaseFloor || !TryResolveGroundHeight(0.0f, groundState))
			{
				return;
			}
		}

		transform->SetPosition(groundState.position);
		m_IsGround = groundState.isGrounded;

		if (gravity)
		{
			gravity->SetGravityVelocity(groundState.gravityVelocity);
		}
	}

	void Player::OnUpdate2(double elapsedTime)
	{
		if (m_SpawnIntroActive)
		{
			return;
		}

		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
		{
			elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);
		}
		ResolveSlopeCollision(elapsedTime);
	}

	void Player::OnCollisionEnter(const CollisionPair& pair)
	{
		CheckGroundCollision(pair);
		CheckItemPickup(pair);

		// 敵との衝突をチェック
		auto other = pair.m_Dest.lock();
		if (!other) return;

		auto otherObj = other->GetGameObject();
		if (!otherObj) return;

		// 敵タグを持つオブジェクトとの衝突か確認
		if (otherObj->FindTag(L"Enemy"))
		{
			// Healthコンポーネントを取得してダメージを適用
			auto hp = GetComponent<Health>();
			if (hp && !hp->IsDead())
			{
				// ダメージ情報を作成（敵との接触は1ダメージ）
				DamageInfo damageInfo;
				damageInfo.m_Damage = 1;
				//damageInfo.m_Attacker = otherObj;
					
				// ダメージを適用
				hp->ApplyDamage(damageInfo);
			}
		}
	}

	void Player::OnCollisionExecute(const CollisionPair& pair)
	{
		// 継続的な衝突でも地面判定を更新
		CheckGroundCollision(pair);
		CheckItemPickup(pair);
	}

	void Player::CheckItemPickup(const CollisionPair& pair)
	{
		auto otherCollision = pair.m_Dest.lock();
		if (!otherCollision)
		{
			return;
		}

		auto otherObject = otherCollision->GetGameObject();
		auto item = std::dynamic_pointer_cast<BaseItem>(otherObject);
		if (!item)
		{
			return;
		}

		item->TryPickupBy(GetThis<GameObject>());
	}

	void Player::CheckGroundCollision(const CollisionPair& pair)
	{
		auto gravity = GetComponent<Gravity>(false);
		Vec3 gravityVelocity = gravity ? gravity->GetGravityVelocity() : Vec3(0.0f, 0.0f, 0.0f);
		bool isGrounded = m_IsGround;
		if (!TryApplyGroundCollision(pair, gravityVelocity, isGrounded))
		{
			return;
		}

		m_IsGround = isGrounded;
		if (gravity)
		{
			gravity->SetGravityVelocity(gravityVelocity);
		}
	}
}




