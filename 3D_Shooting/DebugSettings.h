#pragma once

#include <cmath>

namespace shooting {

	struct GameDebugSettings
	{
		bool playerInvincible = false;
		bool overrideEnemyCount = false;
		int enemyCountOverride = 100;
		float playerDamageMultiplier = 1.0f;
		int startWave = 1;
		float enemySpeedMultiplier = 1.0f;
		bool showCollision = false;
		bool showFps = false;
		bool showElapsedTime = false;
		// Releaseビルドでベンチマークを行う場合だけtrueにする。
		bool enableBenchmarkRecording = false;
		bool useEnemyInstancedRendering = true;
		int enemySpawnPerFrame = 30;
	};

	class GameDebugSettingsStore
	{
	public:
		static GameDebugSettings& Get()
		{
			static GameDebugSettings settings;
			return settings;
		}

		static int ApplyPlayerDamageMultiplier(int baseDamage)
		{
			if (baseDamage <= 0)
			{
				return 0;
			}

			float multiplier = Get().playerDamageMultiplier;
			if (multiplier < 0.0f)
			{
				multiplier = 0.0f;
			}

			const int finalDamage = static_cast<int>(std::lround(static_cast<float>(baseDamage) * multiplier));
			return finalDamage > 0 ? finalDamage : 0;
		}
	};

}
