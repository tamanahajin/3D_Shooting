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

	class GameStage : public Stage, public EnemySpawnPositionResolver
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

		struct StageSpawnBlocker
		{
			Vec3 position;
			float radius = 0.0f;
			bool blocksItems = true;
			bool blocksEnemies = true;
		};

		enum class StageSpawnTarget
		{
			Item,
			Enemy,
		};

		struct StageSpawnBounds
		{
			float minX = 0.0f;
			float maxX = 0.0f;
			float minZ = 0.0f;
			float maxZ = 0.0f;
			bool valid = false;
		};

		WaveController m_waveController;
		std::shared_ptr<ItemFactory> m_itemFactory;
		std::vector<DamageNumberEntry> m_damageNumbers;
		std::vector<SlopeCollisionEntry> m_slopeCollisions;
		std::vector<PlatformSurfaceEntry> m_platformSurfaces;
		std::unordered_map<long long, GroundLookupCell> m_groundLookupCells;
		float m_groundLookupCellSize = 5.0f;
		std::vector<StageSpawnBlocker> m_stageSpawnBlockers;
		StageSpawnBounds m_stageSpawnBounds;
		// アイテム出現用乱数。CreateItemsでステージ開始ごとにシードを入れる。
		std::mt19937 m_itemSpawnRandom;
		HitStopController m_HitStop;
		std::shared_ptr<EnemyInstancedRenderer> m_enemyInstancedRenderer;
		std::shared_ptr<EnemyIndividualRenderer> m_enemyIndividualRenderer;
		bool m_enemyRendererUsesInstancing = true;
		// 初回ウェーブだけ、プレイヤー登場演出の完了を待ってから開始する。
		bool m_WaitingInitialWaveUntilPlayerIntroEnds = false;
		// リザルト表示用の1プレイ分の集計値。
		double m_SurvivalTime = 0.0;
		long long m_TotalDamageDealt = 0;
		int m_BestExplosionKills = 0;

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
		bool IsStageSpawnPositionFree(
			const Vec3& position,
			float radius,
			StageSpawnTarget target) const;
		bool IsInsideStageSpawnBounds(const Vec3& position, float radius) const;
		bool TryResolveEnemySpawnGroundHeight(
			const Vec3& position,
			float clearanceRadius,
			float& outHeight) const;
		bool IsItemSpawnPositionFree(const Vec3& position, float radius) const;
		void ClearStageSpawnBlockers();
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
		/*!
		@brief プレイヤーが操作可能になってから死亡するまでの生存時間を取得する
		@return 生存時間（秒）
		*/
		double GetSurvivalTime() const { return m_SurvivalTime; }
		/*!
		@brief 敵へ実際に与えた総ダメージを取得する
		@return オーバーキル分を除いた総ダメージ
		*/
		long long GetTotalDamageDealt() const { return m_TotalDamageDealt; }
		/*!
		@brief 爆弾1個で死亡が確定した敵数の最高記録を取得する
		@return 1回の爆発による最大撃破数
		*/
		int GetBestExplosionKills() const { return m_BestExplosionKills; }
		/*!
		@brief 敵へ実際に与えたダメージを総ダメージへ加算する
		@param damage 加算するダメージ
		*/
		void RecordDamageDealt(int damage);
		/*!
		@brief 爆弾1個の撃破数で最高記録を更新する
		@param killCount その爆弾で死亡が確定した敵数
		*/
		void RecordExplosionKills(int killCount);

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
		/*!
		@brief アイテムまたは敵の生成を避けるステージ占有領域を追加する
		@param position 占有領域の中心
		@param radius XZ平面上の占有半径
		@param blocksItems アイテム生成を避ける場合は true
		@param blocksEnemies 敵生成を避ける場合は true
		*/
		void AddStageSpawnBlocker(
			const Vec3& position,
			float radius,
			bool blocksItems = true,
			bool blocksEnemies = true);
		/*!
		@brief 地形表面へ高さを合わせ、配置物や地形内部を避けて敵生成位置を解決する
		@param request 生成候補と敵の占有情報
		@param outPosition 採用可能な生成位置
		@return 地形表面上の生成可能位置なら true
		*/
		virtual bool TryResolveEnemySpawnPosition(
			const EnemySpawnPositionRequest& request,
			Vec3& outPosition) const override;

		virtual void OnCreate() override;
		virtual void OnUpdate2(double elapsedTime) override;
	};

}
