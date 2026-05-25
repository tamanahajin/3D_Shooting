/*!
@file GameStageEnvironment.cpp
@brief GameStageのステージ生成処理
*/

#include "stdafx.h"
#include "Project.h"
#include <algorithm>
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

		// 自然物の岩/石だけを一時停止するためのフラグ。戻す場合はtrueにする。
		const bool kEnableScatteredRockObjects = false;
		const float kSlopeCollisionStartCenterZOffset = -2.5f;
		const int kRecoveryItemTargetCount = 3;
		const int kItemMaxSpawnAttempts = 240;
		const float kItemSpawnHalf = 24.0f;
		const float kItemSpawnRadius = 1.25f;
		const float kItemGroundOffset = 0.35f;
		const float kHpRecoveryItemScale = 0.08f;
		const float kHpRecoveryItemDepthScale = 0.04f;
		const int kBombItemTargetCount = 2;
		const int kBombItemGrantCount = 5;
		const float kBombItemScale = 0.018f;

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
				// 高台の天面には自然物を置きたいので、坂だけを予約して斜面上の配置を避ける。
				if (!IsSlopePlacement(placement))
				{
					continue;
				}
				occupied.push_back({ placement.position, placement.occupancyRadius });
			}
		}

		void AddHeightVariationItemSpawnBlockers(GameStage& stage)
		{
			const auto placements = BuildHeightVariationPlacements();
			for (const auto& placement : placements)
			{
				// アイテムは高台の上には置けるようにし、斜面上だけを避ける。
				if (!IsSlopePlacement(placement))
				{
					continue;
				}
				stage.AddItemSpawnBlocker(placement.position, placement.occupancyRadius);
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
				stage.AddItemSpawnBlocker(position, radius);
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


	void GameStage::ClearGroundLookup()
	{
		m_groundLookupCells.clear();
	}


	int GameStage::GetGroundLookupCoord(float value) const
	{
		const float cellSize = (m_groundLookupCellSize > 0.001f) ? m_groundLookupCellSize : 1.0f;
		return static_cast<int>(floorf(value / cellSize));
	}


	long long GameStage::MakeGroundLookupKey(int x, int z) const
	{
		const unsigned long long ux = static_cast<unsigned int>(x);
		const unsigned long long uz = static_cast<unsigned int>(z);
		return static_cast<long long>((ux << 32) ^ uz);
	}


	void GameStage::AddGroundLookupRange(
		float minX,
		float maxX,
		float minZ,
		float maxZ,
		size_t index,
		bool isSlope)
	{
		if (minX > maxX)
		{
			std::swap(minX, maxX);
		}
		if (minZ > maxZ)
		{
			std::swap(minZ, maxZ);
		}

		const int minCellX = GetGroundLookupCoord(minX);
		const int maxCellX = GetGroundLookupCoord(maxX);
		const int minCellZ = GetGroundLookupCoord(minZ);
		const int maxCellZ = GetGroundLookupCoord(maxZ);

		for (int z = minCellZ; z <= maxCellZ; ++z)
		{
			for (int x = minCellX; x <= maxCellX; ++x)
			{
				auto& cell = m_groundLookupCells[MakeGroundLookupKey(x, z)];
				auto& indices = isSlope ? cell.slopeIndices : cell.platformIndices;
				if (std::find(indices.begin(), indices.end(), index) == indices.end())
				{
					indices.push_back(index);
				}
			}
		}
	}


	void GameStage::AddSlopeToGroundLookup(size_t index)
	{
		if (index >= m_slopeCollisions.size())
		{
			return;
		}

		const auto& slope = m_slopeCollisions[index];
		const Vec3 right(slope.direction.z, 0.0f, -slope.direction.x);
		const float alongPadding = 0.65f;
		const float sidePadding = 0.2f;
		const float slopeHalfWidth = slope.width * 0.5f;
		const float halfWidth = ((slopeHalfWidth > 0.0f) ? slopeHalfWidth : 0.0f) + sidePadding;
		const Vec3 start = slope.startCenter - (slope.direction * alongPadding);
		const Vec3 end = slope.startCenter + (slope.direction * (slope.length + alongPadding));

		const Vec3 points[] =
		{
			start + (right * halfWidth),
			start - (right * halfWidth),
			end + (right * halfWidth),
			end - (right * halfWidth),
		};

		float minX = points[0].x;
		float maxX = points[0].x;
		float minZ = points[0].z;
		float maxZ = points[0].z;
		for (const auto& point : points)
		{
			if (point.x < minX) minX = point.x;
			if (point.x > maxX) maxX = point.x;
			if (point.z < minZ) minZ = point.z;
			if (point.z > maxZ) maxZ = point.z;
		}

		AddGroundLookupRange(minX, maxX, minZ, maxZ, index, true);
	}


	void GameStage::AddPlatformToGroundLookup(size_t index)
	{
		if (index >= m_platformSurfaces.size())
		{
			return;
		}

		const auto& surface = m_platformSurfaces[index];
		const Vec3 right(surface.direction.z, 0.0f, -surface.direction.x);
		const float surfacePadding = 0.35f;
		const float halfLength = (surface.length * 0.5f) + surfacePadding;
		const float halfWidth = (surface.width * 0.5f) + surfacePadding;

		const Vec3 points[] =
		{
			surface.center + (surface.direction * halfLength) + (right * halfWidth),
			surface.center + (surface.direction * halfLength) - (right * halfWidth),
			surface.center - (surface.direction * halfLength) + (right * halfWidth),
			surface.center - (surface.direction * halfLength) - (right * halfWidth),
		};

		float minX = points[0].x;
		float maxX = points[0].x;
		float minZ = points[0].z;
		float maxZ = points[0].z;
		for (const auto& point : points)
		{
			if (point.x < minX) minX = point.x;
			if (point.x > maxX) maxX = point.x;
			if (point.z < minZ) minZ = point.z;
			if (point.z > maxZ) maxZ = point.z;
		}

		AddGroundLookupRange(minX, maxX, minZ, maxZ, index, false);
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
		AddSlopeToGroundLookup(m_slopeCollisions.size() - 1);
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
		AddPlatformToGroundLookup(m_platformSurfaces.size() - 1);
	}


	// 位置を坂のローカル軸へ射影し、坂範囲内なら進行率から地面Yを線形補間する。
	bool GameStage::TryGetSlopeGroundHeight(const Vec3& position, float& outHeight) const
	{
		bool found = false;
		float bestHeight = -100000.0f;

		auto evaluateSlope = [&](size_t slopeIndex)
		{
			if (slopeIndex >= m_slopeCollisions.size())
			{
				return;
			}

			const auto& slope = m_slopeCollisions[slopeIndex];
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
				return;
			}
			if (std::fabs(side) > (slope.width * 0.5f) + sidePadding)
			{
				return;
			}

			float t = along / slope.length;
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;

			// 坂端の判定余白内でも、実際の接地高さは坂の端で止める。
			float height = slope.startCenter.y + (slope.height * t);
			if (height < 0.0f) height = 0.0f;
			if (!found || height > bestHeight)
			{
				bestHeight = height;
				found = true;
			}
		};

		// 坂端では高台の天面と判定範囲が重なるため、ここで返さず高い面を最後に選ぶ。
		auto evaluatePlatform = [&](size_t surfaceIndex)
		{
			if (surfaceIndex >= m_platformSurfaces.size())
			{
				return;
			}

			const auto& surface = m_platformSurfaces[surfaceIndex];
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
				return;
			}
			if (std::fabs(side) > (surface.width * 0.5f) + surfacePadding)
			{
				return;
			}

			if (!found || surface.height > bestHeight)
			{
				bestHeight = surface.height;
				found = true;
			}
		};

		if (!m_groundLookupCells.empty())
		{
			const int cellX = GetGroundLookupCoord(position.x);
			const int cellZ = GetGroundLookupCoord(position.z);
			const size_t maxCheckedCount = 128;
			size_t checkedSlopes[128] = {};
			size_t checkedPlatforms[128] = {};
			size_t checkedSlopeCount = 0;
			size_t checkedPlatformCount = 0;

			auto alreadyChecked = [](const size_t* checked, size_t count, size_t index)
			{
				for (size_t i = 0; i < count; ++i)
				{
					if (checked[i] == index)
					{
						return true;
					}
				}
				return false;
			};

			for (int z = -1; z <= 1; ++z)
			{
				for (int x = -1; x <= 1; ++x)
				{
					const auto it = m_groundLookupCells.find(MakeGroundLookupKey(cellX + x, cellZ + z));
					if (it == m_groundLookupCells.end())
					{
						continue;
					}

					for (const auto slopeIndex : it->second.slopeIndices)
					{
						if (alreadyChecked(checkedSlopes, checkedSlopeCount, slopeIndex))
						{
							continue;
						}
						if (checkedSlopeCount < maxCheckedCount)
						{
							checkedSlopes[checkedSlopeCount++] = slopeIndex;
						}
						evaluateSlope(slopeIndex);
					}

					for (const auto platformIndex : it->second.platformIndices)
					{
						if (alreadyChecked(checkedPlatforms, checkedPlatformCount, platformIndex))
						{
							continue;
						}
						if (checkedPlatformCount < maxCheckedCount)
						{
							checkedPlatforms[checkedPlatformCount++] = platformIndex;
						}
						evaluatePlatform(platformIndex);
					}
				}
			}
		}
		else
		{
			for (size_t i = 0; i < m_slopeCollisions.size(); ++i)
			{
				evaluateSlope(i);
			}
			for (size_t i = 0; i < m_platformSurfaces.size(); ++i)
			{
				evaluatePlatform(i);
			}
		}

		if (found)
		{
			outHeight = bestHeight;
		}
		return found;
	}


	bool GameStage::TryRaycastGeneratedGround(
		const Vec3& origin,
		const Vec3& direction,
		float maxDistance,
		Vec3& outPoint,
		Vec3& outNormal,
		float& outDistance) const
	{
		if (maxDistance <= 0.0f)
		{
			return false;
		}

		Vec3 rayDir(direction.x, direction.y, direction.z);
		if (rayDir.length() <= 1e-6f)
		{
			return false;
		}
		rayDir.normalize();

		bool found = false;
		float bestDistance = maxDistance;
		Vec3 bestPoint(0.0f, 0.0f, 0.0f);
		Vec3 bestNormal(0.0f, 1.0f, 0.0f);

		auto tryPlaneHit = [&](const Vec3& planePoint, Vec3 normal, auto&& isInside)
		{
			if (normal.length() <= 1e-6f)
			{
				return;
			}
			normal.normalize();
			if (normal.y < 0.0f)
			{
				normal = normal * -1.0f;
			}

			const float denom = bsmUtil::dot(normal, rayDir);
			if (std::fabs(denom) <= 1e-6f)
			{
				return;
			}

			const float distance = bsmUtil::dot(normal, planePoint - origin) / denom;
			if (distance < 0.0f || distance > bestDistance)
			{
				return;
			}

			const Vec3 hitPoint = origin + (rayDir * distance);
			if (!isInside(hitPoint))
			{
				return;
			}

			found = true;
			bestDistance = distance;
			bestPoint = hitPoint;
			bestNormal = normal;
		};

		for (const auto& slope : m_slopeCollisions)
		{
			const Vec3 right(slope.direction.z, 0.0f, -slope.direction.x);
			const Vec3 ramp = (slope.direction * slope.length) + Vec3(0.0f, slope.height, 0.0f);
			const Vec3 normal = bsmUtil::cross(ramp, right);
			tryPlaneHit(slope.startCenter, normal, [&](const Vec3& point)
			{
				const Vec3 toPoint(point.x - slope.startCenter.x, 0.0f, point.z - slope.startCenter.z);
				const float along = bsmUtil::dot(toPoint, slope.direction);
				const float side = bsmUtil::dot(toPoint, right);
				const float edgePadding = 0.05f;
				return along >= -edgePadding &&
					along <= slope.length + edgePadding &&
					std::fabs(side) <= (slope.width * 0.5f) + edgePadding;
			});
		}

		for (const auto& surface : m_platformSurfaces)
		{
			const Vec3 right(surface.direction.z, 0.0f, -surface.direction.x);
			const Vec3 planePoint(surface.center.x, surface.height, surface.center.z);
			tryPlaneHit(planePoint, Vec3(0.0f, 1.0f, 0.0f), [&](const Vec3& point)
			{
				const Vec3 toPoint(point.x - surface.center.x, 0.0f, point.z - surface.center.z);
				const float along = bsmUtil::dot(toPoint, surface.direction);
				const float side = bsmUtil::dot(toPoint, right);
				const float edgePadding = 0.05f;
				return std::fabs(along) <= (surface.length * 0.5f) + edgePadding &&
					std::fabs(side) <= (surface.width * 0.5f) + edgePadding;
			});
		}

		if (!found)
		{
			return false;
		}

		outPoint = bestPoint;
		outNormal = bestNormal;
		outDistance = bestDistance;
		return true;
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
		ClearGroundLookup();
		m_groundLookupCellSize = kStageLayoutCellSize;

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

		ClearItemSpawnBlockers();
		AddItemSpawnBlocker(Vec3(0.0f, 0.0f, 0.0f), 5.0f);
		AddHeightVariationItemSpawnBlockers(*this);

		// プレイヤー初期位置、アイテム、坂周辺を先に予約し、自然物が重ならないようにする。
		occupied.push_back({ Vec3(0.0f, 0.0f, 0.0f), 5.0f });
		occupied.push_back({ Vec3(2.5f, 0.0f, 2.0f), 1.7f });
		occupied.push_back({ Vec3(-4.0f, 0.0f, 3.5f), 1.7f });
		occupied.push_back({ Vec3(6.0f, 0.0f, -2.5f), 1.7f });
		occupied.push_back({ Vec3(-7.5f, 0.0f, -5.0f), 1.7f });
		occupied.push_back({ Vec3(10.0f, 0.0f, 4.0f), 1.7f });
		AddHeightVariationOccupancy(occupied);

		std::mt19937 gen(20260506);

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
	void GameStage::ClearItemSpawnBlockers()
	{
		m_itemSpawnBlockers.clear();
	}

	void GameStage::AddItemSpawnBlocker(const Vec3& position, float radius)
	{
		if (radius <= 0.0f)
		{
			return;
		}

		ItemSpawnBlocker blocker;
		blocker.position = position;
		blocker.radius = radius;
		m_itemSpawnBlockers.push_back(blocker);
	}

	bool GameStage::IsItemSpawnPositionFree(const Vec3& position, float radius) const
	{
		auto overlaps = [&](const Vec3& otherPosition, float otherRadius)
		{
			const float dx = position.x - otherPosition.x;
			const float dz = position.z - otherPosition.z;
			const float minDistance = radius + otherRadius;
			return (dx * dx + dz * dz) < (minDistance * minDistance);
		};

		for (const auto& blocker : m_itemSpawnBlockers)
		{
			if (overlaps(blocker.position, blocker.radius))
			{
				return false;
			}
		}

		for (const auto& obj : GetGameObjectVec())
		{
			auto item = std::dynamic_pointer_cast<BaseItem>(obj);
			if (!item || item->IsConsumed())
			{
				continue;
			}

			auto transform = item->GetComponent<Transform>(false);
			if (transform && overlaps(transform->GetWorldPosition(), kItemSpawnRadius))
			{
				return false;
			}
		}

		return true;
	}

	bool GameStage::TryFindItemSpawnPosition(Vec3& outPosition)
	{
		std::uniform_real_distribution<float> positionDist(-kItemSpawnHalf, kItemSpawnHalf);

		for (int attempt = 0; attempt < kItemMaxSpawnAttempts; ++attempt)
		{
			Vec3 candidate(positionDist(m_itemSpawnRandom), 0.0f, positionDist(m_itemSpawnRandom));

			float groundHeight = 0.0f;
			if (TryGetSlopeGroundHeight(candidate, groundHeight))
			{
				candidate.y = groundHeight;
			}
			candidate.y += kItemGroundOffset;

			if (!IsItemSpawnPositionFree(candidate, kItemSpawnRadius))
			{
				continue;
			}

			outPosition = candidate;
			return true;
		}

		return false;
	}

	void GameStage::EnsureItemFactory()
	{
		auto stage = GetThis<Stage>();
		if (!m_itemFactory)
		{
			m_itemFactory = std::make_shared<ItemFactory>(stage);
		}
		else
		{
			m_itemFactory->SetStage(stage);
		}
	}

	void GameStage::MaintainRecoveryItems()
	{
		EnsureItemFactory();
		if (!m_itemFactory || !m_itemFactory->IsValid())
		{
			return;
		}

		int activeCount = m_itemFactory->CountActiveItems(ItemKind::HpRecovery);
		while (activeCount < kRecoveryItemTargetCount)
		{
			Vec3 spawnPosition;
			if (!TryFindItemSpawnPosition(spawnPosition))
			{
				break;
			}

			ItemFactory::SpawnDesc spawnDesc;
			spawnDesc.kind = ItemKind::HpRecovery;
			spawnDesc.position = spawnPosition;
			spawnDesc.scale = Vec3(kHpRecoveryItemScale, kHpRecoveryItemScale, kHpRecoveryItemDepthScale);

			if (!m_itemFactory->CreateItem(spawnDesc))
			{
				break;
			}
			++activeCount;
		}
	}


	void GameStage::MaintainBombItems()
	{
		EnsureItemFactory();
		if (!m_itemFactory || !m_itemFactory->IsValid())
		{
			return;
		}

		int activeCount = m_itemFactory->CountActiveItems(ItemKind::Bomb);
		while (activeCount < kBombItemTargetCount)
		{
			Vec3 spawnPosition;
			if (!TryFindItemSpawnPosition(spawnPosition))
			{
				break;
			}

			ItemFactory::SpawnDesc spawnDesc;
			spawnDesc.kind = ItemKind::Bomb;
			spawnDesc.position = spawnPosition;
			spawnDesc.scale = Vec3(kBombItemScale, kBombItemScale, kBombItemScale);
			spawnDesc.bombGrantCount = kBombItemGrantCount;

			if (!m_itemFactory->CreateItem(spawnDesc))
			{
				break;
			}
			++activeCount;
		}
	}
	void GameStage::CreateItems()
	{
		m_itemSpawnRandom.seed(20260513);
		MaintainRecoveryItems();
		MaintainBombItems();
	}

}
