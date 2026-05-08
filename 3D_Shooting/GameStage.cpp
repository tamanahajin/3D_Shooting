/*!
@file GameStage.cpp
@brief ゲームステージクラス　実体
*/

#include "stdafx.h"
#include "Project.h"
#include <random>
#include <map>

namespace shooting {

	namespace
	{
		// 配置済みオブジェクトの占有範囲。XZ平面の円として扱い、ランダム配置の貫通回避に使う。
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

		const int kStageLayoutObjectEmpty = 0;
		const int kStageLayoutObjectBlock = 1;
		const int kStageLayoutObjectSlopeUp = 2;
		const int kStageLayoutObjectSlopeDown = 3;
		const int kStageLayoutObjectSlopeLeft = 4;
		const int kStageLayoutObjectSlopeRight = 5;
		const int kHeightGridMaxLevel = 3;
		const int kDefaultStageLayoutRows = 13;
		const int kDefaultStageLayoutColumns = 13;
		const float kStageLayoutCellSize = 5.0f;
		const float kStageLayoutHeightStep = 5.0f;
		const Vec3 kStageLayoutOrigin(0.0f, 0.0f, 0.0f);
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

		const bool kDrawSlopeCollisionDebug = false;
		const float kSlopeCollisionStartCenterZOffset = -2.5f;

		// FBXごとの基準スケールに配置倍率を掛け、スケール→回転→移動の順でワールド行列を作る。
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


		// 1個ずつGameObject化せず、同じモデル名の配置行列だけを蓄積する。
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


		// 蓄積した配置行列をモデル単位のインスタンシング描画オブジェクトとして実体化する。
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


		// 坂コリジョンの調整用表示。衝突判定は持たず、坂と同じ角度の薄い板だけを描く。
		void AddSlopeCollisionDebugBox(
			GameStage& stage,
			const Vec3& startCenter,
			const Vec3& direction,
			float width,
			float length,
			float height)
		{
			if (!kDrawSlopeCollisionDebug)
			{
				return;
			}

			Vec3 normalizedDirection(direction.x, 0.0f, direction.z);
			if (normalizedDirection.length() <= 0.0001f)
			{
				return;
			}
			normalizedDirection.normalize();

			Vec3 slopeAxis(normalizedDirection.x * length, height, normalizedDirection.z * length);
			slopeAxis.normalize();

			TransParam debugParam;
			debugParam.scale = Vec3(width, 0.08f, std::sqrt((length * length) + (height * height)));
			debugParam.quaternion = bsmUtil::MakeFromToQuat(Vec3(0.0f, 0.0f, 1.0f), slopeAxis);
			debugParam.position = startCenter + (normalizedDirection * (length * 0.5f)) + Vec3(0.0f, (height * 0.5f) + 0.04f, 0.0f);
			stage.AddGameObject<SlopeCollisionDebugBox>(debugParam);
		}


