/*!
@file EnemyFactory.h
@brief 敵生成ファクトリ
*/

#pragma once
#include "stdafx.h"
#include "EnemyBatchController.h"
#include <memory>
#include <map>
#include <random>
#include <vector>

namespace shooting {

	class EnemyBatchController;

	/*!
	@brief 敵の種類

	現在は Default のみ。今後、敵モデルや行動差分を増やす場合はここへ追加する。
	*/
	enum class EnemyKind
	{
		Default
	};

	/*!
	@brief 敵生成の責務を持つクラス

	GameStage は「いつ・何体出すか」だけを決め、
	生成位置の抽選と EnemyBatchController への登録はこのクラスに集約する。
	*/
	class EnemyFactory
	{
	public:
		/*!
		@brief 複数体生成時の配置ルール

		ウェーブ側の調整値からこの構造体に詰めて渡す。
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
		@brief 1回の生成バッチに必要な情報

		敵種別、生成数、中心位置、配置ルール、ステータス上書きをまとめて渡す。
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

		/*!
		@brief 敵ファクトリを生成する
		@param controller 敵を登録するバッチコントローラ
		*/
		explicit EnemyFactory(const std::shared_ptr<EnemyBatchController>& controller);

		/*!
		@brief 敵登録先のバッチコントローラを差し替える
		@param controller 敵を登録するバッチコントローラ
		*/
		void SetController(const std::shared_ptr<EnemyBatchController>& controller);
		/*!
		@brief 現在の登録先が有効かを判定する
		@return 有効な EnemyBatchController を参照している場合は true
		*/
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
		@brief 敵1体を種別設定で生成する
		@param kind 敵種別
		@param position 生成位置
		@return 生成された敵のインデックス。失敗時は size_t(-1)
		*/
		size_t CreateEnemy(EnemyKind kind, const Vec3& position) const;
		/*!
		@brief 敵1体を指定ステータスで生成する
		@param kind 敵種別
		@param position 生成位置
		@param status 敵ステータス
		@return 生成された敵のインデックス。失敗時は size_t(-1)
		*/
		size_t CreateEnemy(EnemyKind kind, const Vec3& position, const EnemyStatus& status) const;

		/*!
		@brief 指定した中心位置の周囲に複数体生成する
		@param desc 生成バッチ情報
		@return 実際に生成できた敵数
		*/
		int CreateEnemiesAround(const SpawnBatchDesc& desc);
		/*!
		@brief 指定した中心位置の周囲に、未処理分から一部だけ敵を生成する
		@param desc 生成バッチ情報
		@param maxProcessCount 今回処理する最大数
		@param acceptedPositions これまでに採用した生成位置。距離チェックに使い、今回分も追加する
		@param processedCount これまでに処理した敵数。今回分だけ増える
		@return 今回実際に生成できた敵数
		*/
		int CreateEnemiesAroundStep(
			const SpawnBatchDesc& desc,
			int maxProcessCount,
			std::vector<Vec3>& acceptedPositions,
			int& processedCount);

	private:
		std::weak_ptr<EnemyBatchController> m_controller;
		std::mt19937 m_randomEngine;
		std::map<EnemyKind, EnemyStatus> m_StatusByKind;

		/*!
		@brief 中心位置の周囲からランダムな生成位置を作成する
		@param center 生成中心
		@param settings 配置ルール
		@return 抽選された生成位置
		*/
		Vec3 CreateRandomPosition(const Vec3& center, const SpawnSettings& settings);
		/*!
		@brief 既存生成位置から十分離れているかを判定する
		@param position 判定する位置
		@param existingPositions すでに採用済みの生成位置
		@param minSpacing 最低距離
		@return 十分離れている場合は true
		*/
		bool IsFarEnough(const Vec3& position, const std::vector<Vec3>& existingPositions, float minSpacing) const;
	};

}
