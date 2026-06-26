#pragma once
#include "stdafx.h"

#include "IBulletPool.h"
#include "BulletPool.h"
#include "Bullet.h"
#include <utility>

namespace shooting {

	class BulletManager : public GameObject
	{
	private:
		std::vector<std::unique_ptr<IBulletPool>> m_pools;

	public:
		explicit BulletManager(const std::shared_ptr<Stage>& stagePtr);
		virtual ~BulletManager() = default;

		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;

		inline int NextTypeId()
		{
			static int id = 0;
			return id++;
		}

		template <class Ty_>
		inline int GetTypeId()
		{
			static int id = NextTypeId();
			return id;
		}

		// 指定型のプールを取得（無ければ作る）
		template <class BulletT>
		BulletPool<BulletT>* GetOrCreatePool()
		{
			const int typeId = GetTypeId<BulletT>();

			if (m_pools.size() <= static_cast<std::size_t>(typeId))
			{
				m_pools.resize(static_cast<std::size_t>(typeId) + 1);
			}

			if (!m_pools[typeId])
			{
				auto p = std::make_unique<BulletPool<BulletT>>(GetStage());
				p->OnCreate(); // 事前生成
				m_pools[typeId] = std::move(p);
			}

			return static_cast<BulletPool<BulletT>*>(m_pools[typeId].get());
		}

		template <class BulletT, class SetupFn>
		void FireEx(const Vec3& pos, const Quat& rot,
					const Vec3& scale, SetupFn&& setup)
		{
			GetOrCreatePool<BulletT>()->Spawn(pos, rot, scale, std::forward<SetupFn>(setup));
		}
	};

}
