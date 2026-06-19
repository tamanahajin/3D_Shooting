/*!
@file GameStageEnvironmentCommon.h
@brief GameStageの環境生成ファイル間で共有する型と関数宣言
*/

#pragma once

#include "stdafx.h"
#include <initializer_list>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace shooting {

	class GameStage;
	struct StageObjectDef;
	enum class StageObjectCategory;

	namespace stage_environment {

		// 配置済みオブジェクトの占有範囲。XZ平面の円として扱い、自然物やスポーンの重なり回避に使う。
		struct PlacementCircle
		{
			Vec3 position;
			float radius;
		};

		// 同じメッシュの配置をまとめるための一時データ。最後にInstancedStaticDrawへ渡して描画負荷を下げる。
		struct StageObjectBatch
		{
			std::wstring meshKey;
			std::wstring materialPrefix;
			std::vector<Mat4x4> worlds;
		};

		struct FixedStageObjectPlacement
		{
			const wchar_t* modelName;
			Vec3 position;
			float yRotation;
			float collisionYRotation;
			float scaleMultiplier;
			float occupancyRadius;
		};

		struct StageLayoutGrid
		{
			std::vector<int> objects;
			std::vector<int> heights;
			int rowCount = 0;
			int columnCount = 0;
		};

		inline constexpr int kStageLayoutObjectEmpty = 0;
		inline constexpr int kStageLayoutObjectBlock = 1;
		inline constexpr int kStageLayoutObjectSlopeUp = 2;
		inline constexpr int kStageLayoutObjectSlopeDown = 3;
		inline constexpr int kStageLayoutObjectSlopeLeft = 4;
		inline constexpr int kStageLayoutObjectSlopeRight = 5;
		inline constexpr int kHeightGridMaxLevel = 3;
		inline constexpr int kDefaultStageLayoutRows = 13;
		inline constexpr int kDefaultStageLayoutColumns = 13;
		inline constexpr float kStageLayoutCellSize = 5.0f;
		inline constexpr float kStageLayoutHeightStep = 5.0f;
		inline const Vec3 kStageLayoutOrigin(0.0f, 0.0f, 0.0f);

		Mat4x4 MakeStageObjectWorld(
			const StageObjectDef& def,
			const Vec3& position,
			float yRotation,
			const Vec3& scaleMultiplier);
		Mat4x4 MakeStageObjectWorld(
			const StageObjectDef& def,
			const Vec3& position,
			float yRotation,
			float scaleMultiplier);
		void AddStageObjectInstance(
			std::map<std::wstring, StageObjectBatch>& batches,
			const StageObjectDef& def,
			const Vec3& position,
			float yRotation,
			float scaleMultiplier);
		void FlushStageObjectBatches(
			GameStage& stage,
			const std::map<std::wstring, StageObjectBatch>& batches);
		const StageObjectDef* PickRandomDef(
			const std::vector<const StageObjectDef*>& defs,
			std::mt19937& gen);
		std::vector<const StageObjectDef*> MergeDefs(std::initializer_list<StageObjectCategory> categories);
		const StageObjectDef* FindStageObjectDefByName(const std::wstring& name);
		const StageObjectDef* FindOuterCliffDef();
		bool IsSlopePlacement(const FixedStageObjectPlacement& placement);
		Vec3 MakeSlopeDirection(float yRotation);
		StageLayoutGrid LoadStageLayoutGrid();
		int GetStageLayoutIndex(const StageLayoutGrid& grid, int row, int column);
		int ClampHeightLevel(int heightLevel);
		Vec3 GetStageLayoutCellPosition(const StageLayoutGrid& grid, int row, int column, float y);
		std::vector<FixedStageObjectPlacement> BuildHeightVariationPlacements();
		std::vector<const StageObjectDef*> GetTreePlacementDefs();
		void AddHeightVariationOccupancy(std::vector<PlacementCircle>& occupied);
		void AddHeightVariationSpawnBlockers(GameStage& stage);

	}
}
