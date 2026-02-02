#pragma once
#include "IObject.h"
#include <vector>
#include <stdexcept>

namespace shooting {

	class GameObject;

	class Component : public IObject
	{
	private:
		bool m_updateActive;
		bool m_drawActive;
	protected:
		std::weak_ptr<GameObject> m_gameObject;

		std::vector<std::weak_ptr<BaseMesh>> m_meshVec;
		std::vector<std::weak_ptr<BaseTexture>> m_textureVec;

		explicit Component(const std::shared_ptr<GameObject>& gameObjectPtr);
		virtual ~Component() {}
	public:
		void AttachGameObject(const std::shared_ptr<GameObject>& gameObjectPtr)
		{
			m_gameObject = gameObjectPtr;
		}
		std::shared_ptr<GameObject> GetGameObject() const
		{
			auto ptr = m_gameObject.lock();
			if (!ptr)
			{
				/*throw BaseException(
					L"GameObjectは有効ではありません",
					L"if (!shptr)",
					L"Component::GetGameObject()const"
				);*/
				throw std::runtime_error("GameObjectは有効ではありません");
			}
			else
			{
				return ptr;
			}
			return nullptr;
		}


		size_t AddBaseMesh(const std::shared_ptr<BaseMesh>& mesh)
		{
			size_t size = m_meshVec.size();
			m_meshVec.push_back(mesh);
			return size;
		}
		size_t AddBaseMesh(const std::wstring& key)
		{
			return AddBaseMesh(BaseScene::Get()->GetMesh(key));
		}

		std::shared_ptr<BaseMesh> GetBaseMesh(size_t index)
		{
			if (index >= m_meshVec.size())
			{
				throw BaseException(
					L"指定のインデックスが範囲外です",
					Util::SizeTToWStr(index),
					L"BaseScene::GetBaseMesh()"
				);
			}
			else
			{
				auto shptr = m_meshVec[index].lock();
				if (shptr)
				{
					return shptr;
				}
				else
				{
					throw BaseException(
						L"指定のメッシュは有効ではありません",
						Util::SizeTToWStr(index),
						L"BaseScene::GetMesh()"
					);

				}
			}
			return nullptr;
		}

		size_t AddBaseTexture(const std::shared_ptr<BaseTexture>& texture)
		{
			size_t size = m_textureVec.size();
			m_textureVec.push_back(texture);
			return size;
		}
		size_t AddBaseTexture(const std::wstring& key)
		{
			return AddBaseTexture(BaseScene::Get()->GetTexture(key));
		}

		std::shared_ptr<BaseTexture> GetBaseTexture(size_t index)
		{
			if (index >= m_textureVec.size())
			{
				return nullptr;
				//throw BaseException(
				//	L"指定のインデックスが範囲外です",
				//	Util::SizeTToWStr(index),
				//	L"BaseScene::GetBaseTexture()"
				//);
			}
			else
			{
				auto shptr = m_textureVec[index].lock();
				if (shptr)
				{
					return shptr;
				}
				else
				{
					return nullptr;
					//throw BaseException(
					//	L"指定のテクスチャは有効ではありません",
					//	Util::SizeTToWStr(index),
					//	L"BaseScene::GetBaseTexture()"
					//);

				}
			}
			return nullptr;
		}

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

		virtual void OnUpdateConstantBuffers() {}
		virtual void OnCommitConstantBuffers() {}

		virtual void OnPreCreate() override {}
		virtual void OnCreate()override {}
		virtual void OnUpdate(double elapsedTime)override {}
		virtual void OnShadowDraw(ID3D12GraphicsCommandList* pCommandList)override {}
		virtual void OnSceneDraw(ID3D12GraphicsCommandList* pCommandList)override {}
		virtual void OnDestroy()override {}
	};
}