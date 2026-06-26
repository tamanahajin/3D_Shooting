/*!
@file BallisticTrajectory.h
@brief 放物線軌道の初速計算と位置サンプリング
*/

#pragma once
#include "BaseMath.h"

namespace shooting {

	struct BallisticTrajectorySolution
	{
		bsm::Vec3 initialVelocity = bsm::Vec3(0.0f, 0.0f, 0.0f);
		float duration = 0.0f;
	};

	namespace BallisticTrajectory {

		float CalculateArcHeight(
			const bsm::Vec3& start,
			const bsm::Vec3& target,
			float baseHeight,
			float heightPerDistance) noexcept;

		/*!
		@brief 指定した頂点高度を通って目標へ到達する初速と飛行時間を求める
		*/
		bool TrySolveApexHeight(
			const bsm::Vec3& start,
			const bsm::Vec3& target,
			const bsm::Vec3& gravity,
			float arcHeight,
			BallisticTrajectorySolution& outSolution) noexcept;

		bsm::Vec3 SamplePosition(
			const bsm::Vec3& start,
			const bsm::Vec3& initialVelocity,
			const bsm::Vec3& gravity,
			float time) noexcept;

		bsm::Vec3 SampleVelocity(
			const bsm::Vec3& initialVelocity,
			const bsm::Vec3& gravity,
			float time) noexcept;
	}

}
