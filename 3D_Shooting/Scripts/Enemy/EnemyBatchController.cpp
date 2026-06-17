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
		auto gameStage = std::dynamic_pointer_cast<GameStage>(GetStage(false));
		m_gameStage = gameStage;
		SetDrawActive(false);
		SetShadowActive(false);
		if (gameStage)
		{
			gameStage->SetSharedGameObject(L"EnemyBatchController", GetThis<GameObject>());
		}
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
	@return 割り当てられた敵スロットのインデックス

	死亡済みスロットがあれば EnemyState 全体を初期値から上書きして再利用する。
	空きがない場合だけ m_enemies を拡張し、当たり判定は EnemyCollisionProxy へ割り当てる。
	*/
	size_t EnemyBatchController::AddEnemy(const Vec3& startPosition, const EnemyStatus& status)
	{
		size_t index = m_enemies.size();
		if (!m_freeEnemyIndices.empty())
		{
			index = m_freeEnemyIndices.back();
			m_freeEnemyIndices.pop_back();
		}
		else
		{
			m_enemies.emplace_back();
		}

		// 再利用スロットには前の敵の速度、死亡状態、演出タイマーなどが残っている。
		// EnemyStateを作り直してから必要な初期値を設定し、状態の持ち越しを防ぐ。
		EnemyState enemy;
		enemy.status = status;
		enemy.position = startPosition;
		enemy.previousPosition = startPosition;
		enemy.rotation = Quat();
		enemy.steeringTimer = static_cast<double>(index & 3) * 0.0125;
		enemy.steeringInterval = status.steeringInterval;
		enemy.animationState = AnimState::Idle;
		enemy.animationTime = 0.0;
		enemy.animationFinished = false;
		enemy.maxHp = status.maxHp > 0 ? status.maxHp : 1;
		enemy.hp = enemy.maxHp;

		m_enemies[index] = enemy;

		// 当たり判定だけは軽量なGameObjectとして残し、描画やAI状態はm_Enemies側でまとめて扱う。
		// 生成スパイクを抑えるため、死亡済みプロキシがあれば再利用する。
		auto proxy = AcquireCollisionProxy(index, startPosition, enemy.status);
		m_enemies[index].proxy = proxy;
		SyncProxyTransform(index);
		return index;
	}

	/*!
	@brief 敵コリジョンプロキシを事前生成してプールへ入れる
	@param count 確保しておきたいプロキシ数

	生成スパイクの主因は EnemyCollisionProxy の GameObject と CollisionCapsule 作成が
	Wave開始フレームに集中すること。先に非アクティブ状態で作っておけば、Wave中は再設定だけで済む。
	*/
	void EnemyBatchController::PrewarmCollisionProxyPool(int count)
	{
		auto gameStage = m_gameStage.lock();
		if (count <= 0 || !gameStage)
		{
			return;
		}

		const int missingCount = count - static_cast<int>(m_collisionProxyPool.size());
		if (missingCount <= 0)
		{
			return;
		}

		const Vec3 pooledPosition(0.0f, -1000.0f, 0.0f);
		const EnemyStatus defaultStatus;
		for (int i = 0; i < missingCount; ++i)
		{
			auto proxy = gameStage->AddGameObject<EnemyCollisionProxy>(
				GetThis<EnemyBatchController>(),
				0,
				pooledPosition,
				defaultStatus);
			ReleaseCollisionProxy(proxy);
		}
	}

	/*!
	@brief 全敵に適用する移動速度倍率を設定する
	@param multiplier 速度倍率。0.1未満は0.1に丸める
	*/
	void EnemyBatchController::SetMoveSpeedMultiplier(float multiplier)
	{
		m_moveSpeedMultiplier = bsmUtil::Max(0.1f, multiplier);
	}

	/*!
	@brief 配列側の位置・回転・スケールをプロキシTransformへ同期する
	@param index 同期する敵のインデックス

	CollisionManager はプロキシの Transform を参照するため、
	バッチ配列で更新した結果を毎フレームここで戻す。
	*/
	void EnemyBatchController::SyncProxyTransform(size_t index)
	{
		if (index >= m_enemies.size())
		{
			return;
		}

		auto proxy = m_enemies[index].proxy.lock();
		if (!proxy)
		{
			return;
		}

		auto transform = proxy->GetComponent<Transform>(false);
		if (!transform)
		{
			return;
		}

		transform->SetPosition(m_enemies[index].position);
		transform->SetQuaternion(m_enemies[index].rotation);
		transform->SetScale(m_enemies[index].status.modelScale);
	}

	/*!
	@brief 空きプロキシを取得し、なければ新規作成する
	@param index 割り当てる敵のインデックス
	@param startPosition 生成位置
	@param status 敵設定
	@return 使用可能な EnemyCollisionProxy

	EnemyCollisionProxy は Transform と CollisionCapsule を持つため、毎回 AddGameObject すると
	Wave開始時にコンポーネント生成コストが集中する。プールに戻したものを優先して再利用する。
	*/
	std::shared_ptr<EnemyCollisionProxy> EnemyBatchController::AcquireCollisionProxy(
		size_t index,
		const Vec3& startPosition,
		const EnemyStatus& status)
	{
		while (!m_collisionProxyPool.empty())
		{
			auto proxy = m_collisionProxyPool.back();
			m_collisionProxyPool.pop_back();
			if (!proxy)
			{
				continue;
			}

			proxy->ResetForEnemy(GetThis<EnemyBatchController>(), index, startPosition, status);
			return proxy;
		}

		auto gameStage = m_gameStage.lock();
		if (!gameStage)
		{
			return nullptr;
		}

		return gameStage->AddGameObject<EnemyCollisionProxy>(
			GetThis<EnemyBatchController>(),
			index,
			startPosition,
			status);
	}

	/*!
	@brief 使用済みプロキシをプールへ戻す
	@param proxy 戻すプロキシ
	*/
	void EnemyBatchController::ReleaseCollisionProxy(const std::shared_ptr<EnemyCollisionProxy>& proxy)
	{
		if (!proxy)
		{
			return;
		}

		proxy->DeactivateForPool();
		m_collisionProxyPool.push_back(proxy);
	}

	/*!
	@brief 敵プロキシと敵状態スロットをそれぞれのプールへ戻す
	@param index 削除する敵のインデックス

	配列要素を詰めると既存プロキシの index がずれるため、要素は残して空きインデックスとして記録する。
	同じ敵を二重に返却しないよう、すでに非アクティブな場合は何もしない。
	*/
	void EnemyBatchController::RemoveEnemyProxy(size_t index)
	{
		if (index >= m_enemies.size())
		{
			return;
		}

		auto& enemy = m_enemies[index];
		if (!enemy.active)
		{
			return;
		}

		auto proxy = enemy.proxy.lock();
		if (proxy)
		{
			ReleaseCollisionProxy(proxy);
		}

		enemy.active = false;
		enemy.deathAnimFinished = true;
		enemy.proxy.reset();
		m_freeEnemyIndices.push_back(index);
	}

	/*!
	@brief 指定敵が現在生存しているかを判定する
	@param index 対象敵のインデックス
	@return 生存中なら true
	*/
	bool EnemyBatchController::IsEnemyAlive(size_t index) const
	{
		if (index >= m_enemies.size())
		{
			return false;
		}

		const auto& enemy = m_enemies[index];
		return enemy.active && !enemy.isDead && enemy.hp > 0;
	}

	/*!
	@brief 生存敵数を数える
	@return active かつ死亡していない敵の数
	*/
	int EnemyBatchController::GetAliveEnemyCount() const
	{
		int count = 0;
		for (const auto& enemy : m_enemies)
		{
			if (enemy.active && !enemy.isDead && enemy.hp > 0)
			{
				++count;
			}
		}
		return count;
	}

	/*!
	@brief 現在確保されている敵状態スロット数を取得する
	@return 再利用待ちの空きスロットを含む m_enemies の要素数
	*/
	int EnemyBatchController::GetTotalEnemyCount() const
	{
		return static_cast<int>(m_enemies.size());
	}
}


