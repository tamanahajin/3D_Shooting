#include "stdafx.h"
#include "Project.h"
#include <cwctype>
#include <filesystem>

namespace shooting {

	namespace
	{
		std::vector<StageObjectDef>& MutableDefs()
		{
			static std::vector<StageObjectDef> defs;
			return defs;
		}

		std::wstring ToLower(std::wstring value)
		{
			for (auto& ch : value)
			{
				ch = static_cast<wchar_t>(std::towlower(ch));
			}
			return value;
		}

		bool Contains(const std::wstring& value, const std::wstring& pattern)
		{
			return value.find(pattern) != std::wstring::npos;
		}

		std::wstring RemoveExtension(const std::wstring& path)
		{
			const auto dot = path.find_last_of(L'.');
			if (dot == std::wstring::npos)
			{
				return path;
			}
			return path.substr(0, dot);
		}

		std::wstring FileStem(const std::wstring& relativePath)
		{
			const auto slash = relativePath.find_last_of(L"/\\");
			const auto fileName = (slash == std::wstring::npos) ? relativePath : relativePath.substr(slash + 1);
			return RemoveExtension(fileName);
		}

		std::wstring MakeResourceKey(const std::wstring& relativePath)
		{
			std::wstring key = L"STAGEOBJ_";
			const auto noExt = RemoveExtension(relativePath);
			for (wchar_t ch : noExt)
			{
				if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9'))
				{
					key.push_back(static_cast<wchar_t>(std::towupper(ch)));
				}
				else
				{
					key.push_back(L'_');
				}
			}
			return key;
		}

		Vec3 ScaleVec(const Vec3& value, float scale)
		{
			return Vec3(value.x * scale, value.y * scale, value.z * scale);
		}

		StageObjectCategory Classify(const std::wstring& relativePath, const std::wstring& name)
		{
			const auto lowerPath = ToLower(relativePath);
			const auto lowerName = ToLower(name);

			if (Contains(lowerPath, L"/outsidewall/"))
			{
				return StageObjectCategory::OutSideWall;
			}
			if (Contains(lowerPath, L"/cliff/") || Contains(lowerName, L"cliff_"))
			{
				return StageObjectCategory::Cliff;
			}
			if (Contains(lowerName, L"platform"))
			{
				return StageObjectCategory::Platform;
			}
			if (Contains(lowerName, L"log"))
			{
				return StageObjectCategory::Log;
			}
			if (Contains(lowerPath, L"/tree/") || Contains(lowerName, L"tree_"))
			{
				return StageObjectCategory::Tree;
			}
			if (Contains(lowerPath, L"/rock/") || Contains(lowerName, L"rock_"))
			{
				return StageObjectCategory::Rock;
			}
			if (Contains(lowerPath, L"/stone/") || Contains(lowerName, L"stone_"))
			{
				return StageObjectCategory::Stone;
			}
			if (Contains(lowerPath, L"/plant/") || Contains(lowerName, L"plant_"))
			{
				return StageObjectCategory::Plant;
			}
			if (Contains(lowerPath, L"/mashroom/") || Contains(lowerPath, L"/mushroom/") || Contains(lowerName, L"mushroom_"))
			{
				return StageObjectCategory::Mushroom;
			}
			return StageObjectCategory::Unknown;
		}

