#pragma once
#include "stdafx.h"
#include <unordered_map>
#include <random>
#include "WaveController.h"
#include "HitStopController.h"

namespace shooting {

	class EnemyBatchController;
	class EnemyIndividualRenderer;
	class EnemyInstancedRenderer;
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


	protected:
		void CreateGround();
		void CreateWalls();
		void CreateHeightVariationObjects();
		void CreateCoverObjects();

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

		WaveController m_waveController;
		std::shared_ptr<ItemFactory> m_itemFactory;
		std::vector<DamageNumberEntry> m_damageNumbers;
		std::vector<SlopeCollisionEntry> m_slopeCollisions;
		std::vector<PlatformSurfaceEntry> m_platformSurfaces;
		std::unordered_map<long long, GroundLookupCell> m_groundLookupCells;
		float m_groundLookupCellSize = 5.0f;
		std::vector<ItemSpawnBlocker> m_itemSpawnBlockers;
		// アイテム出現用乱数。CreateItemsでステージ開始ごとにシードを入れる。
		std::mt19937 m_itemSpawnRandom;
		HitStopController m_HitStop;
		std::shared_ptr<EnemyInstancedRenderer> m_enemyInstancedRenderer;
		std::shared_ptr<EnemyIndividualRenderer> m_enemyIndividualRenderer;
		bool m_enemyRendererUsesInstancing = true;
		// 初回ウェーブだけ、プレイヤー登場演出の完了を待ってから開始する。
		bool m_WaitingInitialWaveUntilPlayerIntroEnds = false;

		void CreateItems();
		void MaintainRecoveryItems();
		void MaintainBombItems();
		void ApplyDebugRuntimeSettings();
		void CreateEnemyRenderers(const std::shared_ptr<EnemyBatchController>& controller);
		void ApplyEnemyRendererMode(bool useInstancing);
		// 初回ウェーブ開始待ち中に呼び、登場演出が終わっていればウェーブ1を開始する。
		void StartInitialWaveAfterPlayerIntro();
		void EnsureItemFactory();
		bool TryFindItemSpawnPosition(Vec3& outPosition);
		bool IsItemSpawnPositionFree(const Vec3& position, float radius) const;
		void ClearItemSpawnBlockers();
		std::shared_ptr<EnemyBatchController> GetEnemyController() const;
		Vec3 GetEnemySpawnCenter() const;
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

		int GetTotalEnemyCount() const { return m_waveController.GetTotalEnemyCount(); }
		int GetCurrentWave() const { return m_waveController.GetCurrentWave(); }
		double GetWaveTimeRemaining() const { return m_waveController.GetWaveTimeRemaining(); }
		WaveSettings& GetWaveSettings() { return m_waveController.GetSettings(); }
		const WaveSettings& GetWaveSettings() const { return m_waveController.GetSettings(); }
		void SetEnemyStatus(EnemyKind kind, const EnemyStatus& status) { m_waveController.SetEnemyStatus(kind, status); }
		EnemyStatus GetEnemyStatus(EnemyKind kind) const { return m_waveController.GetEnemyStatus(kind); }
		int GetAliveEnemyCount() const;
		int GetDefeatedEnemyCount() const;

		void SpawnDamageNumber(const Vec3& position, int damage);
		const std::vector<DamageNumberEntry>& GetDamageNumbers() const { return m_damageNumbers; }
		void RequestHitStop(double duration, double timeScale);
		double GetGameDeltaTime(double rawDeltaTime) const;
		bool IsHitStopActive() const { return m_HitStop.IsActive(); }

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
