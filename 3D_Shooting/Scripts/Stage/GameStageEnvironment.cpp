/*!
@file GameStageEnvironment.cpp
@brief GameStageの基本環境生成
*/

#include "stdafx.h"
#include "Project.h"
#include "GameStageEnvironmentCommon.h"
#include <map>

namespace shooting {

	using namespace stage_environment;

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
		const float groundHalf = 32.0f;
		const float edge = groundHalf + 1.5f;
		const float outerCliffScale = 1.0f;
		const float outerSideLength = (groundHalf / 4.0f) - 1.5f;
		const float wallHalf = edge - 0.3f;
		const float wallLength = wallHalf * 2.0f;
		const float wallThickness = 1.5f;
		const float wallHeight = 36.0f;
		const float wallY = 2.0f;
		const float wallInnerHalf = wallHalf - (wallThickness * 0.5f);

		// 敵生成判定は、見た目ではなく実際の壁コリジョンの内側面に合わせる。
		// 敵半径は判定時にさらに差し引くため、壁との接触と壁外生成を同時に防げる。
		m_stageSpawnBounds.minX = -wallInnerHalf;
		m_stageSpawnBounds.maxX = wallInnerHalf;
		m_stageSpawnBounds.minZ = -wallInnerHalf;
		m_stageSpawnBounds.maxZ = wallInnerHalf;
		m_stageSpawnBounds.valid = true;

		const auto* cliff = FindOuterCliffDef();
		if (cliff)
		{
			std::map<std::wstring, StageObjectBatch> batches;
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
		}

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

}
