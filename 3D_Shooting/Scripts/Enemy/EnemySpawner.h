/*!
@file EnemySpawner.h
@brief 敵の生成位置抽選と分割生成キューを扱う
*/

#pragma once
#include "stdafx.h"
#include "EnemyFactory.h"
#include "EnemySpawnPositionResolver.h"
#include <deque>
#include <map>
#include <memory>
#include <random>
#include <vector>

namespace shooting {

	class EnemyController;

	/*!
	@brief WaveController が決めた生成数を、位置抽選と分割生成に落とし込む

	ウェーブ進行とは分離し、ステージ上に生成できる位置かどうかの検証もここでまとめて行う。
	*/
	class EnemySpawner
	{
	public:
		EnemySpawner();
		explicit EnemySpawner(const std::shared_ptr<EnemyController>& controller);
		void SetController(const std::shared_ptr<EnemyController>& controller);
		void SetSpawnPositionResolver(const std::shared_ptr<EnemySpawnPositionResolver>& resolver);
		bool IsValid() const;

		void SetStatus(EnemyKind kind, const EnemyStatus& status);
		EnemyStatus GetStatus(EnemyKind kind) const;

		/*!
		@brief 指定数の敵を同じフレーム内で生成する
		*/
		int CreateEnemiesAround(const EnemyFactory::SpawnBatchDesc& desc);
		/*!
		@brief 敵生成を数フレームに分散するためのキューへ積む
		*/
		void QueueEnemies(const EnemyFactory::SpawnBatchDesc& desc);
		/*!
		@brief 分割生成キューを1フレーム分だけ処理する
		*/
		int ProcessPendingSpawns(int maxProcessCount);
		bool HasPendingSpawns() const { return !m_pendingSpawnBatches.empty(); }
		void ClearPendingSpawns();

	private:
		struct PendingSpawnBatch
		{
			EnemyFactory::SpawnBatchDesc desc;
			std::vector<Vec3> acceptedPositions;
			int processedCount = 0;
		};

		std::weak_ptr<EnemyController> m_controller;
		std::weak_ptr<EnemySpawnPositionResolver> m_spawnPositionResolver;
		std::shared_ptr<EnemyFactory> m_enemyFactory;
		std::map<EnemyKind, EnemyStatus> m_statusByKind;
		std::deque<PendingSpawnBatch> m_pendingSpawnBatches;
		std::mt19937 m_randomEngine;

		void PrepareEnemyFactory();
		/*!
		@brief 分割生成中に敵ステータスが変わらないよう、生成開始時の設定を固定する
		*/
		EnemyFactory::SpawnBatchDesc MakeFixedStatusDesc(const EnemyFactory::SpawnBatchDesc& desc) const;
		/*!
		@brief 生成バッチの一部だけを処理し、残りを次フレームへ残せるようにする
		*/
		int ProcessSpawnBatchStep(
			const EnemyFactory::SpawnBatchDesc& desc,
			int maxProcessCount,
			std::vector<Vec3>& acceptedPositions,
			int& processedCount);
		/*!
		@brief 地形・配置物・同じバッチ内の敵間隔を満たす生成位置を探す
		*/
		bool TryFindSpawnPosition(
			const EnemyFactory::SpawnBatchDesc& desc,
			const EnemyStatus& status,
			const std::vector<Vec3>& acceptedPositions,
			Vec3& outPosition);
		/*!
		@brief ステージ側の生成可否ルールに合わせて候補位置を補正する
		*/
		bool TryResolveSpawnPosition(
			const Vec3& candidatePosition,
			const EnemyStatus& status,
			Vec3& outPosition) const;
		Vec3 CreateRandomPosition(
			const Vec3& center,
			const EnemyFactory::SpawnSettings& settings);
		bool IsFarEnough(
			const Vec3& position,
			const std::vector<Vec3>& existingPositions,
			float minSpacing) const;
	};

}
