#pragma once
#include "IObject.h"
#include "BaseMath.h"

using namespace shooting::bsm;

namespace shooting {

	class GameObject;

	/// <summary>
	/// 行動クラスの親クラス
	/// </summary>
	class Behavior : public IObject
	{
		std::weak_ptr<GameObject> m_gameObject;
	protected:
		explicit Behavior(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~Behavior();
	public:
		/// <summary>
		/// ゲームオブジェクトを取得します。
		/// </summary>
		/// <returns>このコンポーネントを所持するゲームオブジェクト</returns>
		std::shared_ptr<GameObject> GetGameObject() const;

		std::shared_ptr<Stage> GetStage() const;

		virtual void OnCreate() override {}

		virtual void OnUpdate(double elapsedTime) override {}

		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList) override {}
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList) override {}

		virtual void OnDestroy() override {}
	};

	/// <summary>
	/// 行動ユーティリティクラス
	/// </summary>
	class UtilBehavior : public Behavior
	{
	public:
		UtilBehavior(const std::shared_ptr<GameObject>& gameObjectPtr) : 
			Behavior(gameObjectPtr)
		{
		}

		virtual ~UtilBehavior() {}

		/// <summary>
		/// 進行方向を向くようにする
		/// </summary>
		/// <param name="LerpFact">補間係数（0.0から1.0の間）</param>
		void RotToHead(float LerpFact);

		/// <summary>
		/// 進行方向を向くようにする（速度指定方式）
		/// </summary>
		/// <param name="Velocity"></param>
		/// <param name="LerpFact"></param>
		void RotToHead(const Vec3& Velocity, float LerpFact);
	};

	template<typename T>
	class ObjBehavior : public IObject
	{
		std::weak_ptr<T> m_gameObject;
	protected:
		explicit ObjBehavior(const std::shared_ptr<T>& GameObjectPtr) :
			m_gameObject(GameObjectPtr)
		{
		}

		virtual ~ObjBehavior() {}

	public:
		/// <summary>
		/// ゲームオブジェクトを取得します。
		/// </summary>
		/// <returns>このコンポーネントを所持するゲームオブジェクト</returns>
		std::shared_ptr<T> GetGameObject() const
		{
			auto shPtr = m_gameObject.lock();
			if (!shPtr)
			{
				throw std::runtime_error("GameObject is expired.");
			}
			else
			{
				return shPtr;
			}
		}

		std::shared_ptr<Stage> GetStage() const
		{
			return GetGameObject()->GetStage();
		}

		virtual void OnCreate() override {}
	};
}