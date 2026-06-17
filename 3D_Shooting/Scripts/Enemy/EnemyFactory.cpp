#include "stdafx.h"
#include "EnemyFactory.h"

namespace shooting {

	/*!
	@brief 敵ファクトリを生成する
	@param controller 敵の登録先コントローラ
	*/
	EnemyFactory::EnemyFactory(const std::shared_ptr<EnemyController>& controller) :
		m_controller(controller),
		m_randomEngine(std::random_device{}())
	{
		m_statusByKind[EnemyKind::Default] = EnemyStatus();
	}

	/*!
	@brief 敵の登録先コントローラを差し替える
	@param controller 敵の登録先コントローラ
	*/
	void EnemyFactory::SetController(const std::shared_ptr<EnemyController>& controller)
	{
		m_controller = controller;
	}

	void EnemyFactory::SetSpawnPositionResolver(
		const std::shared_ptr<EnemySpawnPositionResolver>& resolver)
	{
		m_spawnPositionResolver = resolver;
	}

	/*!
	@brief 登録先コントローラが有効かを判定する
	@return 有効なら true
	*/
	bool EnemyFactory::IsValid() const
	{
		return !m_controller.expired();
	}

	/*!
	@brief 敵種別ごとのステータスを設定する
	@param kind 敵種別
	@param status 適用するステータス
	*/
	void EnemyFactory::SetStatus(EnemyKind kind, const EnemyStatus& status)
	{
		m_statusByKind[kind] = status;
	}

	/*!
	@brief 敵種別ごとのステータスを取得する
	@param kind 敵種別
	@return 指定種別のステータス。なければ Default、さらに無ければ既定値
	*/
	EnemyStatus EnemyFactory::GetStatus(EnemyKind kind) const
	{
		auto it = m_statusByKind.find(kind);
		if (it != m_statusByKind.end())
		{
			return it->second;
		}

		auto defaultIt = m_statusByKind.find(EnemyKind::Default);
		if (defaultIt != m_statusByKind.end())
		{
			return defaultIt->second;
		}

		return EnemyStatus();
	}

	/*!
	@brief 敵1体を種別設定で生成する
	@param kind 敵種別
	@param position 生成位置
	@return 生成された敵のインデックス。失敗時は size_t(-1)
	*/
	size_t EnemyFactory::CreateEnemy(EnemyKind kind, const Vec3& position) const
	{
		return CreateEnemy(kind, position, GetStatus(kind));
	}

	/*!
	@brief 敵1体を指定ステータスで生成する
	@param kind 敵種別
	@param position 生成位置
	@param status 敵ステータス
	@return 生成された敵のインデックス。失敗時は size_t(-1)
	*/
	size_t EnemyFactory::CreateEnemy(EnemyKind kind, const Vec3& position, const EnemyStatus& status) const
	{
		Vec3 resolvedPosition;
		if (!TryResolveSpawnPosition(position, status, resolvedPosition))
		{
			return static_cast<size_t>(-1);
		}

		return CreateEnemyAtResolvedPosition(kind, resolvedPosition, status);
	}

	bool EnemyFactory::TryResolveSpawnPosition(
		const Vec3& candidatePosition,
		const EnemyStatus& status,
		Vec3& outPosition) const
	{
		auto resolver = m_spawnPositionResolver.lock();
		if (!resolver)
		{
			// タイトル用など位置解決を設定しない利用箇所では、従来どおり指定位置を採用する。
			outPosition = candidatePosition;
			return true;
		}

		EnemySpawnPositionRequest request;
		request.candidatePosition = candidatePosition;
		request.clearanceRadius = status.collisionRadius;
		request.groundFootOffset = status.groundFootOffset;
		return resolver->TryResolveEnemySpawnPosition(request, outPosition);
	}

	size_t EnemyFactory::CreateEnemyAtResolvedPosition(
		EnemyKind kind,
		const Vec3& position,
		const EnemyStatus& status) const
	{
		auto controller = m_controller.lock();
		if (!controller)
		{
			return static_cast<size_t>(-1);
		}

		// 敵の実体はEnemyControllerの配列に追加する。
		// 今後、敵種別ごとのモデル差分が必要になったら、このswitchに分岐を追加する。
		switch (kind)
		{
		case EnemyKind::Default:
		default:
			return controller->AddEnemy(position, status);
		}
	}

	/*!
	@brief 中心位置の周囲に複数体の敵を生成する
	@param desc 生成バッチ情報
	@return 実際に生成できた敵数

	先に生成位置を抽選してから、同じステータスでまとめて EnemyController へ登録する。
	*/
	int EnemyFactory::CreateEnemiesAround(const SpawnBatchDesc& desc)
	{
		if (desc.count <= 0 || !IsValid())
		{
			return 0;
		}

		std::vector<Vec3> positions;
		positions.reserve(static_cast<size_t>(desc.count));
		int processedCount = 0;
		return CreateEnemiesAroundStep(desc, desc.count, positions, processedCount);
	}

