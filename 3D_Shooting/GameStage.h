#pragma once
#include "stdafx.h"
#include <unordered_map>
#include <random>

namespace shooting {

	class EnemyBatchController;
	class EnemyFactory;
	class ItemFactory;

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

		struct SlopeCollisionEntry
		{
			Vec3 startCenter;
			Vec3 direction;
			float width = 1.0f;
			float length = 1.0f;
			float height = 1.0f;
		};

		struct PlatformSurfaceEntry
		{
			Vec3 center;
			Vec3 direction;
			float width = 1.0f;
			float length = 1.0f;
			float height = 0.0f;
		};

		struct WaveSettings
		{
			double intervalSeconds = 15.0;
			int firstWaveEnemyCount = 5;
			int addEnemyCountPerWave = 1;
			int speedUpEveryWaves = 5;
			float speedMultiplierAddPerStep = 0.08f;
			float spawnMinDistance = 5.0f;
			float spawnMaxDistance = 20.0f;
			float spawnY = 0.525f;
			float minSpawnSpacing = 2.5f;
			int maxSpawnAttempts = 50;
		};

	private:
		struct GroundLookupCell
		{
			std::vector<size_t> slopeIndices;
			std::vector<size_t> platformIndices;
		};

		struct ItemSpawnBlocker
		{
			Vec3 position;
			float radius = 0.0f;
		};

		int m_totalEnemyCount = 0;
		WaveSettings m_waveSettings;
		int m_currentWave = 0;
		double m_waveTimer = 0.0;
		std::shared_ptr<EnemyFactory> m_enemyFactory;
		std::shared_ptr<ItemFactory> m_itemFactory;
		std::vector<DamageNumberEntry> m_damageNumbers;
		std::vector<SlopeCollisionEntry> m_slopeCollisions;
		std::vector<PlatformSurfaceEntry> m_platformSurfaces;
		std::unordered_map<long long, GroundLookupCell> m_groundLookupCells;
		float m_groundLookupCellSize = 5.0f;
		std::vector<ItemSpawnBlocker> m_itemSpawnBlockers;
		std::mt19937 m_itemSpawnRandom = std::mt19937(20260513);

		void CreateGround();
		void CreateWalls();
		void CreateItems();
		void MaintainRecoveryItems();
		void MaintainBombItems();
		void EnsureItemFactory();
		bool TryFindItemSpawnPosition(Vec3& outPosition);
		bool IsItemSpawnPositionFree(const Vec3& position, float radius) const;
		void ClearItemSpawnBlockers();
		void CreateHeightVariationObjects();
		void CreateCoverObjects();
		void UpdateWaves(double elapsedTime);
		void StartNextWave();
		std::shared_ptr<EnemyBatchController> GetEnemyController() const;
		Vec3 GetEnemySpawnCenter() const;
		int GetEnemyCountForWave(int wave) const;
		float GetEnemySpeedMultiplierForWave(int wave) const;
		void ClearGroundLookup();
		int GetGroundLookupCoord(float value) const;
		long long MakeGroundLookupKey(int x, int z) const;
		void AddGroundLookupRange(float minX, float maxX, float minZ, float maxZ, size_t index, bool isSlope);
		void AddSlopeToGroundLookup(size_t index);
		void AddPlatformToGroundLookup(size_t index);

	public:
		GameStage(ID3D12Device* pDevice) :
			Stage(pDevice)
		{
		}
		virtual ~GameStage() {}

		void CreateSeekObject();

		int GetTotalEnemyCount() const { return m_totalEnemyCount; }
		int GetCurrentWave() const { return m_currentWave; }
		double GetWaveTimeRemaining() const { return m_waveTimer; }
		WaveSettings& GetWaveSettings() { return m_waveSettings; }
		const WaveSettings& GetWaveSettings() const { return m_waveSettings; }
		int GetAliveEnemyCount() const;
		int GetDefeatedEnemyCount() const;

		void SpawnDamageNumber(const Vec3& position, int damage);
		const std::vector<DamageNumberEntry>& GetDamageNumbers() const { return m_damageNumbers; }

		void AddSlopeCollision(
			const Vec3& startCenter,
			const Vec3& direction,
			float width,
			float length,
			float height);
		void AddPlatformGroundSurface(
			const Vec3& center,
			float yRotation,
			float width,
			float length,
			float height);
		bool TryGetSlopeGroundHeight(const Vec3& position, float& outHeight) const;
		bool TryRaycastGeneratedGround(
			const Vec3& origin,
			const Vec3& direction,
			float maxDistance,
			Vec3& outPoint,
			Vec3& outNormal,
			float& outDistance) const;
		void AddItemSpawnBlocker(const Vec3& position, float radius);

		virtual void OnCreate() override;
		virtual void OnUpdate2(double elapsedTime) override;
	};

}
