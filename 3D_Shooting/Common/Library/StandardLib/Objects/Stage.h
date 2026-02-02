#pragma once
#include "IObject.h"
#include "GameObject.h"

namespace shooting {

	class Stage : public IObject
	{
		bool m_updateActive;
		void PushBackGameObject(const std::shared_ptr<GameObject>& ptr);
		void RemoveBackGameObject(const std::shared_ptr<GameObject>& ptr);
		//追加・削除まちオブジェクトの追加と削除
		void RemoveTargetGameObject(const std::shared_ptr<GameObject>& targetobj);
		void SetWaitToObjectVec();
		//ゲームオブジェクトの配列
		std::vector<std::shared_ptr<GameObject>> m_gameObjectVec;
		//追加待ちのゲームオブジェクト
		std::vector<std::shared_ptr<GameObject>> m_waitAddObjectVec;
		//削除待ちのゲームオブジェクト
		std::vector<std::shared_ptr<GameObject>> m_waitRemoveObjectVec;
		//シェアオブジェクトポインタのマップ
		std::map<const std::wstring, std::weak_ptr<GameObject> > m_SharedMap;
		//シェアグループのポインタのマップ
		std::map<const std::wstring, std::shared_ptr<GameObjectGroup> >  m_SharedGroupMap;

		// コリジョン管理者
		std::shared_ptr<CollisionManager> m_collisionManager;
	protected:
		ID3D12Device* m_pDevice;
		std::shared_ptr<Camera> m_camera;
		std::shared_ptr<LightSet> m_lightSet;
		Stage(ID3D12Device* pDevice) :
			m_pDevice(pDevice),
			m_updateActive(true)
		{
		}
		virtual ~Stage() {}
	public:
		template<typename T, typename... Ts>
		std::shared_ptr<T> AddGameObject(Ts&&... params)
		{
			try
			{
				auto ptr = ObjectFactory::Create<T>(GetThis<Stage>(), params...);
				m_gameObjectVec.push_back(ptr);
				return ptr;
			}
			catch (...)
			{
				throw;
			}
		}

		/// <summary>
		/// パラメータをOnCreateWithParamに渡すゲームオブジェクトを追加する
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <typeparam name="...Ts"></typeparam>
		/// <param name="...params"></param>
		/// <returns></returns>
		template<typename T, typename... Ts>
		std::shared_ptr<T> AddGameObjectWithParam(Ts&&... params)
		{
			try
			{
				auto ptr = ObjectFactory::CreateGameObjectWithParam<T>(GetThis<Stage>(), params...);
				PushBackGameObject(ptr);
				return ptr;
			}
			catch (...)
			{
				throw;
			}
		}

		// アクセサ
		std::shared_ptr<Camera> GetCamera() const
		{
			return m_camera;
		}
		std::shared_ptr<LightSet> GetLightSet() const
		{
			return m_lightSet;
		}
		bool IsUpdateActive() const
		{
			return m_updateActive;
		}
		void SetUpdateActive(bool active)
		{
			m_updateActive = active;
		}
		std::shared_ptr<CollisionManager> GetCollisionManager() const
		{
			return m_collisionManager;
		}
		std::vector<std::shared_ptr<GameObject>>& GetGameObjectVec()
		{
			return m_gameObjectVec;
		}
		const std::vector<std::shared_ptr<GameObject>>& GetGameObjectVec() const
		{
			return m_gameObjectVec;
		}
		void RemoveGameObject(const std::shared_ptr<GameObject>& Obj);
		
