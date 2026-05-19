#pragma once
#include "stdafx.h"
#include <type_traits>
#include <utility>

namespace shooting {

	// 弾ハンドル（インデックス）
	struct BulletHandle
	{
		uint32_t index;
		bool isValid() const { return index != UINT32_MAX; }
		static BulletHandle Invalid() { return { UINT32_MAX }; }
	};

	template<typename BulletType>
	class BulletPool : public IBulletPool
	{
	private:
		static_assert(std::is_base_of<IBullet, BulletType>::value,
					  "BulletPool<BulletType>: BulletType must derive from IBullet");

		struct BulletSlot
		{
			std::shared_ptr<BulletType> bullet;
			bool active = false; // 「プール的に使用中か」
		};

		std::vector<BulletSlot> m_BulletSlots;
		std::vector<uint32_t>   m_FreeIndices;

		static constexpr size_t POOL_SIZE = 100;

	public:
		explicit BulletPool(const std::shared_ptr<Stage>& stagePtr)
			: IBulletPool(stagePtr)
		{
		}
		virtual ~BulletPool() = default;

		void OnCreate() override
		{
			m_BulletSlots.reserve(POOL_SIZE);

			auto stagePtr = GetStage();
			if (!stagePtr) return;

			for (size_t i = 0; i < POOL_SIZE; ++i)
			{
				TransParam initParam;
				initParam.position = Vec3(0.0f, -100.0f, 0.0f);
				initParam.scale = Vec3(1.0f, 1.0f, 1.0f);
				initParam.quaternion = Quat();

				// Stageに弾オブジェクトを事前生成
				auto bullet = stagePtr->AddGameObject<BulletType>(initParam);

				// プール待機状態にする
				bullet->SetActive(false);
				bullet->SetUpdateActive(false);

				BulletSlot slot;
				slot.bullet = bullet;
				slot.active = false;

				m_BulletSlots.push_back(std::move(slot));
				m_FreeIndices.push_back(static_cast<uint32_t>(i));
			}
		}

		/// <summary>
		/// 重要：弾の「更新（移動・寿命）」は Stage が行う。
		/// BulletPool は「非アクティブになった弾を回収して Free に戻す」だけ担当。
		/// </summary>
		void OnUpdate(double /*elapsedTime*/) override
		{
			for (size_t i = 0; i < m_BulletSlots.size(); ++i)
			{
				auto& slot = m_BulletSlots[i];
				if (!slot.active || !slot.bullet) continue;

				// 弾が終了していれば回収
				if (!slot.bullet->IsActive())
				{
					slot.active = false;

					slot.bullet->SetUpdateActive(false);
					slot.bullet->SetDrawActive(true);
					slot.bullet->SetShadowActive(true);
					slot.bullet->OnReturnToPool();

					if (auto trans = slot.bullet->GetComponent<Transform>())
					{
						trans->SetPosition(Vec3(0.0f, -100.0f, 0.0f));
						trans->SetScale(Vec3(1.0f, 1.0f, 1.0f));
					}

					m_FreeIndices.push_back(static_cast<uint32_t>(i));
				}
			}
		}

		void AllClear() override
		{
			m_FreeIndices.clear();

			for (size_t i = 0; i < m_BulletSlots.size(); ++i)
			{
				auto& slot = m_BulletSlots[i];
				if (!slot.bullet) continue;

				slot.active = false;
				slot.bullet->SetActive(false);
				slot.bullet->SetUpdateActive(false);
				slot.bullet->OnReturnToPool();

				if (auto trans = slot.bullet->GetComponent<Transform>())
				{
					trans->SetPosition(Vec3(0.0f, -100.0f, 0.0f));
					trans->SetScale(Vec3(1.0f, 1.0f, 1.0f));
				}

				m_FreeIndices.push_back(static_cast<uint32_t>(i));
			}
		}

		// 3引数版（既存互換）
		void Spawn(const Vec3& pos, const Quat& rot, const Vec3& scale)
		{
			Spawn(pos, rot, scale, [](BulletType&) {});
		}

		// 4引数版（setupでターゲット等を注入）
		template<class SetupFn>
		void Spawn(const Vec3& pos, const Quat& rot, const Vec3& scale, SetupFn&& setup)
		{
			auto stagePtr = GetStage();
			if (!stagePtr) return;

			if (!m_FreeIndices.empty())
			{
				const uint32_t index = m_FreeIndices.back();
				m_FreeIndices.pop_back();

				auto& slot = m_BulletSlots[index];
				slot.active = true;

				if (auto trans = slot.bullet->GetComponent<Transform>())
				{
					trans->SetPosition(pos);
					trans->SetQuaternion(rot);
					trans->SetScale(scale);
				}

				// ResetForSpawn の前に、ターゲット等をセット
				std::forward<SetupFn>(setup)(*slot.bullet);

				// ここで弾道計算などが走る
				slot.bullet->ResetForSpawn();

				slot.bullet->SetUpdateActive(true);
				return;
			}

			// 足りなければ追加生成
			TransParam tp;
			tp.position = pos;
			tp.quaternion = rot;
			tp.scale = scale;

			auto bullet = stagePtr->AddGameObject<BulletType>(tp);
			std::forward<SetupFn>(setup)(*bullet);
			bullet->ResetForSpawn();
			bullet->SetUpdateActive(true);

			BulletSlot slot;
			slot.bullet = bullet;
			slot.active = true;
			m_BulletSlots.push_back(std::move(slot));
		}
	};

}
