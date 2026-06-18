/*!
@file EnemySpawner.h
@brief 敵の生成位置抽選と分割生成キュー
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
	@brief 敵の生成位置抽選と数フレーム分散生成を担当する

	WaveController は「いつ・何体出すか」だけを決め、
	このクラスが「どこに・何体ずつ生成するか」を処理する。
	*/
	class EnemySpawner
	{
	public:
		EnemySpawner();
		explicit EnemySpawner(const std::shared_ptr<EnemyController>& controller);
		void SetController(const std::shared_ptr<EnemyController>& controller);
		void SetSpawnPositionResolver(const std::shared_ptr<EnemySpawnPositionResolver>& resolver);
		bool IsValid() const;

		/*!
		@brief 敵種別ごとのステータスを設定する
		@param kind 敵種別
		@param status 適用する敵ステータス
		*/
		void SetStatus(EnemyKind kind, const EnemyStatus& status);
		/*!
		@brief 敵種別ごとのステータスを取得する
		@param kind 敵種別
		@return 指定種別の設定。なければ Default、さらに無ければ既定値
		*/
		EnemyStatus GetStatus(EnemyKind kind) const;

		/*!
		@brief 指定した生成バッチを即時に処理する
		@param desc 生成バッチ情報
		@return 実際に生成できた敵数
		*/
		int CreateEnemiesAround(const EnemyFactory::SpawnBatchDesc& desc);
		/*!
		@brief 指定した生成バッチを分割生成キューへ積む
		@param desc 生成バッチ情報
		*/
		void QueueEnemies(const EnemyFactory::SpawnBatchDesc& desc);
		/*!
		@brief 分割生成キューを1フレームぶん進める
		@param maxProcessCount 今回処理する最大数
		@return 今回実際に生成できた敵数
		*/
		int ProcessPendingSpawns(int maxProcessCount);
		/*!
		@brief 分割生成キューに未生成分が残っているかを判定する
		@return 未生成分がある場合は true
		*/
		bool HasPendingSpawns() const { return !m_pendingSpawnBatches.empty(); }
		void ClearPendingSpawns();

	private:
		/*!
		@brief フレームをまたいで消化する敵生成バッチ
		*/
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
		EnemyFactory::SpawnBatchDesc MakeFixedStatusDesc(const EnemyFactory::SpawnBatchDesc& desc) const;
		/*!
		@brief 敵生成を1フレーム分だけ進める
		@brief スパイク防止
		*/
		int ProcessSpawnBatchStep(
			const EnemyFactory::SpawnBatchDesc& desc,
			int maxProcessCount,
			std::vector<Vec3>& acceptedPositions,
			int& processedCount);
		bool TryFindSpawnPosition(
			const EnemyFactory::SpawnBatchDesc& desc,
			const EnemyStatus& status,
			const std::vector<Vec3>& acceptedPositions,
			Vec3& outPosition);
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
