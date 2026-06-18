#include "stdafx.h"
#include "Project.h"

namespace shooting {

	namespace
	{
		const Vec3 kSpawnIntroWalkDirection(0.0f, 0.0f, 1.0f);
		const float kSpawnIntroWalkDistance = 2.4f;
		const double kSpawnIntroPortalOnlyDuration = 1.0;
		const double kSpawnIntroDuration = 1.05;
		const float kSpawnIntroPortalBackOffset = 0.25f;
		const float kSpawnIntroPortalHeight = 0.85f;
		const float kSpawnIntroPortalScale = 1.15f;
		const float kSpawnIntroCameraDistance = 4.2f;
		const float kSpawnIntroCameraHeight = 1.35f;
		const float kSpawnIntroCameraLookHeight = 1.0f;
		// 死亡した瞬間にゲーム内時間をほぼ停止させる時間。
		const double kDeathHitStopDuration = 0.18;
		// 完全停止を避けつつ、死亡した瞬間を強調するための時間倍率。
		const double kDeathHitStopTimeScale = 0.03;
		// 死亡判定から死亡SEを鳴らすまでの実時間。
		const double kDeathSoundDelay = 1.1;
		// 死亡モーションを通常より遅く再生し、ゲームオーバーになったことを強調する。
		const double kDeathAnimationTimeScale = 0.25;

