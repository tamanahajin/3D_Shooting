/*!
@file EnemyFactory.h
@brief 検証済み生成位置を EnemyController へ登録する敵生成ファクトリ
*/

#pragma once
#include "stdafx.h"
#include "EnemyStatus.h"
#include <memory>
#include <map>

namespace shooting {

	class EnemyController;

	enum class EnemyKind
	{
		Default
	};

	/*!
	@brief EnemySpawner が決めた位置へ敵を実体登録するクラス

	生成位置の抽選や地形検証は持たず、敵種別とステータスを EnemyController の配列へ渡す責務に絞る。
	*/
	class EnemyFactory
	{
	public:
		
		/*!
		@brief 敵生成位置の抽選条件
		*/
		struct SpawnSettings
		{
			float minDistance = 5.0f;   // 中心位置から最低限離す距離
			float maxDistance = 20.0f;  // 中心位置から最大で離す距離
			float spawnY = 0.525f;      // 生成時のY座標
			float minSpacing = 1.5f;    // 同じ生成バッチ内の敵同士の最低距離
			int maxAttempts = 50;       // 位置再抽選の最大回数
		};

		/*!
		@brief 複数体生成を1単位として扱うための入力情報
		*/
		struct SpawnBatchDesc
		{
			EnemyKind kind = EnemyKind::Default;
			int count = 0;
			Vec3 center = Vec3(0.0f, 0.0f, 0.0f);
			SpawnSettings settings;
			bool overrideStatus = false;
			EnemyStatus status;
		};

		explicit EnemyFactory(const std::shared_ptr<EnemyController>& controller);
		void SetController(const std::shared_ptr<EnemyController>& controller);
		bool IsValid() const;

		void SetStatus(EnemyKind kind, const EnemyStatus& status);
		EnemyStatus GetStatus(EnemyKind kind) const;

		size_t CreateEnemy(EnemyKind kind, const Vec3& position) const;
		size_t CreateEnemy(EnemyKind kind, const Vec3& position, const EnemyStatus& status) const;

	private:
		std::weak_ptr<EnemyController> m_controller;
		std::map<EnemyKind, EnemyStatus> m_statusByKind;

		size_t CreateEnemyAtResolvedPosition(
			EnemyKind kind,
			const Vec3& position,
			const EnemyStatus& status) const;
	};

}
