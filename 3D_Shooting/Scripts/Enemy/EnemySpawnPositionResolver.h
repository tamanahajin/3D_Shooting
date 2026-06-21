/*!
@file EnemySpawnPositionResolver.h
@brief 敵生成候補をステージ上の有効位置へ解決するインターフェース
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	/*!
	@brief 敵生成位置を検証するための入力情報
	*/
	struct EnemySpawnPositionRequest
	{
		Vec3 candidatePosition = Vec3(0.0f, 0.0f, 0.0f);
		float clearanceRadius = 0.0f;
		float groundFootOffset = 0.35f;
	};

	/*!
	@brief ステージ固有の生成可否ルールを EnemySpawner から切り離すためのインターフェース
	*/
	class EnemySpawnPositionResolver
	{
	public:
		virtual ~EnemySpawnPositionResolver() = default;

		/*!
		@brief 候補位置を、地形・壁・配置物などを考慮した採用可能位置へ補正する
		*/
		virtual bool TryResolveEnemySpawnPosition(
			const EnemySpawnPositionRequest& request,
			Vec3& outPosition) const = 0;
	};

}
