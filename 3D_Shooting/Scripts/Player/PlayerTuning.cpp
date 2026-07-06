#include "stdafx.h"
#include "PlayerTuning.h"
#include "Common/Library/BasicLib/JsonLoader.h"
#include <filesystem>
#include <limits>

namespace shooting {

	namespace
	{
		constexpr wchar_t kPlayerTuningPath[] = L"Data\\PlayerTuning.json";

		bool TryFindObject(
			const JsonValue& parent,
			const std::string& key,
			const std::string& path,
			const JsonValue*& outObject,
			std::string& outError)
		{
			outObject = nullptr;
			const JsonValue* value = parent.Find(key);
			if (!value)
			{
				return true;
			}
			if (!value->IsObject())
			{
				outError = path + "." + key + " はJSONオブジェクトで指定してください。";
				return false;
			}

			outObject = value;
			return true;
		}

		bool ReadOptionalNumber(
			const JsonValue& object,
			const std::string& key,
			const std::string& path,
			double& outValue,
			std::string& outError)
		{
			const JsonValue* value = object.Find(key);
			if (!value)
			{
				return true;
			}
			if (!value->IsNumber())
			{
				outError = path + "." + key + " は数値で指定してください。";
				return false;
			}

			outValue = value->GetNumber();
			return true;
		}

		bool ReadOptionalFloat(
			const JsonValue& object,
			const std::string& key,
			const std::string& path,
			float& outValue,
			std::string& outError)
		{
			double value = outValue;
			if (!ReadOptionalNumber(object, key, path, value, outError))
			{
				return false;
			}
			if (value < -(std::numeric_limits<float>::max)() ||
				value > (std::numeric_limits<float>::max)())
			{
				outError = path + "." + key + " がfloatの範囲を超えています。";
				return false;
			}

			outValue = static_cast<float>(value);
			return true;
		}

		bool ReadOptionalInteger(
			const JsonValue& object,
			const std::string& key,
			const std::string& path,
			int& outValue,
			std::string& outError)
		{
			double value = outValue;
			if (!ReadOptionalNumber(object, key, path, value, outError))
			{
				return false;
			}
			if (std::floor(value) != value ||
				value < (std::numeric_limits<int>::min)() ||
				value > (std::numeric_limits<int>::max)())
			{
				outError = path + "." + key + " は整数で指定してください。";
				return false;
			}

			outValue = static_cast<int>(value);
			return true;
		}

		bool ReadOptionalVec3(
			const JsonValue& object,
			const std::string& key,
			const std::string& path,
			Vec3& outValue,
			std::string& outError)
		{
			const JsonValue* value = object.Find(key);
			if (!value)
			{
				return true;
			}
			if (!value->IsArray() || value->GetArray().size() != 3)
			{
				outError = path + "." + key + " は3要素の配列で指定してください。";
				return false;
			}

			const auto& elements = value->GetArray();
			for (const auto& element : elements)
			{
				if (!element.IsNumber())
				{
					outError = path + "." + key + " の各要素は数値で指定してください。";
					return false;
				}
				const double number = element.GetNumber();
				if (number < -(std::numeric_limits<float>::max)() ||
					number > (std::numeric_limits<float>::max)())
				{
					outError = path + "." + key + " の値がfloatの範囲を超えています。";
					return false;
				}
			}

			outValue = Vec3(
				static_cast<float>(elements[0].GetNumber()),
				static_cast<float>(elements[1].GetNumber()),
				static_cast<float>(elements[2].GetNumber()));
			return true;
		}

		bool ApplyStatsTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* stats = nullptr;
			if (!TryFindObject(root, "stats", "root", stats, outError))
			{
				return false;
			}
			if (!stats)
			{
				return true;
			}

			return ReadOptionalInteger(*stats, "maxHp", "stats", tuning.maxHp, outError) &&
				ReadOptionalInteger(*stats, "initialBombAmmo", "stats", tuning.initialBombAmmo, outError);
		}

		bool ApplyMovementTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* movement = nullptr;
			if (!TryFindObject(root, "movement", "root", movement, outError))
			{
				return false;
			}
			if (!movement)
			{
				return true;
			}

