#include "stdafx.h"
#include "EnemyWaveConfigLoader.h"
#include "Common/Library/BasicLib/JsonLoader.h"
#include <cmath>
#include <limits>

namespace shooting {

	namespace {

		const JsonValue* FindMember(
			const JsonValue& object,
			const std::string& key,
			JsonValue::Type expectedType,
			const std::string& valuePath,
			std::string& outError)
		{
			if (!object.IsObject())
			{
				outError = valuePath + " はJSONオブジェクトである必要があります。";
				return nullptr;
			}

			const JsonValue* value = object.Find(key);
			if (!value)
			{
				outError = valuePath + "." + key + " がありません。";
				return nullptr;
			}
			if (value->GetType() != expectedType)
			{
				outError = valuePath + "." + key + " の型が不正です。";
				return nullptr;
			}
			return value;
		}

		bool ReadNumber(
			const JsonValue& object,
			const std::string& key,
			const std::string& valuePath,
			double& outValue,
			std::string& outError)
		{
			const JsonValue* value = FindMember(
				object,
				key,
				JsonValue::Type::Number,
				valuePath,
				outError);
			if (!value)
			{
				return false;
			}
			outValue = value->GetNumber();
			return true;
		}

		bool ReadFloat(
			const JsonValue& object,
			const std::string& key,
			const std::string& valuePath,
			float& outValue,
			std::string& outError)
		{
			double value = 0.0;
			if (!ReadNumber(object, key, valuePath, value, outError))
			{
				return false;
			}
			if (value < -(std::numeric_limits<float>::max)() ||
				value > (std::numeric_limits<float>::max)())
			{
				outError = valuePath + "." + key + " がfloatの範囲を超えています。";
				return false;
			}
			outValue = static_cast<float>(value);
			return true;
		}

		bool ReadInteger(
			const JsonValue& object,
			const std::string& key,
			const std::string& valuePath,
			int& outValue,
			std::string& outError)
		{
			double value = 0.0;
			if (!ReadNumber(object, key, valuePath, value, outError))
			{
				return false;
			}
			if (std::floor(value) != value ||
				value < (std::numeric_limits<int>::min)() ||
				value > (std::numeric_limits<int>::max)())
			{
				outError = valuePath + "." + key + " は整数で指定してください。";
				return false;
			}
			outValue = static_cast<int>(value);
			return true;
		}

		bool ReadVec3(
			const JsonValue& object,
			const std::string& key,
			const std::string& valuePath,
			Vec3& outValue,
			std::string& outError)
		{
			const JsonValue* value = FindMember(
				object,
				key,
				JsonValue::Type::Array,
				valuePath,
				outError);
			if (!value)
			{
				return false;
			}

			const auto& elements = value->GetArray();
			if (elements.size() != 3)
			{
				outError = valuePath + "." + key + " は3要素の配列で指定してください。";
				return false;
			}
			for (size_t i = 0; i < elements.size(); ++i)
			{
				if (!elements[i].IsNumber())
				{
					outError = valuePath + "." + key + " の各要素は数値で指定してください。";
					return false;
				}
				const double number = elements[i].GetNumber();
				if (number < -(std::numeric_limits<float>::max)() ||
					number > (std::numeric_limits<float>::max)())
				{
					outError = valuePath + "." + key + " の値がfloatの範囲を超えています。";
					return false;
				}
			}

			outValue = Vec3(
				static_cast<float>(elements[0].GetNumber()),
				static_cast<float>(elements[1].GetNumber()),
				static_cast<float>(elements[2].GetNumber()));
			return true;
		}

		bool LoadWaveSettings(
			const JsonValue& root,
			WaveSettings& outSettings,
			std::string& outError)
		{
			const JsonValue* wave = FindMember(
				root,
				"wave",
				JsonValue::Type::Object,
				"root",
				outError);
			if (!wave)
			{
				return false;
			}

			WaveSettings settings;
			if (!ReadNumber(*wave, "intervalSeconds", "wave", settings.intervalSeconds, outError) ||
				!ReadInteger(*wave, "firstWaveEnemyCount", "wave", settings.firstWaveEnemyCount, outError) ||
				!ReadInteger(*wave, "addEnemyCountPerWave", "wave", settings.addEnemyCountPerWave, outError) ||
				!ReadInteger(*wave, "speedUpEveryWaves", "wave", settings.speedUpEveryWaves, outError) ||
				!ReadFloat(*wave, "speedMultiplierAddPerStep", "wave", settings.speedMultiplierAddPerStep, outError) ||
				!ReadFloat(*wave, "spawnMinDistance", "wave", settings.spawnMinDistance, outError) ||
				!ReadFloat(*wave, "spawnMaxDistance", "wave", settings.spawnMaxDistance, outError) ||
				!ReadFloat(*wave, "spawnY", "wave", settings.spawnY, outError) ||
				!ReadFloat(*wave, "minSpawnSpacing", "wave", settings.minSpawnSpacing, outError) ||
				!ReadInteger(*wave, "maxSpawnAttempts", "wave", settings.maxSpawnAttempts, outError))
			{
				return false;
			}

			if (settings.intervalSeconds < 0.0 ||
				settings.firstWaveEnemyCount < 0 ||
				settings.addEnemyCountPerWave < 0 ||
				settings.speedUpEveryWaves < 0 ||
				settings.speedMultiplierAddPerStep < 0.0f ||
				settings.spawnMinDistance < 0.0f ||
				settings.spawnMaxDistance < settings.spawnMinDistance ||
				settings.minSpawnSpacing < 0.0f ||
				settings.maxSpawnAttempts <= 0)
			{
				outError = "wave設定の範囲が不正です。距離、敵数、倍率は0以上、"
					"spawnMaxDistanceはspawnMinDistance以上、maxSpawnAttemptsは1以上にしてください。";
				return false;
			}

			outSettings = settings;
			return true;
		}

