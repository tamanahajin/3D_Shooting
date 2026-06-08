/*!
@file EnemyBatchController.cpp
@brief 敵バッチ管理の基本ライフサイクル

敵は大量生成されるため、1体ごとのGameObject更新を避けて、このクラスの配列で状態をまとめて更新する。
このファイルには生成、初期登録、プロキシ同期、数の問い合わせなど、中心となる管理処理だけを置く。
*/

#include "stdafx.h"
#include "Project.h"

namespace shooting {

	/*!
	@brief 敵バッチコントローラを生成する
	@param stage 所属するステージ
	*/
	EnemyBatchController::EnemyBatchController(const std::shared_ptr<Stage>& stage) :
		GameObject(stage)
	{
	}

	EnemyBatchController::~EnemyBatchController() {}

	/*!
	@brief ステージ参照と共有オブジェクト登録を初期化する

	敵バッチ本体は描画しないため、描画と shadow を無効化する。
	他のオブジェクトは共有キーからこのコントローラを取得する。
	*/
	void EnemyBatchController::OnCreate()
	{
		m_GameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		SetDrawActive(false);
		SetShadowActive(false);
		m_GameStage->SetSharedGameObject(L"EnemyBatchController", GetThis<GameObject>());
		AddTag(L"EnemyBatchController");
	}

	/*!
	@brief 既定ステータスで敵を追加する
	@param startPosition 生成位置
	@return 追加された敵のインデックス
	*/
	size_t EnemyBatchController::AddEnemy(const Vec3& startPosition)
	{
		return AddEnemy(startPosition, EnemyStatus());
	}

	/*!
	@brief 指定ステータスで敵を追加する
	@param startPosition 生成位置
	@param status HP、移動速度、当たり判定などの敵設定
	@return 追加された敵のインデックス

	敵本体の状態は m_Enemies に追加し、当たり判定だけを EnemyCollisionProxy として作成する。
	*/
	size_t EnemyBatchController::AddEnemy(const Vec3& startPosition, const EnemyStatus& status)
	{
		EnemyState enemy;
		enemy.status = status;
		enemy.position = startPosition;
		enemy.previousPosition = startPosition;
		enemy.rotation = Quat();
		enemy.steeringTimer = static_cast<double>(m_Enemies.size() & 3) * 0.0125;
		enemy.steeringInterval = status.steeringInterval;
		enemy.animationState = AnimState::Idle;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
		enemy.maxHp = status.maxHp > 0 ? status.maxHp : 1;
		enemy.hp = enemy.maxHp;

		const size_t index = m_Enemies.size();
		m_Enemies.push_back(enemy);

		// 当たり判定だけは軽量なGameObjectとして残し、描画やAI状態はm_Enemies側でまとめて扱う。
		auto proxy = m_GameStage->AddGameObject<EnemyCollisionProxy>(GetThis<EnemyBatchController>(), index, startPosition, enemy.status);
		m_Enemies[index].proxy = proxy;
		SyncProxyTransform(index);
		return index;
	}

	/*!
	@brief 全敵に適用する移動速度倍率を設定する
	@param multiplier 速度倍率。0.1未満は0.1に丸める
	*/
	void EnemyBatchController::SetMoveSpeedMultiplier(float multiplier)
	{
		m_MoveSpeedMultiplier = bsmUtil::Max(0.1f, multiplier);
	}

	/*!
	@brief 配列側の位置・回転・スケールをプロキシTransformへ同期する
	@param index 同期する敵のインデックス

	CollisionManager はプロキシの Transform を参照するため、
	バッチ配列で更新した結果を毎フレームここで戻す。
	*/
	void EnemyBatchController::SyncProxyTransform(size_t index)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto proxy = m_Enemies[index].proxy.lock();
		if (!proxy)
		{
			return;
		}

		auto transform = proxy->GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		transform->SetPosition(m_Enemies[index].position);
		transform->SetQuaternion(m_Enemies[index].rotation);
		transform->SetScale(m_Enemies[index].status.modelScale);
	}

	/*!
	@brief 敵プロキシをステージから外し、配列上では非アクティブにする
	@param index 削除する敵のインデックス

	配列要素を詰めると既存プロキシの index がずれるため、要素は残して active=false にする。
	*/
	void EnemyBatchController::RemoveEnemyProxy(size_t index)
	{
		if (index >= m_Enemies.size())
		{
			return;
		}

		auto proxy = m_Enemies[index].proxy.lock();
		if (proxy)
		{
			proxy->RemoveTag(L"Enemy");
			proxy->SetDrawActive(false);
			proxy->SetUpdateActive(false);
			m_GameStage->RemoveGameObject(proxy);
		}

		m_Enemies[index].active = false;
		m_Enemies[index].deathAnimFinished = true;
		m_Enemies[index].proxy.reset();
	}

	/*!
	@brief 指定敵が現在生存しているかを判定する
	@param index 対象敵のインデックス
	@return 生存中なら true
	*/
	bool EnemyBatchController::IsEnemyAlive(size_t index) const
	{
		if (index >= m_Enemies.size())
		{
			return false;
		}

		const auto& enemy = m_Enemies[index];
		return enemy.active && !enemy.isDead && enemy.hp > 0;
	}

	/*!
	@brief 生存敵数を数える
	@return active かつ死亡していない敵の数
	*/
	int EnemyBatchController::GetAliveEnemyCount() const
	{
		int count = 0;
		for (const auto& enemy : m_Enemies)
		{
			if (enemy.active && !enemy.isDead && enemy.hp > 0)
			{
				++count;
			}
		}
		return count;
	}

	/*!
	@brief 敵配列に登録済みの総数を取得する
	@return m_Enemies の要素数
	*/
	int EnemyBatchController::GetTotalEnemyCount() const
	{
		return static_cast<int>(m_Enemies.size());
	}
}