			return ReadOptionalFloat(*movement, "moveSpeed", "movement", tuning.moveSpeed, outError) &&
				ReadOptionalFloat(*movement, "jumpSpeed", "movement", tuning.jumpSpeed, outError);
		}

		bool ApplyModelTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* model = nullptr;
			if (!TryFindObject(root, "model", "root", model, outError))
			{
				return false;
			}
			if (!model)
			{
				return true;
			}

			return ReadOptionalFloat(*model, "scale", "model", tuning.modelScale, outError);
		}

		bool ApplyCollisionTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* collision = nullptr;
			if (!TryFindObject(root, "collision", "root", collision, outError))
			{
				return false;
			}
			if (!collision)
			{
				return true;
			}

			return ReadOptionalFloat(*collision, "capsuleRadius", "collision", tuning.collisionCapsuleRadius, outError) &&
				ReadOptionalFloat(*collision, "capsuleSegmentHeight", "collision", tuning.collisionCapsuleSegmentHeight, outError);
		}

		bool ApplyCameraTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* camera = nullptr;
			if (!TryFindObject(root, "camera", "root", camera, outError))
			{
				return false;
			}
			if (!camera)
			{
				return true;
			}

			return ReadOptionalFloat(*camera, "targetHeight", "camera", tuning.cameraTargetHeight, outError);
		}

		bool ApplyDamageTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* damage = nullptr;
			if (!TryFindObject(root, "damage", "root", damage, outError))
			{
				return false;
			}
			if (!damage)
			{
				return true;
			}

			return ReadOptionalNumber(*damage, "invincibleTime", "damage", tuning.damageInvincibleTime, outError);
		}

		bool ApplyLevelTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* level = nullptr;
			if (!TryFindObject(root, "level", "root", level, outError))
			{
				return false;
			}
			if (!level)
			{
				return true;
			}

			return ReadOptionalInteger(*level, "initialLevel", "level", tuning.initialLevel, outError) &&
				ReadOptionalInteger(*level, "requiredExperienceBase", "level", tuning.requiredExperienceBase, outError) &&
				ReadOptionalInteger(*level, "requiredExperienceIncrease", "level", tuning.requiredExperienceIncrease, outError) &&
				ReadOptionalInteger(*level, "gunDamageBonusPerLevel", "level", tuning.gunDamageBonusPerLevel, outError) &&
				ReadOptionalFloat(*level, "experienceOrbPickupRadius", "level", tuning.experienceOrbPickupRadius, outError) &&
				ReadOptionalFloat(*level, "experienceOrbCollectRadius", "level", tuning.experienceOrbCollectRadius, outError) &&
				ReadOptionalFloat(*level, "experienceOrbAttractSpeed", "level", tuning.experienceOrbAttractSpeed, outError) &&
				ReadOptionalFloat(*level, "experienceOrbAttractHeight", "level", tuning.experienceOrbAttractHeight, outError) &&
				ReadOptionalFloat(*level, "experienceOrbFloatAmplitude", "level", tuning.experienceOrbFloatAmplitude, outError) &&
				ReadOptionalFloat(*level, "experienceOrbFloatSpeed", "level", tuning.experienceOrbFloatSpeed, outError) &&
				ReadOptionalFloat(*level, "experienceOrbScale", "level", tuning.experienceOrbScale, outError) &&
				ReadOptionalFloat(*level, "experienceOrbDropHeightOffset", "level", tuning.experienceOrbDropHeightOffset, outError) &&
				ReadOptionalInteger(*level, "experienceOrbPoolInitialSize", "level", tuning.experienceOrbPoolInitialSize, outError);
		}

		bool ApplyDeathTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* death = nullptr;
			if (!TryFindObject(root, "death", "root", death, outError))
			{
				return false;
			}
			if (!death)
			{
				return true;
			}

			return ReadOptionalNumber(*death, "hitStopDuration", "death", tuning.deathHitStopDuration, outError) &&
				ReadOptionalNumber(*death, "hitStopTimeScale", "death", tuning.deathHitStopTimeScale, outError) &&
				ReadOptionalNumber(*death, "soundDelay", "death", tuning.deathSoundDelay, outError) &&
				ReadOptionalNumber(*death, "animationTimeScale", "death", tuning.deathAnimationTimeScale, outError);
		}

		bool ApplySpawnIntroTuning(
			const JsonValue& root,
			PlayerTuning& tuning,
			std::string& outError)
		{
			const JsonValue* spawnIntro = nullptr;
			if (!TryFindObject(root, "spawnIntro", "root", spawnIntro, outError))
			{
				return false;
			}
			if (!spawnIntro)
			{
				return true;
			}

			return ReadOptionalVec3(*spawnIntro, "walkDirection", "spawnIntro", tuning.spawnIntroWalkDirection, outError) &&
				ReadOptionalFloat(*spawnIntro, "walkDistance", "spawnIntro", tuning.spawnIntroWalkDistance, outError) &&
				ReadOptionalNumber(*spawnIntro, "portalOnlyDuration", "spawnIntro", tuning.spawnIntroPortalOnlyDuration, outError) &&
				ReadOptionalNumber(*spawnIntro, "duration", "spawnIntro", tuning.spawnIntroDuration, outError) &&
				ReadOptionalFloat(*spawnIntro, "portalBackOffset", "spawnIntro", tuning.spawnIntroPortalBackOffset, outError) &&
				ReadOptionalFloat(*spawnIntro, "portalHeight", "spawnIntro", tuning.spawnIntroPortalHeight, outError) &&
				ReadOptionalFloat(*spawnIntro, "portalScale", "spawnIntro", tuning.spawnIntroPortalScale, outError) &&
				ReadOptionalFloat(*spawnIntro, "cameraDistance", "spawnIntro", tuning.spawnIntroCameraDistance, outError) &&
				ReadOptionalFloat(*spawnIntro, "cameraHeight", "spawnIntro", tuning.spawnIntroCameraHeight, outError) &&
				ReadOptionalFloat(*spawnIntro, "cameraLookHeight", "spawnIntro", tuning.spawnIntroCameraLookHeight, outError);
		}

		bool ValidatePlayerTuning(const PlayerTuning& tuning, std::string& outError)
		{
			if (tuning.maxHp <= 0 ||
				tuning.initialBombAmmo < 0 ||
				tuning.moveSpeed < 0.0f ||
				tuning.jumpSpeed < 0.0f ||
				tuning.modelScale <= 0.0f ||
				tuning.collisionCapsuleRadius <= 0.0f ||
				tuning.collisionCapsuleSegmentHeight <= 0.0f ||
				tuning.cameraTargetHeight < 0.0f ||
				tuning.damageInvincibleTime < 0.0 ||
				tuning.initialLevel <= 0 ||
				tuning.requiredExperienceBase <= 0 ||
				tuning.requiredExperienceIncrease < 0 ||
				tuning.gunDamageBonusPerLevel < 0 ||
				tuning.experienceOrbPickupRadius <= 0.0f ||
				tuning.experienceOrbCollectRadius <= 0.0f ||
				tuning.experienceOrbAttractSpeed <= 0.0f ||
				tuning.experienceOrbAttractHeight < 0.0f ||
				tuning.experienceOrbFloatAmplitude < 0.0f ||
				tuning.experienceOrbFloatSpeed < 0.0f ||
				tuning.experienceOrbScale <= 0.0f ||
				tuning.experienceOrbDropHeightOffset < 0.0f ||
				tuning.experienceOrbPoolInitialSize < 0 ||
				tuning.deathHitStopDuration < 0.0 ||
				tuning.deathHitStopTimeScale < 0.0 ||
				tuning.deathSoundDelay < 0.0 ||
				tuning.deathAnimationTimeScale < 0.0 ||
				tuning.spawnIntroWalkDistance < 0.0f ||
				tuning.spawnIntroPortalOnlyDuration < 0.0 ||
				tuning.spawnIntroDuration <= 0.0 ||
				tuning.spawnIntroPortalBackOffset < 0.0f ||
				tuning.spawnIntroPortalHeight < 0.0f ||
				tuning.spawnIntroPortalScale <= 0.0f ||
				tuning.spawnIntroCameraDistance < 0.0f ||
				tuning.spawnIntroCameraHeight < 0.0f ||
				tuning.spawnIntroCameraLookHeight < 0.0f)
			{
				outError = "値の範囲が不正です。HPやサイズは0より大きく、時間や速度は用途に応じて0以上にしてください。";
				return false;
			}

			const float walkDirectionLengthSq =
				(tuning.spawnIntroWalkDirection.x * tuning.spawnIntroWalkDirection.x) +
				(tuning.spawnIntroWalkDirection.y * tuning.spawnIntroWalkDirection.y) +
				(tuning.spawnIntroWalkDirection.z * tuning.spawnIntroWalkDirection.z);
			if (walkDirectionLengthSq <= 1e-12f)
			{
				outError = "spawnIntro.walkDirection は長さのあるベクトルで指定してください。";
				return false;
			}

			return true;
		}

		PlayerTuning LoadPlayerTuning()
		{
			PlayerTuning tuning;
			const std::wstring path = App::GetRelativeAssetsDir() + kPlayerTuningPath;

			std::error_code errorCode;
			if (!std::filesystem::exists(std::filesystem::path(path), errorCode))
			{
				return tuning;
			}

			JsonValue root;
			std::string error;
			if (!JsonLoader::LoadFile(path, root, error))
			{
				throw std::runtime_error("PlayerTuning.jsonの読み込みに失敗しました: " + error);
			}
			if (!root.IsObject())
			{
				throw std::runtime_error("PlayerTuning.jsonのルートはオブジェクトで指定してください。");
			}

			if (!ApplyStatsTuning(root, tuning, error) ||
				!ApplyMovementTuning(root, tuning, error) ||
				!ApplyModelTuning(root, tuning, error) ||
				!ApplyCollisionTuning(root, tuning, error) ||
				!ApplyCameraTuning(root, tuning, error) ||
				!ApplyDamageTuning(root, tuning, error) ||
				!ApplyLevelTuning(root, tuning, error) ||
				!ApplyDeathTuning(root, tuning, error) ||
				!ApplySpawnIntroTuning(root, tuning, error) ||
				!ValidatePlayerTuning(tuning, error))
			{
				throw std::runtime_error("PlayerTuning.jsonの値が不正です: " + error);
			}

			// 距離設定の意味が変わらないよう、方向だけをJSON読み込み後に正規化する。
			tuning.spawnIntroWalkDirection.normalize();
			return tuning;
		}
	}

	const PlayerTuning& GetPlayerTuning()
	{
		static const PlayerTuning k = LoadPlayerTuning();
		return k;
	}
}
