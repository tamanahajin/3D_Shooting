#include "stdafx.h"
#include "WeaponTuning.h"
#include "Common/Library/BasicLib/JsonLoader.h"
#include <filesystem>
#include <limits>

namespace shooting {

	namespace
	{
		constexpr wchar_t kWeaponTuningPath[] = L"Data\\WeaponTuning.json";

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

		bool ApplyNormalShotTuning(
			const JsonValue& root,
			WeaponTuning& tuning,
			std::string& outError)
		{
			const JsonValue* normalShot = nullptr;
			if (!TryFindObject(root, "normalShot", "root", normalShot, outError))
			{
				return false;
			}
			if (!normalShot)
			{
				return true;
			}

			return ReadOptionalFloat(*normalShot, "range", "normalShot", tuning.normalShotRange, outError) &&
				ReadOptionalInteger(*normalShot, "damage", "normalShot", tuning.normalShotDamage, outError) &&
				ReadOptionalNumber(*normalShot, "cooldown", "normalShot", tuning.normalShotCooldown, outError);
		}

		bool ApplyDefaultBulletTuning(
			const JsonValue& root,
			WeaponTuning& tuning,
			std::string& outError)
		{
			const JsonValue* defaultBullet = nullptr;
			if (!TryFindObject(root, "defaultBullet", "root", defaultBullet, outError))
			{
				return false;
			}
			if (!defaultBullet)
			{
				return true;
			}

			return ReadOptionalFloat(*defaultBullet, "speed", "defaultBullet", tuning.defaultBulletSpeed, outError) &&
				ReadOptionalInteger(*defaultBullet, "damage", "defaultBullet", tuning.defaultBulletDamage, outError) &&
				ReadOptionalNumber(*defaultBullet, "lifeTime", "defaultBullet", tuning.defaultBulletLifeTime, outError);
		}

		bool ApplyBulletPoolTuning(
			const JsonValue& root,
			WeaponTuning& tuning,
			std::string& outError)
		{
			const JsonValue* bulletPool = nullptr;
			if (!TryFindObject(root, "bulletPool", "root", bulletPool, outError))
			{
				return false;
			}
			if (!bulletPool)
			{
				return true;
			}

			return ReadOptionalInteger(*bulletPool, "initialSize", "bulletPool", tuning.bulletPoolInitialSize, outError);
		}

		bool ApplyWeaponTuning(
			const JsonValue& root,
			WeaponTuning& tuning,
			std::string& outError)
		{
			const JsonValue* bomb = nullptr;
			if (!TryFindObject(root, "bomb", "root", bomb, outError))
			{
				return false;
			}
			if (!bomb)
			{
				return true;
			}

			return ReadOptionalFloat(*bomb, "maxRange", "bomb", tuning.bombMaxRange, outError) &&
				ReadOptionalFloat(*bomb, "startBodyCenterHeight", "bomb", tuning.bombStartBodyCenterHeight, outError) &&
				ReadOptionalFloat(*bomb, "speed", "bomb", tuning.bombSpeed, outError) &&
				ReadOptionalNumber(*bomb, "fuseTime", "bomb", tuning.bombFuseTime, outError) &&
				ReadOptionalNumber(*bomb, "shotCooldown", "bomb", tuning.bombShotCooldown, outError) &&
				ReadOptionalNumber(*bomb, "explosionDuration", "bomb", tuning.explosionDuration, outError) &&
				ReadOptionalInteger(*bomb, "explosionDamage", "bomb", tuning.explosionDamage, outError) &&
				ReadOptionalFloat(*bomb, "arcHeightBase", "bomb", tuning.arcHeightBase, outError) &&
				ReadOptionalFloat(*bomb, "arcHeightPerDistXZ", "bomb", tuning.arcHeightPerDistXZ, outError) &&
				ReadOptionalVec3(*bomb, "gravity", "bomb", tuning.gravity, outError) &&
				ReadOptionalFloat(*bomb, "explosionRadius", "bomb", tuning.explosionRadius, outError) &&
				ReadOptionalVec3(*bomb, "projectileScale", "bomb", tuning.bombProjectileScale, outError);
		}

