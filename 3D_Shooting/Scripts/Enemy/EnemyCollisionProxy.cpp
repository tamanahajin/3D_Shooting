/*!
@file EnemyCollisionProxy.cpp
@brief 敵1体分の軽量コリジョンプロキシ

敵の本体状態はEnemyControllerの配列で持つ。
ここでは衝突イベントだけを受けて、対象indexの敵へダメージや接地情報を転送する。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	/*!
	@brief 敵1体分のコリジョンプロキシを生成する
	@param stage 所属するステージ
	@param controller 敵本体の状態を持つバッチコントローラ
	@param enemyIndex m_enemies 内の対応インデックス
	@param startPosition 初期位置
	@param status 当たり判定サイズとモデルスケールを含む敵設定
	*/
	EnemyCollisionProxy::EnemyCollisionProxy(
		const std::shared_ptr<Stage>& stage,
		const std::shared_ptr<EnemyController>& controller,
		size_t enemyIndex,
		const Vec3& startPosition,
		const EnemyStatus& status) :
		GameObject(stage),
		m_controller(controller),
		m_enemyIndex(enemyIndex),
		m_startPosition(startPosition),
		m_modelScale(status.modelScale),
		m_collisionRadius(status.collisionRadius),
		m_collisionHeight(status.collisionHeight),
		m_inUse(true)
	{
		m_transParam.position = startPosition;
	}

	EnemyCollisionProxy::~EnemyCollisionProxy() {}

	/*!
	@brief Transform と Capsule Collision を初期化する

	描画は EnemyInstancedRenderer がまとめて行うため、このプロキシは描画せず衝突だけを担当する。
	*/
	void EnemyCollisionProxy::OnCreate()
	{
		SetBatchUpdateManaged(true);
		SetDrawActive(false);
		SetShadowActive(false);

		auto transform = GetComponent<Transform>();
		transform->SetPosition(m_startPosition);
		transform->SetScale(m_modelScale);
		transform->SetRotation(0.0f, 0.0f, 0.0f);

		auto collision = AddComponent<CollisionCapsule>();
		collision->SetDebugDraw(false);
		collision->SetMakedRadius(m_collisionRadius);
		collision->SetMakedHeight(m_collisionHeight);
		collision->AddExcludeCollisionTag(L"Enemy");
		collision->AddExcludeCollisionTag(L"Floor");

		AddTag(L"Enemy");
		AddTag(L"EnemyProxy");
		AddTag(L"NoStaticStageCollision");
		AddTag(L"UseStageObjectCollision");
	}

	/*!
	@brief プールから取り出したプロキシを新しい敵に割り当てる
	@param controller 敵本体の状態を持つバッチコントローラ
	@param enemyIndex m_enemies 内の対応インデックス
	@param startPosition 初期位置
	@param status 当たり判定サイズとモデルスケールを含む敵設定

	敵生成時に GameObject と CollisionCapsule を毎回作り直すとスパイクになりやすい。
	この関数では既存プロキシを再利用し、敵配列への参照と当たり判定サイズだけを更新する。
	*/
	void EnemyCollisionProxy::ResetForEnemy(
		const std::shared_ptr<EnemyController>& controller,
		size_t enemyIndex,
		const Vec3& startPosition,
		const EnemyStatus& status)
	{
		m_controller = controller;
		m_enemyIndex = enemyIndex;
		m_startPosition = startPosition;
		m_modelScale = status.modelScale;
		m_collisionRadius = status.collisionRadius;
		m_collisionHeight = status.collisionHeight;
		m_inUse = true;

		SetUpdateActive(true);
		SetDrawActive(false);
		SetShadowActive(false);

		auto transform = GetComponent<Transform>(false);
		if (transform)
		{
			transform->SetPosition(m_startPosition);
			transform->SetScale(m_modelScale);
			transform->SetRotation(0.0f, 0.0f, 0.0f);
			transform->SetToBefore();
		}

		auto collision = GetComponent<CollisionCapsule>(false);
		if (collision)
		{
			collision->SetUpdateActive(true);
			collision->SetDebugDraw(false);
			collision->SetMakedRadius(m_collisionRadius);
			collision->SetMakedHeight(m_collisionHeight);
			collision->WakeUp();
		}

		AddTag(L"Enemy");
		AddTag(L"EnemyProxy");
		AddTag(L"NoStaticStageCollision");
		AddTag(L"UseStageObjectCollision");
	}

	/*!
	@brief 使用中プロキシをプールへ戻せる状態にする

	CollisionManager は GameObject の updateActive と Collision の updateActive を見るため、
	両方を無効化しておけばステージに残したまま判定対象から外せる。
	*/
	void EnemyCollisionProxy::DeactivateForPool()
	{
		m_inUse = false;
		m_controller.reset();
		m_enemyIndex = 0;

		RemoveTag(L"Enemy");
		SetUpdateActive(false);
		SetDrawActive(false);
		SetShadowActive(false);

		if (auto collision = GetComponent<CollisionCapsule>(false))
		{
			collision->SetUpdateActive(false);
			collision->SetDrawActive(false);
		}
	}

	/*!
	@brief 衝突相手の種類に応じて敵バッチへ処理を転送する
	@param pair CollisionManager から渡された衝突情報

	床は接地通知、弾はダメージ、爆弾は爆弾側の衝突処理へ渡す。
	*/
	void EnemyCollisionProxy::HandleCollision(const CollisionPair& pair)
	{
		if (!m_inUse)
		{
			return;
		}

		auto otherCollision = pair.m_Dest.lock();
		if (!otherCollision)
		{
			return;
		}

		auto otherObject = otherCollision->GetGameObject();
		if (!otherObject)
		{
			return;
		}

		if (otherObject->FindTag(L"Floor"))
		{
			auto controller = m_controller.lock();
			if (controller)
			{
				controller->NotifyGroundCollision(m_enemyIndex, pair);
			}
			return;
		}

		if (!otherObject->FindTag(L"Bullet") && !otherObject->FindTag(L"Bomb"))
		{
			return;
		}

		auto bulletBase = std::dynamic_pointer_cast<IBullet>(otherObject);
		if (bulletBase && !bulletBase->IsActive())
		{
			return;
		}

		if (auto bomb = std::dynamic_pointer_cast<BombBullet>(otherObject))
		{
			CollisionPair swapped = pair;
			swapped.m_Src = pair.m_Dest;
			swapped.m_Dest = pair.m_Src;
			bomb->OnCollisionEnter(swapped);
			return;
		}

		auto damageDealer = otherObject->GetComponent<DamageDealer>(false);
		if (!damageDealer)
		{
			return;
		}

		DamageInfo info;
		info.m_Damage = damageDealer->GetDamage();
		info.m_Instigator = otherObject;
		ApplyDamage(info);

		if (damageDealer->DestroyOnHit() && bulletBase)
		{
			bulletBase->SetActive(false);
		}
	}

	/*!
	@brief 衝突開始時のイベントを共通処理へ渡す
	@param pair 衝突情報
	*/
	void EnemyCollisionProxy::OnCollisionEnter(const CollisionPair& pair)
	{
		HandleCollision(pair);
	}

	/*!
	@brief 衝突継続時のイベントを共通処理へ渡す
	@param pair 衝突情報
	*/
	void EnemyCollisionProxy::OnCollisionExecute(const CollisionPair& pair)
	{
		HandleCollision(pair);
	}

	/*!
	@brief 対応する敵へダメージを適用する
	@param info ダメージ情報
	@return このダメージで即死亡、または着地後の死亡が確定した場合は true
	*/
	bool EnemyCollisionProxy::ApplyDamage(const DamageInfo& info)
	{
		if (!m_inUse)
		{
			return false;
		}

		auto controller = m_controller.lock();
		return controller ? controller->ApplyDamage(m_enemyIndex, info) : false;
	}

	/*!
	@brief 対応する敵へノックバックを与える
	@param velocity 与える速度
	*/
	void EnemyCollisionProxy::AddKnockback(const Vec3& velocity)
	{
		if (!m_inUse)
		{
			return;
		}

		auto controller = m_controller.lock();
		if (controller)
		{
			controller->AddKnockback(m_enemyIndex, velocity);
		}
	}

	/*!
	@brief 対応する敵が生存中かを取得する
	@return 生存中なら true
	*/
	bool EnemyCollisionProxy::IsAlive() const
	{
		if (!m_inUse)
		{
			return false;
		}

		auto controller = m_controller.lock();
		return controller ? controller->IsEnemyAlive(m_enemyIndex) : false;
	}
}