		float SmoothStep(float t)
		{
			t = bsmUtil::Clamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		class PlayerSpawnPortal : public GameObject
		{
		private:
			float m_elapsed = 0.0f;
			float m_lifeTime = static_cast<float>(kSpawnIntroPortalOnlyDuration + kSpawnIntroDuration);

		public:
			PlayerSpawnPortal(const std::shared_ptr<Stage>& stage, const TransParam& param)
				: GameObject(stage)
			{
				m_transParam = param;
			}

			void OnCreate() override
			{
				AddTag(L"PlayerSpawnPortal");
				SetAlphaActive(false);
				SetShadowActive(false);

				auto draw = AddComponent<WaveEffectDraw>();
				draw->AddBaseMesh(L"PLAYER_SPAWN_PORTAL_DISC");
				draw->SetColor(Col4(0.015f, 0.018f, 0.03f, 1.0f));
				draw->SetWave(0.355f, 14.0f, 5.8f);
				draw->SetWaveDirection(Vec2(1.0f, 0.45f));
				draw->SetEdgeMask(0.12f, 1.0f);
				draw->SetShakeAxis(Vec3(0.0f, 1.0f, 0.0f));
			}

			void OnUpdate(double elapsedTime) override
			{
				m_elapsed += static_cast<float>(elapsedTime);

				if (auto draw = GetComponent<WaveEffectDraw>(false))
				{
					draw->SetWaveTime(m_elapsed);
				}

				if (m_elapsed >= m_lifeTime)
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
		m_speed(6.0f),
		m_isGround(false)
	{
		m_transParam = param;
	}

	void Player::StartDeathPresentation()
	{
		// ヒットストップ直後に聞こえるよう、死亡SEは実時間タイマーで遅延再生する。
		m_deathSoundPending = true;
		m_deathSoundDelayTimer = kDeathSoundDelay;
		// 死亡SEは残し、インゲームBGMだけを停止してゲームオーバーを明確にする。
		GameAudio::Instance().StopBgm();

		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		if (gameStage)
		{
			gameStage->RequestHitStop(kDeathHitStopDuration, kDeathHitStopTimeScale);
		}
	}

	void Player::UpdateDeathSound(double rawElapsedTime)
	{
		if (!m_deathSoundPending)
		{
			return;
		}

		m_deathSoundDelayTimer -= rawElapsedTime;
		if (m_deathSoundDelayTimer <= 0.0)
		{
			m_deathSoundPending = false;
			m_deathSoundDelayTimer = 0.0;
			GameAudio::Instance().PlaySound(GameSoundId::PlayerDead);
		}
	}

	void Player::UpdateAnimationPlaybackRate(
		double rawElapsedTime,
		double gameElapsedTime)
	{
		auto anim = GetBehavior<AnimationStateBehavior>();
		if (!anim)
		{
			return;
		}

		// Behaviorは生の経過時間で更新されるため、ゲーム内時間との比率を渡して
		// ヒットストップ中のアニメーションにも同じ時間倍率を適用する。
		double playbackTimeScale = rawElapsedTime > 1e-8
			? gameElapsedTime / rawElapsedTime
			: 1.0;
		if (m_isDead)
		{
			playbackTimeScale *= kDeathAnimationTimeScale;
		}
		anim->SetPlaybackTimeScale(playbackTimeScale);
	}

	bool Player::UpdateDeathState()
	{
		auto anim = GetBehavior<AnimationStateBehavior>();
		if (m_isDead)
		{
			if (!anim)
			{
				m_deathAnimFinished = true;
				return true;
			}

			if (!anim->IsFinished())
			{
				anim->ChangeAnimation(AnimState::Dead);
			}
			else
			{
				m_deathAnimFinished = true;
			}
			return true;
		}

		auto transform = GetComponent<Transform>(false);
		if (!transform || transform->GetPosition().y >= kFallDeathY)
		{
			return false;
		}

		if (auto health = GetComponent<Health>(false))
		{
			health->SetHP(0);
		}
		m_isDead = true;
		m_deathAnimFinished = false;
		StartDeathPresentation();
		if (anim)
		{
			anim->ChangeAnimation(AnimState::Dead, true);
		}
		return true;
	}

	bool Player::UpdateSpawnIntroState(double elapsedTime)
	{
		if (!m_spawnIntroActive)
		{
			return false;
		}

		if (IsSpawnIntroCharacterVisible())
		{
			if (auto anim = GetBehavior<AnimationStateBehavior>())
			{
				anim->ChangeAnimation(AnimState::Sprint);
			}
		}
		UpdateSpawnIntro(elapsedTime);

		// 登場演出中は戦闘処理を止めるため、前フレームの爆弾プレビューも明示的に消す。
		if (m_bombPreview)
		{
			m_bombPreview->SetPreviewInput(
				false,
				Vec3(0.0f, 0.0f, 0.0f),
				Vec3(0.0f, 0.0f, 0.0f),
				Vec3(0.0f, 1.0f, 0.0f),
				false);
		}
		return true;
	}

	void Player::UpdateMovementState(double elapsedTime, bool hitStopActive)
	{
		auto anim = GetBehavior<AnimationStateBehavior>();
		if (anim && (!anim->IsPlayingAttack() || anim->IsFinished()))
		{
			if (!m_isGround)
			{
				anim->ChangeAnimation(AnimState::Jump);
			}
			else if (GetMoveVector().length() > 0.0f)
			{
				anim->ChangeAnimation(AnimState::Sprint);
			}
			else
			{
				anim->ChangeAnimation(AnimState::Idle);
			}
		}

		m_inputHandler.PushHandle(GetThis<Player>());
		MovePlayer(static_cast<float>(elapsedTime));
		ResolveSlopeCollision(elapsedTime);

		if (!hitStopActive && App::GetInputDevice().KeyDown(VK_SPACE))
		{
			OnPushA();
		}

		// 次フレームは衝突処理で接地を再確認する。
		m_isGround = false;
	}

	void Player::OnUpdate(double elapsedTime)
	{
		const double rawElapsedTime = elapsedTime;
		UpdateDeathSound(rawElapsedTime);

		bool hitStopActive = false;
		if (auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false)))
		{
			hitStopActive = gameStage->IsHitStopActive();
			elapsedTime = gameStage->GetGameDeltaTime(elapsedTime);
		}

		UpdateAnimationPlaybackRate(rawElapsedTime, elapsedTime);
		if (UpdateDeathState() || UpdateSpawnIntroState(elapsedTime))
		{
			return;
		}

		UpdateMovementState(elapsedTime, hitStopActive);
		UpdateCombat(elapsedTime, hitStopActive);
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

		// 通常操作はWASD
		if (input.KeyDown('A')) ret.x -= 1.0f;
		if (input.KeyDown('D')) ret.x += 1.0f;

		// 前後（Y）※このクラスでは ret.y を「前(+) / 後(-)」として使う
		if (input.KeyDown('W')) ret.y += 1.0f;
		if (input.KeyDown('S')) ret.y -= 1.0f;

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
			pos += angle * elapsedTime * m_speed;
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
		//ptrTransform->SetPosition(m_startPos);
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
		m_mainCamera = std::dynamic_pointer_cast<MainCamera>(GetStage()->GetCamera());
		m_collisionManager = GetStage()->GetCollisionManager();

		if (m_mainCamera)
		{
			//MainCameraである
			//MainCameraに注目するオブジェクト（プレイヤー）の設定
			m_mainCamera->SetTargetObject(GetThis<GameObject>());
			m_mainCamera->SetTargetToAt(Vec3(0, 1.0f, 0));
		}

		AddTag(L"Player");

		auto hp = AddComponent<Health>();
		hp->SetMaxHP(20);
		hp->SetHP(20);

		// HealthはPlayerが所有するため、コールバックからPlayerを強参照すると自己循環になる。
		std::weak_ptr<Player> weakSelf = GetThis<Player>();
		hp->m_OnDamaged = [weakSelf](const DamageInfo& info)
		{
			auto self = weakSelf.lock();
			if (!self)
			{
				return;
			}

			GameAudio::Instance().PlaySound(GameSoundId::PlayerDamage);

			// ダメージエフェクトを開始
			auto effect = self->GetComponent<DamageEffect>();
			if (effect)
			{
				effect->StartEffect();
			}
		};

		hp->m_OnDeath = [weakSelf](const DamageInfo& info)
		{
			auto self = weakSelf.lock();
			if (!self)
			{
				return;
			}

			self->m_isDead = true;
			self->m_deathAnimFinished = false;
			self->StartDeathPresentation();

			auto anim = self->GetBehavior<AnimationStateBehavior>();
			anim->ChangeAnimation(AnimState::Dead);
		};

		// ダメージエフェクト
		auto damageEffect = AddComponent<DamageEffect>();

		m_bombPreview = AddComponent<BombAimPreview>();
		m_bombPreview->SetTuning(GetBombTuning());
		m_bombPreview->SetMaxRange(20.0f); // 最大到達距離を設定

		BeginSpawnIntro();
	}

	void Player::SetSpawnIntroCharacterVisible(bool visible)
	{
		m_spawnIntroCharacterVisible = visible;
		SetDrawActive(visible);
		SetShadowActive(visible);
	}

	void Player::BeginSpawnIntro()
	{
		auto transform = GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		m_spawnIntroActive = true;
		m_spawnIntroSePlayed = false;
		m_spawnIntroTimer = 0.0;
		m_spawnIntroEndPosition = transform->GetPosition();
		m_spawnIntroStartPosition = m_spawnIntroEndPosition - (kSpawnIntroWalkDirection * kSpawnIntroWalkDistance);
		m_spawnIntroStartPosition.y = m_spawnIntroEndPosition.y;
		transform->SetPosition(m_spawnIntroStartPosition);

		SetSpawnIntroCharacterVisible(false);

		if (auto util = GetBehavior<UtilBehavior>())
		{
			util->RotToHead(kSpawnIntroWalkDirection, 1.0f);
		}
		UpdateSpawnIntroCamera(m_spawnIntroStartPosition);

		TransParam portalParam;
		portalParam.position = m_spawnIntroStartPosition - (kSpawnIntroWalkDirection * kSpawnIntroPortalBackOffset)
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
		if (!m_spawnIntroActive)
		{
			return false;
		}

		m_spawnIntroTimer += elapsedTime;
		const double walkTimer = m_spawnIntroTimer - kSpawnIntroPortalOnlyDuration;

		if (walkTimer < 0.0)
		{
			SetSpawnIntroCharacterVisible(false);
			UpdateSpawnIntroCamera(m_spawnIntroStartPosition);
			return true;
		}

		if (!m_spawnIntroCharacterVisible)
		{
			if (!m_spawnIntroSePlayed)
			{
				// BeginSpawnIntro()は画面遷移中にも呼ばれるため、SEは実際にキャラが出始める瞬間まで遅らせる。
				GameAudio::Instance().PlaySound(GameSoundId::Wormhole);
				m_spawnIntroSePlayed = true;
			}

			SetSpawnIntroCharacterVisible(true);

			if (auto anim = GetBehavior<AnimationStateBehavior>())
			{
				// 表示開始時に歩きモーションを頭から再生し、ワープホールから出る瞬間を分かりやすくする。
				anim->ChangeAnimation(AnimState::Sprint, true);
			}
		}

		const float rawT = static_cast<float>(walkTimer / kSpawnIntroDuration);
		const float t = SmoothStep(rawT);

		auto transform = GetComponent<Transform>(false);
		if (transform)
		{
			const Vec3 position = m_spawnIntroStartPosition + (m_spawnIntroEndPosition - m_spawnIntroStartPosition) * t;
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

		m_spawnIntroActive = false;
		SetSpawnIntroCharacterVisible(true);
		m_isGround = true;

		if (transform)
		{
			transform->SetPosition(m_spawnIntroEndPosition);
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

		if (m_mainCamera)
		{
			// 登場カメラの位置・角度を通常追従カメラへ引き継いでから解除する。
			// 単純に SetSpawnIntroView(false) だけ行うと、通常カメラが持っていた古い角度に戻ってしまう。
			m_mainCamera->FinishSpawnIntroViewAndResumeFollow();
		}

		return false;
	}

	void Player::UpdateSpawnIntroCamera(const Vec3& playerPosition)
	{
		if (!m_mainCamera)
		{
			return;
		}

		// プレイヤーは+Z方向へ歩くので、カメラも+Z側に置くとキャラの正面が見える。
		const Vec3 at = playerPosition + Vec3(0.0f, kSpawnIntroCameraLookHeight, 0.0f);
		const Vec3 eye = playerPosition
			+ (kSpawnIntroWalkDirection * kSpawnIntroCameraDistance)
			+ Vec3(0.0f, kSpawnIntroCameraHeight, 0.0f);
		m_mainCamera->SetSpawnIntroView(true, eye, at);
	}

	void Player::OnPushA()
	{
		if (m_isGround)
		{
			auto grav = GetComponent<Gravity>();
			grav->StartJump(Vec3(0, 4.0f, 0));
			m_isGround = false;
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
		groundState.wasGrounded = m_isGround;
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
		m_isGround = groundState.isGrounded;

		if (gravity)
		{
			gravity->SetGravityVelocity(groundState.gravityVelocity);
		}
	}

	void Player::OnUpdate2(double elapsedTime)
	{
		if (m_spawnIntroActive)
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
		bool isGrounded = m_isGround;
		if (!TryApplyGroundCollision(pair, gravityVelocity, isGrounded))
		{
			return;
		}

		m_isGround = isGrounded;
		if (gravity)
		{
			gravity->SetGravityVelocity(gravityVelocity);
		}
	}
}




