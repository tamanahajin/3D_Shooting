/*!
@file EnemySpawnPositionResolver.h
@brief 敵生成位置の検証インターフェース
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	/*!
	@brief 敵生成候補を検証するための入力情報
	*/
	struct EnemySpawnPositionRequest
	{
		Vec3 candidatePosition = Vec3(0.0f, 0.0f, 0.0f);
		float clearanceRadius = 0.0f;
		float groundFootOffset = 0.35f;
	};

	/*!
	@brief 敵生成候補をステージ上の有効な位置へ解決するインターフェース
	*/
	class EnemySpawnPositionResolver
	{
	public:
		virtual ~EnemySpawnPositionResolver() = default;

		/*!
		@brief 敵生成候補を検証し、採用可能な位置へ補正する
		@param request 生成候補と敵の占有情報
		@param outPosition 採用可能な生成位置
		@return 採用可能な場合は true
		*/
		virtual bool TryResolveEnemySpawnPosition(
			const EnemySpawnPositionRequest& request,
			Vec3& outPosition) const = 0;
	};

}
