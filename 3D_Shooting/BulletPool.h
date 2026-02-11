#pragma once
#include "stdafx.h"

namespace shooting {

	// 弾丸ハンドル（インデックスベース）
	struct BulletHandle
	{
		uint32_t index;
		bool isValid() const { return index != UINT32_MAX; }
		static BulletHandle Invalid() { return {UINT32_MAX}; }
	};

	template<typename BulletType>
	class BulletPool : public IBulletPool
	{
	private:
		// 弾丸データを連続メモリに格納
		//struct BulletSlot
		//{
		//	std::shared_ptr<BulletType> bullet;
		//	bool active;
		//	
		//	BulletSlot() : bullet(nullptr), active(false) {}
		//};
		struct BulletSlot
		{
			std::shared_ptr<BulletType> bullet;
			bool active = false;
		};

		std::vector<BulletSlot> m_BulletSlots;  // 連続メモリ
		std::vector<uint32_t> m_FreeIndices;     // 再利用可能なインデックス
		
	public:
		//BulletPool(const std::shared_ptr<Stage>& stagePtr);
		explicit BulletPool(const std::shared_ptr<Stage>& stagePtr)
			: IBulletPool(stagePtr)
		{
		}
		virtual ~BulletPool() = default;
		
		void OnCreate() override
		{
			constexpr size_t POOL_SIZE = 100;
			m_BulletSlots.reserve(POOL_SIZE);

			auto stagePtr = GetStage();
			if (!stagePtr) return;

			for (size_t i = 0; i < POOL_SIZE; ++i)
			{
				TransParam initParam;
				initParam.position = Vec3(0.0f, -100.0f, 0.0f);
				initParam.scale = Vec3(1.0f, 1.0f, 1.0f);
				initParam.quaternion = Quat();

				auto bullet = stagePtr->AddGameObject<BulletType>(initParam);
				bullet->SetUpdateActive(false);

				BulletSlot slot;
				slot.bullet = bullet;
				slot.active = false;

				m_BulletSlots.push_back(std::move(slot));
				m_FreeIndices.push_back(static_cast<uint32_t>(i));
			}
		}

		void OnUpdate(double elapsedTime) override
		{
			for (size_t i = 0; i < m_BulletSlots.size(); ++i)
			{
				auto& slot = m_BulletSlots[i];
				if (!slot.active || !slot.bullet) continue;

				slot.bullet->OnUpdate(elapsedTime);

				// DefaultBullet の寿命切れで戻す（他の弾種は必要なら同様に判定を追加）
				if (auto db = std::dynamic_pointer_cast<DefaultBullet>(slot.bullet))
				{
					if (!db->IsActive())
					{
						slot.active = false;
						slot.bullet->SetUpdateActive(false);

						auto trans = slot.bullet->GetComponent<Transform>();
						trans->SetPosition(Vec3(0.0f, -100.0f, 0.0f));

						m_FreeIndices.push_back(static_cast<uint32_t>(i));
					}
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
				slot.bullet->SetUpdateActive(false);

				auto trans = slot.bullet->GetComponent<Transform>();
				trans->SetPosition(Vec3(0.0f, -100.0f, 0.0f));

				if (auto db = std::dynamic_pointer_cast<DefaultBullet>(slot.bullet))
				{
					db->SetActive(false);
				}

				m_FreeIndices.push_back(static_cast<uint32_t>(i));
			}
		}
		
		BulletType* GetBullet(BulletHandle handle);

		// 発射：位置と回転を渡して弾を再利用アクティブ化
		void Spawn(const Vec3& pos, const Quat& rot, const Vec3& scale)
		{
			auto stagePtr = GetStage();
			if (!stagePtr) return;

			if (!m_FreeIndices.empty())
			{
				uint32_t index = m_FreeIndices.back();
				m_FreeIndices.pop_back();

				auto& slot = m_BulletSlots[index];
				slot.active = true;

				auto trans = slot.bullet->GetComponent<Transform>();
				trans->SetPosition(pos);
				trans->SetQuaternion(rot);
				trans->SetScale(scale);

				if (auto db = std::dynamic_pointer_cast<DefaultBullet>(slot.bullet))
				{
					db->ResetLife();   // ← 下で追加する関数
					db->SetActive(true);
				}

				slot.bullet->SetUpdateActive(true);
				return;
			}

			// 満杯なら追加（避けたいが保険）
			TransParam tp;
			tp.position = pos;
			tp.quaternion = rot;
			tp.scale = scale;

			auto bullet = stagePtr->AddGameObject<BulletType>(tp);

			BulletSlot slot;
			slot.bullet = bullet;
			slot.active = true;
			m_BulletSlots.push_back(std::move(slot));
		}
	};
}