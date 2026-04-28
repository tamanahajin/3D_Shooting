#pragma once
#include "stdafx.h"

namespace shooting {

	class GameStage : public Stage
	{
	private:
		int m_totalEnemyCount = 0;

		void CreateGround();
		void CreateHills();
		void CreateOuterMountains();
		void CreateCoverObjects();

		void AddBox(
			const Vec3& position,
			const Vec3& scale,
			const Quat& rotation = Quat());

		void AddHill(
			const Vec3& center,
			float baseSize,
			float heightStep);

	public:
		GameStage(ID3D12Device* pDevice) :
			Stage(pDevice)
		{
		}
		virtual ~GameStage() {}

		void CreateSeekObject();

		int GetTotalEnemyCount() const { return m_totalEnemyCount; }
		int GetAliveEnemyCount() const;
		int GetDefeatedEnemyCount() const;

		virtual void OnCreate() override;
	};

}