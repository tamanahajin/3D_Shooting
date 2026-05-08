#pragma once
#include "stdafx.h"

namespace shooting {

	enum class StageObjectCategory
	{
		OutSideWall,
		Cliff,
		Tree,
		Log,
		Rock,
		Stone,
		Plant,
		Mushroom,
		Platform,
		Unknown,
	};

	struct StageObjectDef
	{
		std::wstring key;
		std::wstring materialPrefix;
		std::wstring relativePath;
		std::wstring name;
		StageObjectCategory category = StageObjectCategory::Unknown;
		Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
		float placementRadius = 0.5f;
		bool blocksMovement = false;
	};

	class StageObjectCatalog
	{
	public:
		static void RegisterAssets(BaseScene& scene, ID3D12GraphicsCommandList* pCommandList);
		static const std::vector<StageObjectDef>& GetAll();
		static std::vector<const StageObjectDef*> GetByCategory(StageObjectCategory category);
	};

}