		bool ApplyCameraShakeTuning(
			const JsonValue& root,
			WeaponTuning& tuning,
			std::string& outError)
		{
			const JsonValue* cameraShake = nullptr;
			if (!TryFindObject(root, "cameraShake", "root", cameraShake, outError))
			{
				return false;
			}
			if (!cameraShake)
			{
				return true;
			}

			return ReadOptionalFloat(*cameraShake, "intensity", "cameraShake", tuning.cameraShakeIntensity, outError) &&
				ReadOptionalFloat(*cameraShake, "duration", "cameraShake", tuning.cameraShakeDuration, outError) &&
				ReadOptionalFloat(*cameraShake, "maxDistance", "cameraShake", tuning.cameraShakeMaxDistance, outError);
		}

		bool ValidateWeaponTuning(const WeaponTuning& tuning, std::string& outError)
		{
			if (tuning.normalShotRange <= 0.0f ||
				tuning.normalShotDamage < 0 ||
				tuning.normalShotCooldown < 0.0 ||
				tuning.defaultBulletSpeed < 0.0f ||
				tuning.defaultBulletDamage < 0 ||
				tuning.defaultBulletLifeTime <= 0.0 ||
				tuning.bulletPoolInitialSize <= 0 ||
				tuning.bombMaxRange <= 0.0f ||
				tuning.bombStartBodyCenterHeight < 0.0f ||
				tuning.bombSpeed < 0.0f ||
				tuning.bombFuseTime <= 0.0 ||
				tuning.bombShotCooldown < 0.0 ||
				tuning.explosionDuration <= 0.0 ||
				tuning.explosionDamage < 0 ||
				tuning.arcHeightBase < 0.0f ||
				tuning.arcHeightPerDistXZ < 0.0f ||
				tuning.explosionRadius <= 0.0f ||
				tuning.bombProjectileScale.x <= 0.0f ||
				tuning.bombProjectileScale.y <= 0.0f ||
				tuning.bombProjectileScale.z <= 0.0f ||
				tuning.cameraShakeIntensity < 0.0f ||
				tuning.cameraShakeDuration < 0.0f ||
				tuning.cameraShakeMaxDistance < 0.0f)
			{
				outError = "値の範囲が不正です。距離、速度、時間、倍率は用途に応じて0以上または0より大きくしてください。";
				return false;
			}

			if (tuning.gravity.y >= 0.0f)
			{
				outError = "bomb.gravity.y は下向き重力として0未満にしてください。";
				return false;
			}

			return true;
		}

		WeaponTuning LoadWeaponTuning()
		{
			WeaponTuning tuning;
			const std::wstring path = App::GetRelativeAssetsDir() + kWeaponTuningPath;

			std::error_code errorCode;
			if (!std::filesystem::exists(std::filesystem::path(path), errorCode))
			{
				return tuning;
			}

			JsonValue root;
			std::string error;
			if (!JsonLoader::LoadFile(path, root, error))
			{
				throw std::runtime_error("WeaponTuning.jsonの読み込みに失敗しました: " + error);
			}
			if (!root.IsObject())
			{
				throw std::runtime_error("WeaponTuning.jsonのルートはオブジェクトで指定してください。");
			}

			if (!ApplyNormalShotTuning(root, tuning, error) ||
				!ApplyDefaultBulletTuning(root, tuning, error) ||
				!ApplyBulletPoolTuning(root, tuning, error) ||
				!ApplyWeaponTuning(root, tuning, error) ||
				!ApplyCameraShakeTuning(root, tuning, error) ||
				!ValidateWeaponTuning(tuning, error))
			{
				throw std::runtime_error("WeaponTuning.jsonの値が不正です: " + error);
			}

			return tuning;
		}
	}

	const WeaponTuning& GetWeaponTuning()
	{
		static const WeaponTuning k = LoadWeaponTuning();
		return k;
	}

}
