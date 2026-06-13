#include "pch.h"
#include "CppUnitTest.h"
#include "../3D_Shooting/stdafx.h"
#include "../3D_Shooting/Scripts/Enemy/WaveController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace shooting;

namespace ShootingTests
{
	TEST_CLASS(WaveControllerTests)
	{
	public:
		TEST_METHOD_INITIALIZE(ResetDebugSettings)
		{
			auto& debug = GameDebugSettingsStore::Get();
			debug.overrideEnemyCount = false;
			debug.enemyCountOverride = 100;
			debug.enemySpeedMultiplier = 1.0f;
			debug.enemySpawnPerFrame = 10;
		}

		TEST_METHOD(Wave1EnemyCountIs5)
		{
			WaveController wave;

			Assert::AreEqual(5, wave.GetEnemyCountForWave(1));
		}

		TEST_METHOD(Wave2EnemyCountIs6)
		{
			WaveController wave;

			Assert::AreEqual(6, wave.GetEnemyCountForWave(2));
		}

		TEST_METHOD(SpeedIncreasesEvery5Waves)
		{
			WaveController wave;

			Assert::AreEqual(1.0f, wave.GetEnemySpeedMultiplierForWave(4));
			Assert::AreEqual(1.08f, wave.GetEnemySpeedMultiplierForWave(5));
		}
	};
}
