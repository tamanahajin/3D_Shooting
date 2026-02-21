#include "stdafx.h"
#include "Project.h"

namespace shooting {

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

	void Player::MovePlayer()
	{
		float elapsedTime = (float)Scene::GetElapsedTime();
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
			auto utilPtr = GetBehavior<UtilBehavior>();
			utilPtr->RotToHead(angle, 1.0f);
		}
	}

	void Player::OnCreate()
	{
		GetStage()->SetSharedGameObject(L"Player", GetThis<Player>());

		auto ptrShadow = AddComponent<ShadowMap>();
		ptrShadow->AddBaseMesh(L"DEFAULT_CAPSULE");
		//CollisionCapsule衝突判定を付ける
		auto ptrColl = AddComponent<CollisionCapsule>();
		//重力をつける
		auto ptrGra = AddComponent<Gravity>();

		auto ptrDraw = AddComponent<BcPNTStaticDraw>();
		ptrDraw->AddBaseMesh(L"DEFAULT_CAPSULE");
		ptrDraw->AddBaseTexture(L"TRACE3_TX");
		//透明処理
		SetAlphaActive(true);
		//カメラを得る
		m_MainCamera = std::dynamic_pointer_cast<MainCamera>(GetStage()->GetCamera());
		m_CollisionManager = GetStage()->GetCollisionManager();
		m_BulletManager = GetStage()->GetSharedGameObjectEx<BulletManager>(L"BulletManager", false);
		//m_BulletManager = GetStage()->GetBulletManager();

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

		//hp->m_onDamaged = [self = GetThis<Player>()](const DamageInfo& info)
		//	{
		//		// 被弾演出、無敵時間開始、SE など
		//		// self->StartInvincible(1.0);
		//	};

		//hp->m_onDeath = [self = GetThis<Player>()](const DamageInfo& info)
		//	{
		//		// 死亡処理（リトライ、ゲームオーバー、死亡演出など）
		//		// self->SetUpdateActive(false);
		//	};
	}

	void Player::OnUpdate(double elapsedTime)
	{
		m_InputHandler.PushHandle(GetThis<Player>());

		// 移動
		MovePlayer();

		// ジャンプ（地面にいるときのみ）
		if (App::GetInputDevice().KeyDown(VK_SPACE))
		{
			OnPushA();
		}

		// フレームの最後に地面判定をリセット
		// 次フレームで OnCollisionExecute が呼ばれれば再び true になる
		m_IsGround = false;

		// 発射クールダウン更新
		m_ShotCool -= elapsedTime;

		const auto& input = App::GetInputDevice();
		const bool fireInput = input.KeyDown(VK_LBUTTON) || input.KeyDown('J');

		if (fireInput && m_ShotCool <= 0.0)
		{
			auto bulletMgr = GetStage()->GetSharedGameObjectEx<BulletManager>(L"BulletManager", false);
			if (bulletMgr && m_CollisionManager)
			{
				auto trans = GetComponent<Transform>();

				// 銃口
				Vec3 muzzle = trans->GetPosition()
					+ trans->GetForward() * 0.2f
					+ Vec3(0.0f, 0.055f, 0.0f);

				// カメラレイ（クロスヘア=画面中央）
				Vec3 rayOrigin = m_MainCamera->GetEye();
				Vec3 rayDir = m_MainCamera->GetAt() - m_MainCamera->GetEye();
				rayDir.normalize();

				// まず「カメラ → 前方」へRaycastして狙い点を作る
				const float aimMaxDist = 1000; // 既存
				RaycastHit hit;
				Vec3 aimPoint = rayOrigin + rayDir * aimMaxDist;

				if (m_CollisionManager->Raycast(rayOrigin, rayDir, aimMaxDist, hit, GetThis<GameObject>()))
				{
					aimPoint = hit.m_Point;
				}

				// 弾の方向は「銃口 → aimPoint」
				Vec3 shotDir = aimPoint - muzzle;
				if (shotDir.length() < 1e-6f) return;
				shotDir.normalize();

				// ここが重要：rot をプレイヤーの回転のままにしない
				//Quat rot = bsmUtil::MakeBulletQuatFromDir(shotDir); // 下に例
				const Vec3 localForward = Vec3(0, 0, 1); // まずこれを試す（前=-Z想定）
				Quat rot = bsmUtil::MakeFromToQuat(localForward, shotDir);

				bulletMgr->FireByType(m_CurrentBullet, muzzle, rot);
				m_ShotCool = 0.12;
			}
		}
	}

	void Player::OnPushA()
	{
		if (m_IsGround)
		{
			auto grav = GetComponent<Gravity>();
			grav->StartJump(Vec3(0, 4.0f, 0));
		}
	}

	void Player::OnCollisionEnter(const CollisionPair& pair)
	{
		CheckGroundCollision(pair);
	}

	void Player::OnCollisionExecute(const CollisionPair& pair)
	{
		// 継続的な衝突でも地面判定を更新
		CheckGroundCollision(pair);
	}

	void Player::CheckGroundCollision(const CollisionPair& pair)
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
}