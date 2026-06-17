#pragma once
#include "stdafx.h"

namespace shooting {

	class BaseItem : public GameObject
	{
	private:
		Vec3 m_basePosition;
		float m_time = 0.0f;
		bool m_consumed = false;

		void TryPickup(const CollisionPair& pair);
		void Consume();

	protected:
		BaseItem(
			const std::shared_ptr<Stage>& stagePtr,
			const TransParam& param);

		virtual bool CanPickupBy(const std::shared_ptr<GameObject>& collector) const;
		virtual bool ApplyItemEffect(const std::shared_ptr<GameObject>& collector) = 0;
		virtual void OnCreateItem() {}
		virtual void OnUpdateItem(double elapsedTime) {}

	public:
		virtual ~BaseItem() {}

		virtual void OnCreate() override;
		virtual void OnUpdate(double elapsedTime) override;

		bool TryPickupBy(const std::shared_ptr<GameObject>& collector);
		bool IsConsumed() const { return m_consumed; }

		virtual void OnCollisionEnter(const CollisionPair& pair) override;
		virtual void OnCollisionExecute(const CollisionPair& pair) override;
	};

	class HpRecoveryItem : public BaseItem
	{
	private:
		float m_healRate;

	protected:
		virtual bool ApplyItemEffect(const std::shared_ptr<GameObject>& collector) override;
		virtual void OnCreateItem() override;

	public:
		HpRecoveryItem(
			const std::shared_ptr<Stage>& stagePtr,
			const TransParam& param,
			float healRate = 0.25f);
		virtual ~HpRecoveryItem() {}
	};

	class BombItem : public BaseItem
	{
	private:
		int m_bombGrantCount;

	protected:
		virtual bool ApplyItemEffect(const std::shared_ptr<GameObject>& collector) override;
		virtual void OnCreateItem() override;

	public:
		BombItem(
			const std::shared_ptr<Stage>& stagePtr,
			const TransParam& param,
			int bombGrantCount = 5);
		virtual ~BombItem() {}
	};

}
