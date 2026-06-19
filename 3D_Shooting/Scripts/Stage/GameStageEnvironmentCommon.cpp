/*!
@file GameStageEnvironmentCommon.cpp
@brief GameStageの環境生成ファイル間で共有する処理
*/

#include "stdafx.h"
#include "Project.h"
#include "GameStageEnvironmentCommon.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace shooting {
	namespace stage_environment {

		namespace
		{
			const wchar_t* kStageObjectLayoutCsv = L"Stage/stage_objects.csv";
			const wchar_t* kStageHeightLayoutCsv = L"Stage/stage_heights.csv";
			const wchar_t* kHeightGridBlockModel = L"cliff_block_rock";
			const wchar_t* kHeightGridSlopeModel = L"cliff_blockslope_rock";

			const int kDefaultStageObjectLayout[] =
			{
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			};

			const int kDefaultStageHeightLayout[] =
			{
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			};

			std::vector<std::vector<int>> LoadCsvGrid(const std::wstring& relativePath)
			{
				std::vector<std::vector<int>> rows;
				std::ifstream file{
					std::filesystem::path(App::GetRelativeAssetsDir()) / relativePath };
				if (!file.is_open())
				{
					return rows;
				}

				std::string line;
				while (std::getline(file, line))
				{
					const auto commentPos = line.find('#');
					if (commentPos != std::string::npos)
					{
						line = line.substr(0, commentPos);
					}

					std::replace(line.begin(), line.end(), ',', ' ');
					std::replace(line.begin(), line.end(), ';', ' ');
					std::replace(line.begin(), line.end(), '\t', ' ');

					std::stringstream stream(line);
					std::vector<int> values;
					int value = 0;
					while (stream >> value)
					{
						values.push_back(value);
					}
					if (!values.empty())
					{
						rows.push_back(values);
					}
				}

				return rows;
			}

			bool IsRectangularGrid(const std::vector<std::vector<int>>& rows)
			{
				if (rows.empty() || rows.front().empty())
				{
					return false;
				}

				const size_t columnCount = rows.front().size();
				for (const auto& row : rows)
				{
					if (row.size() != columnCount)
					{
						return false;
					}
				}
				return true;
			}

			void FlattenGrid(const std::vector<std::vector<int>>& rows, std::vector<int>& outValues)
			{
				outValues.clear();
				for (const auto& row : rows)
				{
					outValues.insert(outValues.end(), row.begin(), row.end());
				}
			}

			StageLayoutGrid MakeDefaultStageLayoutGrid()
			{
				StageLayoutGrid grid;
				grid.rowCount = kDefaultStageLayoutRows;
				grid.columnCount = kDefaultStageLayoutColumns;
				grid.objects.assign(
					std::begin(kDefaultStageObjectLayout),
					std::end(kDefaultStageObjectLayout));
				grid.heights.assign(
					std::begin(kDefaultStageHeightLayout),
					std::end(kDefaultStageHeightLayout));
				return grid;
			}

			bool GetSlopeDirectionDelta(int objectCode, int& rowDelta, int& columnDelta)
			{
				switch (objectCode)
				{
				case kStageLayoutObjectSlopeUp:
					rowDelta = -1;
					columnDelta = 0;
					return true;
				case kStageLayoutObjectSlopeDown:
					rowDelta = 1;
					columnDelta = 0;
					return true;
				case kStageLayoutObjectSlopeLeft:
					rowDelta = 0;
					columnDelta = -1;
					return true;
				case kStageLayoutObjectSlopeRight:
					rowDelta = 0;
					columnDelta = 1;
					return true;
				default:
					rowDelta = 0;
					columnDelta = 0;
					return false;
				}
			}

			float DirectionToYRotation(int rowDelta, int columnDelta)
			{
				// CSVの列は右へ進むほどワールドXが小さくなるため、左右方向だけ符号を反転する。
				const float x = -static_cast<float>(columnDelta);
				const float z = static_cast<float>(rowDelta);
				return std::atan2(x, -z);
			}

			void AddBlockStackPlacements(
				std::vector<FixedStageObjectPlacement>& placements,
				const StageLayoutGrid& grid,
				int row,
				int column,
				int heightLevel,
				float blockOccupancy)
			{
				for (int level = 1; level <= heightLevel; ++level)
				{
					const float blockY = static_cast<float>(level - 1) * kStageLayoutHeightStep;
					placements.push_back({
						kHeightGridBlockModel,
						GetStageLayoutCellPosition(grid, row, column, blockY),
						0.0f,
						0.0f,
						1.0f,
						blockOccupancy });
				}
			}
		}

		Mat4x4 MakeStageObjectWorld(
			const StageObjectDef& def,
			const Vec3& position,
			float yRotation,
			const Vec3& scaleMultiplier)
		{
			Vec3 scale(
				def.scale.x * scaleMultiplier.x,
				def.scale.y * scaleMultiplier.y,
				def.scale.z * scaleMultiplier.z);

			Mat4x4 scaleMat;
			scaleMat.identity();
			scaleMat.scale(scale);

			Mat4x4 rotMat;
			rotMat.identity();
			rotMat.rotation(Quat(Vec3(0.0f, 1.0f, 0.0f), yRotation));

			Mat4x4 transMat;
			transMat.identity();
			transMat.translation(position);

			Mat4x4 world = scaleMat;
			world *= rotMat;
			world *= transMat;
			return world;
		}

		Mat4x4 MakeStageObjectWorld(
			const StageObjectDef& def,
			const Vec3& position,
			float yRotation,
			float scaleMultiplier)
		{
			return MakeStageObjectWorld(
				def,
				position,
				yRotation,
				Vec3(scaleMultiplier, scaleMultiplier, scaleMultiplier));
		}

		void AddStageObjectInstance(
			std::map<std::wstring, StageObjectBatch>& batches,
			const StageObjectDef& def,
			const Vec3& position,
			float yRotation,
			float scaleMultiplier)
		{
			auto& batch = batches[def.key];
			batch.meshKey = def.key;
			batch.materialPrefix = def.materialPrefix;
			batch.worlds.push_back(MakeStageObjectWorld(def, position, yRotation, scaleMultiplier));
		}

		void FlushStageObjectBatches(GameStage& stage, const std::map<std::wstring, StageObjectBatch>& batches)
		{
			for (const auto& entry : batches)
			{
				const auto& batch = entry.second;
				if (batch.worlds.empty())
				{
					continue;
				}

				stage.AddGameObject<StageObjectInstancedRenderer>(
					batch.meshKey,
					batch.materialPrefix,
					batch.worlds);
			}
		}

		const StageObjectDef* PickRandomDef(
			const std::vector<const StageObjectDef*>& defs,
			std::mt19937& gen)
		{
			if (defs.empty())
			{
				return nullptr;
			}

			std::uniform_int_distribution<int> dist(0, static_cast<int>(defs.size()) - 1);
			return defs[dist(gen)];
		}

		std::vector<const StageObjectDef*> MergeDefs(std::initializer_list<StageObjectCategory> categories)
		{
			std::vector<const StageObjectDef*> result;
			for (auto category : categories)
			{
				auto defs = StageObjectCatalog::GetByCategory(category);
				result.insert(result.end(), defs.begin(), defs.end());
			}
			return result;
		}

		const StageObjectDef* FindStageObjectDefByName(const std::wstring& name)
		{
			const auto& defs = StageObjectCatalog::GetAll();
			for (const auto& def : defs)
			{
				if (def.name == name)
				{
					return &def;
				}
			}
			return nullptr;
		}

		const StageObjectDef* FindOuterCliffDef()
		{
			auto outsideWalls = StageObjectCatalog::GetByCategory(StageObjectCategory::OutSideWall);
			if (!outsideWalls.empty())
			{
				for (const auto* def : outsideWalls)
				{
					if (def->name == L"cliff_block_stone")
					{
						return def;
					}
				}
				return outsideWalls.front();
			}

			auto cliffs = StageObjectCatalog::GetByCategory(StageObjectCategory::Cliff);
			if (cliffs.empty())
			{
				return nullptr;
			}

			for (const auto* def : cliffs)
			{
				if (def->name == L"cliff_block_rock")
				{
					return def;
				}
			}
			for (const auto* def : cliffs)
			{
				if (def->name.find(L"cliff_block") != std::wstring::npos &&
					def->name.find(L"slope") == std::wstring::npos &&
					def->name.find(L"diagonal") == std::wstring::npos &&
					def->name.find(L"quarter") == std::wstring::npos &&
					def->name.find(L"half") == std::wstring::npos)
				{
					return def;
				}
			}

			return cliffs.front();
		}

		bool IsSlopePlacement(const FixedStageObjectPlacement& placement)
		{
			return placement.modelName && std::wstring(placement.modelName).find(L"slope") != std::wstring::npos;
		}

		Vec3 MakeSlopeDirection(float yRotation)
		{
			Vec3 direction(std::sin(yRotation), 0.0f, -std::cos(yRotation));
			if (direction.length() <= 0.0001f)
			{
				return Vec3(0.0f, 0.0f, -1.0f);
			}
			direction.normalize();
			return direction;
		}

		StageLayoutGrid LoadStageLayoutGrid()
		{
			const auto objectRows = LoadCsvGrid(kStageObjectLayoutCsv);
			const auto heightRows = LoadCsvGrid(kStageHeightLayoutCsv);
			if (!IsRectangularGrid(objectRows) || !IsRectangularGrid(heightRows))
			{
				return MakeDefaultStageLayoutGrid();
			}
			if (objectRows.size() != heightRows.size() || objectRows.front().size() != heightRows.front().size())
			{
				return MakeDefaultStageLayoutGrid();
			}

			StageLayoutGrid grid;
			grid.rowCount = static_cast<int>(objectRows.size());
			grid.columnCount = static_cast<int>(objectRows.front().size());
			FlattenGrid(objectRows, grid.objects);
			FlattenGrid(heightRows, grid.heights);
			return grid;
		}

		int GetStageLayoutIndex(const StageLayoutGrid& grid, int row, int column)
		{
			return (row * grid.columnCount) + column;
		}

		int ClampHeightLevel(int heightLevel)
		{
			if (heightLevel < 0) return 0;
			if (heightLevel > kHeightGridMaxLevel) return kHeightGridMaxLevel;
			return heightLevel;
		}

		Vec3 GetStageLayoutCellPosition(const StageLayoutGrid& grid, int row, int column, float y)
		{
			const float centeredColumn = (static_cast<float>(grid.columnCount - 1) * 0.5f) - static_cast<float>(column);
			const float centeredRow = static_cast<float>(row) - (static_cast<float>(grid.rowCount - 1) * 0.5f);
			return Vec3(
				kStageLayoutOrigin.x + (centeredColumn * kStageLayoutCellSize),
				kStageLayoutOrigin.y + y,
				kStageLayoutOrigin.z + (centeredRow * kStageLayoutCellSize));
		}

		std::vector<FixedStageObjectPlacement> BuildHeightVariationPlacements()
		{
			std::vector<FixedStageObjectPlacement> placements;
			const StageLayoutGrid grid = LoadStageLayoutGrid();
			const float blockOccupancy = 3.1f;
			const float slopeOccupancy = 3.6f;

			for (int row = 0; row < grid.rowCount; ++row)
			{
				for (int column = 0; column < grid.columnCount; ++column)
				{
					const int index = GetStageLayoutIndex(grid, row, column);
					const int objectCode = grid.objects[index];
					const int heightLevel = ClampHeightLevel(grid.heights[index]);
					if (objectCode == kStageLayoutObjectEmpty)
					{
						continue;
					}

					if (objectCode == kStageLayoutObjectBlock)
					{
						AddBlockStackPlacements(placements, grid, row, column, heightLevel, blockOccupancy);
						continue;
					}

					int rowDelta = 0;
					int columnDelta = 0;
					if (!GetSlopeDirectionDelta(objectCode, rowDelta, columnDelta))
					{
						continue;
					}

					AddBlockStackPlacements(placements, grid, row, column, heightLevel, blockOccupancy);
					const float slopeY = static_cast<float>(heightLevel) * kStageLayoutHeightStep;
					const float collisionRotation = DirectionToYRotation(rowDelta, columnDelta);
					const bool isHorizontalSlope = (columnDelta != 0);
					const float modelRotation = isHorizontalSlope ? collisionRotation + XM_PI : collisionRotation;
					placements.push_back({
						kHeightGridSlopeModel,
						GetStageLayoutCellPosition(grid, row, column, slopeY),
						modelRotation,
						collisionRotation,
						1.0f,
						slopeOccupancy });
				}
			}

			return placements;
		}

		std::vector<const StageObjectDef*> GetTreePlacementDefs()
		{
			const std::wstring preferredNames[] =
			{
				L"tree_default",
				L"tree_pinedefaulta",
			};

			auto trees = StageObjectCatalog::GetByCategory(StageObjectCategory::Tree);
			std::vector<const StageObjectDef*> result;
			for (const auto& preferredName : preferredNames)
			{
				for (const auto* tree : trees)
				{
					if (tree && tree->name == preferredName)
					{
						result.push_back(tree);
						break;
					}
				}
			}

			if (result.size() >= 2 || trees.empty())
			{
				return result;
			}

			for (const auto* tree : trees)
			{
				if (!tree || (!result.empty() && tree == result.front()))
				{
					continue;
				}
				result.push_back(tree);
				if (result.size() >= 2)
				{
					break;
				}
			}
			return result;
		}

	}
}
