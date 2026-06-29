/*!
@file BombImpactResolver.h
@brief 爆弾プレビューと実弾で共有する着弾面解決
*/

#pragma once
#include "stdafx.h"

namespace shooting {

	struct BombImpactSurface
	{
		Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
		Vec3 normal = Vec3(0.0f, 1.0f, 0.0f);
		bool hasSurface = false;
	};

	/*!
	@brief 爆弾の着弾候補と地形到達判定を解決する

	プレビューと実弾で別々に面解決を行うと、表示した終点と実際の爆発位置が
	少しずれるため、このクラスに集約する。
	*/
	class BombImpactResolver final
	{
	public:
		BombImpactResolver() = delete;

		static BombImpactSurface ResolveTargetSurface(
			const std::shared_ptr<Stage>& stage,
			const std::shared_ptr<GameObject>& ignoredObject,
			const Vec3& start,
			const Vec3& target,
			const Vec3& hitNormal,
			bool hasHit);

		static bool ShouldCheckGeneratedGroundImpact(
			bool hasTargetSurface,
			const Vec3& targetNormal) noexcept;

		static bool TryResolveGeneratedGroundImpact(
			const std::shared_ptr<Stage>& stage,
			const Vec3& previousPosition,
			const Vec3& currentPosition,
			Vec3& outImpactPosition) noexcept;

	private:
		static bool TryGetStageGroundHeight(
			const std::shared_ptr<Stage>& stage,
			const Vec3& position,
			float& outHeight) noexcept;

		static Vec3 SafeNormalize(const Vec3& value) noexcept;
	};

}
