#pragma once
#include "stdafx.h"

namespace shooting {

	class Stage;
	class Behavior;
	class Collision;
	struct CollisionPair;

	/// <summary>
	/// 配置されるオブジェクトの親
	/// </summary>
	class GameObject : public IObject
	{
	private:
		std::weak_ptr<Stage> m_stage;
		
		// updateの有効無効
		bool m_updateActive;
		// drawの有効無効
		bool m_drawActive;
		// 透明かどうか
		bool m_alphaActive;

		// 行動のマップ
		std::map<std::type_index, std::shared_ptr<Behavior>> m_behaviorMap;
		// コンポーネントのマップ
		std::map<std::type_index, std::shared_ptr<Component>> m_componentMap;
		// コンポーネント実行順番
		std::list<std::type_index> m_componentOrder;
		// 指定のコンポーネントを探す（派生クラスも検索）
		template<typename T>
		std::shared_ptr<T> SearchDynamicComponent() const
		{
			auto it = m_componentMap.begin();
			while (it != m_componentMap.end())
			{
				auto ptr = std::dynamic_pointer_cast<T>(it->second);
				if (ptr)
				{
					return ptr;
				}
				++it;
			}
			return nullptr;
		}

		int m_drawLayer = 0; // 描画レイヤー
		std::set<std::wstring> m_tagSet; // タグセット
		std::set<int> m_numTagSet; // 数字タグのセット

		const std::map<std::type_index, std::shared_ptr<Behavior> >& GetBehaviorMap() const
		{
			return m_behaviorMap;
		}
		std::map<std::type_index, std::shared_ptr<Component> >& GetComponentMap()
		{
			return m_componentMap;
		}
		std::shared_ptr<Behavior> SearchBehavior(std::type_index TypeIndex) const
		{
			auto it = m_behaviorMap.find(TypeIndex);
			if (it != m_behaviorMap.end())
			{
				return it->second;
			}
			return nullptr;
		}

		/// <summary>
		/// 作成されたビヘイビアを型インデックスと関連付けてマップに追加します。
		/// </summary>
		/// <param name="TypeIndex">ビヘイビアの型を識別する型インデックス。</param>
		/// <param name="behavior">追加するビヘイビアオブジェクトへの共有ポインタ。</param>
		void AddMakedBehavior(std::type_index TypeIndex, const std::shared_ptr<Behavior>& behavior)
		{
			m_behaviorMap[TypeIndex] = behavior;
		}

	protected:
		TransParam m_transParam;
		GameObject(const std::shared_ptr<Stage>& stage) :
			m_stage(stage),
			m_transParam(),
			m_updateActive(true),
			m_drawActive(true),
			m_alphaActive(false)
		{
		}
		virtual ~GameObject() {}

	public:
		std::shared_ptr<Stage> GetStage(bool exceptionActive = true) const;
		bool IsUpdateActive() const
		{
			return m_updateActive;
		}
		void SetUpdateActive(bool active)
		{
			m_updateActive = active;
		}
		bool IsDrawActive() const
		{
			return m_drawActive;
		}
		void SetDrawActive(bool active)
		{
			m_drawActive = active;
		}
		bool IsAlphaActive() const
		{
			return m_alphaActive;
		}
		void SetAlphaActive(bool active)
		{
			m_alphaActive = active;
		}

		virtual std::shared_ptr<Camera> GetCamera()const;
		virtual std::shared_ptr<LightSet> GetLightSet() const;

		template<typename T, typename... Ts>
		std::shared_ptr<T> AddComponent(Ts&&... params)
		{
			std::type_index typeIndex = std::type_index(typeid(T));
			auto ptr = SearchDynamicComponent<T>();
			// 指定の型のコンポーネントが見つかった
			if (ptr)
			{
				auto retPtr = std::dynamic_pointer_cast<T>(ptr);
				if (retPtr)
				{
					return retPtr;
				}
				else
				{
					throw std::runtime_error("既にコンポーネントが存在するが、型キャストに失敗しました。");
				}
				return ptr;
			}
			else
			{
				// 指定の型のコンポーネントが見つからなかったので新規作成
				std::shared_ptr<T> newPtr = ObjectFactory::Create<T>(GetThis<GameObject>(), params...);
				// そのコンポーネントがまだなければ新規登録
				m_componentOrder.push_back(typeIndex);
				// mapに追加もしくは更新
				m_componentMap[typeIndex] = newPtr;
				newPtr->AttachGameObject(GetThis<GameObject>());
				return newPtr;
			}
		}

		template <typename T>
		std::shared_ptr<T> GetComponent(bool exceptionActive = true)const
		{
			auto Ptr = SearchDynamicComponent<T>();
			if (Ptr)
			{
				//指定の型のコンポーネントが見つかった
				return Ptr;
			}
			else
			{
				if (exceptionActive)
				{
					throw BaseException(
						L"コンポーネントが見つかりません",
						Util::GetWSTypeName<T>(),
						L"GameObject::GetComponent<T>()"
					);
				}
			}
			return nullptr;
		}

		/// <summary>
		/// 指定された型のコンポーネントを削除します。
		/// </summary>
		/// <typeparam name="T">削除するコンポーネントの型。</typeparam>
		template <typename T>
		void RemoveComponent()
		{
			auto typeIndex = type_index(typeid(T));
			//順番リストを検証して削除
			auto it = m_componentOrder.begin();
			while (it != m_componentOrder.end())
			{
				if (*it == typeIndex)
				{
					auto it2 = m_componentMap.find(*it);
					if (it2 != m_componentMap.end())
					{
						//指定の型のコンポーネントが見つかった
						//mapデータを削除
						m_componentMap.erase(it2);
					}
					m_componentOrder.erase(it);
					return;
				}
				it++;
			}
		}