		void AddFixedCollisionBox(
			GameStage& stage,
			const Vec3& position,
			const Vec3& size)
		{
			TransParam param;
			param.scale = Vec3(1.0f, 1.0f, 1.0f);
			param.quaternion = Quat();
			param.position = position;
			stage.AddGameObject<StageCollisionBox>(param, size);
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


		std::string NarrowPath(const std::wstring& path)
		{
			if (path.empty())
			{
				return std::string();
			}

			const int requiredSize = WideCharToMultiByte(
				CP_ACP,
				0,
				path.c_str(),
				-1,
				nullptr,
				0,
				nullptr,
				nullptr);
			if (requiredSize <= 1)
			{
				return std::string();
			}

			std::string result(static_cast<size_t>(requiredSize - 1), '\0');
			WideCharToMultiByte(
				CP_ACP,
				0,
				path.c_str(),
				-1,
				&result[0],
				requiredSize,
				nullptr,
				nullptr);
			return result;
		}

		std::vector<std::vector<int>> LoadCsvGrid(const std::wstring& relativePath)
		{
			std::vector<std::vector<int>> rows;
			std::ifstream file(NarrowPath(App::GetRelativeAssetsDir() + relativePath));
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

		bool IsSlopeObjectCode(int objectCode)
		{
			return objectCode >= kStageLayoutObjectSlopeUp && objectCode <= kStageLayoutObjectSlopeRight;
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

		Vec3 GetStageLayoutCellPosition(const StageLayoutGrid& grid, int row, int column, float y)
		{
			const float centeredColumn = (static_cast<float>(grid.columnCount - 1) * 0.5f) - static_cast<float>(column);
			const float centeredRow = static_cast<float>(row) - (static_cast<float>(grid.rowCount - 1) * 0.5f);
			return Vec3(
				kStageLayoutOrigin.x + (centeredColumn * kStageLayoutCellSize),
				kStageLayoutOrigin.y + y,
				kStageLayoutOrigin.z + (centeredRow * kStageLayoutCellSize));
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

		void AddHeightGridColumnCollisions(GameStage& stage)
		{
			const StageLayoutGrid grid = LoadStageLayoutGrid();
			const float collisionTopClearance = 0.6f;
			const float collisionBottomExtension = 0.1f;
			const float collisionInset = 0.05f;

			for (int row = 0; row < grid.rowCount; ++row)
			{
				for (int column = 0; column < grid.columnCount; ++column)
				{
					const int index = GetStageLayoutIndex(grid, row, column);
					if (grid.objects[index] == kStageLayoutObjectEmpty)
					{
						continue;
					}

					const int heightLevel = ClampHeightLevel(grid.heights[index]);
					if (heightLevel <= 0)
					{
						continue;
					}

					const float bottomY = kStageLayoutOrigin.y - collisionBottomExtension;
					const float topY = kStageLayoutOrigin.y + (static_cast<float>(heightLevel) * kStageLayoutHeightStep) - collisionTopClearance;
					const float height = topY - bottomY;
					if (height <= 0.05f)
					{
						continue;
					}

					TransParam collisionParam;
					collisionParam.scale = Vec3(1.0f, 1.0f, 1.0f);
					collisionParam.quaternion = Quat();
					collisionParam.position = GetStageLayoutCellPosition(grid, row, column, 0.0f);
					collisionParam.position.y = bottomY + (height * 0.5f);

					const float cellCollisionSize = kStageLayoutCellSize - collisionInset;
					stage.AddGameObject<StageCollisionBox>(
						collisionParam,
						Vec3(cellCollisionSize, height, cellCollisionSize));
				}
			}
		}

		Quat MakeSlopeSideCollisionRotation(
			const Vec3& normalizedDirection,
			float length,
			float height)
		{
			Vec3 xAxis(normalizedDirection.z, 0.0f, -normalizedDirection.x);
			xAxis.normalize();

			Vec3 zAxis(
				normalizedDirection.x * length,
				height,
				normalizedDirection.z * length);
			zAxis.normalize();

			Vec3 yAxis = bsmUtil::cross(zAxis, xAxis);
			yAxis.normalize();

			Mat4x4 rotMat;
			rotMat.identity();
			rotMat._11 = xAxis.x;
			rotMat._12 = xAxis.y;
			rotMat._13 = xAxis.z;
			rotMat._21 = yAxis.x;
			rotMat._22 = yAxis.y;
			rotMat._23 = yAxis.z;
			rotMat._31 = zAxis.x;
			rotMat._32 = zAxis.y;
			rotMat._33 = zAxis.z;

			Quat rotation = rotMat.quatInMatrix();
			rotation.normalize();
			return rotation;
		}

		void AddSlopeSideCollisionBoxes(
			GameStage& stage,
			const Vec3& startCenter,
			const Vec3& direction,
			float width,
			float length,
			float height)
		{
			Vec3 normalizedDirection(direction.x, 0.0f, direction.z);
			if (normalizedDirection.length() <= 0.0001f)
			{
				return;
			}
			normalizedDirection.normalize();

			const float sideThickness = 0.45f;
			const float sideOffset = (width * 0.5f) - 1.0f;
			const Vec3 right(normalizedDirection.z, 0.0f, -normalizedDirection.x);
			const float sideSlopeLength = std::sqrt((length * length) + (height * height));
			const Quat sideRotation = MakeSlopeSideCollisionRotation(normalizedDirection, length, height);
			const Vec3 sideLocalOffset(0.2f, height * 0.1f, -1.6f);
			const Vec3 modelRight(-normalizedDirection.z, 0.0f, normalizedDirection.x);
			const Vec3 modelForward(-normalizedDirection.x, 0.0f, -normalizedDirection.z);
			const Vec3 sideWorldOffset =
				(modelRight * sideLocalOffset.x) +
				Vec3(0.0f, sideLocalOffset.y, 0.0f) +
				(modelForward * sideLocalOffset.z);

			for (float sideSign : { -1.0f, 1.0f })
			{
				TransParam sideParam;
				sideParam.scale = Vec3(1.0f, 1.0f, 1.0f);
				sideParam.quaternion = sideRotation;
				sideParam.position = startCenter +
					(normalizedDirection * (length * 0.5f)) +
					(right * (sideOffset * sideSign)) +
					sideWorldOffset;

				stage.AddGameObject<StageCollisionBox>(
					sideParam,
					Vec3(sideThickness, height, sideSlopeLength));
			}
		}

		void AddHeightVariationPlatformSurface(
			GameStage& stage,
			const FixedStageObjectPlacement& placement)
		{
			const auto* blockDef = FindStageObjectDefByName(placement.modelName);
			if (!blockDef)
			{
				return;
			}

			const float fallbackSize = 5.0f;
			const float surfaceWidth = (blockDef ? (10.0f * blockDef->scale.x) : fallbackSize) * placement.scaleMultiplier;
			const float surfaceLength = (blockDef ? (10.0f * blockDef->scale.z) : fallbackSize) * placement.scaleMultiplier;
			const float surfaceHeight = placement.position.y + ((blockDef ? (10.0f * blockDef->scale.y) : fallbackSize) * placement.scaleMultiplier);
			stage.AddPlatformGroundSurface(
				placement.position,
				placement.yRotation,
				surfaceWidth,
				surfaceLength,
				surfaceHeight);
		}


		// 坂コリジョンはスロープモデルの配置を基準に登録する。左右には薄い壁を追加して横抜けを防ぐ。
		void AddHeightVariationCollisionGroup(
			GameStage& stage,
			const FixedStageObjectPlacement& placement)
		{
			const auto* slopeDef = FindStageObjectDefByName(placement.modelName);
			const float fallbackSize = 5.0f;
			const float slopeWidth = (slopeDef ? (10.0f * slopeDef->scale.x) : fallbackSize) * 1.25f * placement.scaleMultiplier;
			const float slopeLength = (slopeDef ? (10.0f * slopeDef->scale.z) : fallbackSize) * placement.scaleMultiplier;
			const float slopeHeight = (slopeDef ? (10.0f * slopeDef->scale.y) : fallbackSize) * placement.scaleMultiplier;
			const Vec3 slopeDirection = MakeSlopeDirection(placement.collisionYRotation);
			const Vec3 slopeStartCenter = placement.position + (slopeDirection * kSlopeCollisionStartCenterZOffset);

			stage.AddSlopeCollision(
				slopeStartCenter,
				slopeDirection,
				slopeWidth,
				slopeLength,
				slopeHeight);

			AddSlopeSideCollisionBoxes(
				stage,
				slopeStartCenter,
				slopeDirection,
				slopeWidth,
				slopeLength,
				slopeHeight);
		}

		void AddHeightVariationOccupancy(std::vector<PlacementCircle>& occupied)
		{
			const auto placements = BuildHeightVariationPlacements();
			for (const auto& placement : placements)
			{
				occupied.push_back({ placement.position, placement.occupancyRadius });
			}
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
				const Vec3 position(posDist(gen), 0.0f, posDist(gen));
				const float radius = def->placementRadius * scaleMultiplier + extraClearance;
				if (!IsPlacementFree(position, radius, occupied))
				{
					continue;
				}

				AddStageObjectInstance(batches, *def, position, rotDist(gen), scaleMultiplier);
				occupied.push_back({ position, radius });
				++placed;
			}
		}


		// 外周用フォルダを優先し、無ければ旧Cliffカテゴリから近いモデルを選ぶ互換用の探索。
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
	}

	// 坂をOBBとして解かず、開始位置・方向・幅・長さ・高さだけの軽量データとして保持する。
	void GameStage::AddSlopeCollision(
		const Vec3& startCenter,
		const Vec3& direction,
		float width,
		float length,
		float height)
	{
		if (width <= 0.0f || length <= 0.0f || height <= 0.0f)
		{
			return;
		}

		Vec3 normalizedDirection(direction.x, 0.0f, direction.z);
		if (normalizedDirection.length() <= 0.0001f)
		{
			return;
		}
		normalizedDirection.normalize();

		SlopeCollisionEntry entry;
		entry.startCenter = startCenter;
		entry.direction = normalizedDirection;
		entry.width = width - 1.8f;
		entry.length = length;
		entry.height = height;
		m_slopeCollisions.push_back(entry);
		AddSlopeCollisionDebugBox(*this, entry.startCenter, entry.direction, entry.width, entry.length, entry.height);
	}


	void GameStage::AddPlatformGroundSurface(
		const Vec3& center,
		float yRotation,
		float width,
		float length,
		float height)
	{
		if (width <= 0.0f || length <= 0.0f)
		{
			return;
		}

		PlatformSurfaceEntry entry;
		entry.center = center;
		entry.direction = MakeSlopeDirection(yRotation);
		entry.width = width;
		entry.length = length;
		entry.height = height;
		m_platformSurfaces.push_back(entry);
	}


	// 位置を坂のローカル軸へ射影し、坂範囲内なら進行率から地面Yを線形補間する。
	bool GameStage::TryGetSlopeGroundHeight(const Vec3& position, float& outHeight) const
	{
		bool found = false;
		float bestHeight = -100000.0f;

		for (const auto& slope : m_slopeCollisions)
		{
			// direction方向が坂の前後、right方向が坂の横幅。内積で坂内の相対位置を求める。
			const Vec3 right(slope.direction.z, 0.0f, -slope.direction.x);
			const Vec3 toPosition(
				position.x - slope.startCenter.x,
				0.0f,
				position.z - slope.startCenter.z);
			const float along = bsmUtil::dot(toPosition, slope.direction);
			const float side = bsmUtil::dot(toPosition, right);
			const float alongPadding = 0.65f;
			const float sidePadding = 0.2f;

			if (along < -alongPadding || along > slope.length + alongPadding)
			{
				continue;
			}
			if (std::fabs(side) > (slope.width * 0.5f) + sidePadding)
			{
				continue;
			}

			float t = along / slope.length;
			if (t > 1.0f) t = 1.0f;

			// 坂を連結した時に2枚目の低い端が急な段差にならないよう、手前側だけ傾きを延長する。
			float height = slope.startCenter.y + (slope.height * t);
			if (height < 0.0f) height = 0.0f;
			if (!found || height > bestHeight)
			{
				bestHeight = height;
				found = true;
			}
		}

		for (const auto& surface : m_platformSurfaces)
		{
			const Vec3 right(surface.direction.z, 0.0f, -surface.direction.x);
			const Vec3 toPosition(
				position.x - surface.center.x,
				0.0f,
				position.z - surface.center.z);
			const float along = bsmUtil::dot(toPosition, surface.direction);
			const float side = bsmUtil::dot(toPosition, right);
			const float surfacePadding = 0.35f;

			if (std::fabs(along) > (surface.length * 0.5f) + surfacePadding)
			{
				continue;
			}
			if (std::fabs(side) > (surface.width * 0.5f) + surfacePadding)
			{
				continue;
			}

			if (!found || surface.height > bestHeight)
			{
				bestHeight = surface.height;
				found = true;
			}
		}

		if (found)
		{
			outHeight = bestHeight;
		}
		return found;
	}


	// ダメージ表示は短命なUIデータだけを保持し、描画はUIManager側でまとめて行う。
	void GameStage::SpawnDamageNumber(const Vec3& position, int damage)
	{
		if (damage <= 0)
		{
			return;
		}

		DamageNumberEntry entry;
		entry.text = std::to_wstring(damage);
		entry.position = position;
		entry.life = 0.9;

		const int offsetIndex = static_cast<int>(m_damageNumbers.size() % 3) - 1;
		entry.velocity = Vec3(static_cast<float>(offsetIndex) * 0.08f, 0.75f, 0.0f);
		m_damageNumbers.push_back(entry);

		const size_t maxDamageNumbers = 64;
		if (m_damageNumbers.size() > maxDamageNumbers)
		{
			m_damageNumbers.erase(m_damageNumbers.begin());
		}
	}

	void GameStage::OnUpdate2(double elapsedTime)
	{
		const float dt = static_cast<float>(elapsedTime);
		for (auto& damageNumber : m_damageNumbers)
		{
			damageNumber.age += elapsedTime;
			damageNumber.position.x += damageNumber.velocity.x * dt;
			damageNumber.position.y += damageNumber.velocity.y * dt;
			damageNumber.position.z += damageNumber.velocity.z * dt;
			damageNumber.velocity.y *= 0.96f;
		}

		m_damageNumbers.erase(
			std::remove_if(
				m_damageNumbers.begin(),
				m_damageNumbers.end(),
				[](const DamageNumberEntry& damageNumber)
				{
					return damageNumber.age >= damageNumber.life;
				}),
			m_damageNumbers.end());
	}
	//--------------------------------------------------------------------------------------
	// ゲームステージ
	//--------------------------------------------------------------------------------------


	// floor.fbxを1枚だけ敷き、XZ方向に拡大してステージ全体を覆う。
	void GameStage::CreateGround()
	{
		const int half = 32;
		const float groundSize = static_cast<float>((half * 2) + 1);

		Mat4x4 scaleMat;
		scaleMat.identity();
		scaleMat.scale(Vec3(groundSize * 0.1f, 0.1f, groundSize * 0.1f));

		Mat4x4 transMat;
		transMat.identity();
		transMat.translation(Vec3(0.0f, 0.0f, 0.0f));

		Mat4x4 world = scaleMat;
		world *= transMat;

		std::vector<Mat4x4> floorWorlds;
		floorWorlds.push_back(world);

		AddGameObject<FloorInstancedRenderer>(
			L"FLOOR_MODEL",
			L"FLOOR_MAT_",
			floorWorlds);

		TransParam colParam;
		colParam.scale = Vec3(1.0f, 1.0f, 1.0f);
		colParam.quaternion = Quat();
		colParam.position = Vec3(0.0f, -0.05f, 0.0f);
		AddGameObject<FloorCollision>(
			colParam,
			Vec3(groundSize, 0.05f, groundSize));
	}


	// 外周は見た目の崖モデルと、ゲームプレイ用の大きい透明コリジョンを別々に作る。
	void GameStage::CreateWalls()
	{
		const auto* cliff = FindOuterCliffDef();
		if (!cliff)
		{
			return;
		}

		std::map<std::wstring, StageObjectBatch> batches;

		const float groundHalf = 32.0f;
		const float edge = groundHalf + 1.5f;
		const float outerCliffScale = 1.0f;
		const float outerSideLength = (groundHalf / 4.0f) - 1.5f;

		auto& batch = batches[cliff->key];
		batch.meshKey = cliff->key;
		batch.materialPrefix = cliff->materialPrefix;

		// 外周崖は4辺それぞれ1枚の長いインスタンスにする。
		const Vec3 sideScale(outerSideLength, outerCliffScale, outerCliffScale);
		batch.worlds.push_back(MakeStageObjectWorld(*cliff, Vec3(0.0f, 0.0f, -edge), 0.0f, sideScale));
		batch.worlds.push_back(MakeStageObjectWorld(*cliff, Vec3(0.0f, 0.0f, edge), XM_PI, sideScale));
		batch.worlds.push_back(MakeStageObjectWorld(*cliff, Vec3(-edge, 0.0f, 0.0f), XM_PIDIV2, sideScale));
		batch.worlds.push_back(MakeStageObjectWorld(*cliff, Vec3(edge, 0.0f, 0.0f), -XM_PIDIV2, sideScale));

		FlushStageObjectBatches(*this, batches);

		const float wallHalf = edge - 0.3f;
		const float wallLength = wallHalf * 2.0f;
		const float wallThickness = 1.5f;
		const float wallHeight = 36.0f;
		const float wallY = 2.0f;

		TransParam wallParam;
		wallParam.scale = Vec3(1.0f, 1.0f, 1.0f);
		wallParam.quaternion = Quat();

		wallParam.position = Vec3(0.0f, wallY, -wallHalf);
		AddGameObject<StageCollisionBox>(wallParam, Vec3(wallLength, wallHeight, wallThickness));

		wallParam.position = Vec3(0.0f, wallY, wallHalf);
		AddGameObject<StageCollisionBox>(wallParam, Vec3(wallLength, wallHeight, wallThickness));

		wallParam.position = Vec3(-wallHalf, wallY, 0.0f);
		AddGameObject<StageCollisionBox>(wallParam, Vec3(wallThickness, wallHeight, wallLength));

		wallParam.position = Vec3(wallHalf, wallY, 0.0f);
		AddGameObject<StageCollisionBox>(wallParam, Vec3(wallThickness, wallHeight, wallLength));
	}


	// 高低差オブジェクトは、見た目のインスタンス配置と坂コリジョン登録を分けて管理する。
	void GameStage::CreateHeightVariationObjects()
	{
		m_slopeCollisions.clear();
		m_platformSurfaces.clear();

		std::map<std::wstring, StageObjectBatch> batches;
		const auto placements = BuildHeightVariationPlacements();
		for (const auto& placement : placements)
		{
			const auto* def = FindStageObjectDefByName(placement.modelName);
			if (!def)
			{
				continue;
			}

			AddStageObjectInstance(
				batches,
				*def,
				placement.position,
				placement.yRotation,
				placement.scaleMultiplier);
		}

		FlushStageObjectBatches(*this, batches);

		for (const auto& placement : placements)
		{
			if (IsSlopePlacement(placement))
			{
				AddHeightVariationCollisionGroup(*this, placement);
			}
			else
			{
				AddHeightVariationPlatformSurface(*this, placement);
			}
		}

		AddHeightGridColumnCollisions(*this);
	}


	// 木・岩などの自然物をまばらに配置する。固定seedなので毎回同じステージになる。
	void GameStage::CreateCoverObjects()
	{
		std::map<std::wstring, StageObjectBatch> batches;
		std::vector<PlacementCircle> occupied;

		// プレイヤー初期位置、アイテム、高低差周辺を先に予約し、自然物が重ならないようにする。
		occupied.push_back({ Vec3(0.0f, 0.0f, 0.0f), 5.0f });
		occupied.push_back({ Vec3(2.5f, 0.0f, 2.0f), 1.7f });
		occupied.push_back({ Vec3(-4.0f, 0.0f, 3.5f), 1.7f });
		occupied.push_back({ Vec3(6.0f, 0.0f, -2.5f), 1.7f });
		occupied.push_back({ Vec3(-7.5f, 0.0f, -5.0f), 1.7f });
		occupied.push_back({ Vec3(10.0f, 0.0f, 4.0f), 1.7f });
		AddHeightVariationOccupancy(occupied);

		std::mt19937 gen(20260506);

		//PlaceScatteredObjects(
		//	batches,
		//	occupied,
		//	GetTreePlacementDefs(),
		//	24,
		//	27.0f,
		//	0.90f,
		//	1.15f,
		//	0.35f,
		//	gen);

		//PlaceScatteredObjects(
		//	batches,
		//	occupied,
		//	MergeDefs({ StageObjectCategory::Rock, StageObjectCategory::Stone }),
		//	22,
		//	28.0f,
		//	0.85f,
		//	1.20f,
		//	0.25f,
		//	gen);

		//PlaceScatteredObjects(
		//	batches,
		//	occupied,
		//	StageObjectCatalog::GetByCategory(StageObjectCategory::Plant),
		//	18,
		//	29.0f,
		//	0.80f,
		//	1.15f,
		//	0.20f,
		//	gen);

		//PlaceScatteredObjects(
		//	batches,
		//	occupied,
		//	StageObjectCatalog::GetByCategory(StageObjectCategory::Mushroom),
		//	8,
		//	29.0f,
		//	0.75f,
		//	1.10f,
		//	0.20f,
		//	gen);

		//PlaceScatteredObjects(
		//	batches,
		//	occupied,
		//	StageObjectCatalog::GetByCategory(StageObjectCategory::Log),
		//	6,
		//	27.5f,
		//	0.85f,
		//	1.15f,
		//	0.35f,
		//	gen);

		FlushStageObjectBatches(*this, batches);
	}
	void GameStage::CreateItems()
	{
		const Vec3 positions[] =
		{
			Vec3(2.5f, 0.35f, 2.0f),
			Vec3(-4.0f, 0.35f, 3.5f),
			Vec3(6.0f, 0.35f, -2.5f),
			Vec3(-7.5f, 0.35f, -5.0f),
			Vec3(10.0f, 0.35f, 4.0f),
		};

		for (const auto& position : positions)
		{
			TransParam itemParam;
			itemParam.scale = Vec3(0.22f, 0.22f, 0.22f);
			itemParam.quaternion = Quat();
			itemParam.position = position;
			AddGameObject<HpRecoveryItem>(itemParam);
		}
	}

	//追いかけるオブジェクトの作成
	void GameStage::CreateSeekObject()
	{
		auto controllerObject = GetSharedGameObject(L"EnemyBatchController", false);
		auto controller = std::dynamic_pointer_cast<EnemyBatchController>(controllerObject);
		if (!controller)
		{
			return;
		}

		// 生成する敵の数
		const size_t enemyCount = 40;
		m_totalEnemyCount = static_cast<int>(enemyCount);
		
		// ランダム配置のパラメータ
		const float minDistance = 5.0f;   // 最小距離
		const float maxDistance = 20.0f;  // 最大距離
		const float yPosition = 0.525f;   // Y座標（地面の高さ）
		
		std::vector<Vec3> positions;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> distRadius(minDistance, maxDistance);
		std::uniform_real_distribution<float> distAngle(0.0f, XM_2PI);
		
		for (size_t count = 0; count < enemyCount; count++)
		{
			Vec3 position;
			bool validPosition = false;
			int attempts = 0;
			const int maxAttempts = 50;
			
			while (!validPosition && attempts < maxAttempts)
			{
				// 極座標でランダムな位置を生成
				float radius = distRadius(gen);
				float angle = distAngle(gen);
				
				position = Vec3(
					radius * cosf(angle),
					yPosition,
					radius * sinf(angle)
				);
				
				// 他のオブジェクトとの最小距離をチェック
				validPosition = true;
				for (const auto& existingPos : positions)
				{
					float dist = (position - existingPos).length();
					if (dist < minDistance * 0.5f)
					{
						validPosition = false;
						break;
					}
				}
				
				attempts++;
			}
			
			positions.push_back(position);
		}
		
		// 配置オブジェクトの作成
		for (const auto& pos : positions)
		{
			controller->AddEnemy(pos);
		}
	}


	int GameStage::GetAliveEnemyCount() const
	{
		auto controllerObject = GetSharedGameObject(L"EnemyBatchController", false);
		auto controller = std::dynamic_pointer_cast<EnemyBatchController>(controllerObject);
		if (controller)
		{
			return controller->GetAliveEnemyCount();
		}
		return 0;
	}

	int GameStage::GetDefeatedEnemyCount() const
	{
		const int defeated = m_totalEnemyCount - GetAliveEnemyCount();
		return (defeated > 0) ? defeated : 0;
	}

	void GameStage::OnCreate()
	{
		//カメラとライトの設定
		m_camera = ObjectFactory::Create<MainCamera>(GetThis<Stage>());
		//m_camera = ObjectFactory::Create<MainCamera>();
		m_camera->SetEye(Vec3(0, 3.43f, -6.37f));
		m_camera->SetAt(Vec3(0, 0.125f, 0));
		m_lightSet = ObjectFactory::Create<LightSet>();
		TransParam param;
		//param.scale = Vec3(1.0f, 1.0f, 1.0f);
		//auto quat = XMQuaternionIdentity();
		//param.quaternion = Quat(quat);
		//param.position = Vec3(5.0f, 2.0f, 3.0f);
		//AddGameObject<WallBox>(param);
		// 地面
		CreateGround();
		// 壁
		CreateWalls();
		// スロープと高台
		CreateHeightVariationObjects();
		// 配置物
		//CreateCoverObjects();
		// アイテム
		//CreateItems();
		// 空
		AddGameObject<SkyDome>();

		

		//param.scale = Vec3(5.0f, 1.0f, 5.0f);
		//param.position = Vec3(10.0f, 0.0, 10.0f);
		//AddGameObject<FixedBox>(param);

		//param.position = Vec3(10.0f, 0.0, 10.0f);
		//param.quaternion = Quat(Vec3(-1, 0, 1), XM_PIDIV4);
		//AddGameObject<FixedBox>(param);

		//param.position = Vec3(-10.0f, 0.0, 10.0f);
		//param.quaternion = Quat(Vec3(0, 1, 1), XM_PIDIV4);
		//AddGameObject<FixedBox>(param);


		param.scale = Vec3(0.3f, 0.3f, 0.3f);
		param.quaternion = Quat();
		param.position = Vec3(0.0f, 0.525f, 0.0f);
		auto player = AddGameObject<Player>(param);
		AddGameObject<PlayerWeapon>(player);

		//AddGameObject<EnemyBatchController>();
		//CreateSeekObject();
		//AddGameObject<EnemyInstancedRenderer>();

		// 弾管理
		AddGameObject<BulletManager>();
	}


}




