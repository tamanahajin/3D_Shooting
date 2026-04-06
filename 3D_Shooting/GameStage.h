#pragma once
#include "stdafx.h"

namespace shooting {

	class GameStage : public Stage
	{
	private:
		int m_totalEnemyCount = 0;

	public:
		GameStage(ID3D12Device* pDevice) :
			Stage(pDevice)
		{
		}
		virtual ~GameStage() {}

		void CreateSeekObject();
		void CreateFloatingEnemies();

		int GetTotalEnemyCount() const { return m_totalEnemyCount; }
		int GetAliveEnemyCount() const;
		int GetDefeatedEnemyCount() const;

		virtual void OnCreate() override;
	};

}