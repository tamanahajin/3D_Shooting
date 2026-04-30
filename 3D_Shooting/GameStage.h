#pragma once
#include "stdafx.h"

namespace shooting {

	class GameStage : public Stage
	{
	public:
		struct DamageNumberEntry
		{
			std::wstring text;
			Vec3 position;
			Vec3 velocity;
			double age = 0.0;
			double life = 0.9;

			float GetAlpha() const
			{
				float t = life > 0.0 ? static_cast<float>(age / life) : 1.0f;
				if (t < 0.0f) t = 0.0f;
				if (t > 1.0f) t = 1.0f;
				return 1.0f - t;
			}
		};

	private:
		int m_totalEnemyCount = 0;
		std::vector<DamageNumberEntry> m_damageNumbers;

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

		void SpawnDamageNumber(const Vec3& position, int damage);
		const std::vector<DamageNumberEntry>& GetDamageNumbers() const { return m_damageNumbers; }

		virtual void OnCreate() override;
		virtual void OnUpdate2(double elapsedTime) override;
	};

}