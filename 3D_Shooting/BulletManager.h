#pragma once
#include "stdafx.h"

#include "IBulletPool.h"
#include "BulletPool.h"
#include "Bullet.h"
#include "BulletType.h"
#include <utility>

namespace shooting {

	class BulletManager : public GameObject
	{
	private:
		std::vector<std::unique_ptr<IBulletPool>> m_Pools;

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

			if (m_Pools.size() <= static_cast<std::size_t>(typeId))
			{
				m_Pools.resize(static_cast<std::size_t>(typeId) + 1);
			}

			if (!m_Pools[typeId])
			{
				auto p = std::make_unique<BulletPool<BulletT>>(GetStage());
				p->OnCreate(); // 事前生成
				m_Pools[typeId] = std::move(p);
			}

			return static_cast<BulletPool<BulletT>*>(m_Pools[typeId].get());
		}

		// テンプレの発射API
		template <class BulletT>
		void Fire(const Vec3& pos, const Quat& rot, const Vec3& scale = Vec3(0.1f, 0.1f, 0.1f))
		{
			GetOrCreatePool<BulletT>()->Spawn(pos, rot, scale);
		}

		void FireBombTo(const Vec3& pos, const Quat& rot, const Vec3& target, const Vec3& scale = Vec3(0.1f, 0.1f, 0.1f))
		{
			// 「Spawnする直前に SetTarget」できるよう、Pool側に preReset を渡す版が理想
			GetOrCreatePool<BombBullet>()->Spawn(pos, rot, scale, [&](BombBullet& b){
				b.SetTarget(target);
			});
		}

		template <class BulletT, class SetupFn>
		void FireEx(const Vec3& pos, const Quat& rot,
					const Vec3& scale, SetupFn&& setup)
		{
			GetOrCreatePool<BulletT>()->Spawn(pos, rot, scale, std::forward<SetupFn>(setup));
		}

		// BulletType で切り替える発射API
		void FireByType(BulletType type, const Vec3& pos, const Quat& rot, const Vec3& scale = Vec3(0.1f, 0.1f, 0.1f))
		{
			switch (type)
			{
			case BulletType::Default:
				Fire<DefaultBullet>(pos, rot, scale);
				break;
			case BulletType::Bomb:
				Fire<BombBullet>(pos, rot, scale);
				break;
			default:
				Fire<DefaultBullet>(pos, rot, scale);
				break;
			}
		}
	};

}
