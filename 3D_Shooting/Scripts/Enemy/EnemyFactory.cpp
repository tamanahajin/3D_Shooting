#include "stdafx.h"
#include "EnemyFactory.h"
#include "EnemyController.h"

namespace shooting {

	EnemyFactory::EnemyFactory(const std::shared_ptr<EnemyController>& controller) :
		m_controller(controller)
	{
		m_statusByKind[EnemyKind::Default] = EnemyStatus();
	}

	void EnemyFactory::SetController(const std::shared_ptr<EnemyController>& controller)
	{
		m_controller = controller;
	}

	bool EnemyFactory::IsValid() const
	{
		return !m_controller.expired();
	}

	void EnemyFactory::SetStatus(EnemyKind kind, const EnemyStatus& status)
	{
		m_statusByKind[kind] = status;
	}

	EnemyStatus EnemyFactory::GetStatus(EnemyKind kind) const
	{
		auto it = m_statusByKind.find(kind);
		if (it != m_statusByKind.end())
		{
			return it->second;
		}

		auto defaultIt = m_statusByKind.find(EnemyKind::Default);
		if (defaultIt != m_statusByKind.end())
		{
			return defaultIt->second;
		}

		return EnemyStatus();
	}

	size_t EnemyFactory::CreateEnemy(EnemyKind kind, const Vec3& position) const
	{
		return CreateEnemy(kind, position, GetStatus(kind));
	}
	size_t EnemyFactory::CreateEnemy(EnemyKind kind, const Vec3& position, const EnemyStatus& status) const
	{
		return CreateEnemyAtResolvedPosition(kind, position, status);
	}

	size_t EnemyFactory::CreateEnemyAtResolvedPosition(
		EnemyKind kind,
		const Vec3& position,
		const EnemyStatus& status) const
	{
		auto controller = m_controller.lock();
		if (!controller)
		{
			return static_cast<size_t>(-1);
		}

		// 今後、敵種別ごとのモデル差分が必要になったら、このswitchに分岐を追加する。
		switch (kind)
		{
		case EnemyKind::Default:
		default:
			return controller->AddEnemy(position, status);
		}
	}

}