		/// <summary>
		/// 共有されてるゲームオブジェクトを取得する
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="key"></param>
		/// <param name="exceptionActive">見つからないときに例外を発行するかどうか</param>
		/// <returns>共有されてるゲームオブジェクト</returns>
		template<typename T>
		std::shared_ptr<T> GetSharedGameObjectEx(const std::wstring& key, bool exceptionActive = true) const
		{
			std::shared_ptr<T> ptr = std::dynamic_pointer_cast<T>(GetSharedGameObject(key, exceptionActive));
			if (ptr)
			{
				return ptr;
			}
			else
			{
				if (exceptionActive)
				{
					// 例外発生
					std::wstring keyerr = key;
					std::wstring str = L"オブジェクトを";
					str += Util::GetWSTypeName<T>();
					str += L"型にキャストできません";
					throw BaseException(
						str,
						keyerr,
						L"Stage::GetSharedGameObjectEx<T>()"
					);
				}
			}
			return nullptr;
		}
		std::shared_ptr<GameObject> GetSharedGameObject(const std::wstring& key, bool exceptionActive = true) const;
		// ゲームオブジェクトを共有する
		void SetSharedGameObject(const std::wstring& key, const std::shared_ptr<GameObject>& ptr);
		// 共有するゲームオブジェクトグループを作成する
		std::shared_ptr<GameObjectGroup> CreateSharedObjectGroup(const std::wstring& key);
		// 共有されてるゲームオブジェクトグループを取得する
		std::shared_ptr<GameObjectGroup> GetSharedObjectGroup(const std::wstring& key, bool exceptionActive = true) const;
		// 共有されてるゲームオブジェクトグループを取得（グループ派生クラスを作った場合用）
		template<typename T>
		std::shared_ptr<T> GetSharedObjectGroup(const std::wstring& key, bool exceptionActive = true) const
		{
			auto returnPtr = std::dynamic_pointer_cast<T> basePtr(GetSharedObjectGroup(key, exceptionActive));
			if (returnPtr)
			{
				return returnPtr;
			}
			else
			{
				if (exceptionActive)
				{
					//例外発生
					std::wstring keyerr = key;
					throw BaseException(
						L"指定のキーのグループはT型に変換できません",
						keyerr,
						L"Stage::GetSharedObjectGroup<T>()"
					);
				}
			}
			return nullptr;
		}
		// 共有するゲームオブジェクトグループを設定する（グループ派生クラスを作った場合用）
		void SetSharedObjectGroup(const std::wstring& key, const std::shared_ptr<GameObjectGroup>& newPtr);
		// 指定のタグを持つゲームオブジェクトの配列を取得する
		void GetUsedTagObjectVec(const std::wstring& tag, std::vector<std::shared_ptr<GameObject>>& objVec) const
		{
			for (const auto& v : GetGameObjectVec())
			{
				if (v->FindTag(tag))
				{
					objVec.push_back(v);
				}
			}
		}
		// ゲームオブジェクトで指定のコンポーネントの親か子のコンポーネントを持つ場合、そのコンポーネントの配列を取得する
		template<typename T>
		void GetUsedDynamicComponentVec(std::vector<std::shared_ptr<T>>& compVec) const
		{
			for (const auto& v : GetGameObjectVec())
			{
				auto ptr = v->GetDynamicComponent<T>(false);
				if (ptr)
				{
					compVec.push_back(ptr);
				}
			}
		}
		// 指定のコンポーネントの親か子のコンポーネントを持つオブジェクトの配列を設定する
		template<typename T>
		void GetUsedDynamicComponentObjectVec(std::vector<std::shared_ptr<GameObject>>& objVec) const
		{
			for (const auto& v : GetGameObjectVec())
			{
				auto ptr = v->GetDynamicComponent<T>(false);
				if (ptr)
				{
					objVec.push_back(v);
				}
			}
		}

		// ステージの更新（シーンから呼ばれる）
		virtual void UpdateStage();
		// 衝突判定の更新（ステージから呼ぶ）
		virtual void UpdateCollision();

		virtual void OnUpdateConstantBuffers();
		virtual void OnCommitConstantBuffers();
		virtual void SetToBefore();

		virtual void OnPreCreate()override;
		virtual void OnCreate()override {}
		virtual void OnUpdate(double elapsedTime)override {}
		virtual void OnUpdate2(double elapsedTime)override {}
		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)override;
		virtual void OnDestroy()override;
	};
}