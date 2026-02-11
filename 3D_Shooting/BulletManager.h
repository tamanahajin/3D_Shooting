#pragma once
#include "stdafx.h"

namespace shooting {

	class BulletManager : public GameObject
	{
	private:
		std::vector<std::unique_ptr<IBulletPool>> m_Pools;
	public:
		explicit BulletManager(const std::shared_ptr<Stage>& stagePtr);
		virtual ~BulletManager() {}
		//構築時
		virtual void OnCreate()override;
		//更新時
		virtual void OnUpdate(double elapsedTime);

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

		//template <class BulletType, class... Args>
		//BulletType& Add(Args&&... args)
		//{
		//	// 各弾丸の型に一意のIDを割り当て
		//	const int typeId = GetTypeId<BulletType>();

		//	// 新しい型が初めて追加される場合、m_Poolsを拡張
		//	if (m_Pools.size() <= static_cast<std::size_t>(typeId))
		//	{
		//		m_Pools.resize(static_cast<std::size_t>(typeId) + 1);
		//	}

		//	// 指定された型のプールが存在しない場合、新規作成
		//	if (!m_Pools[typeId])
		//	{
		//		m_Pools[typeId] = std::make_unique<BulletPool<BulletType>>(GetStage());
		//	}

		//	// プールから弾丸を追加して返す
		//	// Perfect Forwarding : std::forward により、コンストラクタ引数を効率的に転送
		//	// emplace_back でその場構築(コピー / ムーブのオーバーヘッド削減)
		//	// 追加した弾丸への参照を返すことで、即座に設定変更が可能
		//	auto* pool = static_cast<BulletPool<BulletType>*>(m_Pools[typeId].get());
		//	pool->m_Bullets.emplace_back(std::forward<Args>(args)...);
		//	return pool->m_Bullets.back();
		//}

		// テンプレ：特定型のプールを取得（無ければ作る）
		template <class BulletType>
		BulletPool<BulletType>* GetOrCreatePool()
		{
			const int typeId = GetTypeId<BulletType>();

			if (m_Pools.size() <= static_cast<std::size_t>(typeId))
			{
				m_Pools.resize(static_cast<std::size_t>(typeId) + 1);
			}

			if (!m_Pools[typeId])
			{
				auto p = std::make_unique<BulletPool<BulletType>>(GetStage());
				p->OnCreate(); // ← プールの事前生成をここで実行
				m_Pools[typeId] = std::move(p);
			}

			return static_cast<BulletPool<BulletType>*>(m_Pools[typeId].get());
		}

		// プレイヤーなどから呼ぶ「発射API」
		/*template <class BulletType>
		void Fire(const Vec3& pos, const Quat& rot, const Vec3& scale = Vec3(1, 1, 1))
		{
			auto* pool = GetOrCreatePool<BulletType>();
			pool->Spawn(pos, rot, scale);
		}*/
		template <class BulletType>
		void Fire(const Vec3& pos, const Quat& rot, const Vec3& scale = Vec3(0.2f, 0.2f, 0.2f))
		{
			GetOrCreatePool<BulletType>()->Spawn(pos, rot, scale);
		}
	};
}