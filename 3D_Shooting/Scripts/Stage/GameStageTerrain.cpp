/*!
@file GameStageTerrain.cpp
@brief GameStageの高低差地形と生成地形判定
*/

#include "stdafx.h"
#include "Project.h"
#include "GameStageEnvironmentCommon.h"
#include <algorithm>
#include <cmath>

namespace shooting {
	namespace stage_environment {

		namespace
		{
			const bool kDrawSlopeCollisionDebug = false;
			const float kSlopeCollisionStartCenterZOffset = -2.5f;

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

		void AddHeightVariationSpawnBlockers(GameStage& stage)
		{
			const auto placements = BuildHeightVariationPlacements();
			for (const auto& placement : placements)
			{
				// アイテムは従来どおり斜面を避けるが、敵は坂表面へ生成できるようにする。
				if (!IsSlopePlacement(placement))
				{
					continue;
				}
				stage.AddStageSpawnBlocker(
					placement.position,
					placement.occupancyRadius,
					true,
					false);
			}
		}

	}

	using namespace stage_environment;

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

}
