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
			bool inUse = false;
		};

		std::vector<BulletSlot> m_bulletSlots;
		std::vector<uint32_t>   m_freeIndices;

		static constexpr size_t poolSize = 100;

		void ActivateSlot(BulletSlot& slot)
		{
			if (!slot.bullet) return;

			slot.bullet->ResetForSpawn();
			slot.inUse = true;
			slot.bullet->SetActive(true);
		}

		void ReturnSlotToPool(BulletSlot& slot)
		{
			if (!slot.bullet) return;

			slot.inUse = false;
			slot.bullet->SetActive(false);
			slot.bullet->OnReturnToPool();

			if (auto transform = slot.bullet->GetComponent<Transform>(false))
			{
				transform->SetPosition(Vec3(0.0f, -100.0f, 0.0f));
				transform->SetScale(Vec3(1.0f, 1.0f, 1.0f));
			}
		}

	public:
		explicit BulletPool(const std::shared_ptr<Stage>& stagePtr)
			: IBulletPool(stagePtr)
		{
		}
		virtual ~BulletPool() = default;

		void OnCreate() override
		{
			m_bulletSlots.reserve(poolSize);

			auto stagePtr = GetStage();
			if (!stagePtr) return;

			for (size_t i = 0; i < poolSize; ++i)
			{
				TransParam initParam;
				initParam.position = Vec3(0.0f, -100.0f, 0.0f);
				initParam.scale = Vec3(1.0f, 1.0f, 1.0f);
				initParam.quaternion = Quat();

				// Stageに弾オブジェクトを事前生成
				auto bullet = stagePtr->AddGameObject<BulletType>(initParam);

				BulletSlot slot;
				slot.bullet = bullet;
				ReturnSlotToPool(slot);

				m_bulletSlots.push_back(std::move(slot));
				m_freeIndices.push_back(static_cast<uint32_t>(i));
			}
		}

		/// <summary>
		/// 重要：弾の「更新（移動・寿命）」は Stage が行う。
		/// BulletPool は「非アクティブになった弾を回収して Free に戻す」だけ担当。
		/// </summary>
		void OnUpdate(double /*elapsedTime*/) override
		{
			for (size_t i = 0; i < m_bulletSlots.size(); ++i)
			{
				auto& slot = m_bulletSlots[i];
				if (!slot.inUse || !slot.bullet) continue;

				// 弾が終了していれば回収
				if (!slot.bullet->IsActive())
				{
					ReturnSlotToPool(slot);
					m_freeIndices.push_back(static_cast<uint32_t>(i));
				}
			}
		}

		void AllClear() override
		{
			m_freeIndices.clear();

			for (size_t i = 0; i < m_bulletSlots.size(); ++i)
			{
				auto& slot = m_bulletSlots[i];
				if (!slot.bullet) continue;

				ReturnSlotToPool(slot);
				m_freeIndices.push_back(static_cast<uint32_t>(i));
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

			if (!m_freeIndices.empty())
			{
				const uint32_t index = m_freeIndices.back();
				m_freeIndices.pop_back();

				auto& slot = m_bulletSlots[index];

				if (auto trans = slot.bullet->GetComponent<Transform>())
				{
					trans->SetPosition(pos);
					trans->SetQuaternion(rot);
					trans->SetScale(scale);
				}

				// ResetForSpawn の前に、ターゲット等をセット
				std::forward<SetupFn>(setup)(*slot.bullet);

				// ターゲット設定後に弾固有の初期化と有効化を行う。
				ActivateSlot(slot);
				return;
			}

			// 足りなければ追加生成
			TransParam tp;
			tp.position = pos;
			tp.quaternion = rot;
			tp.scale = scale;

			auto bullet = stagePtr->AddGameObject<BulletType>(tp);
			BulletSlot slot;
			slot.bullet = bullet;
			std::forward<SetupFn>(setup)(*slot.bullet);
			ActivateSlot(slot);
			m_bulletSlots.push_back(std::move(slot));
		}
	};

}
