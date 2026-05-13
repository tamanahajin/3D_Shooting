#pragma once
#include "stdafx.h"
#include <memory>
#include <random>
#include <vector>

namespace shooting {

	class EnemyBatchController;

	// 敵の種類。将来、通常敵・遠距離敵・ボスなどを増やすときはここを入口に分岐させる。
	enum class EnemyKind
	{
		Default
	};

	// 敵生成の責務を持つクラス。
	// GameStageは「いつ・何体出すか」だけを決め、実際の生成位置決定とEnemyBatchControllerへの登録はここに集約する。
	class EnemyFactory
	{
	public:
		// 複数体生成時の配置ルール。ウェーブ側の調整値からこの構造体に詰めて渡す。
		struct SpawnSettings
		{
			float minDistance = 5.0f;   // 中心位置から最低限離す距離
			float maxDistance = 20.0f;  // 中心位置から最大で離す距離
			float spawnY = 0.525f;      // 生成時のY座標
			float minSpacing = 2.5f;    // 同じ生成バッチ内の敵同士の最低距離
			int maxAttempts = 50;       // 位置再抽選の最大回数
		};

		// 1回の生成バッチに必要な情報。
		struct SpawnBatchDesc
		{
			EnemyKind kind = EnemyKind::Default;
			int count = 0;
			Vec3 center = Vec3(0.0f, 0.0f, 0.0f);
			SpawnSettings settings;
		};

		explicit EnemyFactory(const std::shared_ptr<EnemyBatchController>& controller);

		void SetController(const std::shared_ptr<EnemyBatchController>& controller);
		bool IsValid() const;

		// 敵1体を生成する。敵種別ごとの生成差分はこの関数に集める。
		size_t CreateEnemy(EnemyKind kind, const Vec3& position) const;

		// 指定した中心位置の周囲に複数体生成する。戻り値は実際に生成できた敵数。
		int CreateEnemiesAround(const SpawnBatchDesc& desc);

	private:
		std::weak_ptr<EnemyBatchController> m_controller;
		std::mt19937 m_randomEngine;

		Vec3 CreateRandomPosition(const Vec3& center, const SpawnSettings& settings);
		bool IsFarEnough(const Vec3& position, const std::vector<Vec3>& existingPositions, float minSpacing) const;
	};

}