	/*!
	@brief 中心位置の周囲に、未処理分から一部だけ敵を生成する
	@param desc 生成バッチ情報
	@param maxProcessCount 今回処理する最大数
	@param acceptedPositions これまでに採用した生成位置。距離チェックに使い、今回分も追加する
	@param processedCount これまでに処理した敵数。今回分だけ増える
	@return 今回実際に生成できた敵数

	ウェーブ開始時の負荷を分散するため、位置抽選と EnemyController への登録を
	maxProcessCount 体ぶんだけ進める。acceptedPositions は同じ生成バッチ内で共有し、
	フレームをまたいでも敵同士の最低距離チェックが効くようにする。
	*/
	int EnemyFactory::CreateEnemiesAroundStep(
		const SpawnBatchDesc& desc,
		int maxProcessCount,
		std::vector<Vec3>& acceptedPositions,
		int& processedCount)
	{
		if (desc.count <= 0 || maxProcessCount <= 0 || !IsValid())
		{
			return 0;
		}

		if (processedCount < 0)
		{
			processedCount = 0;
		}
		if (processedCount >= desc.count)
		{
			return 0;
		}

		if (acceptedPositions.capacity() < static_cast<size_t>(desc.count))
		{
			acceptedPositions.reserve(static_cast<size_t>(desc.count));
		}

		// 同じウェーブの敵、地形、配置物のすべてを通過した候補だけを採用する。
		const int maxAttempts = desc.settings.maxAttempts > 0 ? desc.settings.maxAttempts : 1;
		const EnemyStatus status = desc.overrideStatus ? desc.status : GetStatus(desc.kind);
		int createdCount = 0;
		int processedThisStep = 0;
		while (processedCount < desc.count && processedThisStep < maxProcessCount)
		{
			Vec3 position(desc.center.x, desc.settings.spawnY, desc.center.z);
			bool foundPosition = false;

			for (int attempt = 0; attempt < maxAttempts; ++attempt)
			{
				const Vec3 candidate = CreateRandomPosition(desc.center, desc.settings);
				Vec3 resolvedPosition;
				if (!TryResolveSpawnPosition(candidate, status, resolvedPosition))
				{
					continue;
				}
				if (!IsFarEnough(resolvedPosition, acceptedPositions, desc.settings.minSpacing))
				{
					continue;
				}

				position = resolvedPosition;
				foundPosition = true;
				break;
			}

			if (foundPosition &&
				CreateEnemyAtResolvedPosition(desc.kind, position, status) != static_cast<size_t>(-1))
			{
				acceptedPositions.push_back(position);
				++createdCount;
			}

			// 有効位置が見つからない敵も処理済みにし、混雑時に生成キューが停止し続けることを防ぐ。
			++processedCount;
			++processedThisStep;
		}

		return createdCount;
	}

	/*!
	@brief 中心位置からランダムな距離と角度で生成位置を作る
	@param center 生成中心
	@param settings 配置ルール
	@return 抽選された生成位置
	*/
	Vec3 EnemyFactory::CreateRandomPosition(const Vec3& center, const SpawnSettings& settings)
	{
		// min/maxが逆に設定されても生成できるように、ここで正規化する。
		const float minDistance = settings.minDistance < settings.maxDistance ?
			settings.minDistance : settings.maxDistance;
		const float maxDistance = settings.minDistance < settings.maxDistance ?
			settings.maxDistance : settings.minDistance;

		std::uniform_real_distribution<float> distRadius(minDistance, maxDistance);
		std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI);

		const float radius = distRadius(m_randomEngine);
		const float angle = distAngle(m_randomEngine);
		return Vec3(
			center.x + (radius * cosf(angle)),
			settings.spawnY,
			center.z + (radius * sinf(angle)));
	}

	/*!
	@brief 採用済みの生成位置と十分離れているかを判定する
	@param position 判定する位置
	@param existingPositions 採用済みの生成位置
	@param minSpacing 最低距離
	@return 十分離れている場合は true
	*/
	bool EnemyFactory::IsFarEnough(const Vec3& position, const std::vector<Vec3>& existingPositions, float minSpacing) const
	{
		if (minSpacing <= 0.0f)
		{
			return true;
		}

		const float minSpacingSq = minSpacing * minSpacing;
		for (const auto& existingPosition : existingPositions)
		{
			if ((position - existingPosition).lengthSqr() < minSpacingSq)
			{
				return false;
			}
		}
		return true;
	}

}
