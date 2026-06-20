/*!
@file GameStagePlacementObjects.cpp
@brief GameStageの自然物と編集済み配置物の生成
*/

#include "stdafx.h"
#include "Project.h"
#include "GameStageEnvironmentCommon.h"
#include <chrono>
#include <map>
#include <random>

namespace shooting {
	namespace stage_environment {

		namespace
		{
			// 自然物の岩/石だけを一時停止するためのフラグ。戻す場合はtrueにする。
			const bool kEnableScatteredRockObjects = false;

			std::mt19937 CreateStagePlacementRandomEngine()
			{
				std::random_device randomDevice;
				const auto timeSeed = static_cast<unsigned int>(
					std::chrono::high_resolution_clock::now().time_since_epoch().count());

				// random_deviceだけに依存すると環境によっては再現性が出る可能性があるため、
				// 現在時刻も混ぜてステージ生成ごとに自然物配置を変える。
				std::seed_seq seedSequence{
					randomDevice(),
					randomDevice(),
					randomDevice(),
					randomDevice(),
					timeSeed };
				return std::mt19937(seedSequence);
			}

			// 高さは見ずにXZ平面だけで距離を測る。自然物同士のざっくりした貫通回避用。
			bool IsPlacementFree(
				const Vec3& position,
				float radius,
				const std::vector<PlacementCircle>& occupied)
			{
				for (const auto& circle : occupied)
				{
					const float dx = position.x - circle.position.x;
					const float dz = position.z - circle.position.z;
					const float minDistance = radius + circle.radius;
					if ((dx * dx + dz * dz) < (minDistance * minDistance))
					{
						return false;
					}
				}
				return true;
			}

			// 指定カテゴリのモデルをランダム配置する。失敗回数に上限を置き、混みすぎた時の無限ループを避ける。
			void PlaceScatteredObjects(
				GameStage& stage,
				std::map<std::wstring, StageObjectBatch>& batches,
				std::vector<PlacementCircle>& occupied,
				const std::vector<const StageObjectDef*>& defs,
				size_t targetCount,
				float placementHalf,
				float minScale,
				float maxScale,
				float extraClearance,
				std::mt19937& gen)
			{
				if (defs.empty() || targetCount == 0)
				{
					return;
				}

				std::uniform_real_distribution<float> posDist(-placementHalf, placementHalf);
				std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
				std::uniform_real_distribution<float> scaleDist(minScale, maxScale);

				size_t placed = 0;
				const size_t maxAttempts = targetCount * 80;
				for (size_t attempt = 0; attempt < maxAttempts && placed < targetCount; ++attempt)
				{
					const auto* def = PickRandomDef(defs, gen);
					if (!def)
					{
						return;
					}

					const float scaleMultiplier = scaleDist(gen);
					Vec3 position(posDist(gen), 0.0f, posDist(gen));
					float groundHeight = 0.0f;
					if (stage.TryGetSlopeGroundHeight(position, groundHeight))
					{
						position.y = groundHeight;
					}
					const float radius = def->placementRadius * scaleMultiplier + extraClearance;
					if (!IsPlacementFree(position, radius, occupied))
					{
						continue;
					}

					AddStageObjectInstance(batches, *def, position, rotDist(gen), scaleMultiplier);
					occupied.push_back({ position, radius });
					stage.AddStageSpawnBlocker(position, radius);
					++placed;
				}
			}

			// エディタで指定した配置物を、ランダム自然物と同じインスタンシング描画へ追加する。
			void AddEditedStageObjects(
				GameStage& stage,
				std::map<std::wstring, StageObjectBatch>& batches,
				std::vector<PlacementCircle>& occupied,
				const StageLayoutGrid& grid)
			{
				std::vector<StageEditorObjectPlacement> placements;
				std::string errorMessage;
				if (!StageEditorObjectPlacementFile::Load(
						App::GetRelativeAssetsDir() + StageEditorObjectPlacementFile::GetRelativePath(),
						placements,
						errorMessage))
				{
#if defined(_DEBUG)
					OutputDebugStringA(("Stage editor object load failed: " + errorMessage + "\n").c_str());
#endif
					return;
				}

				for (const auto& placement : placements)
				{
					if (placement.row < 0 ||
						placement.row >= grid.rowCount ||
						placement.column < 0 ||
						placement.column >= grid.columnCount)
					{
						continue;
					}

					const auto* def = FindStageObjectDefByName(placement.modelName);
					if (!def)
					{
						continue;
					}

					Vec3 position = GetStageLayoutCellPosition(
						grid,
						placement.row,
						placement.column,
						0.0f);
					position = position + CalculateStageEditorObjectSubcellOffset(
						placement.subRow,
						placement.subColumn,
						kStageLayoutCellSize);
					float groundHeight = 0.0f;
					if (stage.TryGetSlopeGroundHeight(position, groundHeight))
					{
						// 高台や坂のセルでは、配置物の根元を解決済み地面高さへ合わせる。
						position.y = groundHeight;
					}

					const float yRotation =
						placement.yRotationDegrees * (XM_PI / 180.0f);
					AddStageObjectInstance(batches, *def, position, yRotation, 1.0f);

					const float radius = def->placementRadius;
					occupied.push_back({ position, radius });
					stage.AddStageSpawnBlocker(position, radius);
				}
			}
		}

	}

	using namespace stage_environment;

	// 木・岩などの自然物をまばらに配置する。
	void GameStage::CreateCoverObjects()
	{
		std::map<std::wstring, StageObjectBatch> batches;
		std::vector<PlacementCircle> occupied;

		ClearStageSpawnBlockers();
		AddStageSpawnBlocker(Vec3(0.0f, 0.0f, 0.0f), 5.0f);
		AddHeightVariationSpawnBlockers(*this);

		// プレイヤー初期位置と坂周辺を先に予約し、自然物が重ならないようにする。
		occupied.push_back({ Vec3(0.0f, 0.0f, 0.0f), 5.0f });
		AddHeightVariationOccupancy(occupied);

		// 手動配置を先に占有登録し、後から生成するランダム自然物との重なりを防ぐ。
		AddEditedStageObjects(*this, batches, occupied, LoadStageLayoutGrid());

		std::mt19937 gen = CreateStagePlacementRandomEngine();

		PlaceScatteredObjects(
			*this,
			batches,
			occupied,
			GetTreePlacementDefs(),
			24,
			27.0f,
			0.90f,
			1.15f,
			0.35f,
			gen);

		if (kEnableScatteredRockObjects)
		{
			PlaceScatteredObjects(
				*this,
				batches,
				occupied,
				MergeDefs({ StageObjectCategory::Rock, StageObjectCategory::Stone }),
				22,
				28.0f,
				0.85f,
				1.20f,
				0.25f,
				gen);
		}

		PlaceScatteredObjects(
			*this,
			batches,
			occupied,
			StageObjectCatalog::GetByCategory(StageObjectCategory::Plant),
			18,
			29.0f,
			0.80f,
			1.15f,
			0.20f,
			gen);

		PlaceScatteredObjects(
			*this,
			batches,
			occupied,
			StageObjectCatalog::GetByCategory(StageObjectCategory::Mushroom),
			8,
			29.0f,
			0.75f,
			1.10f,
			0.20f,
			gen);

		PlaceScatteredObjects(
			*this,
			batches,
			occupied,
			StageObjectCatalog::GetByCategory(StageObjectCategory::Log),
			6,
			27.5f,
			0.85f,
			1.15f,
			0.35f,
			gen);

		FlushStageObjectBatches(*this, batches);
	}

}