		StageObjectDef BuildDef(const std::wstring& relativePath)
		{
			StageObjectDef def;
			def.relativePath = L"Model/StageObjects/" + relativePath;
			def.key = MakeResourceKey(relativePath);
			def.materialPrefix = def.key + L"_MAT_";
			def.name = ToLower(FileStem(relativePath));
			def.category = Classify(L"/" + ToLower(relativePath), def.name);

			switch (def.category)
			{
			case StageObjectCategory::OutSideWall:
				def.scale = Vec3(1.0f, 2.0f, 0.2f);
				def.placementRadius = 0.45f;
				def.blocksMovement = true;
				break;
			case StageObjectCategory::Cliff:
				def.scale = Vec3(0.5f, 0.5f, 0.5f);
				def.placementRadius = 0.45f;
				def.blocksMovement = true;
				break;
			case StageObjectCategory::Tree:
				def.scale = Vec3(0.30f, 0.30f, 0.30f);
				def.placementRadius = 0.55f;
				def.blocksMovement = true;
				break;
			case StageObjectCategory::Log:
				def.scale = Vec3(0.30f, 0.30f, 0.30f);
				def.placementRadius = 0.45f;
				def.blocksMovement = true;
				break;
			case StageObjectCategory::Rock:
			case StageObjectCategory::Stone:
				def.scale = Vec3(0.20f, 0.20f, 0.20f);
				def.placementRadius = 0.40f;
				def.blocksMovement = true;
				break;
			case StageObjectCategory::Plant:
				def.scale = Vec3(0.10f, 0.10f, 0.10f);
				def.placementRadius = 0.25f;
				break;
			case StageObjectCategory::Mushroom:
				def.scale = Vec3(0.10f, 0.10f, 0.10f);
				def.placementRadius = 0.25f;
				break;
			case StageObjectCategory::Platform:
				def.scale = Vec3(0.20f, 0.20f, 0.20f);
				def.placementRadius = 0.50f;
				def.blocksMovement = true;
				break;
			default:
				def.scale = Vec3(0.30f, 0.30f, 0.30f);
				def.placementRadius = 0.35f;
				break;
			}

			if (Contains(def.name, L"large") || Contains(def.name, L"group"))
			{
				def.scale = ScaleVec(def.scale, 1.15f);
				def.placementRadius *= 1.30f;
			}
			if (Contains(def.name, L"small") || Contains(def.name, L"flat"))
			{
				def.scale = ScaleVec(def.scale, 0.85f);
				def.placementRadius *= 0.80f;
			}
			if (Contains(def.name, L"tall"))
			{
				def.scale = ScaleVec(def.scale, 1.10f);
				def.placementRadius *= 0.95f;
			}

			return def;
		}

		void CollectFbxFiles(const std::wstring& directory, const std::wstring& relativePrefix, std::vector<std::wstring>& files)
		{
			const std::filesystem::path root(directory);
			std::error_code errorCode;
			if (!std::filesystem::is_directory(root, errorCode) || errorCode)
			{
				return;
			}

			std::filesystem::recursive_directory_iterator it(
				root,
				std::filesystem::directory_options::skip_permission_denied,
				errorCode);
			const std::filesystem::recursive_directory_iterator end;
			for (; it != end; it.increment(errorCode))
			{
				if (errorCode)
				{
					errorCode.clear();
					continue;
				}

				if (!it->is_regular_file(errorCode) || errorCode)
				{
					errorCode.clear();
					continue;
				}

				if (ToLower(it->path().extension().wstring()) != L".fbx")
				{
					continue;
				}

				const std::filesystem::path relativePath =
					std::filesystem::relative(it->path(), root, errorCode);
				if (errorCode)
				{
					errorCode.clear();
					continue;
				}

				// リソースキーやカテゴリ判定は'/'区切り前提なのでgeneric形式で揃える。
				files.push_back(relativePrefix + relativePath.generic_wstring());
			}
		}
	}

	void StageObjectCatalog::RegisterAssets(BaseScene& scene, ID3D12GraphicsCommandList* pCommandList)
	{
		auto& defs = MutableDefs();
		if (!defs.empty())
		{
			return;
		}

		std::vector<std::wstring> files;
		const std::wstring root = App::GetRelativeAssetsDir() + L"Model\\StageObjects";
		CollectFbxFiles(root, L"", files);
		std::sort(files.begin(), files.end());

		for (const auto& file : files)
		{
			StageObjectDef def = BuildDef(file);
			auto parts = BaseMesh::CreateModelMeshWithMaterial(
				pCommandList,
				App::GetRelativeAssetsDir(),
				def.relativePath);

			std::vector<std::shared_ptr<BaseMesh>> meshes;
			meshes.reserve(parts.size());
			for (size_t i = 0; i < parts.size(); ++i)
			{
				meshes.push_back(parts[i].mesh);
				scene.RegisterMaterial(def.materialPrefix + std::to_wstring(i), parts[i].material);
			}

			scene.RegisterModelMesh(def.key, meshes);
			defs.push_back(def);
		}
	}

	const std::vector<StageObjectDef>& StageObjectCatalog::GetAll()
	{
		return MutableDefs();
	}

	std::vector<const StageObjectDef*> StageObjectCatalog::GetByCategory(StageObjectCategory category)
	{
		std::vector<const StageObjectDef*> result;
		for (const auto& def : MutableDefs())
		{
			if (def.category == category)
			{
				result.push_back(&def);
			}
		}
		return result;
	}

}