		bool LoadEnemyStatus(
			const JsonValue& value,
			const std::string& valuePath,
			EnemyStatus& outStatus,
			std::string& outError)
		{
			EnemyStatus status;
			if (!ReadInteger(value, "maxHp", valuePath, status.maxHp, outError) ||
				!ReadInteger(value, "contactDamage", valuePath, status.contactDamage, outError) ||
				!ReadFloat(value, "moveSpeed", valuePath, status.moveSpeed, outError) ||
				!ReadVec3(value, "modelScale", valuePath, status.modelScale, outError) ||
				!ReadFloat(value, "collisionRadius", valuePath, status.collisionRadius, outError) ||
				!ReadFloat(value, "collisionHeight", valuePath, status.collisionHeight, outError) ||
				!ReadFloat(value, "groundFootOffset", valuePath, status.groundFootOffset, outError) ||
				!ReadNumber(value, "steeringInterval", valuePath, status.steeringInterval, outError) ||
				!ReadNumber(value, "damageFlashDuration", valuePath, status.damageFlashDuration, outError) ||
				!ReadFloat(value, "damageNumberOffsetY", valuePath, status.damageNumberOffsetY, outError) ||
				!ReadNumber(value, "hitPushDuration", valuePath, status.hitPushDuration, outError) ||
				!ReadFloat(value, "hitPushDistance", valuePath, status.hitPushDistance, outError) ||
				!ReadFloat(value, "hitPushLeanAngle", valuePath, status.hitPushLeanAngle, outError))
			{
				return false;
			}

			if (status.maxHp <= 0 ||
				status.contactDamage < 0 ||
				status.moveSpeed < 0.0f ||
				status.modelScale.x <= 0.0f ||
				status.modelScale.y <= 0.0f ||
				status.modelScale.z <= 0.0f ||
				status.collisionRadius <= 0.0f ||
				status.collisionHeight <= 0.0f ||
				status.groundFootOffset < 0.0f ||
				status.steeringInterval < 0.0 ||
				status.damageFlashDuration < 0.0 ||
				status.damageNumberOffsetY < 0.0f ||
				status.hitPushDuration < 0.0 ||
				status.hitPushDistance < 0.0f ||
				status.hitPushLeanAngle < 0.0f)
			{
				outError = valuePath + " の範囲が不正です。maxHp、モデル倍率、"
					"コリジョンサイズは0より大きく、その他の値は0以上にしてください。";
				return false;
			}

			outStatus = status;
			return true;
		}

		bool LoadEnemyStatuses(
			const JsonValue& root,
			std::map<EnemyKind, EnemyStatus>& outStatuses,
			std::string& outError)
		{
			const JsonValue* enemies = FindMember(
				root,
				"enemies",
				JsonValue::Type::Object,
				"root",
				outError);
			if (!enemies)
			{
				return false;
			}

			std::map<EnemyKind, EnemyStatus> statuses;
			for (const auto& entry : enemies->GetObject())
			{
				EnemyKind kind;
				if (entry.first == "Default")
				{
					kind = EnemyKind::Default;
				}
				else
				{
					outError = "enemies." + entry.first + " は未対応の敵種別です。";
					return false;
				}

				if (!entry.second.IsObject())
				{
					outError = "enemies." + entry.first + " はJSONオブジェクトで指定してください。";
					return false;
				}

				EnemyStatus status;
				if (!LoadEnemyStatus(
					entry.second,
					"enemies." + entry.first,
					status,
					outError))
				{
					return false;
				}
				statuses[kind] = status;
			}

			if (statuses.find(EnemyKind::Default) == statuses.end())
			{
				outError = "enemies.Default がありません。";
				return false;
			}

			outStatuses = statuses;
			return true;
		}

	}

	bool EnemyWaveConfigLoader::Load(
		const std::wstring& path,
		EnemyWaveConfig& outConfig,
		std::string& outError)
	{
		JsonValue root;
		if (!JsonLoader::LoadFile(path, root, outError))
		{
			return false;
		}
		if (!root.IsObject())
		{
			outError = "JSONのルートはオブジェクトで指定してください。";
			return false;
		}

		// 一時設定へ全項目を変換し、すべて成功した場合だけ呼び出し側へ反映する。
		EnemyWaveConfig config;
		if (!LoadWaveSettings(root, config.waveSettings, outError) ||
			!LoadEnemyStatuses(root, config.enemyStatuses, outError))
		{
			return false;
		}

		outConfig = config;
		outError.clear();
		return true;
	}

}