		/// <summary>
		/// 行動の取得。存在しなければ作成する
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <returns></returns>
		template <typename T>
		std::shared_ptr<T> GetBehavior()
		{
			auto Ptr = SearchBehavior(std::type_index(typeid(T)));
			if (Ptr)
			{
				//指定の型の行動が見つかった
				auto RetPtr = std::dynamic_pointer_cast<T>(Ptr);
				if (RetPtr)
				{
					return RetPtr;
				}
				else
				{
					throw std::runtime_error("行動がありましたが、型キャストできません");
				}
			}
			else
			{
				//無ければ新たに制作する
				std::shared_ptr<T> newPtr = ObjectFactory::Create<T>(GetThis<GameObject>());
				AddMakedBehavior(std::type_index(typeid(T)), newPtr);
				return newPtr;
			}
			return nullptr;
		}

		/// <summary>
		/// 行動の検索
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <returns></returns>
		template <typename T>
		bool FindBehavior()
		{
			auto Ptr = SearchBehavior(type_index(typeid(T)));
			if (Ptr)
			{
				//指定の型の行動が見つかった
				auto RetPtr = dynamic_pointer_cast<T>(Ptr);
				if (RetPtr)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			return false;
		}

		const std::set<std::wstring>& GetTagSet() const
		{
			return m_tagSet;
		}
		std::set<std::wstring>& GetTagSet()
		{
			return m_tagSet;
		}
		bool FindTag(const std::wstring& tagstr) const
		{
			if (m_tagSet.find(tagstr) == m_tagSet.end())
			{
				return false;
			}
			return true;
		}
		void  AddTag(const std::wstring& tagstr)
		{
			if (tagstr == L"")
			{
				//空白なら例外
				throw BaseException(
					L"設定するタグが空です",
					L"if (tagstr == L"")",
					L"GameObject::AddTag()"
				);
			}
			m_tagSet.insert(tagstr);
		}
		void  RemoveTag(const std::wstring& tagstr)
		{
			m_tagSet.erase(tagstr);
		}

		const std::set<int>& GetNumTagSet() const
		{
			return m_numTagSet;
		}
		std::set<int>& GetNumTagSet()
		{
			return m_numTagSet;
		}
		bool FindNumTag(int numtag) const
		{
			if (m_numTagSet.find(numtag) == m_numTagSet.end())
			{
				return false;
			}
			return true;
		}
		void  AddNumTag(int numtag)
		{
			m_numTagSet.insert(numtag);
		}
		void  RemoveNumTag(int numtag)
		{
			m_numTagSet.erase(numtag);
		}

		/// <summary>
		/// 衝突発生時のイベント（デフォルトは何もしない）。複数あった場合は複数回呼ばれる
		/// </summary>
		/// <param name="Other"></param>
		virtual void OnCollisionEnter(std::shared_ptr<GameObject>& Other) {}
		virtual void OnCollisionEnter(const CollisionPair& Pair) {}

		/// <summary>
		/// 衝突し続ける相手があった場合のイベント（デフォルトは何もしない）。複数あった場合は複数回呼ばれる
		/// </summary>
		/// <param name="Other"></param>
		virtual void OnCollisionExecute(std::shared_ptr<GameObject>& Other) {}
		virtual void OnCollisionExecute(const CollisionPair& Pair) {}

		/// <summary>
		/// 衝突を抜けた相手があった場合のイベント（デフォルトは何もしない）。複数あった場合は複数回呼ばれる
		/// </summary>
		/// <param name="Other"></param>
		virtual void OnCollisionExit(std::shared_ptr<GameObject>& Other) {}
		virtual void OnCollisionExit(const CollisionPair& Pair) {}

		void ComponentUpdate();
		void TransformInit();
		void ComponentShadowmapRender();
		void ComponentRender();
		void ComponentDestroy();

		virtual void OnUpdateConstantBuffers();
		virtual void OnCommitConstantBuffers();
		virtual void OnPreCreate()override;
		virtual void OnCreate()override {}
		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)override;
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)override;
		virtual void OnDestroy()override {}
		virtual void SetToBefore();
	};

	/// <summary>
	/// ゲームオブジェクトのweak_ptrをグループ化したクラス
	/// </summary>
	class GameObjectGroup : public IObject
	{
		std::vector<std::weak_ptr<GameObject>> m_Group;
	public:
		GameObjectGroup();
		virtual ~GameObjectGroup();
		/// <summary>
		/// グループ内のオブジェクトのweak_ptr配列を取得する
		/// </summary>
		/// <returns></returns>
		const std::vector<std::weak_ptr<GameObject> >& GetGroupVector() const;
		/// <summary>
		/// グループ内のゲームオブジェクトを取得
		/// </summary>
		/// <param name="index">グループ内オブジェクトのインデックス</param>
		/// <returns></returns>
		std::shared_ptr<GameObject> GameObjectAt(size_t index);
		/// <summary>
		/// グループ内のゲームオブジェクトの数を取得
		/// </summary>
		/// <returns></returns>
		size_t size() const;
		/// <summary>
		/// グループにゲームオブジェクトを追加する
		/// </summary>
		/// <param name="obj"></param>
		void IntoGroup(const std::shared_ptr<GameObject>& obj);
		/// <summary>
		/// グループをクリア
		/// </summary>
		void AllClear();

		virtual void OnCreate()override {}
		virtual void OnUpdate(double elapsedTime) override {}
		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)override {}
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)override {}
		virtual void OnDestroy() override {}
	};
}